#include <arbreplay/market.hpp>
#include <arbreplay/market_event_csv.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/replay_engine.hpp>

#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view usage{
    "usage: arbreplay replay <events.csv> --payout <amount>\n"
    "       arbreplay replay <snapshot-directory>"};

struct ReplayInput {
  std::filesystem::path events_path;
  arbreplay::Money payout;
};

[[nodiscard]] std::string read_file(const std::filesystem::path &path) {
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error{"unable to open snapshot metadata: " +
                             path.string()};
  }

  return {std::istreambuf_iterator<char>{input},
          std::istreambuf_iterator<char>{}};
}

[[nodiscard]] std::string
payout_from_metadata(const std::filesystem::path &metadata_path) {
  const std::string metadata = read_file(metadata_path);
  constexpr std::string_view key{"\"payout_per_set\""};
  const auto key_position = metadata.find(key);
  if (key_position == std::string::npos) {
    throw std::runtime_error{"snapshot metadata is missing payout_per_set"};
  }

  const auto colon_position = metadata.find(':', key_position + key.size());
  if (colon_position == std::string::npos) {
    throw std::runtime_error{"invalid payout_per_set in snapshot metadata"};
  }

  auto value_position = colon_position + 1;
  while (value_position < metadata.size() &&
         std::isspace(static_cast<unsigned char>(metadata[value_position])) !=
             0) {
    ++value_position;
  }
  if (value_position == metadata.size() || metadata[value_position] != '"') {
    throw std::runtime_error{"payout_per_set must be a JSON string"};
  }

  const auto value_end = metadata.find('"', value_position + 1);
  if (value_end == std::string::npos) {
    throw std::runtime_error{"invalid payout_per_set in snapshot metadata"};
  }
  return metadata.substr(value_position + 1, value_end - value_position - 1);
}

[[nodiscard]] ReplayInput
snapshot_input(const std::filesystem::path &directory) {
  if (!std::filesystem::is_directory(directory)) {
    throw std::invalid_argument{
        "a payout is required when replaying an event CSV"};
  }

  const auto payout = arbreplay::Money::from_decimal(
      payout_from_metadata(directory / "metadata.json"));
  if (payout.raw() <= 0) {
    throw std::invalid_argument{"payout must be positive"};
  }
  return {directory / "events.csv", payout};
}

[[nodiscard]] ReplayInput explicit_input(const std::filesystem::path &path,
                                         std::string_view payout_text) {
  const auto payout = arbreplay::Money::from_decimal(payout_text);
  if (payout.raw() <= 0) {
    throw std::invalid_argument{"payout must be positive"};
  }
  const auto events_path =
      std::filesystem::is_directory(path) ? path / "events.csv" : path;
  return {events_path, payout};
}

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
  const bool is_replay = argc >= 2 && std::string_view{argv[1]} == "replay";
  const bool uses_snapshot_metadata = is_replay && argc == 3;
  const bool uses_explicit_payout =
      is_replay && argc == 5 && std::string_view{argv[3]} == "--payout";
  if (!uses_snapshot_metadata && !uses_explicit_payout) {
    std::cerr << usage << '\n';
    return EXIT_FAILURE;
  }

  try {
    const std::filesystem::path path{argv[2]};
    const ReplayInput replay_input = uses_snapshot_metadata
                                         ? snapshot_input(path)
                                         : explicit_input(path, argv[4]);

    std::ifstream input{replay_input.events_path};
    if (!input.is_open()) {
      throw std::runtime_error{"unable to open event CSV: " +
                               replay_input.events_path.string()};
    }

    const auto events = arbreplay::parse_market_events_csv(input);
    auto market = make_binary_market();
    arbreplay::ReplayEngine engine{std::move(market), replay_input.payout};
    const auto detections = engine.replay(events);

    std::cout << "events: " << events.size() << '\n';
    std::cout << "detections: " << detections.size() << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
