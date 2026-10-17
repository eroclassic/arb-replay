#include "arbreplay/replay_engine.hpp"
#include "arbreplay/book_side.hpp"
#include "arbreplay/complete_set_detector.hpp"
#include "arbreplay/complete_set_opportunity.hpp"
#include "arbreplay/detected_complete_set_opportunity.hpp"
#include "arbreplay/market_event.hpp"
#include "arbreplay/price.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
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

struct PendingCandidate {
  MarketEvent::Timestamp arrival_time;
  MarketEvent::ReplayKey signal_key;
};

void process_pending_until(
    const Market &market, Money payout_per_set, FeeRate fee_rate,
    MarketEvent::Timestamp cutoff, bool include_equal,
    std::deque<PendingCandidate> &pending,
    std::vector<DetectedCompleteSetOpportunity> &detections) {
  while (!pending.empty()) {
    const auto arrival = pending.front().arrival_time;
    const bool due = include_equal ? arrival <= cutoff : arrival < cutoff;

    if (!due) {
      break;
    }

    const auto candidate = pending.front();
    pending.pop_front();

    auto opportunity =
        detect_complete_set_opportunity(market, payout_per_set, fee_rate);
    if (opportunity.has_value()) {
      detections.emplace_back(candidate.signal_key, std::move(*opportunity));
    }
  }
}

} // namespace

ReplayEngine::ReplayEngine(Market market, Money payout_per_set,
                           MarketEvent::Timestamp execution_latency,
                           FeeRate fee_rate)
    : market_{std::move(market)}, payout_per_set_{payout_per_set},
      execution_latency_{execution_latency}, fee_rate_{fee_rate} {
  if (execution_latency_ < MarketEvent::Timestamp::zero()) {
    throw std::invalid_argument{"execution latency cannot be negative"};
  }
}

const Market &ReplayEngine::market() const noexcept { return market_; }

std::optional<DetectedCompleteSetOpportunity>
ReplayEngine::apply(const MarketEvent &event) {
  apply_event_to_market(market_, event);

  if (event.side() == OrderSide::bid) {
    return std::nullopt;
  }

  auto opportunity =
      detect_complete_set_opportunity(market_, payout_per_set_, fee_rate_);

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

  auto opportunity = detect_complete_set_opportunity(
      temporary_market, payout_per_set_, fee_rate_);
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
  std::deque<PendingCandidate> pending{};
  const auto unique_events = ordered_unique_events(events);

  std::size_t group_begin = 0;

  while (group_begin < unique_events.size()) {
    const auto group_timestamp = unique_events[group_begin].timestamp();
    std::size_t group_end = group_begin + 1;

    while (group_end < unique_events.size() &&
           unique_events[group_end].timestamp() == group_timestamp) {
      ++group_end;
    }

    process_pending_until(market_, payout_per_set_, fee_rate_, group_timestamp,
                          false, pending, detections);

    for (std::size_t i = group_begin; i < group_end; ++i) {
      const auto &event = unique_events[i];
      auto detected = apply(event);
      if (detected.has_value()) {
        if (execution_latency_ == MarketEvent::Timestamp::zero()) {
          detections.push_back(std::move(*detected));
          continue;
        }

        using TimestampRep = MarketEvent::Timestamp::rep;
        const auto signal_time = event.timestamp().count();
        const auto latency = execution_latency_.count();
        if (latency > std::numeric_limits<TimestampRep>::max() - signal_time) {
          throw std::overflow_error{"execution arrival timestamp overflow"};
        }

        pending.push_back(PendingCandidate{
            event.timestamp() + execution_latency_, detected->replay_key()});
      }
    }

    process_pending_until(market_, payout_per_set_, fee_rate_, group_timestamp,
                          true, pending, detections);

    group_begin = group_end;
  }
  return detections;
}

} // namespace arbreplay
