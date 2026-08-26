#pragma once

#include "book_side.hpp"

namespace arbreplay {
class OutcomeBook {
public:
  [[nodiscard]] BookSide &bids() noexcept { return bids_; }

  [[nodiscard]] const BookSide &bids() const noexcept { return bids_; }

  [[nodiscard]] BookSide &asks() noexcept { return asks_; }

  [[nodiscard]] const BookSide &asks() const noexcept { return asks_; }

private:
  BookSide bids_{OrderSide::bid};
  BookSide asks_{OrderSide::ask};
};

} // namespace arbreplay
