#pragma once

#include "arbreplay/money.hpp"
#include "arbreplay/quantity.hpp"
#include <stdexcept>
namespace arbreplay {

class CompleteSetOpportunity {
public:
  CompleteSetOpportunity(Money cost_per_set, Money payout_per_set,
                         Quantity quantity)
      : cost_per_set_{cost_per_set},
        payout_per_set_{payout_per_set}, quantity_{quantity} {
    if (cost_per_set_.cents() < 0) {
      throw std::invalid_argument{"cost per set cannot be negative"};
    }

    if (payout_per_set_ <= cost_per_set_) {
      throw std::invalid_argument{
          "payout per set must be greater than cost per set"};
    }

    if (quantity_.contracts() == 0) {
      throw std::invalid_argument{"quantity must be positive"};
    }
  }

  [[nodiscard]] Money cost_per_set() const noexcept { return cost_per_set_; }

  [[nodiscard]] Money payout_per_set() const noexcept {
    return payout_per_set_;
  }

  [[nodiscard]] Quantity quantity() const noexcept { return quantity_; }

  [[nodiscard]] bool
  operator==(const CompleteSetOpportunity &other) const noexcept = default;

  [[nodiscard]] Money profit_per_set() const {
    return payout_per_set_ - cost_per_set_;
  }

  [[nodiscard]] Money total_cost() const {
    return cost_per_set_ * quantity_;
  }

  [[nodiscard]] Money total_payout() const {
    return payout_per_set_ * quantity_;
  }

  [[nodiscard]] Money gross_profit() const {
    return profit_per_set() * quantity_;
  }

private:
  Money cost_per_set_;
  Money payout_per_set_;
  Quantity quantity_;
};
} // namespace arbreplay
