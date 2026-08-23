#include <arbreplay/outcome_book.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  using arbreplay::OrderSide;
  using arbreplay::OutcomeBook;
  using arbreplay::Price;
  using arbreplay::Quantity;

  OutcomeBook book;

  CHECK(book.bids().side() == OrderSide::bid);
  CHECK(book.asks().side() == OrderSide::ask);
  CHECK(book.bids().empty());
  CHECK(book.asks().empty());

  book.bids().update(Price::from_cents(60),
                     Quantity::from_contracts(15));
  book.asks().update(Price::from_cents(65),
                     Quantity::from_contracts(20));

  CHECK(book.bids().size() == 1);
  CHECK(book.asks().size() == 1);

  const auto best_bid = book.bids().best_level();
  CHECK(best_bid.has_value());
  if (best_bid.has_value()) {
    CHECK(best_bid->price() == Price::from_cents(60));
    CHECK(best_bid->quantity() == Quantity::from_contracts(15));
  }

  const auto best_ask = book.asks().best_level();
  CHECK(best_ask.has_value());
  if (best_ask.has_value()) {
    CHECK(best_ask->price() == Price::from_cents(65));
    CHECK(best_ask->quantity() == Quantity::from_contracts(20));
  }

  const OutcomeBook& snapshot = book;
  CHECK(snapshot.bids().side() == OrderSide::bid);
  CHECK(snapshot.asks().side() == OrderSide::ask);
  CHECK(snapshot.bids().size() == 1);
  CHECK(snapshot.asks().size() == 1);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " outcome-book test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All OutcomeBook tests passed\n";
  return EXIT_SUCCESS;
}
