#include <arbreplay/complete_set_opportunity.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

int main() {
  using arbreplay::CompleteSetOpportunity;
  using arbreplay::Money;
  using arbreplay::Quantity;

  static_assert(!std::is_default_constructible_v<CompleteSetOpportunity>);

  const auto opportunity = CompleteSetOpportunity{
      Money::from_cents(97), Money::from_cents(100),
      Quantity::from_contracts(10)};

  CHECK(opportunity.cost_per_set() == Money::from_cents(97));
  CHECK(opportunity.payout_per_set() == Money::from_cents(100));
  CHECK(opportunity.quantity() == Quantity::from_contracts(10));
  CHECK(opportunity.profit_per_set() == Money::from_cents(3));
  CHECK(opportunity.total_cost() == Money::from_cents(970));
  CHECK(opportunity.total_payout() == Money::from_cents(1'000));
  CHECK(opportunity.gross_profit() == Money::from_cents(30));

  CHECK(opportunity == CompleteSetOpportunity{
                           Money::from_cents(97), Money::from_cents(100),
                           Quantity::from_contracts(10)});

  CHECK_THROWS_AS(
      CompleteSetOpportunity(Money::from_cents(-1), Money::from_cents(100),
                             Quantity::from_contracts(1)),
      std::invalid_argument);
  CHECK_THROWS_AS(
      CompleteSetOpportunity(Money::from_cents(100), Money::from_cents(100),
                             Quantity::from_contracts(1)),
      std::invalid_argument);
  CHECK_THROWS_AS(
      CompleteSetOpportunity(Money::from_cents(101), Money::from_cents(100),
                             Quantity::from_contracts(1)),
      std::invalid_argument);
  CHECK_THROWS_AS(
      CompleteSetOpportunity(Money::from_cents(97), Money::from_cents(100),
                             Quantity::from_contracts(0)),
      std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " complete set opportunity test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All CompleteSetOpportunity tests passed\n";
  return EXIT_SUCCESS;
}
