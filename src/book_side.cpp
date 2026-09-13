#include <arbreplay/book_side.hpp>

namespace arbreplay {
void BookSide::update(Price price, Quantity quantity) {
  if (quantity.raw() == 0) {
    levels_.erase(price);
    return;
  }

  levels_.insert_or_assign(price, quantity);
}
} // namespace arbreplay
