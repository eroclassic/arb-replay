#include <arbreplay/market.hpp>
#include <arbreplay/market_event_csv.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/replay_engine.hpp>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view usage{
    "usage: arbreplay replay <events.csv> --payout <amount>"};

[[nodiscard]] arbreplay::Market make_binary_market() {
  arbreplay::Market market;
  const bool added_yes =
      market.add_outcome(arbreplay::OutcomeId::from_string(std::string{"YES"}));
  const bool added_no =
      market.add_outcome(arbreplay::OutcomeId::from_string(std::string{"NO"}));

  if (!added_yes || !added_no) {
    throw std::logic_error{"failed to initialize binary market"};
  }

  return market;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 5 || std::string_view{argv[1]} != "replay" ||
      std::string_view{argv[3]} != "--payout") {
    std::cerr << usage << '\n';
    return EXIT_FAILURE;
  }

  try {
    const std::string_view events_path{argv[2]};
    const auto payout = arbreplay::Money::from_decimal(argv[4]);
    if (payout.raw() <= 0) {
      throw std::invalid_argument{"payout must be positive"};
    }

    std::ifstream input{std::string{events_path}};
    if (!input.is_open()) {
      throw std::runtime_error{"unable to open event CSV: " +
                               std::string{events_path}};
    }

    const auto events = arbreplay::parse_market_events_csv(input);
    auto market = make_binary_market();
    arbreplay::ReplayEngine engine{std::move(market), payout};
    const auto detections = engine.replay(events);

    std::cout << "events: " << events.size() << '\n';
    std::cout << "detections: " << detections.size() << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
