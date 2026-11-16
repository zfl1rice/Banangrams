#pragma once

#include <compare>
#include <cstddef>
#include <functional>

namespace bananagrams {

// Signed coordinates allow expansion in every direction; (0, 0) is not special.
struct Position {
    int row{}; // Rows increase downward.
    int col{}; // Columns increase rightward.

    // Compare coordinates by value, including ordering for map keys.
    auto operator<=>(const Position&) const = default;
};

}

template <>
struct std::hash<bananagrams::Position> {
    std::size_t operator()(const bananagrams::Position& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.row);
        std::size_t h2 = std::hash<int>{}(p.col);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

 // namespace bananagrams
