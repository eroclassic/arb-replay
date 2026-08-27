#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace arbreplay {
class OutcomeId {
public:
  [[nodiscard]] static OutcomeId from_string(std::string value) {
    if (value.empty()) {
      throw std::invalid_argument{"Outcome ID cannot be empty"};
    }

    return OutcomeId{std::move(value)};
  }

  [[nodiscard]] std::string_view value() const noexcept { return value_; }

  [[nodiscard]] bool operator==(const OutcomeId &) const noexcept = default;
  [[nodiscard]] auto operator<=>(const OutcomeId &) const noexcept = default;

private:
  std::string value_;

  explicit OutcomeId(std::string value) noexcept : value_{std::move(value)} {}
};

} // namespace arbreplay
