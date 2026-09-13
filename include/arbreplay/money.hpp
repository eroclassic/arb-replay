#pragma once

#include <arbreplay/fixed_point.hpp>
#include <arbreplay/quantity.hpp>

#include <compare>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arbreplay {
enum class RoundingMode { toward_zero, up, down };

class Money {
public:
  static constexpr std::uint8_t precision = fixed_precision;
  static constexpr std::int64_t scale = fixed_scale;

  [[nodiscard]] static Money from_raw(std::int64_t raw) noexcept {
    return Money{raw};
  }

  [[nodiscard]] static Money from_cents(std::int64_t cents) {
    constexpr auto factor = scale / 100;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if (cents > maximum / factor || cents < minimum / factor) {
      throw std::out_of_range{"Money is out of range"};
    }
    return Money{cents * factor};
  }

  [[nodiscard]] static Money from_decimal(std::string_view value) {
    return Money{detail::parse_fixed(value)};
  }

  [[nodiscard]] std::int64_t raw() const noexcept { return raw_; }

  [[nodiscard]] bool operator==(const Money &) const noexcept = default;
  [[nodiscard]] auto operator<=>(const Money &) const noexcept = default;

  [[nodiscard]] Money operator+(const Money &other) const {
    return Money{checked_add(raw_, other.raw_, "Money addition")};
  }

  [[nodiscard]] Money operator-(const Money &other) const {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if (other.raw_ == minimum) {
      if (raw_ >= 0) {
        throw std::overflow_error{"Money subtraction overflow"};
      }
      return Money{raw_ - other.raw_};
    }
    return Money{checked_add(raw_, -other.raw_, "Money subtraction")};
  }

  [[nodiscard]] Money multiply(Quantity quantity, RoundingMode mode) const {
    const auto quantity_raw = quantity.raw();
    if (raw_ == 0 || quantity_raw == 0) {
      return Money{0};
    }

    const auto whole = quantity_raw / Quantity::scale;
    const auto remainder = quantity_raw % Quantity::scale;
    const auto whole_result = checked_multiply(raw_, whole);

    const auto raw_quotient = raw_ / Quantity::scale;
    const auto raw_remainder = raw_ % Quantity::scale;
    const auto fractional_whole = checked_multiply(raw_quotient, remainder);
    const auto fractional_product = raw_remainder * remainder;
    const auto fractional_result = fractional_product / Quantity::scale;
    auto result = checked_add(
        checked_add(whole_result, fractional_whole, "Money multiplication"),
        fractional_result, "Money multiplication");

    if (fractional_product % Quantity::scale != 0) {
      if (mode == RoundingMode::up && fractional_product > 0) {
        result = checked_add(result, 1, "Money multiplication");
      } else if (mode == RoundingMode::down && fractional_product < 0) {
        result = checked_add(result, -1, "Money multiplication");
      }
    }
    return Money{result};
  }

  [[nodiscard]] Money operator*(Quantity quantity) const {
    return multiply(quantity, RoundingMode::toward_zero);
  }

private:
  std::int64_t raw_;

  explicit Money(std::int64_t raw) noexcept : raw_{raw} {}

  [[nodiscard]] static std::int64_t
  checked_add(std::int64_t left, std::int64_t right, const char *operation) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if (right > 0 && left > maximum - right) {
      throw std::overflow_error{std::string{operation} + " overflow"};
    }
    if (right < 0 && left < minimum - right) {
      throw std::overflow_error{std::string{operation} + " underflow"};
    }
    return left + right;
  }

  [[nodiscard]] static std::int64_t checked_multiply(std::int64_t left,
                                                     std::int64_t right) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if (left == 0 || right == 0) {
      return 0;
    }
    if ((left == -1 && right == minimum) || (right == -1 && left == minimum)) {
      throw std::overflow_error{"Money multiplication overflow"};
    }
    if (left > 0) {
      if ((right > 0 && left > maximum / right) ||
          (right < 0 && right < minimum / left)) {
        throw std::overflow_error{"Money multiplication overflow"};
      }
    } else if ((right > 0 && left < minimum / right) ||
               (right < 0 && left < maximum / right)) {
      throw std::overflow_error{"Money multiplication overflow"};
    }
    return left * right;
  }
};
} // namespace arbreplay
