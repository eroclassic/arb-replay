# ArbReplay

**A C++20 prediction-market arbitrage replay engine.**

ArbReplay models prediction-market order books and detects depth-aware
complete-set arbitrage across binary and multi-outcome markets. The current
engine provides the in-memory market model, configurable fee-adjusted
opportunity detection, and deterministic event replay with strict CSV ingestion
and an offline CLI. Execution simulation is the next layer.

## Architecture

ArbReplay models binary and multi-outcome prediction markets using the same
composable order-book hierarchy:

```text
Market
└── outcomes: map<OutcomeId, OutcomeBook>
    └── OutcomeBook
        ├── bids: BookSide
        │   └── levels: map<Price, Quantity>
        └── asks: BookSide
            └── levels: map<Price, Quantity>

detect_complete_set_opportunity(Market, payout)
└── CompleteSetOpportunity
    └── levels: vector<CompleteSetOpportunityLevel>
        └── combined cost per set + executable quantity
```

A binary market contains YES and NO outcome books. A multi-outcome market uses
the same structure with additional mutually exclusive outcomes. The detector
walks the ask depth without mutating the supplied market.

| Type | Responsibility |
|---|---|
| `Market` | Owns every possible outcome for one prediction question |
| `OutcomeBook` | Holds the bids and asks for one tradable outcome |
| `BookSide` | Maintains one ordered collection of bids or asks |
| `BookLevel` | Pairs one price with its available quantity |
| `Price` | Represents a contract price from 0 through 1 with six-decimal fixed-point precision |
| `Quantity` | Represents a non-negative contract quantity with six-decimal fixed-point precision |
| `Money` | Represents signed monetary amounts with six-decimal fixed-point precision and checked arithmetic |
| `CompleteSetOpportunityLevel` | Represents complete sets available at one combined cost |
| `CompleteSetOpportunity` | Aggregates profitable levels and calculates cost, payout, estimated fees, and gross/net profit |

## Complete-set strategy

A buy-complete-set strategy purchases one contract for every possible outcome.
Because exactly one outcome settles as the winner, the set produces a fixed
payout regardless of which outcome occurs.

For a binary market:

```text
YES best ask           42 cents x 5
NO best ask            55 cents x 20
Combined cost          97 cents per set
Settlement payout     100 cents per set
Executable quantity     5 sets
Gross profit           15 cents
```

Detection uses the strict condition:

```text
sum of current outcome asks < payout per complete set
```

Equality is break-even and is not reported as an opportunity. The outcome with
the least remaining quantity limits each opportunity level. When that liquidity
is consumed, its cursor advances to the next ask price and the detector
recalculates the combined cost. This continues until an outcome runs out of
liquidity or the next combined level is no longer profitable.

### Market-definition assumption

`detect_complete_set_opportunity` assumes that every outcome in the supplied
`Market` is mutually exclusive and that the outcomes collectively exhaust every
valid settlement result. The current `Market` model does not independently
verify this property. Validated market-definition metadata must enforce it
before external market data is accepted.

With the default zero fee rate, the detector reports gross opportunities from
displayed ask liquidity. A configurable flat fee estimate filters levels that
would not remain profitable after fees. Queue position, stale quotes, partial
fills, venue-specific fee schedules, and legging risk are not yet included. CSV
replay can optionally recheck an opportunity after a configured execution
delay.

### Numeric representation

`Price`, `Quantity`, and `Money` store signed 64-bit raw integers at a fixed
scale of 1,000,000. Venue decimal strings are parsed directly into this format;
floating-point conversion is not used. This represents sub-cent prices and
fractional contracts from venue feeds while keeping replay deterministic.

Opportunity costs round upward and payouts round downward whenever a product
cannot be represented exactly. This conservative policy prevents fractional
rounding from creating false-positive arbitrage.

## CSV input

Recorded venue data is normalized into absolute order-book level updates before
replay. See [the normalized market-event CSV format](docs/csv-format.md) and
[the sample event file](data/sample_events.csv).

## Repository layout

```text
include/arbreplay/  Public C++ API and domain types
src/                Engine implementation
tests/              Unit and integration tests
data/               Synthetic and recorded market-data fixtures
docs/               Architecture and research documentation
```

## Build

Requirements: C++20 compiler and CMake 3.20 or newer.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Replay CLI

Replay a normalized event CSV (provide its payout) or a snapshot/recording
directory (payout is read from its metadata):

