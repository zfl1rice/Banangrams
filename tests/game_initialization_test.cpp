#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/game.hpp"
#include "bananagrams/game_config.hpp"
#include "bananagrams/lexicon.hpp"
#include "bananagrams/observation.hpp"

#include <array>
#include <string>
#include <vector>

TEST_CASE("Game initializes a solo match") {
    using namespace bananagrams;

    // A tiny test configuration, not production defaults. Identical letters
    // let us check the deal without assuming a particular random draw order.
    std::array<int, 26> distribution{};
    distribution[0] = 5; // Five A tiles in the entire game.
    const GameConfig config{GameMode::solo, 1, 2, distribution, 42};
    const Lexicon lexicon{std::vector<std::string>{"CAT", "ARE", "AT", "QI"}};

    const Game game{config, lexicon};
    const PlayerObservation observation = game.observe(0);

    CHECK(game.status() == MatchStatus::active);
    CHECK_FALSE(game.winner().has_value());
    CHECK(observation.status == MatchStatus::active);
    CHECK_FALSE(observation.winner.has_value());

    CHECK(observation.board.empty());
    CHECK(observation.board.size() == 0);
    CHECK_FALSE(observation.rack.empty());
    CHECK(observation.rack.size() == 2);
    CHECK(observation.rack.count('A') == 2);
    CHECK(observation.pool_size == 3);
    CHECK(observation.opponents.empty());

    // Dealing moves tiles from the pool to the rack without losing any.
    CHECK(observation.board.size() + observation.rack.size() + observation.pool_size == 5);
}
