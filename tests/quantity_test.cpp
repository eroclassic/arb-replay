#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
  using arbreplay::Quantity;

  const auto zero = Quantity::from_contracts(0);
  const auto one = Quantity::from_contracts(1);
  const auto large = Quantity::from_contracts(1'000'000);
  const auto maximum =
      Quantity::from_contracts(std::numeric_limits<std::int64_t>::max());

  CHECK(zero.contracts() == 0);
  CHECK(one.contracts() == 1);
  CHECK(large.contracts() == 1'000'000);
  CHECK(maximum.contracts() == std::numeric_limits<std::int64_t>::max());

  CHECK_THROWS_AS(Quantity::from_contracts(-1), std::out_of_range);
  CHECK_THROWS_AS(Quantity::from_contracts(
                      std::numeric_limits<std::int64_t>::min()),
                  std::out_of_range);

  CHECK(Quantity::from_contracts(42) == Quantity::from_contracts(42));
  CHECK(Quantity::from_contracts(42) != Quantity::from_contracts(43));

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " quantity test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All Quantity tests passed\n";
  return EXIT_SUCCESS;
}

