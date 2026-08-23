#pragma once

#include "book_level.hpp"
#include "price.hpp"
#include "quantity.hpp"
#include <cstddef>
#include <map>
#include <optional>

namespace arbreplay {
enum class OrderSide { bid, ask };

class BookSide {
public:
  explicit BookSide(OrderSide side) noexcept : side_(side) {}

  [[nodiscard]] OrderSide side() const noexcept { return side_; }
  [[nodiscard]] bool empty() const noexcept { return levels_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return levels_.size(); }
  [[nodiscard]] std::optional<BookLevel> best_level() const {
    if (empty()) {
      return std::nullopt;
    }

    if (side_ == OrderSide::bid) {
      const auto iterator = levels_.rbegin();
      return BookLevel{iterator->first, iterator->second};
    }

    const auto iterator = levels_.begin();
    return BookLevel{iterator->first, iterator->second};
  }

  void update(Price price, Quantity quantity);

private:
  OrderSide side_;
  std::map<Price, Quantity> levels_;
};
} // namespace arbreplay
