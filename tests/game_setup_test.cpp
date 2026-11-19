#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/game.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace bananagrams;

GameConfig config(GameMode mode, std::size_t players, std::size_t rack_size, int a_count) {
    std::array<int, 26> distribution{};
    distribution[0] = a_count;
    return GameConfig{mode, players, rack_size, distribution, 42};
}

Lexicon words() {
    return Lexicon{std::vector<std::string>{"AA", "AAA", "AAB"}};
}

void check_success(const ActionResult& result, ActionEvent event) {
    CHECK(result.success);
    CHECK(result.error == ActionError::none);
    CHECK(result.event == event);
}
} // namespace

TEST_CASE("Constructor rejects invalid configurations before dealing") {
    auto settings = config(GameMode::solo, 1, 2, 5);
    SUBCASE("solo with no players") {
        settings.player_count = 0;
    }
    SUBCASE("solo with two players") {
        settings.player_count = 2;
    }
    SUBCASE("multiplayer with one player") {
        settings.mode = GameMode::multiplayer;
    }
    SUBCASE("unknown mode") {
        settings.mode = static_cast<GameMode>(99);
    }
    SUBCASE("zero starting rack") {
        settings.starting_rack_size = 0;
    }
    SUBCASE("negative distribution") {
        settings.tile_distribution[1] = -1;
    }
    SUBCASE("empty distribution") {
        settings.tile_distribution.fill(0);
    }
    SUBCASE("insufficient tiles") {
        settings.starting_rack_size = 6;
    }
    SUBCASE("rack size that would overflow a multiplication") {
        settings.mode = GameMode::multiplayer;
        settings.player_count = 2;
        settings.starting_rack_size = std::numeric_limits<std::size_t>::max();
    }
    SUBCASE("player count that would overflow a multiplication") {
        settings.mode = GameMode::multiplayer;
        settings.player_count = std::numeric_limits<std::size_t>::max();
    }
    SUBCASE("supply exceeds supported counters") {
        settings.tile_distribution[0] = std::numeric_limits<int>::max();
        settings.tile_distribution[1] = 1;
    }
    CHECK_THROWS_AS((Game{settings, words()}), std::invalid_argument);
}

TEST_CASE("Starting racks are dealt and the supplied lexicon survives construction") {
    const Lexicon lexicon = words();
    Game game{config(GameMode::solo, 1, 2, 2), lexicon};
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    const auto result = game.place_tile(0, 'A', {0, 1});
    check_success(result, ActionEvent::completed);
    REQUIRE(result.validation.has_value());
    CHECK(result.validation->valid);
    CHECK(result.validation->invalid_words.empty());
    CHECK(lexicon.contains("AA"));
    CHECK(game.place_tile(0, 'A', {0, 2}).error == ActionError::finished_match);
}

TEST_CASE("Solo draw commits the pool and rack together") {
    Game game{config(GameMode::solo, 1, 2, 3), words()};
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    const auto drawn = game.place_tile(0, 'A', {0, 1});
    check_success(drawn, ActionEvent::solo_draw);
    REQUIRE(drawn.validation.has_value());
    CHECK(drawn.validation->valid);
    // The last pool tile is now available in the rack; playing it clears the entire supply.
    const auto finished = game.place_tile(0, 'A', {0, 2});
    check_success(finished, ActionEvent::completed);
    REQUIRE(finished.validation.has_value());
    CHECK(finished.validation->valid);
}

TEST_CASE("Multiplayer starting deals and an exact-size peel reach every player") {
    Game game{config(GameMode::multiplayer, 2, 2, 6), words()};
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 1}), ActionEvent::peel);
    // Player 1 has two starting tiles plus one peel tile, so all three must be playable.
    check_success(game.place_tile(1, 'A', {10, 0}), ActionEvent::none);
    check_success(game.place_tile(1, 'A', {10, 1}), ActionEvent::none);
    check_success(game.place_tile(1, 'A', {10, 2}), ActionEvent::completed);
    CHECK(game.place_tile(0, 'A', {0, 2}).error == ActionError::finished_match);
}

TEST_CASE("Multiplayer finishes when fewer tiles than players remain") {
    Game game{config(GameMode::multiplayer, 2, 2, 5), words()};
    check_success(game.place_tile(1, 'A', {0, 0}), ActionEvent::none);
    check_success(game.place_tile(1, 'A', {0, 1}), ActionEvent::completed);
    CHECK(game.place_tile(0, 'A', {10, 0}).error == ActionError::finished_match);
}

TEST_CASE("Failed placements preserve the starting rack and board") {
    Game game{config(GameMode::solo, 1, 2, 2), words()};
    ActionResult failure;
    ActionError expected{};
    SUBCASE("invalid player") {
        failure = game.place_tile(1, 'A', {0, 0});
        expected = ActionError::invalid_player;
    }
    SUBCASE("invalid letter") {
        failure = game.place_tile(0, 'a', {0, 0});
        expected = ActionError::invalid_letter;
    }
    SUBCASE("unavailable letter") {
        failure = game.place_tile(0, 'Z', {0, 0});
        expected = ActionError::unavailable_rack_tile;
    }
    CHECK_FALSE(failure.success);
    CHECK(failure.error == expected);
    CHECK(failure.event == ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 1}), ActionEvent::completed);
}

TEST_CASE("Same-letter replacement leaves the remaining rack tile available") {
    Game game{config(GameMode::solo, 1, 2, 2), words()};
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    const auto noop = game.place_tile(0, 'A', {0, 0});
    check_success(noop, ActionEvent::none);
    CHECK_FALSE(noop.validation.has_value());
    check_success(game.place_tile(0, 'A', {0, 1}), ActionEvent::completed);
}

TEST_CASE("Replacing a different letter preserves the displaced tile") {
    auto settings = config(GameMode::solo, 1, 3, 2);
    settings.tile_distribution[1] = 1;
    Game game{settings, words()};
    check_success(game.place_tile(0, 'B', {0, 0}), ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 1}), ActionEvent::none);
    const auto result = game.place_tile(0, 'B', {0, 2});
    check_success(result, ActionEvent::completed);
    REQUIRE(result.validation.has_value());
    CHECK(result.validation->valid);
}

TEST_CASE("An invalid board commits placement and does not finish or draw") {
    Game game{config(GameMode::solo, 1, 2, 3), words()};
    check_success(game.place_tile(0, 'A', {0, 0}), ActionEvent::none);
    const auto result = game.place_tile(0, 'A', {0, 3});
    check_success(result, ActionEvent::none);
    REQUIRE(result.validation.has_value());
    CHECK_FALSE(result.validation->valid);
    CHECK(result.validation->connected_components.size() == 2);
    CHECK(game.place_tile(0, 'A', {0, 1}).error == ActionError::unavailable_rack_tile);
    // The invalid placement really exists and can be returned, then placed to connect the word.
    check_success(game.return_tiles(0, {{0, 3}}), ActionEvent::none);
    check_success(game.place_tile(0, 'A', {0, 1}), ActionEvent::solo_draw);
    check_success(game.place_tile(0, 'A', {0, 2}), ActionEvent::completed);
}
