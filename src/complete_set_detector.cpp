#include "arbreplay/complete_set_detector.hpp"
#include "arbreplay/book_side.hpp"
#include "arbreplay/complete_set_opportunity.hpp"
#include "arbreplay/complete_set_opportunity_level.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace arbreplay {
namespace {
struct AskCursor {
  BookSide::Levels::const_iterator current;
  BookSide::Levels::const_iterator end;
  std::uint64_t remaining;
};
} // namespace
[[nodiscard]] std::optional<CompleteSetOpportunity>
detect_complete_set_opportunity(const Market &market, Money payout_per_set) {
  if (payout_per_set.cents() <= 0) {
    throw std::invalid_argument{"payout per set must be positive"};
  }

  if (market.size() < 2) {
    return std::nullopt;
  }

  std::vector<AskCursor> cursors;
  cursors.reserve(market.size());

  for (const auto &[outcome_id, outcome_book] : market) {
    (void)outcome_id;

    const auto &ask_levels = outcome_book.asks().levels();

    if (ask_levels.empty()) {
      return std::nullopt;
    }

    const auto best_ask = ask_levels.begin();
    const auto end = ask_levels.end();
    cursors.push_back(AskCursor{
        best_ask,
        end,
        best_ask->second.contracts(),
    });
  }

  std::vector<CompleteSetOpportunityLevel> opportunity_levels;

  while (true) {
    auto combined_cost = Money::from_cents(0);
    auto minimum_remaining = std::numeric_limits<std::uint64_t>::max();
    for (const auto &cursor : cursors) {
      const Price ask_price = cursor.current->first;
      const Money ask_cost = Money::from_cents(ask_price.cents());

      combined_cost = combined_cost + ask_cost;
      minimum_remaining = std::min(minimum_remaining, cursor.remaining);
    }

    if (combined_cost >= payout_per_set) {
      break;
    }

    opportunity_levels.push_back(CompleteSetOpportunityLevel{
        combined_cost,
        Quantity::from_contracts(static_cast<std::int64_t>(minimum_remaining)),
    });

    bool liquidity_exhausted = false;

    for (auto &cursor : cursors) {
      cursor.remaining -= minimum_remaining;
      if (cursor.remaining == 0) {
        ++cursor.current;

        if (cursor.current == cursor.end) {
          liquidity_exhausted = true;
          continue;
        }

        cursor.remaining = cursor.current->second.contracts();
      }
    }

    if (liquidity_exhausted) {
      break;
    }
  }

  if (opportunity_levels.empty()) {
    return std::nullopt;
  }

  return CompleteSetOpportunity{
      payout_per_set,
      opportunity_levels,
  };
}
} // namespace arbreplay
