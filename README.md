# ArbReplay

**A C++20 prediction-market arbitrage replay engine.**

ArbReplay will reconstruct recorded prediction-market order books and measure
how many theoretical arbitrage opportunities remain profitable after fees,
available liquidity, latency, and partial execution are considered.

## Architecture

ArbReplay models binary and multi-outcome prediction markets using the same
composable order-book hierarchy:

```text
ReplayEngine
└── Market
    └── outcomes: collection of OutcomeBook
        ├── OutcomeBook: YES
        │   ├── bids: BookSide
        │   │   └── levels: map<Price, Quantity>
        │   └── asks: BookSide
        │       └── levels: map<Price, Quantity>
        ├── OutcomeBook: NO
        │   ├── bids: BookSide
        │   │   └── levels: map<Price, Quantity>
        │   └── asks: BookSide
        │       └── levels: map<Price, Quantity>
        └── OutcomeBook: additional outcome
            ├── bids: BookSide
            └── asks: BookSide
```

A binary market contains YES and NO outcome books. A multi-outcome market uses
the same structure with additional mutually exclusive outcomes.

| Type | Responsibility |
|---|---|
| `ReplayEngine` | Applies recorded market events chronologically |
| `Market` | Owns every possible outcome for one prediction question |
| `OutcomeBook` | Holds the bids and asks for one tradable outcome |
| `BookSide` | Maintains one ordered collection of bids or asks |
| `BookLevel` | Pairs one price with its available quantity |
| `Price` | Represents one contract price from 0 to 100 cents |
| `Quantity` | Represents a non-negative number of contracts |

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

Run the same smoke test with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make sanitize
```
