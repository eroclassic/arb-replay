#pragma once

#include <arbreplay/money.hpp>

#include <cstdint>
#include <stdexcept>

namespace arbreplay {

// A configurable proportional fee estimate expressed in basis points.
// 100 basis points = 1%; this is not a venue-specific fee schedule.
class FeeRate {
public:
  static constexpr std::uint32_t basis_points_per_whole = 10'000;

  FeeRate() noexcept : basis_points_{0} {}

  [[nodiscard]] static FeeRate from_basis_points(std::uint32_t basis_points) {
    if (basis_points > basis_points_per_whole) {
      throw std::invalid_argument{"fee rate cannot exceed 100 percent"};
    }
    return FeeRate{basis_points};
  }

  [[nodiscard]] std::uint32_t basis_points() const noexcept {
    return basis_points_;
  }

  // Round up to the smallest representable Money unit so the estimate does
  // not understate fees.
  [[nodiscard]] Money apply(Money notional) const {
    const auto raw = notional.raw();
    if (raw < 0) {
      throw std::invalid_argument{"fee notional cannot be negative"};
    }

    constexpr auto denominator =
        static_cast<std::int64_t>(basis_points_per_whole);
    const auto whole = raw / denominator;
    const auto remainder = raw % denominator;
    const auto whole_fee = whole * static_cast<std::int64_t>(basis_points_);
    const auto fractional_numerator =
        remainder * static_cast<std::int64_t>(basis_points_);
    auto fractional_fee = fractional_numerator / denominator;
    if (fractional_numerator % denominator != 0) {
      ++fractional_fee;
    }

    return Money::from_raw(whole_fee + fractional_fee);
  }

  [[nodiscard]] bool operator==(const FeeRate &) const noexcept = default;

private:
  std::uint32_t basis_points_;

  explicit FeeRate(std::uint32_t basis_points) noexcept
      : basis_points_{basis_points} {}
};

} // namespace arbreplay