```sh
./build/debug/arbreplay replay data/sample_events.csv --payout 1.000000
./build/debug/arbreplay replay data/recordings/EVENT_SLUG-live
```

Default output is a compact summary: event count, detection count, and the sum
of estimated net profits. Use `--verbose` for individual detections. The sum
is not realized P&L; detections may overlap the same displayed liquidity.

Optional flags for CSV replay:

- `--fee-bps 100`: estimated flat fee rate (100 bps = 1%). Defaults to zero;
  opportunities with non-positive estimated net profit are omitted.
- `--latency-ns 500000000`: recheck liquidity after a modeled delay. This
  applies to time-ordered event CSVs, not point-in-time snapshots.
- `--verbose`: include per-detection costs, fees, profit, quantity, and levels.

Fees are a configurable estimate, not a venue-specific schedule. Latency is a
simplified availability check, not order/fill simulation.

## Polymarket snapshot capture

The standard-library-only adapter captures one live binary Polymarket order
book and converts it into ArbReplay's normalized CSV format. It requires
Python 3.10 or newer.

Copy the event slug from its Polymarket URL, then run:

```sh
python3 tools/polymarket_snapshot.py EVENT_SLUG \
  --output data/recordings/EVENT_SLUG
```

If an event contains multiple eligible markets, the command lists their slugs.
Select one explicitly:

```sh
python3 tools/polymarket_snapshot.py EVENT_SLUG \
  --market MARKET_SLUG \
  --output data/recordings/EVENT_SLUG
```

Each capture preserves the raw event and order-book responses alongside the
normalized input:

```text
data/recordings/EVENT_SLUG/
├── event.json
├── yes-book.json
├── no-book.json
├── metadata.json
└── events.csv
```

Replay the resulting snapshot directory. The CLI finds `events.csv` and reads
`payout_per_set` from `metadata.json` automatically:

```sh
./build/debug/arbreplay replay data/recordings/EVENT_SLUG
```

For standalone CSV files, continue to supply the payout explicitly:

```sh
./build/debug/arbreplay replay data/sample_events.csv --payout 1.000000
```

The adapter captures one point-in-time book, not history. It is useful for a
quick current-state check or smoke test; the recorder below is the right input
for event-by-event replay and latency experiments. Snapshot rows are applied
atomically before checking for an opportunity, so no partial-book detections
are reported. Results still reflect displayed liquidity, not guaranteed fills
or realized profit.

## Kalshi snapshot capture

Capture one Kalshi binary-market order book and normalize it into the same CSV
format:

```sh
python3 tools/kalshi_snapshot.py MARKET_TICKER \
  --output data/recordings/MARKET_TICKER
```

The adapter saves the raw `orderbook.json`, `metadata.json`, and normalized
`events.csv`. Kalshi's binary order book provides YES and NO bid levels; the
adapter maps each side's bids directly and derives the complementary asks at
`1 - bid price`, preserving the level quantity. Replay the directory with the
same CLI command used for Polymarket snapshots:

```sh
./build/debug/arbreplay replay data/recordings/MARKET_TICKER
```

The adapter handles point-in-time snapshots only; it does not connect to a live
stream or place orders.

## Record a live Polymarket book

The read-only recorder subscribes to one binary market's public market stream,
saves the raw WebSocket messages, and writes normalized absolute level updates
to `events.csv`. Install its optional WebSocket dependency:

```sh
python3 -m pip install -r requirements-recorder.txt
```

Pass either an event slug or full event URL, then choose a recording duration.
End-to-end example:

```sh
EVENT_SLUG="btc-updown-5m-1792283100"
python3 tools/polymarket_recorder.py "$EVENT_SLUG" \
  --output "data/recordings/${EVENT_SLUG}-live" \
  --duration-seconds 1800

./build/debug/arbreplay replay "data/recordings/${EVENT_SLUG}-live"
```

If the event has multiple eligible markets, add `--market MARKET_SLUG` to the
record command. The recorder writes `events.csv`, `raw.jsonl`, `metadata.json`,
and `event.json`. If the output folder is non-empty, it asks before deleting
its contents (`n` is the default). A WebSocket disconnect marks the capture
incomplete rather than implying uninterrupted data.

This first recorder supports Polymarket only. It records market data and does
not place orders. Recorded feed updates are not a guarantee of executable
liquidity or realized profit.

Run the complete test suite with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make sanitize
```
