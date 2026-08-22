# ArbReplay

**A C++20 prediction-market arbitrage replay engine.**

ArbReplay will reconstruct recorded prediction-market order books and measure
how many theoretical arbitrage opportunities remain profitable after fees,
available liquidity, latency, and partial execution are considered.

This is an offline research and learning project. Version one will not place
real-money trades.

## Current status

Project infrastructure only. The core implementation is intentionally empty so
that it can be hand-coded and fully understood by the project owner.

## Repository layout

```text
include/arbreplay/  Public C++ headers (hand-coded core)
src/                C++ implementation files (hand-coded core)
tests/              Smoke test now; domain tests added during each milestone
benchmarks/         Performance benchmarks
data/               Small synthetic or recorded fixtures (never credentials)
docs/               Design notes, research report, and learning boundary
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

See [docs/LEARNING_BOUNDARY.md](docs/LEARNING_BOUNDARY.md) for the division
between hand-coded learning work and appropriate AI assistance.
