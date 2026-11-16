#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bananagrams {

enum class GameMode { solo, multiplayer };

// Explicit inputs only: production distributions and rack sizes are not chosen yet.
// Game construction will reject invalid configurations with invalid_argument:
// solo needs exactly 1 player, multiplayer >= 2, starting rack > 0,
// counts >= 0, and enough tiles to deal all racks (check totals without overflow).
struct GameConfig {
    GameMode mode;
    std::size_t player_count;
    std::size_t starting_rack_size;
    std::array<int, 26> tile_distribution;
    std::uint32_t seed;

    // Store all required inputs; validation belongs to Game construction.
    GameConfig(GameMode mode_value, std::size_t players, std::size_t rack_size,
               std::array<int, 26> distribution, std::uint32_t seed_value)
        : mode(mode_value), player_count(players), starting_rack_size(rack_size),
          tile_distribution(distribution), seed(seed_value) {}
};

} // namespace bananagrams
