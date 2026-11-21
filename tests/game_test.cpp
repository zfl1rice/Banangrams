#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/game.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace bananagrams;

// Tiny synthetic distributions/lexicons make rule tests independent of random letter order.
Game all_a_game(std::size_t players, std::size_t rack_size, int total,
                std::vector<std::string> words = {"AA", "AAA", "AAAA"}) {
    std::array<int, 26> distribution{};
    distribution[0] = total;
    return Game{GameConfig{players == 1 ? GameMode::solo : GameMode::multiplayer, players,
                           rack_size, distribution, 42},
                Lexicon{words}};
}

void success(const ActionResult& result, ActionEvent event = ActionEvent::none) {
    REQUIRE(result.success);
    CHECK(result.error == ActionError::none);
    CHECK(result.event == event);
}

void failure(const ActionResult& result, ActionError error) {
    CHECK_FALSE(result.success);
    CHECK(result.error == error);
    CHECK(result.event == ActionEvent::none);
    CHECK_FALSE(result.validation.has_value());
}

using OpponentView = std::tuple<std::size_t, std::size_t, std::optional<std::map<Position, char>>>;
std::map<PlayerId, OpponentView> opponents(const PlayerObservation& observation) {
    std::map<PlayerId, OpponentView> result;
    for (const auto& opponent : observation.opponents) {
        std::optional<std::map<Position, char>> board;
        if (opponent.revealed_board.has_value()) {
            board = opponent.revealed_board->tiles();
        }
        REQUIRE(result
                    .emplace(opponent.player, OpponentView{opponent.unplaced_tile_count,
                                                           opponent.total_tile_count, board})
                    .second);
    }
    return result;
}

void same_observation(const PlayerObservation& actual, const PlayerObservation& expected) {
    CHECK(actual.board.tiles() == expected.board.tiles());
    CHECK(actual.board.size() == expected.board.size());
    CHECK(actual.rack.counts() == expected.rack.counts());
    CHECK(actual.rack.size() == expected.rack.size());
    CHECK(actual.pool_size == expected.pool_size);
    CHECK(actual.status == expected.status);
    CHECK(actual.winner == expected.winner);
    CHECK(opponents(actual) == opponents(expected));
}

std::vector<PlayerObservation> snapshot(const Game& game, std::size_t players) {
    std::vector<PlayerObservation> result;
    for (PlayerId id = 0; id < players; ++id) {
        result.push_back(game.observe(id));
    }
    return result;
}

void unchanged(const Game& game, const std::vector<PlayerObservation>& before) {
    for (PlayerId id = 0; id < before.size(); ++id) {
        same_observation(game.observe(id), before[id]);
    }
    CHECK(game.status() == before[0].status);
    CHECK(game.winner() == before[0].winner);
}

void conserved(const Game& game, std::size_t players, std::size_t expected) {
    const auto pool_size = game.observe(0).pool_size;
    std::size_t total = pool_size;
    for (PlayerId id = 0; id < players; ++id) {
        const auto view = game.observe(id);
        CHECK(view.pool_size == pool_size);
        CHECK(view.status == game.status());
        CHECK(view.winner == game.winner());
        CHECK(view.board.size() == view.board.tiles().size());
        std::size_t rack_total = 0;
        for (int count : view.rack.counts()) {
            REQUIRE(count >= 0);
            rack_total += static_cast<std::size_t>(count);
        }
        CHECK(view.rack.size() == rack_total);
        total += view.board.size() + rack_total;
    }
    CHECK(total == expected);
}
} // namespace

TEST_CASE("Selection validation checks only selected tiles without changing any player") {
    auto game = all_a_game(2, 5, 14, {"AA"});
    success(game.place_tile(0, 'A', {-1, -2}));
    success(game.place_tile(0, 'A', {-1, -1}));
    success(game.place_tile(0, 'A', {-1, 0}));
    success(game.place_tile(0, 'A', {0, -1}));
    const auto before = snapshot(game, 2);
    const Game& read_only = game;
    auto result = read_only.validate_selection(0, {{-1, -2}, {-1, -1}, {0, -1}});
    success(result);
    REQUIRE(result.validation);
    CHECK(result.validation->valid); // AA horizontally and vertically, ignoring adjacent A.
    unchanged(game, before);

    result = read_only.validate_selection(0, {{-1, -2}, {-1, -1}, {-1, 0}});
    success(result);
    REQUIRE(result.validation);
    CHECK_FALSE(result.validation->valid);
    REQUIRE(result.validation->invalid_words.size() == 1);
    CHECK(result.validation->invalid_words[0].text == "AAA");
    CHECK(result.validation->invalid_words[0].positions.front() == Position{-1, -2});

    result = read_only.validate_selection(0, {{-1, -2}, {-1, 0}});
    success(result);
    REQUIRE(result.validation);
    CHECK_FALSE(result.validation->valid);
    CHECK(result.validation->connected_components.size() == 2);

    result = read_only.validate_selection(0, {{-1, -1}});
    success(result);
    REQUIRE(result.validation);
    CHECK_FALSE(result.validation->valid);
    CHECK_FALSE(result.validation->has_at_least_two_tiles);
    unchanged(game, before);
}

