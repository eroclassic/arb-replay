#include <arbreplay/money.hpp>
#include <arbreplay/settlement_terms.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

int main() {
  using arbreplay::Money;
  using arbreplay::SettlementTerms;

  static_assert(!std::is_default_constructible_v<SettlementTerms>);

  const auto one_dollar =
      SettlementTerms::from_payout(Money::from_cents(100));
  const auto ten_dollars =
      SettlementTerms::from_payout(Money::from_cents(1'000));

  CHECK(one_dollar.payout() == Money::from_cents(100));
  CHECK(ten_dollars.payout() == Money::from_cents(1'000));

  CHECK(one_dollar ==
        SettlementTerms::from_payout(Money::from_cents(100)));
  CHECK(one_dollar != ten_dollars);

  CHECK_THROWS_AS(SettlementTerms::from_payout(Money::from_cents(0)),
                  std::invalid_argument);
  CHECK_THROWS_AS(SettlementTerms::from_payout(Money::from_cents(-1)),
                  std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " settlement terms test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All SettlementTerms tests passed\n";
  return EXIT_SUCCESS;
}
