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

  CHECK(loss.raw() == -300'000);
  CHECK(zero.raw() == 0);
  CHECK(profit.raw() == 480'000);

  CHECK(Money::from_decimal("0.455").raw() == 455'000);
  CHECK(Money::from_decimal("-12.5").raw() == -12'500'000);
  CHECK(Money::from_decimal("9223372036854.775807").raw() ==
        std::numeric_limits<std::int64_t>::max());
  CHECK(Money::from_decimal("-9223372036854.775808").raw() ==
        std::numeric_limits<std::int64_t>::min());
  CHECK_THROWS_AS(Money::from_decimal("9223372036854.775808"),
                  std::out_of_range);
  CHECK_THROWS_AS(Money::from_decimal("-9223372036854.775809"),
                  std::out_of_range);
  CHECK_THROWS_AS(Money::from_decimal("."), std::invalid_argument);
  CHECK_THROWS_AS(Money::from_decimal("1.0000001"), std::invalid_argument);

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
      Money::from_raw(std::numeric_limits<std::int64_t>::max());
  const auto minimum =
      Money::from_raw(std::numeric_limits<std::int64_t>::min());
  const auto one = Money::from_raw(1);
  const auto negative_one = Money::from_raw(-1);

  CHECK_THROWS_AS(maximum + one, std::overflow_error);
  CHECK_THROWS_AS(minimum + negative_one, std::overflow_error);
  CHECK_THROWS_AS(maximum - negative_one, std::overflow_error);
  CHECK_THROWS_AS(minimum - one, std::overflow_error);

  CHECK(Money::from_cents(8) * Quantity::from_contracts(6) ==
        Money::from_cents(48));
  CHECK(Money::from_cents(-8) * Quantity::from_contracts(6) ==
        Money::from_cents(-48));
  CHECK(Money::from_cents(0) * Quantity::from_contracts(6) == zero);
  CHECK(Money::from_cents(8) * Quantity::from_contracts(0) == zero);
  CHECK(maximum * Quantity::from_contracts(1) == maximum);
  CHECK(minimum * Quantity::from_contracts(1) == minimum);

  const auto fractional_price = Money::from_decimal("0.333333");
  const auto fractional_quantity = Quantity::from_decimal("1.5");
  CHECK(fractional_price.multiply(fractional_quantity,
                                  arbreplay::RoundingMode::toward_zero) ==
        Money::from_raw(499'999));
  CHECK(fractional_price.multiply(fractional_quantity,
                                  arbreplay::RoundingMode::up) ==
        Money::from_raw(500'000));
  CHECK(fractional_price.multiply(fractional_quantity,
                                  arbreplay::RoundingMode::down) ==
        Money::from_raw(499'999));

  const auto negative_fractional_price = Money::from_decimal("-0.333333");
  CHECK(negative_fractional_price.multiply(
            fractional_quantity, arbreplay::RoundingMode::toward_zero) ==
        Money::from_raw(-499'999));
  CHECK(negative_fractional_price.multiply(fractional_quantity,
                                           arbreplay::RoundingMode::up) ==
        Money::from_raw(-499'999));
  CHECK(negative_fractional_price.multiply(fractional_quantity,
                                           arbreplay::RoundingMode::down) ==
        Money::from_raw(-500'000));

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
