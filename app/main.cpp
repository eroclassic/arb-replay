#include <arbreplay/fee_rate.hpp>
#include <arbreplay/market.hpp>
#include <arbreplay/market_event_csv.hpp>
#include <arbreplay/money.hpp>
#include <arbreplay/outcome_id.hpp>
#include <arbreplay/replay_engine.hpp>

#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

constexpr std::string_view usage{
    "usage: arbreplay replay <events.csv> --payout <amount> "
    "[--latency-ns <nanoseconds>] [--fee-bps <basis-points>]\n"
    "       arbreplay replay <snapshot-directory> "
    "[--fee-bps <basis-points>]"};

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

[[nodiscard]] arbreplay::MarketEvent::Timestamp
parse_latency(std::string_view value) {
  arbreplay::MarketEvent::Timestamp::rep nanoseconds{};
  const auto *begin = value.data();
  const auto *end = begin + value.size();
  const auto [parsed_end, error] = std::from_chars(begin, end, nanoseconds);
  if (error != std::errc{} || parsed_end != end || nanoseconds < 0) {
    throw std::invalid_argument{
        "latency must be a non-negative integer number of nanoseconds"};
  }
  return arbreplay::MarketEvent::Timestamp{nanoseconds};
}

[[nodiscard]] arbreplay::FeeRate parse_fee_rate(std::string_view value) {
  std::uint32_t basis_points{};
  const auto *begin = value.data();
  const auto *end = begin + value.size();
  const auto [parsed_end, error] = std::from_chars(begin, end, basis_points);
  if (error != std::errc{} || parsed_end != end) {
    throw std::invalid_argument{
        "fee rate must be a non-negative integer number of basis points"};
  }
  return arbreplay::FeeRate::from_basis_points(basis_points);
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 3 || std::string_view{argv[1]} != "replay") {
    std::cerr << usage << '\n';
    return EXIT_FAILURE;
  }

  try {
    const std::filesystem::path path{argv[2]};
    const bool has_snapshot_metadata =
        std::filesystem::is_directory(path) &&
        (argc == 3 || std::string_view{argv[3]} != "--payout");
    if (!has_snapshot_metadata &&
        (argc < 5 || std::string_view{argv[3]} != "--payout")) {
      std::cerr << usage << '\n';
      return EXIT_FAILURE;
    }
    const ReplayInput replay_input = has_snapshot_metadata
                                         ? snapshot_input(path)
                                         : explicit_input(path, argv[4]);
    int option_index = has_snapshot_metadata ? 3 : 5;

    auto latency = arbreplay::MarketEvent::Timestamp::zero();
    auto fee_rate = arbreplay::FeeRate{};
    while (option_index < argc) {
      const std::string_view option{argv[option_index]};
      if (option_index + 1 >= argc) {
        throw std::invalid_argument{"missing value for " + std::string{option}};
      }
      const std::string_view value{argv[option_index + 1]};
      if (option == "--latency-ns") {
        if (has_snapshot_metadata) {
          throw std::invalid_argument{
              "execution latency requires time-ordered event CSV replay"};
        }
        latency = parse_latency(value);
      } else if (option == "--fee-bps") {
        fee_rate = parse_fee_rate(value);
      } else {
        std::cerr << usage << '\n';
        return EXIT_FAILURE;
      }
      option_index += 2;
    }

    std::ifstream input{replay_input.events_path};
    if (!input.is_open()) {
      throw std::runtime_error{"unable to open event CSV: " +
                               replay_input.events_path.string()};
    }

    const auto events = arbreplay::parse_market_events_csv(input);
    auto market = make_binary_market();
    arbreplay::ReplayEngine engine{std::move(market), replay_input.payout,
                                   latency, fee_rate};
    std::size_t detection_count{};
    if (has_snapshot_metadata) {
      detection_count = engine.apply_snapshot(events).has_value() ? 1 : 0;
    } else {
      detection_count = engine.replay(events).size();
    }

    std::cout << "events: " << events.size() << '\n';
    std::cout << "detections: " << detection_count << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
