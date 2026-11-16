#pragma once

#include "bananagrams/board.hpp"
#include "bananagrams/rack.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace bananagrams {

using PlayerId = std::size_t;
enum class MatchStatus { active, finished };

// Only counts are visible during play; boards are revealed after the match ends.
struct OpponentObservation {
    PlayerId player{};
    std::size_t unplaced_tile_count{};
    std::size_t total_tile_count{}; // Rack plus board.
    std::optional<Board> revealed_board;
};

// Independent snapshot: no opponent rack letters, hidden pool contents, or RNG state.
struct PlayerObservation {
    Board board;
    Rack rack;
    std::vector<OpponentObservation> opponents;
    std::size_t pool_size{};
    MatchStatus status{MatchStatus::active};
    std::optional<PlayerId> winner;
};

} // namespace bananagrams
