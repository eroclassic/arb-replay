#pragma once

#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include <stdexcept>

namespace arbreplay {
class CompleteSetOpportunityLevel {
public:
  CompleteSetOpportunityLevel(Money cost_per_set, Quantity quantity)
      : cost_per_set_{cost_per_set}, quantity_{quantity} {
    if (cost_per_set_.cents() < 0) {
      throw std::invalid_argument{"cost per set cannot be negative"};
    }
    if (quantity_.contracts() == 0) {
      throw std::invalid_argument{"quantity must be positive"};
    }
  }

  [[nodiscard]] Money cost_per_set() const noexcept { return cost_per_set_; }
  [[nodiscard]] Quantity quantity() const noexcept { return quantity_; }
  [[nodiscard]] Money total_cost() const { return cost_per_set_ * quantity_; }

  [[nodiscard]] bool operator==(
      const CompleteSetOpportunityLevel &other) const noexcept = default;

private:
  Money cost_per_set_;
  Quantity quantity_;
};
} // namespace arbreplay
