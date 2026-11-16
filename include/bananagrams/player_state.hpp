#pragma once

#include "bananagrams/board.hpp"
#include "bananagrams/rack.hpp"

namespace bananagrams {

// The same owned state serves human and future bot players.
struct PlayerState {
    Board board;
    Rack rack;
};

} // namespace bananagrams
