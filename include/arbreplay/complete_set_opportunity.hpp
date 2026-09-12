#pragma once

#include <arbreplay/complete_set_opportunity_level.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arbreplay {
class CompleteSetOpportunity {
public:
  CompleteSetOpportunity(Money payout_per_set,
                         std::vector<CompleteSetOpportunityLevel> levels)
      : payout_per_set_{payout_per_set}, levels_{std::move(levels)} {
    if (payout_per_set_.cents() <= 0) {
      throw std::invalid_argument{"payout per set must be positive"};
    }
    if (levels_.empty()) {
      throw std::invalid_argument{
          "opportunity must contain at least one level"};
    }
    for (const auto &level : levels_) {
      if (level.cost_per_set() >= payout_per_set_) {
        throw std::invalid_argument{
            "every opportunity level must cost less than its payout"};
      }
    }
  }

  [[nodiscard]] Money payout_per_set() const noexcept {
    return payout_per_set_;
  }

  [[nodiscard]] const std::vector<CompleteSetOpportunityLevel> &
  levels() const noexcept {
    return levels_;
  }

  [[nodiscard]] Quantity total_quantity() const {
    std::int64_t total = 0;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    for (const auto &level : levels_) {
      const auto contracts =
          static_cast<std::int64_t>(level.quantity().contracts());
      if (total > maximum - contracts) {
        throw std::overflow_error{"total opportunity quantity overflow"};
      }
      total += contracts;
    }
    return Quantity::from_contracts(total);
  }

  [[nodiscard]] Money total_cost() const {
    auto total = Money::from_cents(0);
    for (const auto &level : levels_) {
      total = total + level.total_cost();
    }
    return total;
  }

  [[nodiscard]] Money total_payout() const {
    return payout_per_set_ * total_quantity();
  }

  [[nodiscard]] Money gross_profit() const {
    return total_payout() - total_cost();
  }

  [[nodiscard]] bool
  operator==(const CompleteSetOpportunity &other) const noexcept = default;

private:
  Money payout_per_set_;
  std::vector<CompleteSetOpportunityLevel> levels_;
};
} // namespace arbreplay
