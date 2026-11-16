#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/action_result.hpp"
#include "bananagrams/board.hpp"
#include "bananagrams/board_validator.hpp"
#include "bananagrams/game.hpp"
#include "bananagrams/game_config.hpp"
#include "bananagrams/lexicon.hpp"
#include "bananagrams/observation.hpp"
#include "bananagrams/player_state.hpp"
#include "bananagrams/position.hpp"
#include "bananagrams/rack.hpp"
#include "bananagrams/tile_pool.hpp"

#include <array>
#include <map>
#include <type_traits>

// Scaffold checks only: these assertions never call unfinished gameplay methods.
static_assert(std::is_copy_constructible_v<bananagrams::Board>);
static_assert(std::is_copy_constructible_v<bananagrams::PlayerObservation>);
static_assert(!std::is_default_constructible_v<bananagrams::GameConfig>);

TEST_CASE("Position has signed coordinates and value comparisons") {
    using bananagrams::Position;
    const Position origin{};
    CHECK(origin.row == 0);
    CHECK(origin.col == 0);
    CHECK((Position{-1, 2} == Position{-1, 2}));
    CHECK((Position{-1, 2} < origin));
    CHECK((Position{0, -1} < origin));

    const std::map<Position, char> sample{{{-1, 2}, 'A'}, {{0, 0}, 'T'}};
    CHECK(sample.at(Position{-1, 2}) == 'A');
}

TEST_CASE("GameConfig retains explicitly supplied inputs") {
    using namespace bananagrams;
    std::array<int, 26> tiny_distribution{};
    tiny_distribution[0] = 3;
    const GameConfig config{GameMode::solo, 1, 2, tiny_distribution, 42};
    CHECK(config.mode == GameMode::solo);
    CHECK(config.player_count == 1);
    CHECK(config.starting_rack_size == 2);
    CHECK(config.tile_distribution == tiny_distribution);
    CHECK(config.seed == 42);
}
