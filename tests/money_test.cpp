#include <arbreplay/money.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

int main() {
  using arbreplay::Money;
  using arbreplay::Quantity;

  static_assert(!std::is_default_constructible_v<Money>);

  const auto loss = Money::from_cents(-30);
  const auto zero = Money::from_cents(0);
  const auto profit = Money::from_cents(48);
  const auto cost = Money::from_cents(552);
  const auto payout = Money::from_cents(600);

  CHECK(loss.cents() == -30);
  CHECK(zero.cents() == 0);
  CHECK(profit.cents() == 48);

  CHECK(Money::from_cents(48) == Money::from_cents(48));
  CHECK(Money::from_cents(48) != Money::from_cents(49));
  CHECK(loss < zero);
  CHECK(zero < profit);
  CHECK(profit <= Money::from_cents(48));
  CHECK(profit >= Money::from_cents(48));

  CHECK(cost + profit == payout);
  CHECK(payout - cost == profit);
  CHECK(cost - payout == Money::from_cents(-48));
  CHECK(loss + Money::from_cents(30) == zero);

  const auto maximum =
      Money::from_cents(std::numeric_limits<std::int64_t>::max());
  const auto minimum =
      Money::from_cents(std::numeric_limits<std::int64_t>::min());
  const auto one = Money::from_cents(1);
  const auto negative_one = Money::from_cents(-1);

  CHECK_THROWS_AS(maximum + one, std::overflow_error);
  CHECK_THROWS_AS(minimum + negative_one, std::overflow_error);
  CHECK_THROWS_AS(maximum - negative_one, std::overflow_error);
  CHECK_THROWS_AS(minimum - one, std::overflow_error);

  CHECK(Money::from_cents(8) * Quantity::from_contracts(6) ==
        Money::from_cents(48));
  CHECK(Money::from_cents(-8) * Quantity::from_contracts(6) ==
        Money::from_cents(-48));
  CHECK(Money::from_cents(0) * Quantity::from_contracts(6) ==
        zero);
  CHECK(Money::from_cents(8) * Quantity::from_contracts(0) ==
        zero);
  CHECK(maximum * Quantity::from_contracts(1) == maximum);
  CHECK(minimum * Quantity::from_contracts(1) == minimum);

  const auto two = Quantity::from_contracts(2);
  CHECK_THROWS_AS(maximum * two, std::overflow_error);
  CHECK_THROWS_AS(minimum * two, std::overflow_error);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " money test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All Money tests passed\n";
  return EXIT_SUCCESS;
}
