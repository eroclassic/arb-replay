#pragma once

#include <compare>
#include <cstdint>
#include <stdexcept>

namespace arbreplay {
class Money {
public:
  [[nodiscard]] static Money from_cents(std::int64_t cents) {
    return Money{cents};
  }

  [[nodiscard]] std::int64_t cents() const noexcept { return cents_; }

  [[nodiscard]] bool operator==(const Money &other) const noexcept = default;

  [[nodiscard]] auto operator<=>(const Money &other) const noexcept = default;

  [[nodiscard]] Money operator+(const Money &other) const {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();

    if (other.cents_ > 0 && cents_ > maximum - other.cents_) {
      throw std::overflow_error{"Money addition overflow"};
    }

    if (other.cents_ < 0 && cents_ < minimum - other.cents_) {
      throw std::overflow_error{"Money addition underflow"};
    }

    return Money{cents_ + other.cents_};
  }

  [[nodiscard]] Money operator-(const Money &other) const {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();

    if (other.cents_ < 0 && cents_ > maximum + other.cents_) {
      throw std::overflow_error{"Money substraction overflow"};
    }

    if (other.cents_ > 0 && cents_ < minimum + other.cents_) {
      throw std::overflow_error{"Money substraction underflow"};
    }

    return Money{cents_ - other.cents_};
  }

private:
  std::int64_t cents_;

  explicit Money(std::int64_t cents) noexcept : cents_{cents} {}
};
} // namespace arbreplay
