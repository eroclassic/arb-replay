#pragma once

#include <arbreplay/market_event.hpp>

#include <iosfwd>
#include <vector>

namespace arbreplay {

// Parses normalized, absolute order-book level updates from CSV input.
//
// Expected header:
// observed_at_ns,sequence,outcome_id,side,price,quantity
[[nodiscard]] std::vector<MarketEvent>
parse_market_events_csv(std::istream &input);

} // namespace arbreplay
