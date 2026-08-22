#pragma once

#include <cstdint>
#include <stdexcept>

namespace arbreplay {
    class Quantity {
        public:
            [[nodiscard]] static Quantity from_contracts(std::int64_t contracts) {
                if (contracts < 0) {
                    throw std::out_of_range{"contracts cannot be negative"};
                }

                return Quantity{static_cast<std::uint64_t>(contracts)};
            }

            [[nodiscard]] std::uint64_t contracts() const noexcept{
                return contracts_;
            }

            [[nodiscard]] bool operator==(const Quantity &) const noexcept = default;

        private:
            std::uint64_t contracts_;

            explicit Quantity(std::uint64_t contracts) noexcept : contracts_{contracts} {}
    };
}