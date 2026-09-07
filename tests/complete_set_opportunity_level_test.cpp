#include <arbreplay/complete_set_opportunity_level.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

int main() {
  using arbreplay::CompleteSetOpportunityLevel;
  using arbreplay::Money;
  using arbreplay::Quantity;

  static_assert(!std::is_default_constructible_v<CompleteSetOpportunityLevel>);

  const auto level = CompleteSetOpportunityLevel{
      Money::from_cents(97), Quantity::from_contracts(5)};

  CHECK(level.cost_per_set() == Money::from_cents(97));
  CHECK(level.quantity() == Quantity::from_contracts(5));
  CHECK(level.total_cost() == Money::from_cents(485));
  CHECK((level == CompleteSetOpportunityLevel{
                      Money::from_cents(97), Quantity::from_contracts(5)}));

  CHECK_THROWS_AS(
      CompleteSetOpportunityLevel(Money::from_cents(-1),
                                  Quantity::from_contracts(1)),
      std::invalid_argument);
  CHECK_THROWS_AS(
      CompleteSetOpportunityLevel(Money::from_cents(97),
                                  Quantity::from_contracts(0)),
      std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " complete set opportunity level test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All CompleteSetOpportunityLevel tests passed\n";
  return EXIT_SUCCESS;
}
