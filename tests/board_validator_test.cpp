#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/board_validator.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace bananagrams;
using Components = std::vector<std::vector<Position>>;

Board make_board(std::initializer_list<std::pair<Position, char>> tiles) {
    Board board;
    for (const auto& [position, letter] : tiles) {
        REQUIRE(board.place(position, letter));
    }
    return board;
}

// Component and traversal order are unspecified; membership and multiplicity matter.
void check_components(const ValidationResult& result, Components expected) {
    auto actual = result.connected_components;
    for (auto& component : actual) {
        std::sort(component.begin(), component.end());
    }
    for (auto& component : expected) {
        std::sort(component.begin(), component.end());
    }
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    CHECK(actual == expected);
}

// Occurrence order is unspecified, but positions within each word follow reading order.
void check_words(const ValidationResult& result, const std::vector<WordOccurrence>& expected) {
    REQUIRE(result.invalid_words.size() == expected.size());
    for (const auto& word : expected) {
        INFO("Expected invalid word: " << word.text);
        const auto matches =
            std::count_if(result.invalid_words.begin(), result.invalid_words.end(),
                          [&word](const WordOccurrence& actual) {
                              return actual.text == word.text && actual.positions == word.positions;
                          });
        CHECK(matches == 1);
    }
}
} // namespace

TEST_CASE("Validator rejects an empty board with an empty report") {
    const Board board;
    const Lexicon lexicon{std::vector<std::string>{"AT"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK_FALSE(result.has_at_least_two_tiles);
    check_components(result, {});
    check_words(result, {});
}

TEST_CASE("Validator reports a single tile as one invalid component") {
    const auto board = make_board({{{-3, 7}, 'A'}});
    const Lexicon lexicon{std::vector<std::string>{"AT"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK_FALSE(result.has_at_least_two_tiles);
    check_components(result, {{{-3, 7}}});
    check_words(result, {});
}

TEST_CASE("Validator accepts horizontal and vertical words at negative coordinates") {
    Board board;
    std::vector<Position> positions;
    SUBCASE("horizontal") {
        positions = {{-4, -3}, {-4, -2}, {-4, -1}};
    }
    SUBCASE("vertical") {
        positions = {{-3, -4}, {-2, -4}, {-1, -4}};
    }
    const std::string text = "CAT";
    for (std::size_t index = 0; index < positions.size(); ++index) {
        REQUIRE(board.place(positions[index], text[index]));
    }
    // Single-letter perpendicular runs and substrings are not dictionary entries.
    const Lexicon lexicon{std::vector<std::string>{"CAT"}};
    const auto result = validate_board(board, lexicon);
    CHECK(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {positions});
    check_words(result, {});
}

TEST_CASE("Validator reports maximal invalid words with positions in reading order") {
    Board board;
    std::vector<Position> positions;
    SUBCASE("horizontal") {
        positions = {{2, -1}, {2, 0}, {2, 1}};
    }
    SUBCASE("vertical") {
        positions = {{-1, 2}, {0, 2}, {1, 2}};
    }
    const std::string text = "CAT";
    for (std::size_t index = 0; index < positions.size(); ++index) {
        REQUIRE(board.place(positions[index], text[index]));
    }
    // A valid suffix must not make the longer, missing word valid.
    const Lexicon lexicon{std::vector<std::string>{"AT"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {positions});
    check_words(result, {{"CAT", positions}});
}

TEST_CASE("Validator finds both words sharing the same starting tile") {
    const auto board =
        make_board({{{0, 0}, 'C'}, {{0, 1}, 'A'}, {{0, 2}, 'T'}, {{1, 0}, 'A'}, {{2, 0}, 'R'}});
    std::vector<std::string> words;
    std::vector<WordOccurrence> expected;
    SUBCASE("both valid") {
        words = {"CAT", "CAR"};
    }
    SUBCASE("only vertical invalid") {
        words = {"CAT"};
        expected = {{"CAR", {{0, 0}, {1, 0}, {2, 0}}}};
    }
    SUBCASE("both invalid") {
        words = {"QI"};
        expected = {{"CAT", {{0, 0}, {0, 1}, {0, 2}}}, {"CAR", {{0, 0}, {1, 0}, {2, 0}}}};
    }
    const Lexicon lexicon{words};
    const auto result = validate_board(board, lexicon);
    CHECK(result.valid == expected.empty());
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {2, 0}}});
    check_words(result, expected);
}

TEST_CASE("Validator accepts words crossing at an interior tile") {
    const auto board =
        make_board({{{0, 0}, 'C'}, {{0, 1}, 'A'}, {{0, 2}, 'T'}, {{1, 1}, 'R'}, {{2, 1}, 'E'}});
    const Lexicon lexicon{std::vector<std::string>{"CAT", "ARE"}};
    const auto result = validate_board(board, lexicon);
    CHECK(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{0, 0}, {0, 1}, {0, 2}, {1, 1}, {2, 1}}});
    check_words(result, {});
}

TEST_CASE("Validator reports every disconnected component and its invalid words") {
    const auto board =
        make_board({{{0, 0}, 'C'}, {{0, 1}, 'A'}, {{0, 2}, 'T'}, {{5, 5}, 'Q'}, {{6, 5}, 'I'}});
    std::vector<std::string> words;
    std::vector<WordOccurrence> expected;
    SUBCASE("all words valid but board disconnected") {
        words = {"CAT", "QI"};
    }
    SUBCASE("invalid word in first component") {
        words = {"QI"};
        expected = {{"CAT", {{0, 0}, {0, 1}, {0, 2}}}};
    }
    SUBCASE("invalid word in later component") {
        words = {"CAT"};
        expected = {{"QI", {{5, 5}, {6, 5}}}};
    }
    const Lexicon lexicon{words};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{0, 0}, {0, 1}, {0, 2}}, {{5, 5}, {6, 5}}});
    check_words(result, expected);
}

