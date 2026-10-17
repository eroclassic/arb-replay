#include <arbreplay/fee_rate.hpp>
#include <arbreplay/money.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  using arbreplay::FeeRate;
  using arbreplay::Money;

  CHECK(FeeRate{}.basis_points() == 0);
  CHECK(FeeRate{}.apply(Money::from_decimal("0.97")) == Money::from_raw(0));

  const auto two_percent = FeeRate::from_basis_points(200);
  CHECK(two_percent.basis_points() == 200);
  CHECK(two_percent.apply(Money::from_decimal("0.97")) ==
        Money::from_decimal("0.0194"));

  const auto ten_thousandth_percent = FeeRate::from_basis_points(1);
  CHECK(ten_thousandth_percent.apply(Money::from_raw(1)) == Money::from_raw(1));

  const auto maximum = FeeRate::from_basis_points(10'000);
  CHECK(maximum.apply(Money::from_decimal("0.97")) ==
        Money::from_decimal("0.97"));

  CHECK_THROWS_AS(FeeRate::from_basis_points(10'001), std::invalid_argument);
  CHECK_THROWS_AS(two_percent.apply(Money::from_raw(-1)),
                  std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " fee rate test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All FeeRate tests passed\n";
  return EXIT_SUCCESS;
}