TEST_CASE("Selection validation rejects malformed requests without side effects") {
    auto game = all_a_game(1, 3, 4);
    success(game.place_tile(0, 'A', {0, 0}));
    const auto before = snapshot(game, 1);
    failure(game.validate_selection(1, {{0, 0}}), ActionError::invalid_player);
    failure(game.validate_selection(0, {}), ActionError::invalid_selection);
    failure(game.validate_selection(0, {{0, 0}, {0, 0}}), ActionError::invalid_selection);
    failure(game.validate_selection(0, {{0, 0}, {0, 1}}), ActionError::missing_board_tile);
    unchanged(game, before);
}

TEST_CASE("Selection validation remains read-only after completion") {
    auto game = all_a_game(1, 2, 2);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 1}), ActionEvent::completed);
    const auto before = snapshot(game, 1);
    const auto result = game.validate_selection(0, {{0, 0}, {0, 1}});
    success(result);
    REQUIRE(result.validation);
    CHECK(result.validation->valid);
    unchanged(game, before);
}

TEST_CASE("Multiplayer observations describe starting racks without revealing boards") {
    auto game = all_a_game(3, 2, 9);
    for (PlayerId id = 0; id < 3; ++id) {
        const auto view = game.observe(id);
        CHECK(view.board.empty());
        CHECK(view.rack.size() == 2);
        CHECK(view.rack.count('A') == 2);
        CHECK(view.pool_size == 3);
        CHECK(view.status == MatchStatus::active);
        CHECK_FALSE(view.winner.has_value());
        REQUIRE(view.opponents.size() == 2);
        const auto by_id = opponents(view);
        CHECK(by_id.count(id) == 0);
        for (PlayerId other = 0; other < 3; ++other) {
            if (other == id) {
                continue;
            }
            REQUIRE(by_id.count(other) == 1);
            CHECK(std::get<0>(by_id.at(other)) == 2);
            CHECK(std::get<1>(by_id.at(other)) == 2);
            CHECK_FALSE(std::get<2>(by_id.at(other)).has_value());
        }
    }
    conserved(game, 3, 9);
}

TEST_CASE("All actions reject invalid player IDs without changing any player") {
    auto game = all_a_game(2, 3, 10);
    const auto before = snapshot(game, 2);
    for (PlayerId invalid : {PlayerId{2}, std::numeric_limits<PlayerId>::max()}) {
        failure(game.place_tile(invalid, 'A', {0, 0}), ActionError::invalid_player);
        failure(game.return_tiles(invalid, {{0, 0}}), ActionError::invalid_player);
        failure(game.move_tiles(invalid, {{0, 0}}, {0, 0}), ActionError::invalid_player);
        failure(game.dump(invalid, 'A'), ActionError::invalid_player);
        CHECK_THROWS_AS(static_cast<void>(game.observe(invalid)), std::out_of_range);
        unchanged(game, before);
    }
}

TEST_CASE("Placement and dump reject unsupported or unavailable letters atomically") {
    auto game = all_a_game(2, 3, 10);
    const auto before = snapshot(game, 2);
    for (char letter : {'a', '?', '\0', static_cast<char>(0xFF)}) {
        failure(game.place_tile(1, letter, {0, 0}), ActionError::invalid_letter);
        failure(game.dump(1, letter), ActionError::invalid_letter);
        unchanged(game, before);
    }
    failure(game.place_tile(1, 'Z', {0, 0}), ActionError::unavailable_rack_tile);
    failure(game.dump(1, 'Z'), ActionError::unavailable_rack_tile);
    unchanged(game, before);
}

