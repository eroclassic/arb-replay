#pragma once

#include "arbreplay/book_side.hpp"
#include "arbreplay/outcome_id.hpp"
#include "arbreplay/price.hpp"
#include "arbreplay/quantity.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace arbreplay {

class MarketEvent {
public:
  using Timestamp = std::chrono::nanoseconds;
  using ReplayKey = std::pair<Timestamp, std::uint64_t>;

  MarketEvent(Timestamp timestamp, std::uint64_t sequence, OutcomeId outcome_id,
              OrderSide side, Price price, Quantity quantity)
      : timestamp_{timestamp}, sequence_{sequence}, outcome_id_{std::move(
                                                        outcome_id)},
        side_{side}, price_{price}, quantity_{quantity} {
    if (timestamp_ < Timestamp::zero()) {
      throw std::invalid_argument{"timestamp cannot be negative"};
    }
  }

  [[nodiscard]] Timestamp timestamp() const noexcept { return timestamp_; }

  [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }

  [[nodiscard]] const OutcomeId &outcome_id() const noexcept {
    return outcome_id_;
  }

  [[nodiscard]] OrderSide side() const noexcept { return side_; }

  [[nodiscard]] Price price() const noexcept { return price_; }

  [[nodiscard]] Quantity quantity() const noexcept { return quantity_; }

  [[nodiscard]] bool operator==(const MarketEvent &) const noexcept = default;

  [[nodiscard]] ReplayKey replay_key() const noexcept {
    return ReplayKey{timestamp_, sequence_};
  }

private:
  Timestamp timestamp_;
  std::uint64_t sequence_;
  OutcomeId outcome_id_;
  OrderSide side_;
  Price price_;
  Quantity quantity_;
};
} // namespace arbreplay
