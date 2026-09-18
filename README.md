# ArbReplay

**A C++20 prediction-market arbitrage replay engine.**

ArbReplay models prediction-market order books and detects depth-aware
complete-set arbitrage across binary and multi-outcome markets. The current
engine provides the in-memory market model, gross-opportunity detector, and
deterministic event replay with strict CSV ingestion and an offline CLI.
Execution simulation is the next layer.

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
| `CompleteSetOpportunity` | Aggregates every profitable level and calculates total cost, payout, and gross profit |

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

The detector currently reports gross opportunities from displayed ask
liquidity. Fees, latency, queue position, stale quotes, partial fills, and
legging risk are not yet included.

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
benchmarks/         Performance benchmarks
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

`--payout` is required and must be a positive fixed-point decimal. The command
prints the number of parsed events and detected gross opportunities:

```text
events: 6
detections: 2
```

The CLI reports detected opportunities, not executed trades. It does not add
their profits together because consecutive detections may refer to overlapping
order-book liquidity.

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

Replay the resulting snapshot with its recorded payout:

```sh
./build/debug/arbreplay replay \
  data/recordings/EVENT_SLUG/events.csv \
  --payout 1.000000
```

The adapter currently captures a point-in-time snapshot, not historical data
or a live stream. ArbReplay applies normalized rows one at a time, so detections
reported while the initial snapshot is being assembled are not yet reliable
trading signals. Atomic snapshot seeding is required before using those
detections as research results.

Run the complete test suite with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make sanitize
```