TEST_CASE("Replacement preserves exact letters and same-letter replacement is a no-op") {
    std::array<int, 26> distribution{};
    distribution[0] = 2;
    distribution[1] = 1;
    Game game{GameConfig{GameMode::solo, 1, 3, distribution, 42},
              Lexicon{std::vector<std::string>{"AA", "AAB"}}};
    success(game.place_tile(0, 'B', {-2, -1}));
    success(game.place_tile(0, 'A', {-2, -1}));
    const auto view = game.observe(0);
    CHECK(view.board.tile_at({-2, -1}) == std::optional<char>{'A'});
    CHECK(view.board.size() == 1);
    CHECK(view.rack.count('A') == 1);
    CHECK(view.rack.count('B') == 1);
    CHECK(view.rack.size() == 2);
    const auto before = snapshot(game, 1);
    const auto noop = game.place_tile(0, 'A', {-2, -1});
    success(noop);
    CHECK_FALSE(noop.validation.has_value());
    unchanged(game, before);
    conserved(game, 1, 3);
}

TEST_CASE("Invalid placements commit with an accurate validation report") {
    auto game = all_a_game(1, 2, 2, {"QI"});
    success(game.place_tile(0, 'A', {0, 0}));
    const auto result = game.place_tile(0, 'A', {0, 1});
    success(result);
    REQUIRE(result.validation.has_value());
    CHECK_FALSE(result.validation->valid);
    REQUIRE(result.validation->invalid_words.size() == 1);
    CHECK(result.validation->invalid_words[0].text == "AA");
    const std::vector<Position> expected{{0, 0}, {0, 1}};
    CHECK(result.validation->invalid_words[0].positions == expected);
    CHECK(game.observe(0).rack.empty());
    CHECK(game.observe(0).board.size() == 2);
    CHECK(game.status() == MatchStatus::active);
    CHECK_FALSE(game.winner().has_value());
    success(game.return_tiles(0, {{0, 1}}));
    CHECK(result.validation->invalid_words[0].positions == expected);
    conserved(game, 1, 2);
}

TEST_CASE("Returning tiles transfers a whole selection without affecting opponents or pool") {
    auto game = all_a_game(2, 3, 8);
    success(game.place_tile(1, 'A', {-1, 0}));
    success(game.place_tile(1, 'A', {-1, 1}));
    const auto opponent_before = game.observe(0);
    const auto result = game.return_tiles(1, {{-1, 1}, {-1, 0}});
    success(result);
    CHECK_FALSE(result.validation.has_value());
    const auto actor = game.observe(1);
    CHECK(actor.board.empty());
    CHECK(actor.rack.size() == 3);
    CHECK(actor.rack.count('A') == 3);
    CHECK(actor.pool_size == 2);
    const auto opponent_after = game.observe(0);
    CHECK(opponent_after.board.tiles() == opponent_before.board.tiles());
    CHECK(opponent_after.rack.counts() == opponent_before.rack.counts());
    CHECK(opponent_after.pool_size == opponent_before.pool_size);
    conserved(game, 2, 8);
}

TEST_CASE("Invalid return selections leave all tiles in their original locations") {
    auto game = all_a_game(2, 3, 8);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 1}));
    std::vector<Position> positions;
    ActionError error{};
    SUBCASE("empty") {
        error = ActionError::invalid_selection;
    }
    SUBCASE("duplicate") {
        positions = {{0, 0}, {0, 0}};
        error = ActionError::invalid_selection;
    }
    SUBCASE("valid then missing") {
        positions = {{0, 0}, {9, 9}};
        error = ActionError::missing_board_tile;
    }
    SUBCASE("missing then valid") {
        positions = {{9, 9}, {0, 0}};
        error = ActionError::missing_board_tile;
    }
    const auto before = snapshot(game, 2);
    failure(game.return_tiles(0, positions), error);
    unchanged(game, before);
}

TEST_CASE("Group movement preserves distinct letters when destinations overlap sources") {
    std::array<int, 26> distribution{};
    distribution[0] = distribution[1] = distribution[2] = distribution[3] = 1;
    Game game{GameConfig{GameMode::solo, 1, 4, distribution, 42},
              Lexicon{std::vector<std::string>{"ABCD"}}};
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'B', {0, 1}));
    success(game.place_tile(0, 'C', {0, 2}));
    std::vector<Position> selection;
    SUBCASE("forward source order") {
        selection = {{0, 0}, {0, 1}, {0, 2}};
    }
    SUBCASE("reverse source order") {
        selection = {{0, 2}, {0, 1}, {0, 0}};
    }
    const auto before = game.observe(0);
    success(game.move_tiles(0, selection, {0, 1}));
    const auto after = game.observe(0);
    const std::map<Position, char> expected{{{0, 1}, 'A'}, {{0, 2}, 'B'}, {{0, 3}, 'C'}};
    CHECK(after.board.tiles() == expected);
    CHECK(after.rack.counts() == before.rack.counts());
    CHECK(after.pool_size == before.pool_size);
    conserved(game, 1, 4);
}

