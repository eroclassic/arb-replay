# ArbReplay

**A C++20 prediction-market arbitrage replay engine.**

ArbReplay will reconstruct recorded prediction-market order books and measure
how many theoretical arbitrage opportunities remain profitable after fees,
available liquidity, latency, and partial execution are considered.

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
