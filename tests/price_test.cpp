#include <arbreplay/price.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  using arbreplay::Price;

  const auto zero = Price::from_cents(0);
  const auto full_payout = Price::from_cents(100);

  CHECK(zero.raw() == 0);
  CHECK(full_payout.raw() == Price::scale);
  CHECK_THROWS_AS(Price::from_cents(-1), std::out_of_range);
  CHECK_THROWS_AS(Price::from_cents(101), std::out_of_range);

  CHECK(Price::from_decimal("0.455").raw() == 455'000);
  CHECK(Price::from_decimal(".0001").raw() == 100);
  CHECK(Price::from_decimal("1.000000") == full_payout);
  CHECK_THROWS_AS(Price::from_decimal("1.000001"), std::out_of_range);
  CHECK_THROWS_AS(Price::from_decimal("0.1234567"), std::invalid_argument);
  CHECK_THROWS_AS(Price::from_decimal("not-a-price"), std::invalid_argument);

  CHECK(zero.complement() == full_payout);
  CHECK(full_payout.complement() == zero);
  CHECK(Price::from_cents(37).complement() == Price::from_cents(63));
  CHECK(Price::from_decimal("0.455").complement() ==
        Price::from_decimal("0.545"));
  CHECK(Price::from_cents(37).complement().complement() ==
        Price::from_cents(37));

  CHECK(Price::from_cents(42) == Price::from_cents(42));
  CHECK(Price::from_cents(42) != Price::from_cents(43));

  CHECK(zero < full_payout);
  CHECK(zero <= full_payout);
  CHECK(full_payout > zero);
  CHECK(full_payout >= zero);
  CHECK(Price::from_cents(42) <= Price::from_cents(42));
  CHECK(Price::from_cents(42) >= Price::from_cents(42));

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " price test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All Price tests passed\n";
  return EXIT_SUCCESS;
}
