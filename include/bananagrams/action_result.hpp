#pragma once

#include "bananagrams/board_validator.hpp"

#include <optional>

namespace bananagrams {

// Expected action failures use codes; TODO stubs still throw logic_error.
enum class ActionError {
    none,
    invalid_player,
    invalid_letter,
    unavailable_rack_tile,
    occupied_destination,
    invalid_selection,
    missing_board_tile,
    coordinate_overflow,
    inventory_overflow,
    finished_match,
    invalid_move
};

// A successful action can cause an automatic solo draw, multiplayer peel, or finish.
enum class ActionEvent { none, solo_draw, peel, completed };

// An invalid board report does not turn a successful placement into failure.
struct ActionResult {
    bool success{};
    ActionError error{ActionError::none};
    std::optional<ValidationResult> validation;
    ActionEvent event{ActionEvent::none};
};

} // namespace bananagrams