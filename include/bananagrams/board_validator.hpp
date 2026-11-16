#pragma once

#include "bananagrams/board.hpp"
#include "bananagrams/lexicon.hpp"
#include "bananagrams/position.hpp"

#include <string>
#include <vector>

namespace bananagrams {

// Word positions follow reading order: left to right or top to bottom.
struct WordOccurrence {
    std::string text;
    std::vector<Position> positions;
};

// Positions let the UI highlight invalid words and disconnected components.
struct ValidationResult {
    bool valid{};
    std::vector<WordOccurrence> invalid_words;
    std::vector<std::vector<Position>> connected_components;
    bool has_at_least_two_tiles{};
};

// Read-only validation: require >= 2 tiles, one orthogonal component, and lexicon
// membership for every maximal horizontal/vertical run of length >= 2.
// Ignore single-letter runs. Currently throws logic_error; no traversal exists yet.
[[nodiscard]] ValidationResult validate_board(const Board& board, const Lexicon& lexicon);

} // namespace bananagrams
