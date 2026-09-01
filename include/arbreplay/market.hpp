#pragma once

#include <arbreplay/outcome_book.hpp>
#include <arbreplay/outcome_id.hpp>

#include <cstddef>
#include <map>
#include <utility>

namespace arbreplay {
class Market {
public:
  [[nodiscard]] bool empty() const noexcept { return outcomes_.empty(); }

  [[nodiscard]] std::size_t size() const noexcept { return outcomes_.size(); }

  [[nodiscard]] OutcomeBook *find_outcome(const OutcomeId &id) {
    auto iterator = outcomes_.find(id);

    if (iterator == outcomes_.end()) {
      return nullptr;
    }

    return &iterator->second;
  }

  [[nodiscard]] const OutcomeBook *find_outcome(const OutcomeId &id) const {
    auto iterator = outcomes_.find(id);

    if (iterator == outcomes_.end()) {
      return nullptr;
    }

    return &iterator->second;
  }

  [[nodiscard]] bool add_outcome(OutcomeId id) {
    return outcomes_.try_emplace(std::move(id)).second;
  }

private:
  std::map<OutcomeId, OutcomeBook> outcomes_;
};

} // namespace arbreplay
