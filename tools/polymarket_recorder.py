#!/usr/bin/env python3
"""Record a Polymarket binary market's live order-book updates for replay."""

from __future__ import annotations

import argparse
import asyncio
import csv
import json
import sys
import time
from decimal import Decimal
from pathlib import Path
from typing import Any, Mapping, Sequence
from urllib.parse import quote

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools import polymarket_snapshot


WEBSOCKET_URL = "wss://ws-subscriptions-clob.polymarket.com/ws/market"
CSV_HEADER = polymarket_snapshot.CSV_HEADER
OUTCOME_BY_SIDE = {"BUY": "bid", "SELL": "ask"}


class RecorderError(RuntimeError):
    """Raised when the live feed cannot be recorded safely."""


class EventWriter:
    """Write normalized book-level updates in the replay CSV format."""

    def __init__(self, output_directory: Path, market: Mapping[str, Any]) -> None:
        if output_directory.exists() and any(output_directory.iterdir()):
            raise RecorderError(f"output directory is not empty: {output_directory}")
        output_directory.mkdir(parents=True, exist_ok=True)
        self.output_directory = output_directory
        self._raw_file = (output_directory / "raw.jsonl").open(
            "w", encoding="utf-8"
        )
        self._csv_file = (output_directory / "events.csv").open(
            "w", encoding="utf-8", newline=""
        )
        self._writer = csv.DictWriter(
            self._csv_file, fieldnames=CSV_HEADER, lineterminator="\n"
        )
        self._writer.writeheader()
        self._csv_file.flush()
        tokens = polymarket_snapshot.outcome_token_map(market)
        self.token_to_outcome = {token: outcome for outcome, token in tokens.items()}
        self.sequence = 0
        self.books: dict[tuple[str, str], dict[str, str]] = {}
        self.initialized_assets: set[str] = set()

    def close(self) -> None:
        self._csv_file.close()
        self._raw_file.close()

    def write_raw(self, message: Any) -> None:
        self._raw_file.write(json.dumps(message, separators=(",", ":")) + "\n")
        self._raw_file.flush()

    def _write_level(
        self, timestamp_ns: int, outcome: str, side: str, price: str, size: str
    ) -> None:
        self.sequence += 1
        self._writer.writerow(
            {
                "observed_at_ns": timestamp_ns,
                "sequence": self.sequence,
                "outcome_id": outcome,
                "side": side,
                "price": price,
                "quantity": size,
            }
        )

    @staticmethod
    def _timestamp_ns(value: Any) -> int:
        if value is None:
            return time.time_ns()
        try:
            timestamp_ms = int(value)
        except (TypeError, ValueError) as error:
            raise RecorderError("feed timestamp must be Unix milliseconds") from error
        if timestamp_ms < 0:
            raise RecorderError("feed timestamp cannot be negative")
        if timestamp_ms > 9_223_372_036_854:
            raise RecorderError(
                "feed timestamp is outside signed 64-bit nanosecond range"
            )
        return timestamp_ms * 1_000_000

    def _replace_book(
        self, asset_id: str, side: str, levels: Any, timestamp_ns: int
    ) -> None:
        if asset_id not in self.token_to_outcome:
            return
        outcome = self.token_to_outcome[asset_id]
        if not isinstance(levels, list):
            raise RecorderError(f"book {side} must be an array")

        new_levels: dict[str, str] = {}
        for level in levels:
            if not isinstance(level, dict):
                raise RecorderError("book levels must be objects")
            price = level.get("price")
            size = level.get("size")
            polymarket_snapshot._fixed_decimal(price, f"{side} price", Decimal("1"))
            polymarket_snapshot._fixed_decimal(
                size, f"{side} size", polymarket_snapshot.MAX_FIXED
            )
            new_levels[price] = size

        key = (asset_id, side)
        previous = self.books.get(key, {})
        for price in sorted(previous.keys() | new_levels.keys()):
            size = new_levels.get(price, "0")
            if previous.get(price) != size:
                self._write_level(timestamp_ns, outcome, side, price, size)
        self.books[key] = new_levels

    def handle_message(self, message: Mapping[str, Any]) -> None:
        event_type = message.get("event_type") or message.get("type")
        if event_type == "book":
            asset_id = message.get("asset_id")
            if not isinstance(asset_id, str):
                raise RecorderError("book message is missing asset_id")
            timestamp_ns = self._timestamp_ns(message.get("timestamp"))
            self._replace_book(asset_id, "bid", message.get("bids"), timestamp_ns)
            self._replace_book(asset_id, "ask", message.get("asks"), timestamp_ns)
            self.initialized_assets.add(asset_id)
        elif event_type == "price_change":
            changes = message.get("price_changes")
            if not isinstance(changes, list):
                raise RecorderError("price_change message is missing price_changes")
            timestamp_ns = self._timestamp_ns(message.get("timestamp"))
            for change in changes:
                if not isinstance(change, dict):
                    raise RecorderError("price changes must be objects")
                asset_id = change.get("asset_id")
                if asset_id not in self.token_to_outcome:
                    continue
                if asset_id not in self.initialized_assets:
                    raise RecorderError(
                        "received a price change before the initial book snapshot"
                    )
                side = OUTCOME_BY_SIDE.get(change.get("side"))
                if side is None:
                    raise RecorderError("price change side must be BUY or SELL")
                price = change.get("price")
                size = change.get("size")
                polymarket_snapshot._fixed_decimal(price, "price", Decimal("1"))
                polymarket_snapshot._fixed_decimal(
                    size, "size", polymarket_snapshot.MAX_FIXED
                )
                # Polymarket sends the replacement size at this price level,
                # not a signed quantity delta.
                self.books.setdefault((asset_id, side), {})[price] = size
                self._write_level(
                    timestamp_ns, self.token_to_outcome[asset_id], side, price, size
                )
        self._csv_file.flush()


