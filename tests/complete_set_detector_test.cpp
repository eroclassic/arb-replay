#include <arbreplay/complete_set_detector.hpp>
#include <arbreplay/market.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>

#include "test_support.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void add_outcome_with_ask(arbreplay::Market &market, const char *id,
                          int price_cents, std::int64_t contracts) {
  const auto outcome_id = arbreplay::OutcomeId::from_string(std::string{id});
  CHECK(market.add_outcome(outcome_id));

  auto *book = market.find_outcome(outcome_id);
  CHECK(book != nullptr);
  if (book != nullptr) {
    book->asks().update(arbreplay::Price::from_cents(price_cents),
                        arbreplay::Quantity::from_contracts(contracts));
  }
}

void add_outcome_with_raw_ask(arbreplay::Market &market, const char *id,
                              int price_cents, std::int64_t quantity_raw) {
  const auto outcome_id = arbreplay::OutcomeId::from_string(std::string{id});
  CHECK(market.add_outcome(outcome_id));

  auto *book = market.find_outcome(outcome_id);
  CHECK(book != nullptr);
  if (book != nullptr) {
    book->asks().update(arbreplay::Price::from_cents(price_cents),
                        arbreplay::Quantity::from_raw(quantity_raw));
  }
}
} // namespace

int main() {
  using arbreplay::detect_complete_set_opportunity;
  using arbreplay::Market;
  using arbreplay::Money;
  using arbreplay::OutcomeId;
  using arbreplay::Quantity;

  {
    const Market empty_market;
    CHECK(!detect_complete_set_opportunity(empty_market, Money::from_cents(100))
               .has_value());
  }

  {
    Market market;
    add_outcome_with_ask(market, "ONLY", 50, 10);
    CHECK(!detect_complete_set_opportunity(market, Money::from_cents(100))
               .has_value());
  }

  {
    Market market;
    add_outcome_with_ask(market, "YES", 42, 10);
    add_outcome_with_ask(market, "NO", 55, 10);

    CHECK_THROWS_AS(
        detect_complete_set_opportunity(market, Money::from_cents(0)),
        std::invalid_argument);
    CHECK_THROWS_AS(
        detect_complete_set_opportunity(market, Money::from_cents(-1)),
        std::invalid_argument);
  }

  {
    Market market;
    add_outcome_with_ask(market, "YES", 42, 20);
    add_outcome_with_ask(market, "NO", 55, 10);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->payout_per_set() == Money::from_cents(100));
      CHECK(opportunity->levels().size() == 1);
      CHECK(opportunity->levels().front().cost_per_set() ==
            Money::from_cents(97));
      CHECK(opportunity->total_quantity() == Quantity::from_contracts(10));
      CHECK(opportunity->gross_profit() == Money::from_cents(30));
    }
  }

  {
    Market market;
    const auto yes = OutcomeId::from_string(std::string{"FRACTIONAL_YES"});
    const auto no = OutcomeId::from_string(std::string{"FRACTIONAL_NO"});
    CHECK(market.add_outcome(yes));
    CHECK(market.add_outcome(no));

    auto *yes_book = market.find_outcome(yes);
    auto *no_book = market.find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);
    if (yes_book != nullptr) {
      yes_book->asks().update(arbreplay::Price::from_decimal("0.455"),
                              Quantity::from_decimal("12.5"));
    }
    if (no_book != nullptr) {
      no_book->asks().update(arbreplay::Price::from_decimal("0.54"),
                             Quantity::from_decimal("10.25"));
    }

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_decimal("1"));
    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->total_quantity() == Quantity::from_decimal("10.25"));
      CHECK(opportunity->total_cost() == Money::from_decimal("10.19875"));
      CHECK(opportunity->total_payout() == Money::from_decimal("10.25"));
      CHECK(opportunity->gross_profit() == Money::from_decimal("0.05125"));
    }
  }

  {
    Market market;
    const auto yes = OutcomeId::from_string(std::string{"BOUNDARY_YES"});
    const auto no = OutcomeId::from_string(std::string{"BOUNDARY_NO"});
    CHECK(market.add_outcome(yes));
    CHECK(market.add_outcome(no));

    auto *yes_book = market.find_outcome(yes);
    auto *no_book = market.find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);

    if (yes_book != nullptr) {
      yes_book->asks().update(arbreplay::Price::from_cents(42),
                              Quantity::from_contracts(5));
      yes_book->asks().update(arbreplay::Price::from_cents(45),
                              Quantity::from_contracts(10));
    }
    if (no_book != nullptr) {
      no_book->asks().update(arbreplay::Price::from_cents(55),
                             Quantity::from_contracts(15));
    }

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->levels().size() == 1);
      CHECK(opportunity->levels().front().cost_per_set() ==
            Money::from_cents(97));
      CHECK(opportunity->levels().front().quantity() ==
            Quantity::from_contracts(5));
    }
  }

  {
    Market market;
    add_outcome_with_ask(market, "CUSTOM_A", 50, 4);
    add_outcome_with_ask(market, "CUSTOM_B", 49, 4);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(125));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->payout_per_set() == Money::from_cents(125));
      CHECK(opportunity->gross_profit() == Money::from_cents(104));
    }
  }

  {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    Market market;
    add_outcome_with_raw_ask(market, "HUGE_YES", 1, maximum);
    add_outcome_with_raw_ask(market, "HUGE_NO", 1, maximum);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      constexpr auto expected_cost_raw =
          maximum / 50 + (maximum % 50 == 0 ? 0 : 1);
      CHECK(opportunity->total_cost() == Money::from_raw(expected_cost_raw));
      CHECK(opportunity->total_payout() == Money::from_raw(maximum));
      CHECK(opportunity->gross_profit() ==
            Money::from_raw(maximum - expected_cost_raw));
    }
  }

  {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    Market market;
    const auto yes = OutcomeId::from_string(std::string{"TOTAL_YES"});
    const auto no = OutcomeId::from_string(std::string{"TOTAL_NO"});
    CHECK(market.add_outcome(yes));
    CHECK(market.add_outcome(no));

    auto *yes_book = market.find_outcome(yes);
    auto *no_book = market.find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);

    if (yes_book != nullptr) {
      yes_book->asks().update(arbreplay::Price::from_cents(1),
                              Quantity::from_raw(maximum));
      yes_book->asks().update(arbreplay::Price::from_cents(2),
                              Quantity::from_raw(maximum));
    }
    if (no_book != nullptr) {
      no_book->asks().update(arbreplay::Price::from_cents(1),
                             Quantity::from_raw(maximum));
      no_book->asks().update(arbreplay::Price::from_cents(2),
                             Quantity::from_raw(maximum));
    }

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->levels().size() == 2);
      CHECK_THROWS_AS(opportunity->total_quantity(), std::overflow_error);
    }
  }

  {
    Market market;
    add_outcome_with_ask(market, "ALICE", 25, 12);
    add_outcome_with_ask(market, "BOB", 30, 8);
    add_outcome_with_ask(market, "CAROL", 40, 15);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->levels().size() == 1);
      CHECK(opportunity->levels().front().cost_per_set() ==
            Money::from_cents(95));
      CHECK(opportunity->total_quantity() == Quantity::from_contracts(8));
      CHECK(opportunity->gross_profit() == Money::from_cents(40));
    }
  }

  {
    Market market;
    add_outcome_with_ask(market, "YES", 45, 10);
    add_outcome_with_ask(market, "NO", 55, 10);

    CHECK(!detect_complete_set_opportunity(market, Money::from_cents(100))
               .has_value());
  }

  {
    Market market;
    add_outcome_with_ask(market, "YES", 60, 10);
    add_outcome_with_ask(market, "NO", 45, 10);

    CHECK(!detect_complete_set_opportunity(market, Money::from_cents(100))
               .has_value());
  }

  {
    Market market;
    const auto yes = OutcomeId::from_string(std::string{"YES"});
    const auto no = OutcomeId::from_string(std::string{"NO"});
    CHECK(market.add_outcome(yes));
    CHECK(market.add_outcome(no));

    auto *yes_book = market.find_outcome(yes);
    CHECK(yes_book != nullptr);
    if (yes_book != nullptr) {
      yes_book->asks().update(arbreplay::Price::from_cents(42),
                              Quantity::from_contracts(10));
    }

    CHECK(!detect_complete_set_opportunity(market, Money::from_cents(100))
               .has_value());
  }

  {
    Market market;
    add_outcome_with_ask(market, "A", 20, 50);
    add_outcome_with_ask(market, "B", 30, 7);
    add_outcome_with_ask(market, "C", 40, 25);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->total_quantity() == Quantity::from_contracts(7));
      CHECK(opportunity->total_cost() == Money::from_cents(630));
      CHECK(opportunity->total_payout() == Money::from_cents(700));
      CHECK(opportunity->gross_profit() == Money::from_cents(70));
    }
  }

  {
    Market market;
    const auto yes = OutcomeId::from_string(std::string{"DEPTH_YES"});
    const auto no = OutcomeId::from_string(std::string{"DEPTH_NO"});
    CHECK(market.add_outcome(yes));
    CHECK(market.add_outcome(no));

    auto *yes_book = market.find_outcome(yes);
    auto *no_book = market.find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);

    if (yes_book != nullptr) {
      yes_book->asks().update(arbreplay::Price::from_cents(42),
                              Quantity::from_contracts(5));
      yes_book->asks().update(arbreplay::Price::from_cents(44),
                              Quantity::from_contracts(20));
    }
    if (no_book != nullptr) {
      no_book->asks().update(arbreplay::Price::from_cents(55),
                             Quantity::from_contracts(20));
    }

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->levels().size() == 2);
      CHECK(opportunity->levels()[0].cost_per_set() == Money::from_cents(97));
      CHECK(opportunity->levels()[0].quantity() == Quantity::from_contracts(5));
      CHECK(opportunity->levels()[1].cost_per_set() == Money::from_cents(99));
      CHECK(opportunity->levels()[1].quantity() ==
            Quantity::from_contracts(15));
      CHECK(opportunity->total_quantity() == Quantity::from_contracts(20));
      CHECK(opportunity->gross_profit() == Money::from_cents(30));
    }
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " complete set detector test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All complete-set detector tests passed\n";
  return EXIT_SUCCESS;
}
