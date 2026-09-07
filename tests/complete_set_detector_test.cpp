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
#include <string>

namespace {
void add_outcome_with_ask(arbreplay::Market &market, const char *id,
                          int price_cents, std::int64_t contracts) {
  const auto outcome_id =
      arbreplay::OutcomeId::from_string(std::string{id});
  CHECK(market.add_outcome(outcome_id));

  auto *book = market.find_outcome(outcome_id);
  CHECK(book != nullptr);
  if (book != nullptr) {
    book->asks().update(arbreplay::Price::from_cents(price_cents),
                        arbreplay::Quantity::from_contracts(contracts));
  }
}
} // namespace

int main() {
  using arbreplay::Market;
  using arbreplay::Money;
  using arbreplay::OutcomeId;
  using arbreplay::Quantity;
  using arbreplay::detect_complete_set_opportunity;

  {
    Market market;
    add_outcome_with_ask(market, "YES", 42, 20);
    add_outcome_with_ask(market, "NO", 55, 10);

    const auto opportunity =
        detect_complete_set_opportunity(market, Money::from_cents(100));

    CHECK(opportunity.has_value());
    if (opportunity.has_value()) {
      CHECK(opportunity->cost_per_set() == Money::from_cents(97));
      CHECK(opportunity->payout_per_set() == Money::from_cents(100));
      CHECK(opportunity->quantity() == Quantity::from_contracts(10));
      CHECK(opportunity->gross_profit() == Money::from_cents(30));
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
      CHECK(opportunity->cost_per_set() == Money::from_cents(95));
      CHECK(opportunity->quantity() == Quantity::from_contracts(8));
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
      CHECK(opportunity->quantity() == Quantity::from_contracts(7));
      CHECK(opportunity->total_cost() == Money::from_cents(630));
      CHECK(opportunity->total_payout() == Money::from_cents(700));
      CHECK(opportunity->gross_profit() == Money::from_cents(70));
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
