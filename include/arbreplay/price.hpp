#pragma once

#include <cstdint>
#include <stdexcept>

namespace arbreplay {
    class Price {
    public:
        [[nodiscard]] static Price from_cents(int cents) {
            if (cents < 0 || cents > 100) {
                throw std::out_of_range("Price must be between 0 and 100");
            }

            return Price{static_cast<std::uint8_t>(cents)};
        }

        [[nodiscard]] int cents() const noexcept {
            return static_cast<int>(cents_);
        }

        [[nodiscard]] Price complement() const {
            return Price::from_cents(100 - cents());
        }

        [[nodiscard]] bool operator==(const Price& other) const noexcept = default;

    private:
        std::uint8_t cents_;

        explicit Price(std::uint8_t cents) noexcept : cents_{cents} {}
    };
}
