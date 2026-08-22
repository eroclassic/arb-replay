#pragma once

#include <stdexcept>

#include "price.hpp"
#include "quantity.hpp"

namespace arbreplay {
    class BookLevel {
        public:
            explicit BookLevel(Price price, Quantity quantity) : price_(price), quantity_(quantity) {
                if (quantity.contracts() == 0) {
                    throw std::out_of_range("Book level quantity cannot be zero");
                }
            }

            [[nodiscard]] Price price() const noexcept { return price_; }
            [[nodiscard]] Quantity quantity() const noexcept { return quantity_; }

            [[nodiscard]] bool operator==(const BookLevel &) const noexcept = default;

        private:
            Price price_;
            Quantity quantity_;
    };
}
