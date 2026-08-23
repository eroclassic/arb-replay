#include <arbreplay/book_side.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  using arbreplay::BookSide;
  using arbreplay::OrderSide;
  using arbreplay::Price;
  using arbreplay::Quantity;

  BookSide bids{OrderSide::bid};
  BookSide asks{OrderSide::ask};

  CHECK(bids.side() == OrderSide::bid);
  CHECK(asks.side() == OrderSide::ask);
  CHECK(bids.empty());
  CHECK(bids.size() == 0);
  CHECK(!bids.best_level().has_value());

  bids.update(Price::from_cents(40), Quantity::from_contracts(10));
  bids.update(Price::from_cents(65), Quantity::from_contracts(20));
  bids.update(Price::from_cents(55), Quantity::from_contracts(30));

  CHECK(!bids.empty());
  CHECK(bids.size() == 3);

  const auto best_bid = bids.best_level();
  CHECK(best_bid.has_value());
  if (best_bid.has_value()) {
    CHECK(best_bid->price() == Price::from_cents(65));
    CHECK(best_bid->quantity() == Quantity::from_contracts(20));
  }

  bids.update(Price::from_cents(65), Quantity::from_contracts(99));
  CHECK(bids.size() == 3);

  const auto replaced_bid = bids.best_level();
  CHECK(replaced_bid.has_value());
  if (replaced_bid.has_value()) {
    CHECK(replaced_bid->price() == Price::from_cents(65));
    CHECK(replaced_bid->quantity() == Quantity::from_contracts(99));
  }

  bids.update(Price::from_cents(65), Quantity::from_contracts(0));
  CHECK(bids.size() == 2);

  const auto next_bid = bids.best_level();
  CHECK(next_bid.has_value());
  if (next_bid.has_value()) {
    CHECK(next_bid->price() == Price::from_cents(55));
  }

  bids.update(Price::from_cents(80), Quantity::from_contracts(0));
  CHECK(bids.size() == 2);

  asks.update(Price::from_cents(60), Quantity::from_contracts(10));
  asks.update(Price::from_cents(35), Quantity::from_contracts(20));
  asks.update(Price::from_cents(45), Quantity::from_contracts(30));

  const auto best_ask = asks.best_level();
  CHECK(best_ask.has_value());
  if (best_ask.has_value()) {
    CHECK(best_ask->price() == Price::from_cents(35));
    CHECK(best_ask->quantity() == Quantity::from_contracts(20));
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " book-side test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All BookSide tests passed\n";
  return EXIT_SUCCESS;
}
