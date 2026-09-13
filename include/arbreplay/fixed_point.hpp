#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace arbreplay {
inline constexpr std::uint8_t fixed_precision = 6;
inline constexpr std::int64_t fixed_scale = 1'000'000;

namespace detail {
struct DecimalParts {
  std::uint64_t whole{};
  std::uint64_t fraction{};
};

[[nodiscard]] inline bool parse_sign(std::string_view text,
                                     std::size_t &position) {
  if (text[position] != '+' && text[position] != '-') {
    return false;
  }
  const bool negative = text[position] == '-';
  ++position;
  return negative;
}

[[nodiscard]] inline std::uint64_t magnitude_limit(bool negative) noexcept {
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  if (negative) {
    return static_cast<std::uint64_t>(maximum) + 1U;
  }
  return static_cast<std::uint64_t>(maximum);
}

[[nodiscard]] inline std::uint64_t
append_digit(std::uint64_t value, std::uint64_t digit, std::uint64_t limit) {
  if (value > (limit - digit) / 10U) {
    throw std::out_of_range{"fixed-point value is out of range"};
  }
  return value * 10U + digit;
}

[[nodiscard]] inline DecimalParts parse_decimal_parts(std::string_view text,
                                                      std::size_t position,
                                                      std::uint64_t limit) {
  DecimalParts parts{};
  std::uint8_t fractional_digits = 0;
  bool saw_digit = false;
  bool saw_decimal_point = false;

  for (; position < text.size(); ++position) {
    const char character = text[position];
    if (character == '.') {
      if (saw_decimal_point) {
        throw std::invalid_argument{
            "fixed-point value has multiple decimal points"};
      }
      saw_decimal_point = true;
      continue;
    }
    if (character < '0' || character > '9') {
      throw std::invalid_argument{"fixed-point value contains a non-digit"};
    }

    saw_digit = true;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (!saw_decimal_point) {
      parts.whole = append_digit(parts.whole, digit, limit);
      continue;
    }

    if (fractional_digits == fixed_precision) {
      throw std::invalid_argument{"fixed-point value has too much precision"};
    }
    parts.fraction = parts.fraction * 10U + digit;
    ++fractional_digits;
  }

  if (!saw_digit) {
    throw std::invalid_argument{"fixed-point value must contain digits"};
  }
  for (; fractional_digits < fixed_precision; ++fractional_digits) {
    parts.fraction *= 10U;
  }
  return parts;
}

[[nodiscard]] inline std::uint64_t combine_scaled(DecimalParts parts,
                                                  std::uint64_t limit) {
  const auto scale = static_cast<std::uint64_t>(fixed_scale);
  if (parts.whole > (limit - parts.fraction) / scale) {
    throw std::out_of_range{"fixed-point value is out of range"};
  }
  return parts.whole * scale + parts.fraction;
}

[[nodiscard]] inline std::int64_t apply_sign(std::uint64_t magnitude,
                                             bool negative) noexcept {
  if (!negative) {
    return static_cast<std::int64_t>(magnitude);
  }
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  constexpr auto negative_limit = static_cast<std::uint64_t>(maximum) + 1U;
  if (magnitude == negative_limit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(magnitude);
}

[[nodiscard]] inline std::int64_t parse_fixed(std::string_view text) {
  if (text.empty()) {
    throw std::invalid_argument{"fixed-point value cannot be empty"};
  }

  std::size_t position = 0;
  const bool negative = parse_sign(text, position);
  if (position == text.size()) {
    throw std::invalid_argument{"fixed-point value must contain digits"};
  }

  const auto limit = magnitude_limit(negative);
  const auto parts = parse_decimal_parts(text, position, limit);
  return apply_sign(combine_scaled(parts, limit), negative);
}
} // namespace detail
} // namespace arbreplay
