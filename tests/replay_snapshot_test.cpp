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

std::vector<arbreplay::MarketEvent>
make_profitable_snapshot(const arbreplay::OutcomeId &yes,
                         const arbreplay::OutcomeId &no) {
  using arbreplay::MarketEvent;
  using arbreplay::OrderSide;
  using arbreplay::Price;
  using arbreplay::Quantity;

  constexpr auto timestamp = MarketEvent::Timestamp{1'000};
  return {
      {timestamp, std::uint64_t{1}, yes, OrderSide::bid, Price::from_cents(30),
       Quantity::from_contracts(2)},
      {timestamp, std::uint64_t{2}, yes, OrderSide::ask, Price::from_cents(40),
       Quantity::from_contracts(5)},
      {timestamp, std::uint64_t{3}, yes, OrderSide::ask, Price::from_cents(45),
       Quantity::from_contracts(10)},
      {timestamp, std::uint64_t{4}, no, OrderSide::bid, Price::from_cents(40),
       Quantity::from_contracts(3)},
      {timestamp, std::uint64_t{5}, no, OrderSide::ask, Price::from_cents(50),
       Quantity::from_contracts(4)},
      {timestamp, std::uint64_t{6}, no, OrderSide::ask, Price::from_cents(55),
       Quantity::from_contracts(8)},
  };
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

  {
    ReplayEngine engine{make_binary_market(yes, no), Money::from_cents(100)};
    const auto snapshot = make_profitable_snapshot(yes, no);

    // A snapshot is committed as one unit and therefore produces at most one
    // detection, after every book level has been installed.
    const auto detection = engine.seed_snapshot(snapshot);

    CHECK(detection.has_value());
    if (detection.has_value()) {
      CHECK(detection->replay_key() == snapshot.back().replay_key());
      CHECK(detection->opportunity().levels().size() == 2);
      CHECK(detection->opportunity().total_quantity() ==
            Quantity::from_contracts(5));
      CHECK(detection->opportunity().gross_profit() == Money::from_cents(45));
    }

    const auto *yes_book = engine.market().find_outcome(yes);
    const auto *no_book = engine.market().find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);
    if (yes_book != nullptr && no_book != nullptr) {
      CHECK(yes_book->asks().size() == 2);
      CHECK(no_book->asks().size() == 2);
      CHECK(yes_book->bids().size() == 1);
      CHECK(no_book->bids().size() == 1);
    }
  }

  {
    const auto sorted_snapshot = make_profitable_snapshot(yes, no);
    const std::vector<MarketEvent> shuffled_snapshot{
        sorted_snapshot[5], sorted_snapshot[1], sorted_snapshot[3],
        sorted_snapshot[0], sorted_snapshot[4], sorted_snapshot[2]};

    ReplayEngine sorted_engine{make_binary_market(yes, no),
                               Money::from_cents(100)};
    ReplayEngine shuffled_engine{make_binary_market(yes, no),
                                 Money::from_cents(100)};

    const auto sorted_detection = sorted_engine.seed_snapshot(sorted_snapshot);
    const auto shuffled_detection =
        shuffled_engine.seed_snapshot(shuffled_snapshot);

    CHECK(sorted_detection == shuffled_detection);
    const auto *sorted_yes = sorted_engine.market().find_outcome(yes);
    const auto *shuffled_yes = shuffled_engine.market().find_outcome(yes);
    const auto *sorted_no = sorted_engine.market().find_outcome(no);
    const auto *shuffled_no = shuffled_engine.market().find_outcome(no);
    CHECK(sorted_yes != nullptr);
    CHECK(shuffled_yes != nullptr);
    CHECK(sorted_no != nullptr);
    CHECK(shuffled_no != nullptr);
    if (sorted_yes != nullptr && shuffled_yes != nullptr &&
        sorted_no != nullptr && shuffled_no != nullptr) {
      CHECK(sorted_yes->asks().levels() == shuffled_yes->asks().levels());
      CHECK(sorted_yes->bids().levels() == shuffled_yes->bids().levels());
      CHECK(sorted_no->asks().levels() == shuffled_no->asks().levels());
      CHECK(sorted_no->bids().levels() == shuffled_no->bids().levels());
    }
  }

  {
    ReplayEngine engine{make_binary_market(yes, no), Money::from_cents(100)};
    const auto unknown = OutcomeId::from_string(std::string{"UNKNOWN"});
    const std::vector invalid_snapshot{
        MarketEvent{MarketEvent::Timestamp{2'000}, std::uint64_t{1}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(5)},
        MarketEvent{MarketEvent::Timestamp{2'000}, std::uint64_t{2}, unknown,
                    OrderSide::ask, Price::from_cents(50),
                    Quantity::from_contracts(5)}};

    // Atomicity includes failure: no prefix of an invalid snapshot may leak
    // into the engine's live market.
    CHECK_THROWS_AS(engine.seed_snapshot(invalid_snapshot),
                    std::invalid_argument);

    const auto *yes_book = engine.market().find_outcome(yes);
    const auto *no_book = engine.market().find_outcome(no);
    CHECK(yes_book != nullptr);
    CHECK(no_book != nullptr);
    if (yes_book != nullptr && no_book != nullptr) {
      CHECK(yes_book->asks().empty());
      CHECK(yes_book->bids().empty());
      CHECK(no_book->asks().empty());
      CHECK(no_book->bids().empty());
    }
  }

  {
    ReplayEngine engine{make_binary_market(yes, no), Money::from_cents(100)};
    const std::vector<MarketEvent> empty_snapshot{};

    CHECK(!engine.seed_snapshot(empty_snapshot).has_value());
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " replay snapshot test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All replay snapshot tests passed\n";
  return EXIT_SUCCESS;
}
