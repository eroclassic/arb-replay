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

from tools import polymarket_recorder


MARKET = {
    "outcomes": '["Yes", "No"]',
    "clobTokenIds": '["yes-token", "no-token"]',
}


class PolymarketRecorderTest(unittest.TestCase):
    def test_builds_gamma_event_url_from_slug(self) -> None:
        self.assertEqual(
            polymarket_recorder.event_url_for_slug("mlb-tb-cle-2026-10-16"),
            "https://gamma-api.polymarket.com/events/slug/mlb-tb-cle-2026-10-16",
        )

    def test_accepts_slug_or_event_link(self) -> None:
        self.assertEqual(
            polymarket_recorder.event_slug_from_input("btc-updown-5m-1792283100"),
            "btc-updown-5m-1792283100",
        )
        self.assertEqual(
            polymarket_recorder.event_slug_from_input(
                "https://polymarket.com/event/btc-updown-5m-1792283100"
            ),
            "btc-updown-5m-1792283100",
        )
        self.assertEqual(
            polymarket_recorder.event_slug_from_input(
                "https://polymarket.com/sports/mlb/mlb-tb-cle-2026-10-16"
            ),
            "mlb-tb-cle-2026-10-16",
        )

    def test_rejects_non_polymarket_links(self) -> None:
        with self.assertRaisesRegex(
            polymarket_recorder.RecorderError, "polymarket.com"
        ):
            polymarket_recorder.event_slug_from_input(
                "https://example.com/event/btc-updown-5m-1792283100"
            )

    def test_records_initial_books_and_absolute_level_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            writer = polymarket_recorder.EventWriter(
                Path(temporary_directory), MARKET
            )
            try:
                writer.handle_message(
                    {
                        "event_type": "book",
                        "asset_id": "yes-token",
                        "timestamp": "1760000000123",
                        "bids": [{"price": "0.41", "size": "12.5"}],
                        "asks": [{"price": "0.43", "size": "3"}],
                    }
                )
                writer.handle_message(
                    {
                        "event_type": "book",
                        "asset_id": "no-token",
                        "timestamp": "1760000000123",
                        "bids": [],
                        "asks": [],
                    }
                )
                writer.handle_message(
                    {
                        "event_type": "price_change",
                        "timestamp": "1760000001123",
                        "price_changes": [
                            {
                                "asset_id": "yes-token",
                                "price": "0.41",
                                "size": "8",
                                "side": "BUY",
                            },
                            {
                                "asset_id": "no-token",
                                "price": "0.55",
                                "size": "0",
                                "side": "SELL",
                            },
                        ],
                    }
                )
            finally:
                writer.close()

            with (Path(temporary_directory) / "events.csv").open(
                encoding="utf-8", newline=""
            ) as events_file:
                rows = list(csv.DictReader(events_file))

            self.assertEqual(len(rows), 4)
            self.assertEqual(rows[0]["observed_at_ns"], "1760000000123000000")
            self.assertEqual(rows[0]["outcome_id"], "YES")
            self.assertEqual(rows[0]["side"], "bid")
            self.assertEqual(rows[1]["side"], "ask")
            self.assertEqual(rows[2]["quantity"], "8")
            self.assertEqual(rows[3]["side"], "ask")
            self.assertEqual(rows[3]["quantity"], "0")

    def test_full_book_replacement_emits_zero_for_removed_levels(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            writer = polymarket_recorder.EventWriter(
                Path(temporary_directory), MARKET
            )
            try:
                first_book = {
                    "event_type": "book",
                    "asset_id": "yes-token",
                    "timestamp": "1760000000123",
                    "bids": [
                        {"price": "0.4", "size": "2"},
                        {"price": "0.39", "size": "3"},
                    ],
                    "asks": [],
                }
                writer.handle_message(first_book)
                writer.handle_message(
                    {
                        **first_book,
                        "timestamp": "1760000002123",
                        "bids": [{"price": "0.4", "size": "1"}],
                    }
                )
            finally:
                writer.close()

            with (Path(temporary_directory) / "events.csv").open(
                encoding="utf-8", newline=""
            ) as events_file:
                rows = list(csv.DictReader(events_file))

            removed_level = [row for row in rows if row["price"] == "0.39"][-1]
            updated_level = [row for row in rows if row["price"] == "0.4"][-1]
            self.assertEqual(removed_level["quantity"], "0")
            self.assertEqual(updated_level["quantity"], "1")

    def test_refuses_to_overwrite_existing_recording(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            (output / "keep.txt").write_text("existing", encoding="utf-8")
            with self.assertRaisesRegex(
                polymarket_recorder.RecorderError, "not empty"
            ):
                polymarket_recorder.EventWriter(output, MARKET)
            self.assertEqual((output / "keep.txt").read_text(), "existing")

    def test_overwrite_prompt_decline_preserves_existing_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "capture"
            output.mkdir()
            existing_file = output / "events.csv"
            existing_file.write_text("keep this capture", encoding="utf-8")

            accepted = polymarket_recorder.prepare_output_directory(
                output, input_fn=lambda _prompt: "n"
            )

            self.assertFalse(accepted)
            self.assertEqual(existing_file.read_text(encoding="utf-8"), "keep this capture")

    def test_overwrite_prompt_yes_clears_only_the_recording_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "capture"
            nested = output / "nested"
            nested.mkdir(parents=True)
            (nested / "old.csv").write_text("old", encoding="utf-8")
            sibling = Path(temporary_directory) / "keep.txt"
            sibling.write_text("keep", encoding="utf-8")

            accepted = polymarket_recorder.prepare_output_directory(
                output, input_fn=lambda _prompt: "yes"
            )

            self.assertTrue(accepted)
            self.assertEqual(list(output.iterdir()), [])
            self.assertEqual(sibling.read_text(encoding="utf-8"), "keep")

    def test_never_clears_project_root(self) -> None:
        with self.assertRaisesRegex(
            polymarket_recorder.RecorderError, "protected directory"
        ):
            polymarket_recorder.prepare_output_directory(
                PROJECT_ROOT, input_fn=lambda _prompt: "yes"
            )

    def test_rejects_update_before_initial_book_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            writer = polymarket_recorder.EventWriter(
                Path(temporary_directory), MARKET
            )
            try:
                with self.assertRaisesRegex(
                    polymarket_recorder.RecorderError,
                    "before the initial book snapshot",
                ):
                    writer.handle_message(
                        {
                            "event_type": "price_change",
                            "timestamp": "1760000001123",
                            "price_changes": [
                                {
                                    "asset_id": "yes-token",
                                    "price": "0.41",
                                    "size": "8",
                                    "side": "BUY",
                                }
                            ],
                        }
                    )
            finally:
                writer.close()

    def test_rejects_nonpositive_recording_duration(self) -> None:
        self.assertEqual(
            polymarket_recorder.main(
                ["example-event", "--output", "unused", "--duration-seconds", "0"]
            ),
            2,
        )

    def test_recorded_events_replay_through_cli(self) -> None:
        arbreplay_cli = os.environ.get("ARBREPLAY_CLI")
        if not arbreplay_cli:
            self.skipTest("ARBREPLAY_CLI is not set")

        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "capture"
            writer = polymarket_recorder.EventWriter(output, MARKET)
            try:
                for asset_id, ask_price in (
                    ("yes-token", "0.4"),
                    ("no-token", "0.5"),
                ):
                    writer.handle_message(
                        {
                            "event_type": "book",
                            "asset_id": asset_id,
                            "timestamp": "1760000000123",
                            "bids": [],
                            "asks": [{"price": ask_price, "size": "2"}],
                        }
                    )
            finally:
                writer.close()
            (output / "metadata.json").write_text(
                json.dumps({"payout_per_set": "1.000000"}), encoding="utf-8"
            )

            completed = subprocess.run(
                [arbreplay_cli, "replay", str(output)],
                check=False,
                capture_output=True,
                text=True,
            )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("events: 2", completed.stdout)
        self.assertIn("detections: 1", completed.stdout)


if __name__ == "__main__":
    unittest.main()
