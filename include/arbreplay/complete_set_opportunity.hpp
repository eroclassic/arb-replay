#pragma once

#include <arbreplay/complete_set_opportunity_level.hpp>
#include <arbreplay/fee_rate.hpp>
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
                         std::vector<CompleteSetOpportunityLevel> levels,
                         FeeRate fee_rate = FeeRate{})
      : payout_per_set_{payout_per_set}, levels_{std::move(levels)},
        fee_rate_{fee_rate} {
    if (payout_per_set_.raw() <= 0) {
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
      const auto level_cost = level.total_cost();
      const auto level_payout =
          payout_per_set_.multiply(level.quantity(), RoundingMode::down);
      if (level_payout <= level_cost + fee_rate_.apply(level_cost)) {
        throw std::invalid_argument{
            "every opportunity level must have positive net profit"};
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

  [[nodiscard]] FeeRate fee_rate() const noexcept { return fee_rate_; }

  [[nodiscard]] Quantity total_quantity() const {
    std::int64_t total = 0;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    for (const auto &level : levels_) {
      const auto quantity_raw = level.quantity().raw();
      if (total > maximum - quantity_raw) {
        throw std::overflow_error{"total opportunity quantity overflow"};
      }
      total += quantity_raw;
    }
    return Quantity::from_raw(total);
  }

  [[nodiscard]] Money total_cost() const {
    auto total = Money::from_raw(0);
    for (const auto &level : levels_) {
      total = total + level.total_cost();
    }
    return total;
  }

  [[nodiscard]] Money total_payout() const {
    return payout_per_set_.multiply(total_quantity(), RoundingMode::down);
  }

  [[nodiscard]] Money gross_profit() const {
    return total_payout() - total_cost();
  }

  [[nodiscard]] Money estimated_fees() const {
    auto total = Money::from_raw(0);
    for (const auto &level : levels_) {
      total = total + fee_rate_.apply(level.total_cost());
    }
    return total;
  }

  [[nodiscard]] Money net_profit() const {
    return gross_profit() - estimated_fees();
  }

  [[nodiscard]] bool
  operator==(const CompleteSetOpportunity &other) const noexcept = default;

private:
  Money payout_per_set_;
  std::vector<CompleteSetOpportunityLevel> levels_;
  FeeRate fee_rate_;
};
} // namespace arbreplay
