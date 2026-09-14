#include <arbreplay/market_event_csv.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace arbreplay {
namespace {

constexpr std::string_view expected_header{
    "observed_at_ns,sequence,outcome_id,side,price,quantity"};
constexpr std::size_t field_count = 6;

[[noreturn]] void throw_csv_error(std::size_t line_number,
                                  std::string_view reason) {
  throw std::invalid_argument{"CSV line " + std::to_string(line_number) + ": " +
                              std::string{reason}};
}

void remove_carriage_return(std::string &line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
}

[[nodiscard]] std::array<std::string_view, field_count>
split_row(const std::string &line) {
  std::array<std::string_view, field_count> fields{};
  const std::string_view row{line};
  std::size_t field_begin = 0;

  for (std::size_t index = 0; index < field_count; ++index) {
    const auto comma = row.find(',', field_begin);
    const bool final_field = index == field_count - 1;

    if ((!final_field && comma == std::string_view::npos) ||
        (final_field && comma != std::string_view::npos)) {
      throw std::invalid_argument{"expected exactly six fields"};
    }

    const auto field_end = comma == std::string_view::npos ? row.size() : comma;
    fields[index] = row.substr(field_begin, field_end - field_begin);
    field_begin = field_end + 1;
  }

  return fields;
}

template <typename Integer>
[[nodiscard]] Integer parse_integer(std::string_view field,
                                    std::string_view field_name) {
  Integer value{};
  const auto *begin = field.data();
  const auto *end = begin + field.size();
  const auto result = std::from_chars(begin, end, value);

  if (field.empty() || result.ec != std::errc{} || result.ptr != end) {
    throw std::invalid_argument{"invalid " + std::string{field_name}};
  }

  return value;
}

[[nodiscard]] OrderSide parse_side(std::string_view field) {
  if (field == "bid") {
    return OrderSide::bid;
  }
  if (field == "ask") {
    return OrderSide::ask;
  }
  throw std::invalid_argument{"side must be 'bid' or 'ask'"};
}

[[nodiscard]] MarketEvent parse_event_row(const std::string &line,
                                          std::size_t line_number) {
  try {
    const auto fields = split_row(line);
    const auto observed_at_ns =
        parse_integer<std::int64_t>(fields[0], "observed_at_ns");
    const auto sequence = parse_integer<std::uint64_t>(fields[1], "sequence");

    return MarketEvent{
        MarketEvent::Timestamp{observed_at_ns},
        sequence,
        OutcomeId::from_string(std::string{fields[2]}),
        parse_side(fields[3]),
        Price::from_decimal(fields[4]),
        Quantity::from_decimal(fields[5]),
    };
  } catch (const std::invalid_argument &error) {
    throw_csv_error(line_number, error.what());
  } catch (const std::out_of_range &error) {
    throw_csv_error(line_number, error.what());
  }
}

} // namespace

std::vector<MarketEvent> parse_market_events_csv(std::istream &input) {
  std::string line;
  if (!std::getline(input, line)) {
    throw_csv_error(1, "missing header");
  }

  remove_carriage_return(line);
  if (line != expected_header) {
    throw_csv_error(1, "unexpected header");
  }

  std::vector<MarketEvent> events;
  std::size_t line_number = 1;

  while (std::getline(input, line)) {
    ++line_number;
    remove_carriage_return(line);
    events.push_back(parse_event_row(line, line_number));
  }

  if (input.bad()) {
    throw std::runtime_error{"failed while reading CSV input"};
  }

  return events;
}

} // namespace arbreplay
