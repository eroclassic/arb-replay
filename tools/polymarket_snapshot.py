#!/usr/bin/env python3
"""Capture one Polymarket binary-market snapshot as ArbReplay input."""

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
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


GAMMA_EVENT_URL = "https://gamma-api.polymarket.com/events/slug/{slug}"
CLOB_BOOK_URL = "https://clob.polymarket.com/book"
CSV_HEADER = (
    "observed_at_ns",
    "sequence",
    "outcome_id",
    "side",
    "price",
    "quantity",
)
OUTCOMES = ("YES", "NO")
FIXED_PATTERN = re.compile(r"^[0-9]+(?:\.[0-9]{1,6})?$")
MAX_FIXED = Decimal("9223372036854.775807")

JsonObject = dict[str, Any]
JsonFetcher = Callable[[str], JsonObject]


class SnapshotError(RuntimeError):
    """Raised when Polymarket data cannot be normalized safely."""


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
        raise SnapshotError(f"expected a JSON object from {url}")
    return value


def _decode_array(value: Any, field_name: str) -> list[Any]:
    if isinstance(value, str):
        try:
            value = json.loads(value)
        except json.JSONDecodeError as error:
            raise SnapshotError(f"{field_name} is not valid JSON") from error
    if not isinstance(value, list):
        raise SnapshotError(f"{field_name} must be an array")
    return value


def outcome_token_map(market: Mapping[str, Any]) -> dict[str, str]:
    outcomes = _decode_array(market.get("outcomes"), "outcomes")
    tokens = _decode_array(market.get("clobTokenIds"), "clobTokenIds")
    if len(outcomes) != 2 or len(tokens) != 2:
        raise SnapshotError("market must contain exactly two outcomes and tokens")

    mapping: dict[str, str] = {}
    for outcome, token in zip(outcomes, tokens, strict=True):
        if not isinstance(outcome, str) or not isinstance(token, str) or not token:
            raise SnapshotError("outcome names and token IDs must be strings")
        normalized_outcome = outcome.upper()
        if normalized_outcome not in OUTCOMES:
            raise SnapshotError("binary market outcomes must be YES and NO")
        if normalized_outcome in mapping:
            raise SnapshotError("market contains a duplicate outcome")
        mapping[normalized_outcome] = token

    if set(mapping) != set(OUTCOMES):
        raise SnapshotError("binary market outcomes must be YES and NO")
    return mapping


def _validate_market(market: Mapping[str, Any]) -> None:
    if market.get("active") is not True:
        raise SnapshotError("market is not active")
    if market.get("closed") is not False:
        raise SnapshotError("market is closed")
    if market.get("acceptingOrders") is not True:
        raise SnapshotError("market is not accepting orders")
    if market.get("enableOrderBook") is not True:
        raise SnapshotError("market does not enable the CLOB")
    if not isinstance(market.get("slug"), str) or not market["slug"]:
        raise SnapshotError("market is missing its slug")
    if not isinstance(market.get("conditionId"), str) or not market["conditionId"]:
        raise SnapshotError("market is missing its condition ID")
    outcome_token_map(market)


def select_market(
    event: Mapping[str, Any], requested_market_slug: str | None = None
) -> JsonObject:
    markets = event.get("markets")
    if not isinstance(markets, list):
        raise SnapshotError("event does not contain a markets array")

    if requested_market_slug is not None:
        matches = [
            market
            for market in markets
            if isinstance(market, dict)
            and market.get("slug") == requested_market_slug
        ]
        if len(matches) != 1:
            raise SnapshotError(
                f"event does not contain market '{requested_market_slug}'"
            )
        _validate_market(matches[0])
        return matches[0]

    candidates: list[JsonObject] = []
    for market in markets:
        if not isinstance(market, dict):
            continue
        try:
            _validate_market(market)
        except SnapshotError:
            continue
        candidates.append(market)

    if not candidates:
        raise SnapshotError("event has no active binary CLOB market")
    if len(candidates) > 1:
        slugs = ", ".join(sorted(market["slug"] for market in candidates))
        raise SnapshotError(
            f"event has multiple eligible markets; pass --market with one of: {slugs}"
        )
    return candidates[0]


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
    book: Mapping[str, Any], side: str
) -> list[tuple[str, str]]:
    raw_levels = book.get(side)
    if not isinstance(raw_levels, list):
        raise SnapshotError(f"order book is missing its {side} array")

    levels: list[tuple[str, str, Decimal]] = []
    seen_prices: set[Decimal] = set()
    for level in raw_levels:
        if not isinstance(level, dict):
            raise SnapshotError(f"{side} level must be an object")
        price_text = level.get("price")
        quantity_text = level.get("size")
        price = _fixed_decimal(price_text, f"{side} price", Decimal("1"))
        _fixed_decimal(quantity_text, f"{side} size", MAX_FIXED)
        if price in seen_prices:
            raise SnapshotError(f"{side} contains duplicate price {price_text}")
        seen_prices.add(price)
        levels.append((price_text, quantity_text, price))

    levels.sort(key=lambda level: level[2], reverse=side == "bids")
    return [(price, quantity) for price, quantity, _ in levels]


