#include <arbreplay/outcome_id.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

int main() {
  using arbreplay::OutcomeId;

  static_assert(!std::is_default_constructible_v<OutcomeId>);

  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto no = OutcomeId::from_string(std::string{"NO"});

  CHECK(yes.value() == std::string_view{"YES"});
  CHECK(no.value() == std::string_view{"NO"});

  CHECK_THROWS_AS(OutcomeId::from_string(std::string{}),
                  std::invalid_argument);

  CHECK(OutcomeId::from_string(std::string{"YES"}) ==
        OutcomeId::from_string(std::string{"YES"}));
  CHECK(OutcomeId::from_string(std::string{"YES"}) !=
        OutcomeId::from_string(std::string{"NO"}));
  CHECK(OutcomeId::from_string(std::string{"CANDIDATE_A"}) <
        OutcomeId::from_string(std::string{"CANDIDATE_B"}));

  CHECK(OutcomeId::from_string(std::string{"yes"}) !=
        OutcomeId::from_string(std::string{"YES"}));

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " outcome-id test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All OutcomeId tests passed\n";
  return EXIT_SUCCESS;
}
