#include <arbreplay/market.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  using arbreplay::Market;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;

  Market market;
  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto no = OutcomeId::from_string(std::string{"NO"});
  const auto missing = OutcomeId::from_string(std::string{"MISSING"});

  CHECK(market.empty());
  CHECK(market.size() == 0);
  CHECK(market.find_outcome(yes) == nullptr);

  CHECK(market.add_outcome(yes));
  CHECK(!market.empty());
  CHECK(market.size() == 1);
  CHECK(market.find_outcome(yes) != nullptr);

  CHECK(!market.add_outcome(yes));
  CHECK(market.size() == 1);

  CHECK(market.add_outcome(no));
  CHECK(market.size() == 2);
  CHECK(market.find_outcome(no) != nullptr);
  CHECK(market.find_outcome(missing) == nullptr);

  auto* yes_book = market.find_outcome(yes);
  CHECK(yes_book != nullptr);
  if (yes_book != nullptr) {
    yes_book->bids().update(Price::from_cents(60),
                            Quantity::from_contracts(15));
  }

  auto* no_book = market.find_outcome(no);
  CHECK(no_book != nullptr);
  if (no_book != nullptr) {
    CHECK(no_book->bids().empty());
    no_book->asks().update(Price::from_cents(45),
                           Quantity::from_contracts(20));
  }

  CHECK(!market.add_outcome(yes));
  const auto* preserved_yes_book = market.find_outcome(yes);
  CHECK(preserved_yes_book != nullptr);
  if (preserved_yes_book != nullptr) {
    CHECK(preserved_yes_book->bids().size() == 1);
  }

  const Market& snapshot = market;
  const auto* snapshot_yes = snapshot.find_outcome(yes);
  const auto* snapshot_no = snapshot.find_outcome(no);

  CHECK(snapshot_yes != nullptr);
  CHECK(snapshot_no != nullptr);
  CHECK(snapshot.find_outcome(missing) == nullptr);
  if (snapshot_yes != nullptr) {
    CHECK(snapshot_yes->bids().best_level().has_value());
  }
  if (snapshot_no != nullptr) {
    CHECK(snapshot_no->asks().best_level().has_value());
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " market test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All Market tests passed\n";
  return EXIT_SUCCESS;
}
