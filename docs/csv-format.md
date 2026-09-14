# Normalized market-event CSV format

ArbReplay consumes venue-neutral, absolute order-book level updates. A venue
adapter converts native Kalshi, Polymarket, or other exchange messages into this
format before replay.

Each CSV file contains events for exactly one market and begins with this exact
header:

```csv
observed_at_ns,sequence,outcome_id,side,price,quantity
```

## Fields

| Field | Type | Rules |
|---|---|---|
| `observed_at_ns` | signed 64-bit integer | Non-negative Unix timestamp in nanoseconds representing when the event became observable |
| `sequence` | unsigned 64-bit integer | Deterministic tie-breaker for events with the same timestamp |
| `outcome_id` | string | Non-empty identifier for one mutually exclusive outcome |
| `side` | string | Exactly `bid` or `ask` |
| `price` | fixed-point decimal | Value from `0` through `1`, with no more than six fractional digits |
| `quantity` | fixed-point decimal | Non-negative absolute quantity, with no more than six fractional digits |

`quantity` is the new total quantity at a price level, not a signed change.
A quantity of zero removes that level from the reconstructed order book.

Events do not need to appear in chronological order. `ReplayEngine` orders them
by `(observed_at_ns, sequence)` and rejects conflicting events with the same
key.

## Example

```csv
observed_at_ns,sequence,outcome_id,side,price,quantity
1726401600000000000,1,YES,bid,0.400000,15.000000
1726401600000000000,2,YES,ask,0.420000,10.000000
1726401600000000000,3,NO,bid,0.530000,12.000000
1726401600000000000,4,NO,ask,0.550000,8.500000
1726401601000000000,5,YES,ask,0.420000,0
```

The final row removes the YES ask at `0.420000`.

## Parser behavior

- The header and side values are case-sensitive.
- Unix LF and Windows CRLF line endings are accepted.
- A header without data rows is valid and produces an empty event list.
- Empty files, incorrect headers, malformed numbers, and rows with other than
  six fields are rejected.
- Parse failures are reported as `std::invalid_argument` with the source line
  number.
- Stream read failures are reported as `std::runtime_error`.

The version-one parser implements a deliberately small CSV subset. Quoted
fields, embedded commas, embedded newlines, and comments are not supported.

## Venue normalization

Venue adapters must produce absolute levels:

- Native full-book snapshots are diffed against the previous snapshot.
- Signed quantity deltas are accumulated into absolute quantities.
- Missing venue sequence numbers are replaced with deterministic adapter
  sequence numbers.
- Native token or outcome identifiers are mapped to stable `outcome_id` values.
- Prices are represented as payout fractions between zero and one.

Venue name, native market identifier, payout terms, and outcome mappings are
market metadata and are intentionally not repeated in every event row.
