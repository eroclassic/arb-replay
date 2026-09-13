#pragma once

#include "arbreplay/complete_set_opportunity.hpp"
#include "arbreplay/market_event.hpp"

#include <utility>

namespace arbreplay {
class DetectedCompleteSetOpportunity {
public:
  DetectedCompleteSetOpportunity(MarketEvent::ReplayKey replay_key,
                                 CompleteSetOpportunity opportunity)
      : replay_key_{replay_key}, opportunity_{std::move(opportunity)} {}

  [[nodiscard]] MarketEvent::ReplayKey replay_key() const noexcept {
    return replay_key_;
  }

  [[nodiscard]] const CompleteSetOpportunity &opportunity() const noexcept {
    return opportunity_;
  }

  [[nodiscard]] bool
  operator==(const DetectedCompleteSetOpportunity &) const noexcept = default;

private:
  MarketEvent::ReplayKey replay_key_;
  CompleteSetOpportunity opportunity_;
};
} // namespace arbreplay
