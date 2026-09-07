#pragma once

#include <arbreplay/complete_set_opportunity.hpp>
#include <arbreplay/market.hpp>
#include <arbreplay/money.hpp>

#include <optional>
namespace arbreplay {
[[nodiscard]] std::optional<CompleteSetOpportunity>
detect_complete_set_opportunity(const Market &market, Money payout_per_set);
} // namespace arbreplay