TEST_CASE("Validator treats diagonal neighbors as separate single-tile components") {
    const auto board = make_board({{{-1, -1}, 'A'}, {{0, 0}, 'T'}, {{1, 1}, 'Q'}});
    const Lexicon lexicon{std::vector<std::string>{"AT", "QI"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{-1, -1}}, {{0, 0}}, {{1, 1}}});
    check_words(result, {});
}

TEST_CASE("Validator does not join words across a gap or merge repeated occurrences") {
    const auto board = make_board({{{0, 0}, 'A'}, {{0, 1}, 'T'}, {{0, 3}, 'A'}, {{0, 4}, 'T'}});
    const Lexicon lexicon{std::vector<std::string>{"ATAT"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{0, 0}, {0, 1}}, {{0, 3}, {0, 4}}});
    check_words(result, {{"AT", {{0, 0}, {0, 1}}}, {"AT", {{0, 3}, {0, 4}}}});
}

TEST_CASE("Validator handles a connectivity cycle and all four maximal runs") {
    const auto board = make_board({{{0, 0}, 'A'}, {{0, 1}, 'T'}, {{1, 0}, 'T'}, {{1, 1}, 'O'}});
    std::vector<std::string> words;
    std::vector<WordOccurrence> expected;
    SUBCASE("all runs valid") {
        words = {"AT", "TO"};
    }
    SUBCASE("all runs invalid") {
        words = {"QI"};
        expected = {{"AT", {{0, 0}, {0, 1}}},
                    {"AT", {{0, 0}, {1, 0}}},
                    {"TO", {{0, 1}, {1, 1}}},
                    {"TO", {{1, 0}, {1, 1}}}};
    }
    const Lexicon lexicon{words};
    const auto result = validate_board(board, lexicon);
    CHECK(result.valid == expected.empty());
    CHECK(result.has_at_least_two_tiles);
    check_components(result, {{{0, 0}, {0, 1}, {1, 0}, {1, 1}}});
    check_words(result, expected);
}

TEST_CASE("Validator handles word starts and ends at every coordinate boundary") {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    const std::array<std::array<Position, 2>, 8> cases{{{{{low, low}, {low, low + 1}}},
                                                        {{{high, low}, {high, low + 1}}},
                                                        {{{low, high - 1}, {low, high}}},
                                                        {{{high, high - 1}, {high, high}}},
                                                        {{{low, low}, {low + 1, low}}},
                                                        {{{low, high}, {low + 1, high}}},
                                                        {{{high - 1, low}, {high, low}}},
                                                        {{{high - 1, high}, {high, high}}}}};
    for (std::size_t index = 0; index < cases.size(); ++index) {
        CAPTURE(index);
        const auto& positions = cases[index];
        const auto board = make_board({{positions[0], 'A'}, {positions[1], 'T'}});
        for (const bool in_lexicon : {false, true}) {
            CAPTURE(in_lexicon);
            const Lexicon lexicon{std::vector<std::string>{in_lexicon ? "AT" : "QI"}};
            const auto result = validate_board(board, lexicon);
            CHECK(result.valid == in_lexicon);
            CHECK(result.has_at_least_two_tiles);
            check_components(result, {{positions[0], positions[1]}});
            if (in_lexicon) {
                check_words(result, {});
            } else {
                check_words(result, {{"AT", {positions[0], positions[1]}}});
            }
        }
    }
}

TEST_CASE("Validator does not connect opposite integer limits by wrapping") {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    Board board;
    Components expected;
    SUBCASE("columns") {
        board = make_board({{{0, low}, 'A'}, {{0, high}, 'T'}});
        expected = {{{0, low}}, {{0, high}}};
    }
    SUBCASE("rows") {
        board = make_board({{{low, 0}, 'A'}, {{high, 0}, 'T'}});
        expected = {{{low, 0}}, {{high, 0}}};
    }
    const Lexicon lexicon{std::vector<std::string>{"AT", "TA"}};
    const auto result = validate_board(board, lexicon);
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    check_components(result, expected);
    check_words(result, {});
}

TEST_CASE("Validation preserves inputs and reports remain independent of later board edits") {
    auto board = make_board({{{0, 0}, 'C'}, {{0, 1}, 'A'}, {{0, 2}, 'T'}});
    const Lexicon lexicon{std::vector<std::string>{"QI"}};
    const auto before = board.tiles();
    const auto lexicon_size = lexicon.size();
    const auto result = validate_board(board, lexicon);
    CHECK(board.tiles() == before);
    CHECK(lexicon.size() == lexicon_size);
    CHECK(lexicon.contains("QI"));
    CHECK_FALSE(lexicon.contains("CAT"));
    CHECK_FALSE(result.valid);
    CHECK(result.has_at_least_two_tiles);
    REQUIRE(board.remove(Position{0, 0}).has_value());
    check_components(result, {{{0, 0}, {0, 1}, {0, 2}}});
    check_words(result, {{"CAT", {{0, 0}, {0, 1}, {0, 2}}}});
}
