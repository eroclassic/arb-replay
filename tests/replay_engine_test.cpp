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
#include <vector>

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
    CHECK(opportunity->replay_key() == no_ask.replay_key());
    CHECK(opportunity->opportunity().payout_per_set() ==
          Money::from_cents(100));
    CHECK(opportunity->opportunity().total_quantity() ==
          Quantity::from_contracts(8));
    CHECK(opportunity->opportunity().gross_profit() == Money::from_cents(40));
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

  {
    Market replay_market;
    CHECK(replay_market.add_outcome(yes));
    CHECK(replay_market.add_outcome(no));
    ReplayEngine replay_engine{std::move(replay_market),
                               Money::from_cents(100)};

    const std::vector<MarketEvent> no_events{};
    CHECK(replay_engine.replay(no_events).empty());
  }

  {
    Market replay_market;
    CHECK(replay_market.add_outcome(yes));
    CHECK(replay_market.add_outcome(no));
    ReplayEngine replay_engine{std::move(replay_market),
                               Money::from_cents(100)};

    const auto first_yes_ask = MarketEvent{
        MarketEvent::Timestamp{2'000}, std::uint64_t{10}, yes,
        OrderSide::ask, Price::from_cents(45), Quantity::from_contracts(10)};
    const auto first_no_ask = MarketEvent{
        MarketEvent::Timestamp{2'001}, std::uint64_t{11}, no, OrderSide::ask,
        Price::from_cents(50), Quantity::from_contracts(8)};
    const auto irrelevant_yes_bid = MarketEvent{
        MarketEvent::Timestamp{2'002}, std::uint64_t{12}, yes,
        OrderSide::bid, Price::from_cents(35), Quantity::from_contracts(3)};
    const auto remove_first_yes_ask = MarketEvent{
        MarketEvent::Timestamp{2'003}, std::uint64_t{13}, yes,
        OrderSide::ask, Price::from_cents(45), Quantity::from_contracts(0)};
    const auto second_yes_ask = MarketEvent{
        MarketEvent::Timestamp{2'004}, std::uint64_t{14}, yes,
        OrderSide::ask, Price::from_cents(40), Quantity::from_contracts(4)};

    const std::vector events{first_yes_ask,         first_no_ask,
                             irrelevant_yes_bid,   remove_first_yes_ask,
                             second_yes_ask};
    const auto detections = replay_engine.replay(events);

    CHECK(detections.size() == 2);
    if (detections.size() == 2) {
      CHECK(detections[0].replay_key() == first_no_ask.replay_key());
      CHECK(detections[0].opportunity().gross_profit() ==
            Money::from_cents(40));
      CHECK(detections[1].replay_key() == second_yes_ask.replay_key());
      CHECK(detections[1].opportunity().gross_profit() ==
            Money::from_cents(40));
    }

    const auto *final_yes_book = replay_engine.market().find_outcome(yes);
    const auto *final_no_book = replay_engine.market().find_outcome(no);
    CHECK(final_yes_book != nullptr);
    CHECK(final_no_book != nullptr);
    if (final_yes_book != nullptr) {
      const auto best_ask = final_yes_book->asks().best_level();
      CHECK(best_ask.has_value());
      if (best_ask.has_value()) {
        CHECK(best_ask->price() == Price::from_cents(40));
        CHECK(best_ask->quantity() == Quantity::from_contracts(4));
      }
    }
    if (final_no_book != nullptr) {
      const auto best_ask = final_no_book->asks().best_level();
      CHECK(best_ask.has_value());
      if (best_ask.has_value()) {
        CHECK(best_ask->price() == Price::from_cents(50));
        CHECK(best_ask->quantity() == Quantity::from_contracts(8));
      }
    }
  }

  {
    Market replay_market;
    CHECK(replay_market.add_outcome(yes));
    CHECK(replay_market.add_outcome(no));
    ReplayEngine replay_engine{std::move(replay_market),
                               Money::from_cents(100)};

    const std::vector events{unknown_event};
    CHECK_THROWS_AS(replay_engine.replay(events), std::invalid_argument);
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " replay engine test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All ReplayEngine tests passed\n";
  return EXIT_SUCCESS;
}