TEST_CASE("Invalid movement selections and collisions fail atomically") {
    auto game = all_a_game(2, 3, 10);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 1}));
    std::vector<Position> selection;
    Position offset{1, 0};
    SUBCASE("empty selection") {}
    SUBCASE("empty selection with zero offset") {
        offset = {0, 0};
    }
    SUBCASE("duplicate selection") {
        selection = {{0, 0}, {0, 0}};
    }
    SUBCASE("duplicates with zero offset") {
        selection = {{0, 0}, {0, 0}};
        offset = {0, 0};
    }
    SUBCASE("missing source after a valid source") {
        selection = {{0, 0}, {5, 5}};
    }
    SUBCASE("missing source with zero offset") {
        selection = {{5, 5}};
        offset = {0, 0};
    }
    SUBCASE("unselected destination occupied") {
        selection = {{0, 0}};
        offset = {0, 1};
    }
    SUBCASE("coordinate overflow") {
        selection = {{0, 1}};
        offset = {0, std::numeric_limits<int>::max()};
    }
    SUBCASE("coordinate underflow") {
        success(game.place_tile(0, 'A', {std::numeric_limits<int>::min(), 5}));
        selection = {{std::numeric_limits<int>::min(), 5}};
        offset = {-1, 0};
    }
    const auto before = snapshot(game, 2);
    failure(game.move_tiles(0, selection, offset), ActionError::invalid_move);
    unchanged(game, before);
}

TEST_CASE("A valid zero-offset move leaves empty-rack state unchanged without validation") {
    auto game = all_a_game(1, 2, 3);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 2}));
    REQUIRE(game.observe(0).rack.empty());
    const auto before = snapshot(game, 1);
    const auto result = game.move_tiles(0, {{0, 0}, {0, 2}}, {0, 0});
    success(result);
    CHECK_FALSE(result.validation.has_value());
    unchanged(game, before);
}

TEST_CASE("Repairing an empty-rack solo board draws or completes the match") {
    int total = 0;
    ActionEvent expected{};
    SUBCASE("remaining pool tile") {
        total = 3;
        expected = ActionEvent::solo_draw;
    }
    SUBCASE("empty pool") {
        total = 2;
        expected = ActionEvent::completed;
    }
    auto game = all_a_game(1, 2, total);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 2}));
    const auto result = game.move_tiles(0, {{0, 2}}, {0, -1});
    success(result, expected);
    REQUIRE(result.validation.has_value());
    CHECK(result.validation->valid);
    const auto after = game.observe(0);
    const std::map<Position, char> board{{{0, 0}, 'A'}, {{0, 1}, 'A'}};
    CHECK(after.board.tiles() == board);
    CHECK(after.pool_size == 0);
    CHECK(after.rack.size() == static_cast<std::size_t>(total - 2));
    CHECK(game.status() == (total == 2 ? MatchStatus::finished : MatchStatus::active));
    CHECK(game.winner() == (total == 2 ? std::optional<PlayerId>{0} : std::nullopt));
    conserved(game, 1, static_cast<std::size_t>(total));
}

TEST_CASE("Repairing a multiplayer board peels for everyone or awards the acting player") {
    int total = 0;
    ActionEvent expected{};
    SUBCASE("exactly enough pool tiles") {
        total = 6;
        expected = ActionEvent::peel;
    }
    SUBCASE("too few pool tiles") {
        total = 5;
        expected = ActionEvent::completed;
    }
    auto game = all_a_game(2, 2, total);
    success(game.place_tile(1, 'A', {0, 0}));
    success(game.place_tile(1, 'A', {0, 2}));
    const auto result = game.move_tiles(1, {{0, 2}}, {0, -1});
    success(result, expected);
    REQUIRE(result.validation.has_value());
    CHECK(result.validation->valid);
    CHECK(game.observe(0).rack.size() == (total == 6 ? 3U : 2U));
    CHECK(game.observe(1).rack.size() == (total == 6 ? 1U : 0U));
    CHECK(game.observe(0).pool_size == (total == 6 ? 0U : 1U));
    CHECK(game.status() == (total == 5 ? MatchStatus::finished : MatchStatus::active));
    CHECK(game.winner() == (total == 5 ? std::optional<PlayerId>{1} : std::nullopt));
    conserved(game, 2, static_cast<std::size_t>(total));
}

