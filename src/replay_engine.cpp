#include "arbreplay/replay_engine.hpp"

#include <optional>
#include <utility>

namespace arbreplay {

ReplayEngine::ReplayEngine(Market market, Money payout_per_set)
    : market_{std::move(market)}, payout_per_set_{payout_per_set} {}

const Market &ReplayEngine::market() const noexcept { return market_; }

// TODO: Apply one MarketEvent to the appropriate outcome book and side, then
// run complete-set detection against the updated market.
std::optional<CompleteSetOpportunity>
ReplayEngine::apply(const MarketEvent &event) {
  (void)event;
  return std::nullopt;
}

} // namespace arbreplay
