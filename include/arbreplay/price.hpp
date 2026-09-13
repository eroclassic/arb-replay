#pragma once

#include <arbreplay/fixed_point.hpp>

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace arbreplay {
class Price {
public:
  static constexpr std::uint8_t precision = fixed_precision;
  static constexpr std::int64_t scale = fixed_scale;

  [[nodiscard]] static Price from_raw(std::int64_t raw) {
    if (raw < 0 || raw > scale) {
      throw std::out_of_range{"Price must be between 0 and 1"};
    }
    return Price{raw};
  }

  [[nodiscard]] static Price from_cents(std::int64_t cents) {
    if (cents < 0 || cents > 100) {
      throw std::out_of_range{"Price must be between 0 and 100 cents"};
    }
    return Price{cents * (scale / 100)};
  }

  [[nodiscard]] static Price from_decimal(std::string_view value) {
    return from_raw(detail::parse_fixed(value));
  }

  [[nodiscard]] std::int64_t raw() const noexcept { return raw_; }

  [[nodiscard]] Price complement() const noexcept {
    return Price{scale - raw_};
  }

  [[nodiscard]] bool operator==(const Price &) const noexcept = default;
  [[nodiscard]] auto operator<=>(const Price &) const noexcept = default;

private:
  std::int64_t raw_;

  explicit Price(std::int64_t raw) noexcept : raw_{raw} {}
};
} // namespace arbreplay