TEST_CASE("A move leaving an invalid board still commits without peeling") {
    auto game = all_a_game(2, 2, 6);
    success(game.place_tile(1, 'A', {0, 0}));
    success(game.place_tile(1, 'A', {0, 2}));
    const auto result = game.move_tiles(1, {{0, 2}}, {0, 1});
    success(result);
    REQUIRE(result.validation.has_value());
    CHECK_FALSE(result.validation->valid);
    CHECK(result.validation->connected_components.size() == 2);
    CHECK(game.observe(1).board.contains({0, 3}));
    CHECK_FALSE(game.observe(1).board.contains({0, 2}));
    CHECK(game.observe(1).rack.empty());
    CHECK(game.observe(0).rack.size() == 2);
    CHECK(game.observe(0).pool_size == 2);
    CHECK(game.status() == MatchStatus::active);
    CHECK_FALSE(game.winner().has_value());
    conserved(game, 2, 6);
}

TEST_CASE("Movement safely reaches coordinate limits without changing rack inventory") {
    for (const int edge : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        CAPTURE(edge);
        auto game = all_a_game(1, 2, 3);
        success(game.place_tile(0, 'A', {0, 0}));
        success(game.move_tiles(0, {{0, 0}}, {edge, edge}));
        CHECK(game.observe(0).board.contains({edge, edge}));
        CHECK(game.observe(0).rack.size() == 1);
        CHECK(game.observe(0).pool_size == 1);
        conserved(game, 1, 3);
    }
}

TEST_CASE("Dump returns a tile first and draws up to three without affecting opponents") {
    for (const int pool_before : {0, 1, 2, 5}) {
        CAPTURE(pool_before);
        auto game = all_a_game(2, 2, 4 + pool_before);
        success(game.place_tile(0, 'A', {5, 5}));
        success(game.place_tile(1, 'A', {0, 0}));
        const auto before = snapshot(game, 2);
        const auto result = game.dump(1, 'A');
        success(result);
        CHECK_FALSE(result.validation.has_value());
        const auto actor = game.observe(1);
        const auto opponent = game.observe(0);
        const auto drawn = static_cast<std::size_t>(std::min(pool_before + 1, 3));
        CHECK(actor.rack.size() == before[1].rack.size() - 1 + drawn);
        CHECK(actor.pool_size == static_cast<std::size_t>(pool_before + 1) - drawn);
        CHECK(actor.board.tiles() == before[1].board.tiles());
        CHECK(opponent.board.tiles() == before[0].board.tiles());
        CHECK(opponent.rack.counts() == before[0].rack.counts());
        CHECK(game.status() == MatchStatus::active);
        CHECK_FALSE(game.winner().has_value());
        conserved(game, 2, static_cast<std::size_t>(4 + pool_before));
    }
}

TEST_CASE("Empty-pool dump can immediately redraw the exact returned letter") {
    std::array<int, 26> distribution{};
    distribution[0] = distribution[1] = 1;
    Game game{GameConfig{GameMode::solo, 1, 2, distribution, 42},
              Lexicon{std::vector<std::string>{"AB"}}};
    const auto before = snapshot(game, 1);
    success(game.dump(0, 'B'));
    unchanged(game, before);
    CHECK(game.observe(0).rack.count('B') == 1);
    conserved(game, 1, 2);
}

