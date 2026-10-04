#include "arbreplay/replay_engine.hpp"
#include "arbreplay/book_side.hpp"
#include "arbreplay/complete_set_detector.hpp"
#include "arbreplay/complete_set_opportunity.hpp"
#include "arbreplay/detected_complete_set_opportunity.hpp"
#include "arbreplay/market_event.hpp"
#include "arbreplay/price.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace arbreplay {
namespace {

void apply_event_to_market(Market &market, const MarketEvent &event) {
  auto *outcome = market.find_outcome(event.outcome_id());
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
    break;
  }
}

[[nodiscard]] Market make_empty_market_like(const Market &source) {
  Market result;

  for (const auto &entry : source) {
    const bool added = result.add_outcome(entry.first);
    if (!added) {
      throw std::logic_error{"duplicate outcome while rebuilding market"};
    }
  }

  return result;
}

[[nodiscard]] std::vector<MarketEvent>
ordered_unique_events(const std::vector<MarketEvent> &events) {
  auto sorted_events = events;
  std::sort(sorted_events.begin(), sorted_events.end(),
            [](const MarketEvent &left, const MarketEvent &right) {
              return left.replay_key() < right.replay_key();
            });

  std::vector<MarketEvent> unique_events{};
  unique_events.reserve(sorted_events.size());

  for (const auto &event : sorted_events) {
    if (unique_events.empty()) {
      unique_events.push_back(event);
      continue;
    }

    const auto &prev = unique_events.back();
    if (event.replay_key() == prev.replay_key()) {
      if (event != prev) {
        throw std::invalid_argument{
            "Two events with same key but different payload"};
      }
      continue;
    }

    unique_events.push_back(event);
  }

  return unique_events;
}

} // namespace

ReplayEngine::ReplayEngine(Market market, Money payout_per_set)
    : market_{std::move(market)}, payout_per_set_{payout_per_set} {}

const Market &ReplayEngine::market() const noexcept { return market_; }

std::optional<DetectedCompleteSetOpportunity>
ReplayEngine::apply(const MarketEvent &event) {
  apply_event_to_market(market_, event);

  if (event.side() == OrderSide::bid) {
    return std::nullopt;
  }

  auto opportunity = detect_complete_set_opportunity(market_, payout_per_set_);

  if (!opportunity.has_value()) {
    return std::nullopt;
  }

  return DetectedCompleteSetOpportunity{event.replay_key(),
                                        std::move(*opportunity)};
}

std::optional<DetectedCompleteSetOpportunity>
ReplayEngine::apply_snapshot(const std::vector<MarketEvent> &events) {
  if (events.empty()) {
    return std::nullopt;
  }

  const auto snapshot_events = ordered_unique_events(events);
  auto temporary_market = make_empty_market_like(market_);

  for (const auto &event : snapshot_events) {
    apply_event_to_market(temporary_market, event);
  }

  auto opportunity =
      detect_complete_set_opportunity(temporary_market, payout_per_set_);
  std::optional<DetectedCompleteSetOpportunity> opp;

  if (opportunity.has_value()) {
    opp = DetectedCompleteSetOpportunity{snapshot_events.back().replay_key(),
                                         std::move(*opportunity)};
  }
  market_ = std::move(temporary_market);

  if (opp.has_value()) {
    return opp;
  } else {
    return std::nullopt;
  }
}

std::vector<DetectedCompleteSetOpportunity>
ReplayEngine::replay(const std::vector<MarketEvent> &events) {
  std::vector<DetectedCompleteSetOpportunity> detections{};
  const auto unique_events = ordered_unique_events(events);

  for (const auto &event : unique_events) {
    auto detected_event = apply(event);
    if (detected_event.has_value()) {
      detections.push_back(std::move(*detected_event));
    }
  }

  return detections;
}

} // namespace arbreplay
