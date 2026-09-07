#include "arbreplay/complete_set_detector.hpp"
#include "arbreplay/complete_set_opportunity.hpp"
#include <optional>

namespace arbreplay {
[[nodiscard]] std::optional<CompleteSetOpportunity>
detect_complete_set_opportunity(const Market &market, Money payout_per_set) {
  for (const auto &[outcome_id, outcome_book] : market) {
  }
  return std::nullopt;
}
} // namespace arbreplay
