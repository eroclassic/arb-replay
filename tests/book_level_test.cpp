#include <arbreplay/book_level.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  using arbreplay::BookLevel;
  using arbreplay::Price;
  using arbreplay::Quantity;

  const auto price = Price::from_cents(37);
  const auto quantity = Quantity::from_contracts(250);
  const BookLevel level{price, quantity};

  CHECK(level.price() == price);
  CHECK(level.quantity() == quantity);

  CHECK((BookLevel{Price::from_cents(42), Quantity::from_contracts(10)} ==
         BookLevel{Price::from_cents(42), Quantity::from_contracts(10)}));
  CHECK((BookLevel{Price::from_cents(42), Quantity::from_contracts(10)} !=
         BookLevel{Price::from_cents(43), Quantity::from_contracts(10)}));
  CHECK((BookLevel{Price::from_cents(42), Quantity::from_contracts(10)} !=
         BookLevel{Price::from_cents(42), Quantity::from_contracts(11)}));

  CHECK_THROWS_AS(
      BookLevel(Price::from_cents(50), Quantity::from_contracts(0)),
      std::out_of_range);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " book-level test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All BookLevel tests passed\n";
  return EXIT_SUCCESS;
}
