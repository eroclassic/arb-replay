#pragma once

#include <arbreplay/fixed_point.hpp>

#include <compare>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace arbreplay {
class Quantity {
public:
  static constexpr std::uint8_t precision = fixed_precision;
  static constexpr std::int64_t scale = fixed_scale;

  [[nodiscard]] static Quantity from_raw(std::int64_t raw) {
    if (raw < 0) {
      throw std::out_of_range{"Quantity cannot be negative"};
    }
    return Quantity{raw};
  }

  [[nodiscard]] static Quantity from_contracts(std::int64_t contracts) {
    if (contracts < 0) {
      throw std::out_of_range{"Contracts cannot be negative"};
    }
    if (contracts > std::numeric_limits<std::int64_t>::max() / scale) {
      throw std::out_of_range{"Quantity is out of range"};
    }
    return Quantity{contracts * scale};
  }

  [[nodiscard]] static Quantity from_decimal(std::string_view value) {
    return from_raw(detail::parse_fixed(value));
  }

  [[nodiscard]] std::int64_t raw() const noexcept { return raw_; }

  [[nodiscard]] bool operator==(const Quantity &) const noexcept = default;
  [[nodiscard]] auto operator<=>(const Quantity &) const noexcept = default;

private:
  std::int64_t raw_;

  explicit Quantity(std::int64_t raw) noexcept : raw_{raw} {}
};
} // namespace arbreplay
