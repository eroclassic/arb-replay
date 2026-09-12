#include <arbreplay/market.hpp>
#include <arbreplay/market_event.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/price.hpp>
#include <arbreplay/quantity.hpp>
#include <arbreplay/replay_engine.hpp>

#include "test_support.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

int main() {
  using arbreplay::Market;
  using arbreplay::MarketEvent;
  using arbreplay::Money;
  using arbreplay::OrderSide;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;
  using arbreplay::ReplayEngine;

  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto no = OutcomeId::from_string(std::string{"NO"});

  Market market;
  CHECK(market.add_outcome(yes));
  CHECK(market.add_outcome(no));

  ReplayEngine engine{std::move(market), Money::from_cents(100)};

  const auto yes_ask = MarketEvent{
      MarketEvent::Timestamp{1'000}, std::uint64_t{1}, yes, OrderSide::ask,
      Price::from_cents(45), Quantity::from_contracts(10)};

  CHECK(!engine.apply(yes_ask).has_value());

  const auto *yes_book = engine.market().find_outcome(yes);
  CHECK(yes_book != nullptr);
  if (yes_book != nullptr) {
    const auto best_ask = yes_book->asks().best_level();
    CHECK(best_ask.has_value());
    if (best_ask.has_value()) {
      CHECK(best_ask->price() == Price::from_cents(45));
      CHECK(best_ask->quantity() == Quantity::from_contracts(10));
    }
    CHECK(yes_book->bids().empty());
  }

  const auto no_bid = MarketEvent{
      MarketEvent::Timestamp{1'001}, std::uint64_t{2}, no, OrderSide::bid,
      Price::from_cents(52), Quantity::from_contracts(7)};

  CHECK(!engine.apply(no_bid).has_value());

  const auto *no_book = engine.market().find_outcome(no);
  CHECK(no_book != nullptr);
  if (no_book != nullptr) {
    CHECK(no_book->bids().best_level().has_value());
    CHECK(no_book->asks().empty());
  }

  const auto no_ask = MarketEvent{
      MarketEvent::Timestamp{1'002}, std::uint64_t{3}, no, OrderSide::ask,
      Price::from_cents(50), Quantity::from_contracts(8)};

  const auto opportunity = engine.apply(no_ask);
  CHECK(opportunity.has_value());
  if (opportunity.has_value()) {
    CHECK(opportunity->payout_per_set() == Money::from_cents(100));
    CHECK(opportunity->total_quantity() == Quantity::from_contracts(8));
    CHECK(opportunity->gross_profit() == Money::from_cents(40));
  }

  const auto remove_yes_ask = MarketEvent{
      MarketEvent::Timestamp{1'003}, std::uint64_t{4}, yes, OrderSide::ask,
      Price::from_cents(45), Quantity::from_contracts(0)};

  CHECK(!engine.apply(remove_yes_ask).has_value());
  yes_book = engine.market().find_outcome(yes);
  CHECK(yes_book != nullptr);
  if (yes_book != nullptr) {
    CHECK(yes_book->asks().empty());
  }

  const auto unknown = OutcomeId::from_string(std::string{"UNKNOWN"});
  const auto unknown_event = MarketEvent{
      MarketEvent::Timestamp{1'004}, std::uint64_t{5}, unknown, OrderSide::ask,
      Price::from_cents(10), Quantity::from_contracts(1)};

  CHECK_THROWS_AS(engine.apply(unknown_event), std::invalid_argument);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " replay engine test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All ReplayEngine tests passed\n";
  return EXIT_SUCCESS;
}
