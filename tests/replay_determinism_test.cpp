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

namespace {
arbreplay::Market make_binary_market(const arbreplay::OutcomeId &yes,
                                     const arbreplay::OutcomeId &no) {
  arbreplay::Market market;
  CHECK(market.add_outcome(yes));
  CHECK(market.add_outcome(no));
  return market;
}
} // namespace

int main() {
  using arbreplay::MarketEvent;
  using arbreplay::Money;
  using arbreplay::OrderSide;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;
  using arbreplay::ReplayEngine;

  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto no = OutcomeId::from_string(std::string{"NO"});

  const auto earlier_yes_ask = MarketEvent{
      MarketEvent::Timestamp{1'000}, std::uint64_t{1}, yes, OrderSide::ask,
      Price::from_cents(40), Quantity::from_contracts(5)};
  const auto later_sequence_yes_ask = MarketEvent{
      MarketEvent::Timestamp{1'000}, std::uint64_t{2}, yes, OrderSide::ask,
      Price::from_cents(60), Quantity::from_contracts(5)};
  const auto later_no_ask = MarketEvent{
      MarketEvent::Timestamp{1'001}, std::uint64_t{1}, no, OrderSide::ask,
      Price::from_cents(50), Quantity::from_contracts(5)};

  const std::vector sorted_events{earlier_yes_ask, later_sequence_yes_ask,
                                  later_no_ask};
  const std::vector unsorted_events{later_sequence_yes_ask, later_no_ask,
                                    earlier_yes_ask};
  const auto original_unsorted_events = unsorted_events;

  ReplayEngine sorted_engine{make_binary_market(yes, no),
                             Money::from_cents(100)};
  ReplayEngine unsorted_engine{make_binary_market(yes, no),
                               Money::from_cents(100)};

  const auto sorted_detections = sorted_engine.replay(sorted_events);
  const auto unsorted_detections = unsorted_engine.replay(unsorted_events);

  CHECK(unsorted_events == original_unsorted_events);
  CHECK(unsorted_detections == sorted_detections);

  const auto *sorted_yes_book = sorted_engine.market().find_outcome(yes);
  const auto *unsorted_yes_book = unsorted_engine.market().find_outcome(yes);
  CHECK(sorted_yes_book != nullptr);
  CHECK(unsorted_yes_book != nullptr);
  if (sorted_yes_book != nullptr && unsorted_yes_book != nullptr) {
    CHECK(sorted_yes_book->asks().best_level() ==
          unsorted_yes_book->asks().best_level());
    const auto final_yes_ask = unsorted_yes_book->asks().best_level();
    CHECK(final_yes_ask.has_value());
    if (final_yes_ask.has_value()) {
      CHECK(final_yes_ask->price() == Price::from_cents(60));
    }
  }

  {
    ReplayEngine duplicate_engine{make_binary_market(yes, no),
                                  Money::from_cents(100)};
    const auto yes_ask = MarketEvent{
        MarketEvent::Timestamp{2'000}, std::uint64_t{1}, yes,
        OrderSide::ask, Price::from_cents(40), Quantity::from_contracts(5)};
    const auto no_ask = MarketEvent{
        MarketEvent::Timestamp{2'001}, std::uint64_t{1}, no, OrderSide::ask,
        Price::from_cents(50), Quantity::from_contracts(5)};
    const std::vector events{yes_ask, no_ask, no_ask};

    const auto detections = duplicate_engine.replay(events);
    CHECK(detections.size() == 1);
    if (detections.size() == 1) {
      CHECK(detections.front().replay_key() == no_ask.replay_key());
    }
  }

  {
    ReplayEngine conflicting_engine{make_binary_market(yes, no),
                                    Money::from_cents(100)};
    const auto first = MarketEvent{
        MarketEvent::Timestamp{3'000}, std::uint64_t{7}, yes,
        OrderSide::ask, Price::from_cents(40), Quantity::from_contracts(5)};
    const auto conflicting = MarketEvent{
        MarketEvent::Timestamp{3'000}, std::uint64_t{7}, yes,
        OrderSide::ask, Price::from_cents(41), Quantity::from_contracts(5)};
    const std::vector events{first, conflicting};

    CHECK_THROWS_AS(conflicting_engine.replay(events), std::invalid_argument);
  }

  {
    const std::vector events{later_no_ask, earlier_yes_ask,
                             later_sequence_yes_ask};
    ReplayEngine first_engine{make_binary_market(yes, no),
                              Money::from_cents(100)};
    ReplayEngine second_engine{make_binary_market(yes, no),
                               Money::from_cents(100)};

    const auto first_detections = first_engine.replay(events);
    const auto second_detections = second_engine.replay(events);

    CHECK(first_detections == second_detections);
    const auto *first_yes_book = first_engine.market().find_outcome(yes);
    const auto *second_yes_book = second_engine.market().find_outcome(yes);
    CHECK(first_yes_book != nullptr);
    CHECK(second_yes_book != nullptr);
    if (first_yes_book != nullptr && second_yes_book != nullptr) {
      CHECK(first_yes_book->asks().best_level() ==
            second_yes_book->asks().best_level());
    }
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures
              << " replay determinism test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All replay determinism tests passed\n";
  return EXIT_SUCCESS;
}