TEST_CASE("Failed actions and observations preserve the subsequent random draw sequence") {
    std::array<int, 26> distribution{};
    distribution[0] = distribution[1] = distribution[2] = distribution[3] = 5;
    const GameConfig config{GameMode::multiplayer, 2, 3, distribution, 31415};
    Game game{config, Lexicon{std::vector<std::string>{"AA"}}};
    Game control{config, Lexicon{std::vector<std::string>{"AA"}}};
    failure(game.dump(0, 'Z'), ActionError::unavailable_rack_tile);
    failure(game.move_tiles(0, {}, {1, 0}), ActionError::invalid_move);
    failure(game.return_tiles(0, {{8, 8}}), ActionError::missing_board_tile);
    failure(game.place_tile(0, '?', {0, 0}), ActionError::invalid_letter);
    CHECK_THROWS_AS(static_cast<void>(game.observe(2)), std::out_of_range);
    unchanged(game, snapshot(control, 2));
    for (int turn = 0; turn < 5; ++turn) {
        const auto rack = control.observe(0).rack.counts();
        const auto found =
            std::find_if(rack.begin(), rack.end(), [](int count) { return count > 0; });
        REQUIRE(found != rack.end());
        const auto index = static_cast<int>(found - rack.begin());
        const char letter = static_cast<char>('A' + index);
        success(game.dump(0, letter));
        success(control.dump(0, letter));
        unchanged(game, snapshot(control, 2));
        conserved(game, 2, 20);
    }
}

TEST_CASE("Active observation snapshots own their board and rack copies") {
    auto game = all_a_game(2, 3, 8);
    success(game.place_tile(0, 'A', {-1, -1}));
    auto old_view = game.observe(0);
    const auto original_board = old_view.board.tiles();
    const auto original_rack = old_view.rack.counts();
    success(game.place_tile(0, 'A', {-1, 0}));
    CHECK(old_view.board.tiles() == original_board);
    CHECK(old_view.rack.counts() == original_rack);
    CHECK(old_view.pool_size == 2);
    const auto current = snapshot(game, 2);
    REQUIRE(old_view.board.place({99, 99}, 'Z'));
    REQUIRE(old_view.rack.add('Z'));
    old_view.opponents.clear();
    unchanged(game, current);
}

TEST_CASE("Opponent counts update during play but boards appear only after completion") {
    auto game = all_a_game(3, 2, 6);
    success(game.place_tile(0, 'A', {9, 9}));
    success(game.place_tile(1, 'A', {0, 0}));
    const auto before_finish = game.observe(2);
    REQUIRE(before_finish.opponents.size() == 2);
    for (const auto& opponent : before_finish.opponents) {
        CHECK(opponent.unplaced_tile_count == 1);
        CHECK(opponent.total_tile_count == 2);
        CHECK_FALSE(opponent.revealed_board.has_value());
    }
    success(game.place_tile(1, 'A', {0, 1}), ActionEvent::completed);
    CHECK(game.winner() == std::optional<PlayerId>{1});
    for (PlayerId id = 0; id < 3; ++id) {
        const auto view = game.observe(id);
        CHECK(view.status == MatchStatus::finished);
        CHECK(view.winner == std::optional<PlayerId>{1});
        REQUIRE(view.opponents.size() == 2);
        for (const auto& opponent : view.opponents) {
            REQUIRE(opponent.revealed_board.has_value());
            const auto actual = game.observe(opponent.player);
            CHECK(opponent.revealed_board->tiles() == actual.board.tiles());
            CHECK(opponent.unplaced_tile_count == actual.rack.size());
            CHECK(opponent.total_tile_count == actual.rack.size() + actual.board.size());
        }
    }
    // Old snapshots do not acquire newly revealed information retroactively.
    CHECK(before_finish.status == MatchStatus::active);
    CHECK_FALSE(before_finish.winner.has_value());
    for (const auto& opponent : before_finish.opponents) {
        CHECK_FALSE(opponent.revealed_board.has_value());
    }
    auto editable = game.observe(2);
    const auto finished = snapshot(game, 3);
    REQUIRE(editable.opponents[0].revealed_board->place({99, 99}, 'Z'));
    unchanged(game, finished);
    conserved(game, 3, 6);
}

TEST_CASE("Every action including zero-offset movement rejects a finished match") {
    auto game = all_a_game(1, 2, 2);
    success(game.place_tile(0, 'A', {0, 0}));
    success(game.place_tile(0, 'A', {0, 1}), ActionEvent::completed);
    const auto before = snapshot(game, 1);
    failure(game.place_tile(0, 'A', {1, 0}), ActionError::finished_match);
    failure(game.return_tiles(0, {{0, 0}}), ActionError::finished_match);
    failure(game.move_tiles(0, {{0, 0}}, {0, 0}), ActionError::finished_match);
    failure(game.dump(0, 'A'), ActionError::finished_match);
    unchanged(game, before);
    CHECK(game.status() == MatchStatus::finished);
    CHECK(game.winner() == std::optional<PlayerId>{0});
}
