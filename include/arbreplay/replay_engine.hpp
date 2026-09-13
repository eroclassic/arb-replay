#pragma once

#include "arbreplay/market_event.hpp"
#include <arbreplay/detected_complete_set_opportunity.hpp>
#include <arbreplay/market.hpp>
#include <arbreplay/money.hpp>
#include <optional>
#include <vector>

namespace arbreplay {

class ReplayEngine {
public:
  explicit ReplayEngine(Market market, Money payout_per_set);

  [[nodiscard]] std::optional<DetectedCompleteSetOpportunity>
  apply(const MarketEvent &event);

  [[nodiscard]] const Market &market() const noexcept;

  [[nodiscard]] std::vector<DetectedCompleteSetOpportunity>
  replay(const std::vector<MarketEvent> &events);

private:
  Market market_;
  Money payout_per_set_;
};

} // namespace arbreplay
