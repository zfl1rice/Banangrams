#pragma once

#include <compare>

namespace bananagrams {

// Signed coordinates allow expansion in every direction; (0, 0) is not special.
struct Position {
    int row{}; // Rows increase downward.
    int col{}; // Columns increase rightward.

    // Compare coordinates by value, including ordering for map keys.
    auto operator<=>(const Position&) const = default;
};

} // namespace bananagrams