async def record_market(
    event_slug: str,
    output_directory: Path,
    duration_seconds: int,
    market_slug: str | None = None,
) -> None:
    try:
        import websockets
    except ImportError as error:
        raise RecorderError(
            "missing WebSocket dependency; install with: "
            "python3 -m pip install -r requirements-recorder.txt"
        ) from error

    event = polymarket_snapshot.fetch_json(event_url_for_slug(event_slug))
    market = polymarket_snapshot.select_market(event, market_slug)
    tokens = polymarket_snapshot.outcome_token_map(market)
    writer = EventWriter(output_directory, market)
    started_at_ns: int | None = None
    metadata = {
        "schema_version": 1,
        "venue": "polymarket",
        "capture_type": "websocket_orderbook",
        "payout_per_set": "1.000000",
        "started_at_ns": None,
        "requested_duration_seconds": duration_seconds,
        "status": "incomplete",
        "market": {
            "id": market.get("id"),
            "slug": market.get("slug"),
            "question": market.get("question"),
            "condition_id": market.get("conditionId"),
        },
        "outcomes": [
            {"outcome_id": outcome, "token_id": token}
            for outcome, token in tokens.items()
        ],
        "data_gaps": "unexpected disconnect stops recording; inspect exit status",
    }
    (output_directory / "event.json").write_text(
        json.dumps(event, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_directory / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    try:
        async with websockets.connect(WEBSOCKET_URL, ping_interval=None) as socket:
            await socket.send(
                json.dumps({"assets_ids": list(tokens.values()), "type": "market"})
            )
            started_at_ns = time.time_ns()
            metadata["started_at_ns"] = started_at_ns
            (output_directory / "metadata.json").write_text(
                json.dumps(metadata, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            deadline = asyncio.get_running_loop().time() + duration_seconds
            next_heartbeat = asyncio.get_running_loop().time() + 10
            while True:
                remaining = deadline - asyncio.get_running_loop().time()
                if remaining <= 0:
                    break
                timeout = min(
                    remaining,
                    max(0.1, next_heartbeat - asyncio.get_running_loop().time()),
                )
                try:
                    raw_message = await asyncio.wait_for(socket.recv(), timeout=timeout)
                except asyncio.TimeoutError:
                    if asyncio.get_running_loop().time() >= deadline:
                        break
                    await socket.send("PING")
                    next_heartbeat = asyncio.get_running_loop().time() + 10
                    continue
                if raw_message == "PONG":
                    continue
                try:
                    decoded = json.loads(raw_message)
                except json.JSONDecodeError as error:
                    raise RecorderError("WebSocket sent invalid JSON") from error
                messages = decoded if isinstance(decoded, list) else [decoded]
                for message in messages:
                    if not isinstance(message, dict):
                        raise RecorderError("WebSocket message must be a JSON object")
                    writer.write_raw(message)
                    writer.handle_message(message)
            if writer.initialized_assets != set(tokens.values()):
                missing = sorted(set(tokens.values()) - writer.initialized_assets)
                raise RecorderError(
                    "recording ended before initial books arrived for token(s): "
                    + ", ".join(missing)
                )
            metadata["status"] = "complete"
    except websockets.exceptions.WebSocketException as error:
        raise RecorderError(
            f"WebSocket disconnected or failed before duration elapsed: {error}"
        ) from error
    finally:
        writer.close()
        metadata["ended_at_ns"] = time.time_ns()
        metadata["elapsed_seconds"] = (
            (metadata["ended_at_ns"] - started_at_ns) / 1e9
            if started_at_ns is not None
            else None
        )
        (output_directory / "metadata.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Record a Polymarket binary market's live order book."
    )
    parser.add_argument("event_slug", help="Polymarket event slug")
    parser.add_argument(
        "--market", dest="market_slug", help="market slug for a multi-market event"
    )
    parser.add_argument(
        "--output", type=Path, required=True, help="new or empty recording directory"
    )
    parser.add_argument(
        "--duration-seconds", type=int, required=True, help="how long to record"
    )
    return parser


def event_url_for_slug(event_slug: str) -> str:
    """Build the Gamma event lookup URL for a slug."""
    return polymarket_snapshot.GAMMA_EVENT_URL.format(
        slug=quote(event_slug, safe="")
    )


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _argument_parser().parse_args(argv)
    if arguments.duration_seconds <= 0:
        print("error: duration must be a positive number of seconds", file=sys.stderr)
        return 2
    try:
        asyncio.run(
            record_market(
                arguments.event_slug,
                arguments.output,
                arguments.duration_seconds,
                arguments.market_slug,
            )
        )
    except KeyboardInterrupt:
        print("recording interrupted", file=sys.stderr)
        return 130
    except (RecorderError, polymarket_snapshot.SnapshotError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"recording saved to {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
