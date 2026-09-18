#!/usr/bin/env python3

from __future__ import annotations

import copy
import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from tools import polymarket_snapshot


FIXTURES = PROJECT_ROOT / "tests" / "fixtures" / "polymarket"
OBSERVED_AT_NS = 1_760_000_000_123_456_789


def load_fixture(name: str) -> dict:
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


class PolymarketSnapshotTest(unittest.TestCase):
    def setUp(self) -> None:
        self.event = load_fixture("event.json")
        self.market = self.event["markets"][0]
        self.books = {
            "YES": load_fixture("yes-book.json"),
            "NO": load_fixture("no-book.json"),
        }

    def test_normalizes_binary_snapshot_deterministically(self) -> None:
        rows = polymarket_snapshot.normalize_snapshot(
            self.market, self.books, OBSERVED_AT_NS
        )

        self.assertEqual(len(rows), 6)
        self.assertEqual(
            [(row["sequence"], row["outcome_id"], row["side"]) for row in rows],
            [
                (1, "YES", "bid"),
                (2, "YES", "bid"),
                (3, "YES", "ask"),
                (4, "NO", "bid"),
                (5, "NO", "ask"),
                (6, "NO", "ask"),
            ],
        )
        self.assertEqual(rows[0]["price"], "0.410001")
        self.assertEqual(rows[0]["quantity"], "12.500000")
        self.assertEqual(rows[4]["price"], "0.560000")
        self.assertTrue(
            all(row["observed_at_ns"] == OBSERVED_AT_NS for row in rows)
        )

    def test_writes_raw_payloads_metadata_and_parseable_csv(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            events_path = polymarket_snapshot.write_snapshot(
                output,
                self.event,
                self.market,
                self.books,
                OBSERVED_AT_NS,
            )

            self.assertEqual(events_path, output / "events.csv")
            self.assertTrue((output / "event.json").is_file())
            self.assertTrue((output / "yes-book.json").is_file())
            self.assertTrue((output / "no-book.json").is_file())

            metadata = json.loads(
                (output / "metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["venue"], "polymarket")
            self.assertEqual(metadata["payout_per_set"], "1.000000")
            self.assertEqual(
                metadata["outcomes"],
                [
                    {"outcome_id": "YES", "token_id": "yes-token"},
                    {"outcome_id": "NO", "token_id": "no-token"},
                ],
            )

            with events_path.open(encoding="utf-8", newline="") as input_file:
                rows = list(csv.DictReader(input_file))
            self.assertEqual(len(rows), 6)
            self.assertEqual(
                list(rows[0]),
                list(polymarket_snapshot.CSV_HEADER),
            )

            arbreplay_cli = os.environ.get("ARBREPLAY_CLI")
            if arbreplay_cli:
                completed = subprocess.run(
                    [
                        arbreplay_cli,
                        "replay",
                        str(output),
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertIn("events: 6", completed.stdout)

    def test_capture_uses_metadata_token_mapping(self) -> None:
        responses = {
            polymarket_snapshot.GAMMA_EVENT_URL.format(slug="example-event"):
                self.event,
            f"{polymarket_snapshot.CLOB_BOOK_URL}?token_id=yes-token":
                self.books["YES"],
            f"{polymarket_snapshot.CLOB_BOOK_URL}?token_id=no-token":
                self.books["NO"],
        }
        requested_urls: list[str] = []

        def fetcher(url: str) -> dict:
            requested_urls.append(url)
            return responses[url]

        with tempfile.TemporaryDirectory() as temporary_directory:
            events_path = polymarket_snapshot.capture_snapshot(
                "example-event",
                Path(temporary_directory),
                fetcher=fetcher,
                clock=lambda: OBSERVED_AT_NS,
            )
            self.assertTrue(events_path.is_file())

        self.assertEqual(
            requested_urls,
            [
                polymarket_snapshot.GAMMA_EVENT_URL.format(
                    slug="example-event"
                ),
                f"{polymarket_snapshot.CLOB_BOOK_URL}?token_id=yes-token",
                f"{polymarket_snapshot.CLOB_BOOK_URL}?token_id=no-token",
            ],
        )

    def test_requires_explicit_selection_for_multiple_markets(self) -> None:
        event = copy.deepcopy(self.event)
        second_market = copy.deepcopy(self.market)
        second_market["id"] = "market-2"
        second_market["slug"] = "second-market"
        event["markets"].append(second_market)

        with self.assertRaisesRegex(
            polymarket_snapshot.SnapshotError,
            "multiple eligible markets",
        ):
            polymarket_snapshot.select_market(event)

        selected = polymarket_snapshot.select_market(event, "second-market")
        self.assertEqual(selected["id"], "market-2")

    def test_rejects_non_binary_outcome_mapping(self) -> None:
        market = copy.deepcopy(self.market)
        market["outcomes"] = "[\"Up\", \"Down\"]"

        with self.assertRaisesRegex(
            polymarket_snapshot.SnapshotError,
            "outcomes must be YES and NO",
        ):
            polymarket_snapshot.outcome_token_map(market)

    def test_rejects_more_than_six_decimal_places(self) -> None:
        books = copy.deepcopy(self.books)
        books["YES"]["asks"][0]["price"] = "0.4300001"

        with self.assertRaisesRegex(
            polymarket_snapshot.SnapshotError,
            "at most six fractional digits",
        ):
            polymarket_snapshot.normalize_snapshot(
                self.market, books, OBSERVED_AT_NS
            )

    def test_refuses_to_overwrite_snapshot_without_force(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            polymarket_snapshot.write_snapshot(
                output,
                self.event,
                self.market,
                self.books,
                OBSERVED_AT_NS,
            )

            with self.assertRaisesRegex(
                polymarket_snapshot.SnapshotError,
                "refusing to overwrite",
            ):
                polymarket_snapshot.write_snapshot(
                    output,
                    self.event,
                    self.market,
                    self.books,
                    OBSERVED_AT_NS,
                )

    def test_cli_rejects_snapshot_without_metadata(self) -> None:
        arbreplay_cli = os.environ.get("ARBREPLAY_CLI")
        if not arbreplay_cli:
            self.skipTest("ARBREPLAY_CLI is not set")

        with tempfile.TemporaryDirectory() as temporary_directory:
            completed = subprocess.run(
                [arbreplay_cli, "replay", temporary_directory],
                check=False,
                capture_output=True,
                text=True,
            )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("unable to open snapshot metadata", completed.stderr)

    def test_cli_rejects_snapshot_without_events_csv(self) -> None:
        arbreplay_cli = os.environ.get("ARBREPLAY_CLI")
        if not arbreplay_cli:
            self.skipTest("ARBREPLAY_CLI is not set")

        with tempfile.TemporaryDirectory() as temporary_directory:
            snapshot = Path(temporary_directory)
            (snapshot / "metadata.json").write_text(
                '{"payout_per_set":"1.000000"}\n', encoding="utf-8"
            )
            completed = subprocess.run(
                [arbreplay_cli, "replay", str(snapshot)],
                check=False,
                capture_output=True,
                text=True,
            )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("unable to open event CSV", completed.stderr)


if __name__ == "__main__":
    unittest.main()
