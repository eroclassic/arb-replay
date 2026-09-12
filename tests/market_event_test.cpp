#include <arbreplay/market_event.hpp>

#include "test_support.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

int main() {
  using arbreplay::MarketEvent;
  using arbreplay::OrderSide;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;

  static_assert(!std::is_default_constructible_v<MarketEvent>);

  const auto yes = OutcomeId::from_string(std::string{"YES"});
  const auto event = MarketEvent{
      MarketEvent::Timestamp{1'000'000},
      std::uint64_t{42},
      yes,
      OrderSide::ask,
      Price::from_cents(55),
      Quantity::from_contracts(20),
  };

  CHECK(event.timestamp() == MarketEvent::Timestamp{1'000'000});
  CHECK(event.sequence() == 42);
  CHECK(event.outcome_id() == yes);
  CHECK(event.side() == OrderSide::ask);
  CHECK(event.price() == Price::from_cents(55));
  CHECK(event.quantity() == Quantity::from_contracts(20));

  const auto removal = MarketEvent{
      MarketEvent::Timestamp{1'000'001},
      std::uint64_t{43},
      yes,
      OrderSide::ask,
      Price::from_cents(55),
      Quantity::from_contracts(0),
  };
  CHECK(removal.quantity().contracts() == 0);

  CHECK_THROWS_AS(
      MarketEvent(MarketEvent::Timestamp{-1}, std::uint64_t{1}, yes,
                  OrderSide::bid, Price::from_cents(40),
                  Quantity::from_contracts(10)),
      std::invalid_argument);

  const auto same_as_event = MarketEvent{
      MarketEvent::Timestamp{1'000'000},
      std::uint64_t{42},
      yes,
      OrderSide::ask,
      Price::from_cents(55),
      Quantity::from_contracts(20),
  };
  CHECK(event == same_as_event);

  const auto same_time_later_sequence = MarketEvent{
      MarketEvent::Timestamp{1'000'000},
      std::uint64_t{43},
      yes,
      OrderSide::ask,
      Price::from_cents(56),
      Quantity::from_contracts(20),
  };
  const auto later_time_lower_sequence = MarketEvent{
      MarketEvent::Timestamp{1'000'001},
      std::uint64_t{1},
      yes,
      OrderSide::ask,
      Price::from_cents(57),
      Quantity::from_contracts(20),
  };

  CHECK(event.replay_key() < same_time_later_sequence.replay_key());
  CHECK(same_time_later_sequence.replay_key() <
        later_time_lower_sequence.replay_key());

  const auto conflicting_same_key = MarketEvent{
      MarketEvent::Timestamp{1'000'000},
      std::uint64_t{42},
      yes,
      OrderSide::bid,
      Price::from_cents(54),
      Quantity::from_contracts(5),
  };
  CHECK(event.replay_key() == conflicting_same_key.replay_key());
  CHECK(event != conflicting_same_key);

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " market event test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All MarketEvent tests passed\n";
  return EXIT_SUCCESS;
}
