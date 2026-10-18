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

Replay a normalized CSV file as a binary YES/NO market:

```sh
./build/debug/arbreplay replay data/sample_events.csv --payout 1.000000
```

`--payout` is required and must be a positive fixed-point decimal. By default,
the CLI prints a compact summary: event count, detection count, and the sum of
estimated net profits across detections. This is a signal-profit sum, not
realized P&L, because detections may refer to overlapping liquidity:

```text
events: 6
detections: 2
estimated_net_profit_sum: 0.425000
note: signal-profit sum, not realized P&L; detections may overlap
```

Pass `--verbose` to include signal timestamps, available quantity, costs, fees,
gross/net profit, and the opportunity's price levels. Amounts and quantities
use six-decimal fixed-point notation:

```sh
./build/debug/arbreplay replay data/sample_events.csv \
  --payout 1.000000 --fee-bps 100 --verbose
```

Example verbose opportunity:

```text
opportunity 1:
  signal_time_ns: 1726401600000000000
  sequence: 4
  available_sets: 8.500000
  payout_per_set: 1.000000
  total_cost: 8.245000
  estimated_fees: 0.082450
  gross_profit: 0.255000
  estimated_net_profit: 0.172550
  levels:
    - cost_per_set: 0.970000, quantity: 8.500000
```

Optionally model a fixed execution delay in nanoseconds:

```sh
./build/debug/arbreplay replay data/sample_events.csv \
  --payout 1.000000 --latency-ns 500000000
```

Optionally configure a flat estimated taker fee in basis points (`200` means
2% of the purchase notional):

```sh
./build/debug/arbreplay replay data/sample_events.csv \
  --payout 1.000000 --fee-bps 200
```

The fee rate defaults to zero and is rounded up to the nearest representable
`Money` unit. This configurable flat rate is an estimate, not a venue-specific
fee schedule. Opportunities with zero or negative estimated net profit are
not reported.

Latency is a non-negative integer and defaults to zero. The replay engine
rechecks a detected opportunity against the book when the delayed order would
arrive; signals that are no longer available are discarded. This is a
simplified availability check, not order execution or fill simulation. It
applies to time-ordered CSV replay, not point-in-time snapshot directories.

The CLI reports detected opportunities, not executed trades. Compact mode sums
estimated net profits across detections for a quick signal summary, but that
sum is not realized P&L because detections may refer to overlapping order-book
liquidity. Verbose mode shows each detection separately. Venue-specific fees,
queue position, partial fills, and execution behavior are not simulated. The
configurable flat fee is only an estimate, not a venue-specific fee schedule.

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

The adapter currently captures a point-in-time snapshot, not historical data
or a live stream. ArbReplay applies the snapshot rows atomically: it builds a
temporary book, commits the complete snapshot, and then checks for an
opportunity once. This avoids reporting detections from a partially assembled
snapshot. A detected opportunity still reflects displayed liquidity and is
not a guarantee of execution or realized profit.

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

Pass either the event slug or the full Polymarket event URL, then choose a
duration in seconds. For example:

```sh
EVENT_URL='https://polymarket.com/event/btc-updown-5m-1792283100'
python3 tools/polymarket_recorder.py "$EVENT_URL" \
  --output data/recordings/btc-updown-5m-1792283100-live \
  --duration-seconds 1800
```

If the event contains multiple eligible markets, pass `--market MARKET_SLUG`.
The recorder creates `events.csv`, `raw.jsonl`, `metadata.json`, and
`event.json`. If that output directory already contains files, it asks before
deleting them; answering `n` (the default) leaves the previous recording
untouched. A WebSocket disconnect ends the run and leaves metadata marked
incomplete so a gap is not mistaken for a continuous capture. Inspect the
recording, then replay it with:

```sh
./build/debug/arbreplay replay \
  data/recordings/btc-updown-5m-1792283100-live
```

This first recorder supports Polymarket only. It records market data and does
not place orders. Recorded feed updates are not a guarantee of executable
liquidity or realized profit.

Run the complete test suite with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make sanitize
```