def normalize_snapshot(
    market: Mapping[str, Any],
    books: Mapping[str, Mapping[str, Any]],
    observed_at_ns: int,
) -> list[dict[str, str | int]]:
    _validate_market(market)
    if observed_at_ns < 0 or observed_at_ns > 9_223_372_036_854_775_807:
        raise SnapshotError("observed_at_ns is outside signed 64-bit range")

    tokens = outcome_token_map(market)
    condition_id = market["conditionId"]
    rows: list[dict[str, str | int]] = []
    sequence = 1

    for outcome in OUTCOMES:
        book = books.get(outcome)
        if not isinstance(book, Mapping):
            raise SnapshotError(f"missing {outcome} order book")
        if book.get("asset_id") != tokens[outcome]:
            raise SnapshotError(f"{outcome} book token does not match metadata")
        if book.get("market") != condition_id:
            raise SnapshotError(f"{outcome} book market does not match metadata")

        for source_side, normalized_side in (("bids", "bid"), ("asks", "ask")):
            for price, quantity in _validated_levels(book, source_side):
                rows.append(
                    {
                        "observed_at_ns": observed_at_ns,
                        "sequence": sequence,
                        "outcome_id": outcome,
                        "side": normalized_side,
                        "price": price,
                        "quantity": quantity,
                    }
                )
                sequence += 1

    return rows


def _write_json(path: Path, value: Mapping[str, Any]) -> None:
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def write_snapshot(
    output_directory: Path,
    event: Mapping[str, Any],
    market: Mapping[str, Any],
    books: Mapping[str, Mapping[str, Any]],
    observed_at_ns: int,
    *,
    force: bool = False,
) -> Path:
    rows = normalize_snapshot(market, books, observed_at_ns)
    output_directory.mkdir(parents=True, exist_ok=True)

    output_paths = {
        "event": output_directory / "event.json",
        "yes": output_directory / "yes-book.json",
        "no": output_directory / "no-book.json",
        "metadata": output_directory / "metadata.json",
        "events": output_directory / "events.csv",
    }
    existing = [path for path in output_paths.values() if path.exists()]
    if existing and not force:
        names = ", ".join(path.name for path in existing)
        raise SnapshotError(f"refusing to overwrite existing files: {names}")

    tokens = outcome_token_map(market)
    metadata = {
        "schema_version": 1,
        "venue": "polymarket",
        "observed_at_ns": observed_at_ns,
        "payout_per_set": "1.000000",
        "event": {
            "id": event.get("id"),
            "slug": event.get("slug"),
            "title": event.get("title"),
        },
        "market": {
            "id": market.get("id"),
            "slug": market.get("slug"),
            "question": market.get("question"),
            "condition_id": market.get("conditionId"),
        },
        "outcomes": [
            {"outcome_id": outcome, "token_id": tokens[outcome]}
            for outcome in OUTCOMES
        ],
        "source_book_timestamps": {
            outcome: books[outcome].get("timestamp") for outcome in OUTCOMES
        },
    }

    _write_json(output_paths["event"], event)
    _write_json(output_paths["yes"], books["YES"])
    _write_json(output_paths["no"], books["NO"])
    _write_json(output_paths["metadata"], metadata)

    with output_paths["events"].open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=CSV_HEADER, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    return output_paths["events"]


def capture_snapshot(
    event_slug: str,
    output_directory: Path,
    *,
    market_slug: str | None = None,
    force: bool = False,
    fetcher: JsonFetcher = fetch_json,
    clock: Callable[[], int] = time.time_ns,
) -> Path:
    event_url = GAMMA_EVENT_URL.format(slug=quote(event_slug, safe=""))
    event = fetcher(event_url)
    market = select_market(event, market_slug)
    tokens = outcome_token_map(market)

    books: dict[str, JsonObject] = {}
    for outcome in OUTCOMES:
        book_url = f"{CLOB_BOOK_URL}?{urlencode({'token_id': tokens[outcome]})}"
        books[outcome] = fetcher(book_url)

    return write_snapshot(
        output_directory,
        event,
        market,
        books,
        clock(),
        force=force,
    )


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture a Polymarket binary-market order-book snapshot."
    )
    parser.add_argument("event_slug", help="Polymarket event slug")
    parser.add_argument(
        "--market",
        dest="market_slug",
        help="market slug when the event contains multiple eligible markets",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="directory for raw JSON, metadata, and normalized events.csv",
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
            arguments.event_slug,
            arguments.output,
            market_slug=arguments.market_slug,
            force=arguments.force,
        )
    except SnapshotError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"wrote snapshot: {events_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
