#include <arbreplay/complete_set_opportunity.hpp>
#include <arbreplay/complete_set_opportunity_level.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

int main() {
  using arbreplay::CompleteSetOpportunity;
  using arbreplay::CompleteSetOpportunityLevel;
  using arbreplay::Money;
  using arbreplay::Quantity;

  static_assert(!std::is_default_constructible_v<CompleteSetOpportunity>);

  const std::vector levels{
      CompleteSetOpportunityLevel{Money::from_cents(97),
                                  Quantity::from_contracts(5)},
      CompleteSetOpportunityLevel{Money::from_cents(99),
                                  Quantity::from_contracts(15)}};
  const auto opportunity =
      CompleteSetOpportunity{Money::from_cents(100), levels};

  CHECK(opportunity.payout_per_set() == Money::from_cents(100));
  CHECK(opportunity.levels() == levels);
  CHECK(opportunity.total_quantity() == Quantity::from_contracts(20));
  CHECK(opportunity.total_cost() == Money::from_cents(1'970));
  CHECK(opportunity.total_payout() == Money::from_cents(2'000));
  CHECK(opportunity.gross_profit() == Money::from_cents(30));
  CHECK((opportunity ==
         CompleteSetOpportunity{Money::from_cents(100), levels}));

  CHECK_THROWS_AS(CompleteSetOpportunity(Money::from_cents(0), levels),
                  std::invalid_argument);
  CHECK_THROWS_AS(CompleteSetOpportunity(Money::from_cents(-1), levels),
                  std::invalid_argument);
  CHECK_THROWS_AS(CompleteSetOpportunity(Money::from_cents(100), {}),
                  std::invalid_argument);

  const std::vector break_even{
      CompleteSetOpportunityLevel{Money::from_cents(100),
                                  Quantity::from_contracts(1)}};
  CHECK_THROWS_AS(
      CompleteSetOpportunity(Money::from_cents(100), break_even),
      std::invalid_argument);

  const std::vector loss{
      CompleteSetOpportunityLevel{Money::from_cents(101),
                                  Quantity::from_contracts(1)}};
  CHECK_THROWS_AS(CompleteSetOpportunity(Money::from_cents(100), loss),
                  std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " complete set opportunity test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All CompleteSetOpportunity tests passed\n";
  return EXIT_SUCCESS;
}
