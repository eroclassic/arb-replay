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
#include <vector>

namespace {

constexpr std::string_view usage{
    "usage: arbreplay replay <events.csv> --payout <amount> "
    "[--latency-ns <nanoseconds>] [--fee-bps <basis-points>] [--verbose]\n"
    "       arbreplay replay <snapshot-directory> "
    "[--fee-bps <basis-points>] [--verbose]"};

struct ReplayInput {
  std::filesystem::path events_path;
  arbreplay::Money payout;
};

[[nodiscard]] std::string format_fixed(std::int64_t raw) {
  constexpr auto scale = static_cast<std::uint64_t>(arbreplay::Money::scale);
  const bool negative = raw < 0;
  const auto magnitude = negative
                             ? static_cast<std::uint64_t>(-(raw + 1)) + 1U
                             : static_cast<std::uint64_t>(raw);
  const auto whole = magnitude / scale;
  const auto fraction = magnitude % scale;
  auto fraction_text = std::to_string(fraction);
  fraction_text.insert(0, arbreplay::Money::precision - fraction_text.size(),
                       '0');
  return (negative ? "-" : "") + std::to_string(whole) + "." + fraction_text;
}

void print_detections(
    const std::vector<arbreplay::DetectedCompleteSetOpportunity> &detections) {
  std::cout << "detections: " << detections.size() << '\n';
  for (std::size_t i = 0; i < detections.size(); ++i) {
    const auto &detected = detections[i];
    const auto &opportunity = detected.opportunity();
    std::cout << "opportunity " << i + 1 << ":\n"
              << "  signal_time_ns: " << detected.replay_key().first.count()
              << '\n'
              << "  sequence: " << detected.replay_key().second << '\n'
              << "  available_sets: "
              << format_fixed(opportunity.total_quantity().raw()) << '\n'
              << "  payout_per_set: "
              << format_fixed(opportunity.payout_per_set().raw()) << '\n'
              << "  total_cost: " << format_fixed(opportunity.total_cost().raw())
              << '\n'
              << "  estimated_fees: "
              << format_fixed(opportunity.estimated_fees().raw()) << '\n'
              << "  gross_profit: "
              << format_fixed(opportunity.gross_profit().raw()) << '\n'
              << "  estimated_net_profit: "
              << format_fixed(opportunity.net_profit().raw()) << '\n'
              << "  levels:\n";
    for (const auto &level : opportunity.levels()) {
      std::cout << "    - cost_per_set: "
                << format_fixed(level.cost_per_set().raw())
                << ", quantity: " << format_fixed(level.quantity().raw())
                << '\n';
    }
  }
}

void print_summary(
    const std::vector<arbreplay::DetectedCompleteSetOpportunity> &detections) {
  auto estimated_net_profit_sum = arbreplay::Money::from_raw(0);
  for (const auto &detected : detections) {
    estimated_net_profit_sum =
        estimated_net_profit_sum + detected.opportunity().net_profit();
  }
  std::cout << "detections: " << detections.size() << '\n'
            << "estimated_net_profit_sum: "
            << format_fixed(estimated_net_profit_sum.raw()) << '\n'
            << "note: signal-profit sum, not realized P&L; detections may "
               "overlap\n";
}

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
    bool verbose = false;
    while (option_index < argc) {
      const std::string_view option{argv[option_index]};
      if (option == "--verbose") {
        verbose = true;
        ++option_index;
        continue;
      }
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
    std::vector<arbreplay::DetectedCompleteSetOpportunity> detections{};
    if (has_snapshot_metadata) {
      auto detection = engine.apply_snapshot(events);
      if (detection.has_value()) {
        detections.push_back(std::move(*detection));
      }
    } else {
      detections = engine.replay(events);
    }

    std::cout << "events: " << events.size() << '\n';
    if (verbose) {
      print_detections(detections);
    } else {
      print_summary(detections);
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
