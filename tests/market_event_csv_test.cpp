#include <arbreplay/market_event_csv.hpp>

#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<arbreplay::MarketEvent> parse(std::string csv) {
  std::istringstream input{std::move(csv)};
  return arbreplay::parse_market_events_csv(input);
}

} // namespace

int main() {
  using arbreplay::MarketEvent;
  using arbreplay::OrderSide;
  using arbreplay::OutcomeId;
  using arbreplay::Price;
  using arbreplay::Quantity;

  {
    const auto events =
        parse("observed_at_ns,sequence,outcome_id,side,price,quantity\n"
              "1000000000,1,YES,ask,0.420000,10.000000\n"
              "1000000000,2,NO,bid,0.550001,8.500000\n");

    CHECK(events.size() == 2);
    if (events.size() == 2) {
      CHECK(events[0].timestamp() == MarketEvent::Timestamp{1'000'000'000});
      CHECK(events[0].sequence() == 1);
      CHECK(events[0].outcome_id() ==
            OutcomeId::from_string(std::string{"YES"}));
      CHECK(events[0].side() == OrderSide::ask);
      CHECK(events[0].price() == Price::from_decimal("0.420000"));
      CHECK(events[0].quantity() == Quantity::from_decimal("10.000000"));

      CHECK(events[1].timestamp() == MarketEvent::Timestamp{1'000'000'000});
      CHECK(events[1].sequence() == 2);
      CHECK(events[1].outcome_id() ==
            OutcomeId::from_string(std::string{"NO"}));
      CHECK(events[1].side() == OrderSide::bid);
      CHECK(events[1].price() == Price::from_decimal("0.550001"));
      CHECK(events[1].quantity() == Quantity::from_decimal("8.500000"));
    }
  }

  {
    const auto removals =
        parse("observed_at_ns,sequence,outcome_id,side,price,quantity\r\n"
              "2000000000,3,YES,ask,0.420000,0\r\n");

    CHECK(removals.size() == 1);
    if (removals.size() == 1) {
      CHECK(removals.front().quantity() == Quantity::from_raw(0));
    }
  }

  {
    const auto no_events =
        parse("observed_at_ns,sequence,outcome_id,side,price,quantity\n");
    CHECK(no_events.empty());
  }

  const std::vector<std::string> malformed_inputs{
      "",
      "timestamp,sequence,outcome_id,side,price,quantity\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,ask,0.42\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,ask,0.42,10,unexpected\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "-1,2,YES,ask,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,-2,YES,ask,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,,ask,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,buy,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,ASK,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,ask,1.000001,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,2,YES,ask,0.42,-1\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "not-a-time,2,YES,ask,0.42,10\n",
      "observed_at_ns,sequence,outcome_id,side,price,quantity\n"
      "1,not-a-sequence,YES,ask,0.42,10\n",
  };

  for (const auto &input : malformed_inputs) {
    CHECK_THROWS_AS(parse(input), std::invalid_argument);
  }

  if (test_support::failures != 0) {
    std::cerr << test_support::failures << " market event CSV test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All market event CSV tests passed\n";
  return EXIT_SUCCESS;
}
