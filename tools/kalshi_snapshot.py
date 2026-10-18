#!/usr/bin/env python3
"""Capture a Kalshi binary-market order book as ArbReplay input."""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import time
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any, Callable, Mapping, Sequence
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


ORDERBOOK_URL = (
    "https://external-api.kalshi.com/trade-api/v2/markets/{ticker}/orderbook"
)
CSV_HEADER = (
    "observed_at_ns",
    "sequence",
    "outcome_id",
    "side",
    "price",
    "quantity",
)
FIXED_PATTERN = re.compile(r"^[0-9]+(?:\.[0-9]{1,6})?$")
FIXED_QUANTUM = Decimal("0.000001")
MAX_FIXED = Decimal("9223372036854.775807")

JsonObject = dict[str, Any]
JsonFetcher = Callable[[str], JsonObject]


class SnapshotError(RuntimeError):
    """Raised when Kalshi data cannot be normalized safely."""


def fetch_json(url: str) -> JsonObject:
    request = Request(url, headers={"User-Agent": "ArbReplay/0.1"})
    try:
        with urlopen(request, timeout=20) as response:
            payload = response.read()
    except (HTTPError, URLError, TimeoutError) as error:
        raise SnapshotError(f"request failed for {url}: {error}") from error

    try:
        value = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise SnapshotError(f"invalid JSON returned by {url}") from error

    if not isinstance(value, dict):
        raise SnapshotError("expected a JSON object from Kalshi")
    return value


def _fixed_decimal(value: Any, field_name: str, maximum: Decimal) -> Decimal:
    if not isinstance(value, str) or FIXED_PATTERN.fullmatch(value) is None:
        raise SnapshotError(
            f"{field_name} must be a non-negative decimal string "
            "with at most six fractional digits"
        )
    try:
        parsed = Decimal(value)
    except InvalidOperation as error:
        raise SnapshotError(f"{field_name} is not a valid decimal") from error
    if parsed > maximum:
        raise SnapshotError(f"{field_name} is outside ArbReplay's range")
    return parsed


def _validated_levels(
    orderbook: Mapping[str, Any], field_name: str, side_name: str
) -> list[tuple[Decimal, Decimal]]:
    raw_levels = orderbook.get(field_name)
    if not isinstance(raw_levels, list):
        raise SnapshotError(f"orderbook_fp is missing its {field_name} array")

    levels: list[tuple[Decimal, Decimal]] = []
    seen_prices: set[Decimal] = set()
    for level in raw_levels:
        if not isinstance(level, list) or len(level) != 2:
            raise SnapshotError(
                f"{field_name} level must contain price and quantity"
            )
        price = _fixed_decimal(level[0], f"{field_name} price", Decimal("1"))
        quantity = _fixed_decimal(level[1], f"{field_name} quantity", MAX_FIXED)
        if quantity <= 0:
            raise SnapshotError(f"{field_name} quantity must be positive")
        if price in seen_prices:
            raise SnapshotError(f"{field_name} contains duplicate price {level[0]}")
        seen_prices.add(price)
        levels.append((price, quantity))

    levels.sort(key=lambda level: level[0], reverse=side_name == "bid")
    return levels


def _format_fixed(value: Decimal) -> str:
    return format(value.quantize(FIXED_QUANTUM), "f")


def normalize_snapshot(
    payload: Mapping[str, Any], observed_at_ns: int
) -> list[dict[str, str | int]]:
    if observed_at_ns < 0 or observed_at_ns > 9_223_372_036_854_775_807:
        raise SnapshotError("observed_at_ns is outside signed 64-bit range")

    orderbook = payload.get("orderbook_fp")
    if not isinstance(orderbook, Mapping):
        raise SnapshotError("response is missing the orderbook_fp object")

    yes_bids = _validated_levels(orderbook, "yes_dollars", "bid")
    no_bids = _validated_levels(orderbook, "no_dollars", "bid")

    # Kalshi publishes YES and NO bid ladders. By binary complement,
    # a bid on one outcome is an ask on the other at 1 - bid price.
    levels = {
        ("YES", "bid"): yes_bids,
        ("YES", "ask"): [(Decimal("1") - price, qty) for price, qty in no_bids],
        ("NO", "bid"): no_bids,
        ("NO", "ask"): [(Decimal("1") - price, qty) for price, qty in yes_bids],
    }

    rows: list[dict[str, str | int]] = []
    sequence = 1
    for outcome in ("YES", "NO"):
        for side in ("bid", "ask"):
            for price, quantity in levels[(outcome, side)]:
                rows.append(
                    {
                        "observed_at_ns": observed_at_ns,
                        "sequence": sequence,
                        "outcome_id": outcome,
                        "side": side,
                        "price": _format_fixed(price),
                        "quantity": _format_fixed(quantity),
                    }
                )
                sequence += 1
    return rows


def _write_json(path: Path, value: Mapping[str, Any]) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_snapshot(
    output_directory: Path,
    ticker: str,
    payload: Mapping[str, Any],
    observed_at_ns: int,
    *,
    force: bool = False,
) -> Path:
    rows = normalize_snapshot(payload, observed_at_ns)
    output_directory.mkdir(parents=True, exist_ok=True)

    output_paths = {
        "orderbook": output_directory / "orderbook.json",
        "metadata": output_directory / "metadata.json",
        "events": output_directory / "events.csv",
    }
    existing = [path for path in output_paths.values() if path.exists()]
    if existing and not force:
        names = ", ".join(path.name for path in existing)
        raise SnapshotError(f"refusing to overwrite existing files: {names}")

    metadata = {
        "schema_version": 1,
        "venue": "kalshi",
        "observed_at_ns": observed_at_ns,
        "payout_per_set": "1.000000",
        "market": {"ticker": ticker},
    }
    _write_json(output_paths["orderbook"], payload)
    _write_json(output_paths["metadata"], metadata)
    with output_paths["events"].open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=CSV_HEADER, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    return output_paths["events"]


def capture_snapshot(
    ticker: str,
    output_directory: Path,
    *,
    force: bool = False,
    fetcher: JsonFetcher = fetch_json,
    clock: Callable[[], int] = time.time_ns,
) -> Path:
    if not ticker:
        raise SnapshotError("market ticker cannot be empty")
    url = ORDERBOOK_URL.format(ticker=quote(ticker, safe=""))
    payload = fetcher(url)
    return write_snapshot(
        output_directory,
        ticker,
        payload,
        clock(),
        force=force,
    )


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture a Kalshi binary-market order-book snapshot."
    )
    parser.add_argument("ticker", help="Kalshi market ticker")
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="directory for raw orderbook JSON, metadata, and events.csv",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="overwrite snapshot files already present in the output directory",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _argument_parser().parse_args(argv)
    try:
        events_path = capture_snapshot(
            arguments.ticker,
            arguments.output,
            force=arguments.force,
        )
    except SnapshotError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"wrote snapshot: {events_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
