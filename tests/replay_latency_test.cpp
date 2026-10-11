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
  const bool added_yes = market.add_outcome(yes);
  const bool added_no = market.add_outcome(no);
  if (!added_yes || !added_no) {
    throw std::logic_error{"failed to create binary test market"};
  }
  return market;
}

arbreplay::ReplayEngine make_engine(const arbreplay::OutcomeId &yes,
                                    const arbreplay::OutcomeId &no,
                                    arbreplay::MarketEvent::Timestamp latency) {
  return arbreplay::ReplayEngine{make_binary_market(yes, no),
                                 arbreplay::Money::from_cents(100), latency};
}
} // namespace

int main() {
  using arbreplay::MarketEvent;
  using arbreplay::OrderSide;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;

  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto no = OutcomeId::from_string(std::string{"NO"});

  {
    const std::vector events{
        MarketEvent{MarketEvent::Timestamp{1'000}, std::uint64_t{1}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(5)},
        MarketEvent{MarketEvent::Timestamp{1'001}, std::uint64_t{2}, no,
                    OrderSide::ask, Price::from_cents(50),
                    Quantity::from_contracts(5)}};

    arbreplay::ReplayEngine default_latency_engine{
        make_binary_market(yes, no), arbreplay::Money::from_cents(100)};
    auto zero_latency_engine = make_engine(yes, no, MarketEvent::Timestamp{0});

    CHECK(zero_latency_engine.replay(events) ==
          default_latency_engine.replay(events));
  }

  {
    // The opportunity exists at t=1,001, but the YES ask is removed before
    // the delayed order arrives at t=1,011.
    const std::vector events{
        MarketEvent{MarketEvent::Timestamp{1'000}, std::uint64_t{1}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(5)},
        MarketEvent{MarketEvent::Timestamp{1'001}, std::uint64_t{2}, no,
                    OrderSide::ask, Price::from_cents(50),
                    Quantity::from_contracts(5)},
        MarketEvent{MarketEvent::Timestamp{1'005}, std::uint64_t{3}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(0)},
        MarketEvent{MarketEvent::Timestamp{1'012}, std::uint64_t{4}, yes,
                    OrderSide::bid, Price::from_cents(30),
                    Quantity::from_contracts(1)}};

    auto engine = make_engine(yes, no, MarketEvent::Timestamp{10});
    CHECK(engine.replay(events).empty());
  }

  {
    // No event lands exactly at t=2,011. The order should use the latest book
    // state at or before arrival, where the executable quantity is now 3.
    const auto opportunity_signal = MarketEvent{MarketEvent::Timestamp{2'001},
                                                std::uint64_t{2},
                                                no,
                                                OrderSide::ask,
                                                Price::from_cents(50),
                                                Quantity::from_contracts(5)};
    const std::vector events{
        MarketEvent{MarketEvent::Timestamp{2'000}, std::uint64_t{1}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(5)},
        opportunity_signal,
        MarketEvent{MarketEvent::Timestamp{2'005}, std::uint64_t{3}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(3)},
        MarketEvent{MarketEvent::Timestamp{2'012}, std::uint64_t{4}, yes,
                    OrderSide::bid, Price::from_cents(30),
                    Quantity::from_contracts(1)}};

    auto engine = make_engine(yes, no, MarketEvent::Timestamp{10});
    const auto detections = engine.replay(events);

    CHECK(!detections.empty());
    if (!detections.empty()) {
      CHECK(detections.front().replay_key() == opportunity_signal.replay_key());
      CHECK(detections.front().opportunity().total_quantity() ==
            Quantity::from_contracts(3));
    }
  }

  {
    // An update timestamped exactly at arrival is visible before the delayed
    // order is evaluated.
    const auto opportunity_signal = MarketEvent{MarketEvent::Timestamp{3'001},
                                                std::uint64_t{2},
                                                no,
                                                OrderSide::ask,
                                                Price::from_cents(50),
                                                Quantity::from_contracts(5)};
    const std::vector events{
        MarketEvent{MarketEvent::Timestamp{3'000}, std::uint64_t{1}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(5)},
        opportunity_signal,
        MarketEvent{MarketEvent::Timestamp{3'011}, std::uint64_t{3}, yes,
                    OrderSide::ask, Price::from_cents(40),
                    Quantity::from_contracts(0)},
        MarketEvent{MarketEvent::Timestamp{3'012}, std::uint64_t{4}, yes,
                    OrderSide::bid, Price::from_cents(30),
                    Quantity::from_contracts(1)}};

    auto engine = make_engine(yes, no, MarketEvent::Timestamp{10});
    CHECK(engine.replay(events).empty());
  }

  {
    CHECK_THROWS_AS(make_engine(yes, no, MarketEvent::Timestamp{-1}),
                    std::invalid_argument);
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " replay latency test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All replay latency tests passed\n";
  return EXIT_SUCCESS;
}
