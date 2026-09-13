#include "arbreplay/replay_engine.hpp"
#include "arbreplay/book_side.hpp"
#include "arbreplay/complete_set_detector.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace arbreplay {

ReplayEngine::ReplayEngine(Market market, Money payout_per_set)
    : market_{std::move(market)}, payout_per_set_{payout_per_set} {}

const Market &ReplayEngine::market() const noexcept { return market_; }

std::optional<DetectedCompleteSetOpportunity>
ReplayEngine::apply(const MarketEvent &event) {
  auto *outcome = market_.find_outcome(event.outcome_id());
  if (outcome == nullptr) {
    throw std::invalid_argument{std::string{"unknown outcome: "} +
                                std::string{event.outcome_id().value()}};
  }
  switch (event.side()) {
  case OrderSide::ask:
    outcome->asks().update(event.price(), event.quantity());
    break;
  case OrderSide::bid:
    outcome->bids().update(event.price(), event.quantity());
    return std::nullopt;
  }

  auto opportunity = detect_complete_set_opportunity(market_, payout_per_set_);

  if (!opportunity.has_value()) {
    return std::nullopt;
  }

  return DetectedCompleteSetOpportunity{event.replay_key(),
                                        std::move(*opportunity)};
}

std::vector<DetectedCompleteSetOpportunity>
ReplayEngine::replay(const std::vector<MarketEvent> &events) {
  std::vector<DetectedCompleteSetOpportunity> detections{};

  for (const auto &event : events) {
    auto detected_event = ReplayEngine::apply(event);
    if (detected_event.has_value()) {
      detections.push_back(std::move(*detected_event));
    }
  }

  return detections;
}

} // namespace arbreplay
