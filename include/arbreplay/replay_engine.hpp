#pragma once

#include "arbreplay/market_event.hpp"
#include <arbreplay/complete_set_opportunity.hpp>
#include <arbreplay/market.hpp>
#include <arbreplay/money.hpp>
#include <optional>

namespace arbreplay {

class ReplayEngine {
public:
  explicit ReplayEngine(Market market, Money payout_per_set);

  [[nodiscard]] std::optional<CompleteSetOpportunity>
  apply(const MarketEvent &event);

  [[nodiscard]] const Market &market() const noexcept;

private:
  Market market_;
  Money payout_per_set_;
};

} // namespace arbreplay
