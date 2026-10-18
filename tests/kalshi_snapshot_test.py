#!/usr/bin/env python3

from __future__ import annotations

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

from tools import kalshi_snapshot


FIXTURE = PROJECT_ROOT / "tests" / "fixtures" / "kalshi" / "orderbook.json"
OBSERVED_AT_NS = 1_760_000_000_123_456_789
TICKER = "EXAMPLE-MARKET"


def load_fixture() -> dict:
    return json.loads(FIXTURE.read_text(encoding="utf-8"))


class KalshiSnapshotTest(unittest.TestCase):
    def setUp(self) -> None:
        self.payload = load_fixture()

    def test_normalizes_yes_no_bids_and_complementary_asks(self) -> None:
        rows = kalshi_snapshot.normalize_snapshot(self.payload, OBSERVED_AT_NS)

        self.assertEqual(
            [
                (row["outcome_id"], row["side"], row["price"], row["quantity"])
                for row in rows
            ],
            [
                ("YES", "bid", "0.400000", "2.000000"),
                ("YES", "bid", "0.350000", "3.250000"),
                ("YES", "ask", "0.800000", "1.500000"),
                ("NO", "bid", "0.200000", "1.500000"),
                ("NO", "ask", "0.600000", "2.000000"),
                ("NO", "ask", "0.650000", "3.250000"),
            ],
        )
        self.assertEqual([row["sequence"] for row in rows], list(range(1, 7)))
        self.assertTrue(all(row["observed_at_ns"] == OBSERVED_AT_NS for row in rows))

    def test_snapshot_files_include_raw_payload_and_replay_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            events_path = kalshi_snapshot.write_snapshot(
                output, TICKER, self.payload, OBSERVED_AT_NS
            )

            self.assertEqual(events_path, output / "events.csv")
            self.assertEqual(
                json.loads((output / "orderbook.json").read_text(encoding="utf-8")),
                self.payload,
            )
            metadata = json.loads(
                (output / "metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["venue"], "kalshi")
            self.assertEqual(metadata["market"]["ticker"], TICKER)
            self.assertEqual(metadata["payout_per_set"], "1.000000")
            self.assertEqual(metadata["observed_at_ns"], OBSERVED_AT_NS)

            with events_path.open(encoding="utf-8", newline="") as input_file:
                rows = list(csv.DictReader(input_file))
            self.assertEqual(len(rows), 6)
            self.assertEqual(list(rows[0]), list(kalshi_snapshot.CSV_HEADER))

            arbreplay_cli = os.environ.get("ARBREPLAY_CLI")
            if arbreplay_cli:
                completed = subprocess.run(
                    [arbreplay_cli, "replay", str(output)],
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertIn("events: 6", completed.stdout)
                self.assertIn("detections: 0", completed.stdout)

    def test_capture_requests_encoded_market_ticker(self) -> None:
        requested_urls: list[str] = []

        def fetcher(url: str) -> dict:
            requested_urls.append(url)
            return self.payload

        with tempfile.TemporaryDirectory() as temporary_directory:
            result = kalshi_snapshot.capture_snapshot(
                "TICKER/NEEDS-ESCAPING",
                Path(temporary_directory),
                fetcher=fetcher,
                clock=lambda: OBSERVED_AT_NS,
            )
            self.assertTrue(result.is_file())

        self.assertEqual(
            requested_urls,
            [
                kalshi_snapshot.ORDERBOOK_URL.format(
                    ticker="TICKER%2FNEEDS-ESCAPING"
                )
            ],
        )

    def test_rejects_out_of_range_price(self) -> None:
        payload = load_fixture()
        payload["orderbook_fp"]["yes_dollars"][0][0] = "1.0001"
        with self.assertRaisesRegex(kalshi_snapshot.SnapshotError, "price"):
            kalshi_snapshot.normalize_snapshot(payload, OBSERVED_AT_NS)

    def test_rejects_invalid_level_shape(self) -> None:
        payload = load_fixture()
        payload["orderbook_fp"]["yes_dollars"][0] = ["0.4"]
        with self.assertRaisesRegex(kalshi_snapshot.SnapshotError, "price and quantity"):
            kalshi_snapshot.normalize_snapshot(payload, OBSERVED_AT_NS)

    def test_refuses_to_overwrite_existing_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            (output / "events.csv").write_text("keep me\n", encoding="utf-8")
            with self.assertRaisesRegex(kalshi_snapshot.SnapshotError, "overwrite"):
                kalshi_snapshot.write_snapshot(
                    output, TICKER, self.payload, OBSERVED_AT_NS
                )
            self.assertEqual(
                (output / "events.csv").read_text(encoding="utf-8"), "keep me\n"
            )


if __name__ == "__main__":
    unittest.main()
