#include "bananagrams/board_validator.hpp"

#include <stdexcept>

namespace bananagrams {

// TODO: Report maximal words and connected components with their tile positions.
ValidationResult validate_board(const Board& /*board*/, const Lexicon& /*lexicon*/) {
    throw std::logic_error("TODO: validate_board");
}

} // namespace bananagrams
