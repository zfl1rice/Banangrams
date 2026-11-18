#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/board.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Print sparse coordinates rather than a grid, including very large coordinates.
// This also makes direct map comparisons readable instead of displaying {?}.
namespace doctest {

template <>
struct StringMaker<std::map<bananagrams::Position, char>> {
    static String convert(const std::map<bananagrams::Position, char>& tiles) {
        if (tiles.empty()) {
            return "{ empty }";
        }
        std::ostringstream out;
        out << "{";
        for (const auto& [position, letter] : tiles) {
            out << "\n  (" << position.row << ", " << position.col << ") = ";
            if (letter >= 'A' && letter <= 'Z') {
                out << '\'' << letter << '\'';
            } else {
                out << "byte(" << static_cast<unsigned int>(static_cast<unsigned char>(letter)) << ')';
            }
        }
        out << "\n}";
        return String(out.str().c_str());
    }
};

} // namespace doctest

namespace {

using bananagrams::Board;
using bananagrams::Position;
using Tiles = std::map<Position, char>;
using TilesView = decltype(std::declval<const Board&>().tiles());
static_assert(std::is_same_v<TilesView, const Tiles&>);
static_assert(!std::is_assignable_v<decltype((std::declval<TilesView>().begin()->second)), char>);

void check_absent(const Board& board, Position position) {
    INFO("row := " << position.row << ", col := " << position.col);
    CHECK_FALSE(board.contains(position));
    CHECK(board.tile_at(position) == std::nullopt);
}

void check_board(const Board& board, const Tiles& expected) {
    INFO("Expected board: " << doctest::toString(expected));
    INFO("Actual board: " << doctest::toString(board.tiles()));
    CHECK(board.tiles() == expected);
    CHECK(board.size() == expected.size());
    CHECK(board.empty() == expected.empty());
    CHECK(board.size() == board.tiles().size());
    CHECK(board.empty() == (board.size() == 0));
    for (const auto& [position, letter] : expected) {
        INFO("row := " << position.row << ", col := " << position.col);
        INFO("letter := " << std::string(1, letter));
        CHECK(board.contains(position));
        CHECK(board.tile_at(position) == std::optional<char>{letter});
    }
}

Board make_board(const Tiles& tiles) {
    Board board{};
    for (const auto& [position, letter] : tiles) {
        REQUIRE(board.place(position, letter));
    }
    return board;
}

void check_rejected_move(Board& board, const std::vector<Position>& sources, Position offset) {
    const auto before = board.tiles();
    const auto size_before = board.size();
    const auto empty_before = board.empty();
    CHECK_FALSE(board.move_tiles(sources, offset));
    CHECK(board.tiles() == before);
    CHECK(board.size() == size_before);
    CHECK(board.empty() == empty_before);
    check_board(board, before);
}

// Only used for small, representable offsets; overflow tests have their own oracle.
void check_translation(const Tiles& initial, const std::vector<Position>& sources, Position offset) {
    INFO("row offset := " << offset.row << ", col offset := " << offset.col);
    auto board = make_board(initial);
    Tiles expected = initial;
    for (const auto& source : sources) {
        expected.erase(source);
    }
    for (const auto& source : sources) {
        expected[{source.row + offset.row, source.col + offset.col}] = initial.at(source);
    }
    CHECK(board.move_tiles(sources, offset));
    check_board(board, expected);
    CHECK(board.tiles().size() == initial.size());
    for (const auto& source : sources) {
        if (!expected.contains(source)) {
            check_absent(board, source);
        }
    }
}

} // namespace

TEST_CASE("Board starts empty with either initialization style") {
    SUBCASE("Brace initialization") {
        Board board{};
        check_board(board, {});
        check_absent(board, {0, 0});
        check_absent(board, {-1, 2});
    }
    SUBCASE("Default initialization") {
        Board board;
        check_board(board, {});
        check_absent(board, {0, 0});
    }
}

TEST_CASE("Board places unique and repeated letters at signed positions") {
    Board board{};
    Tiles expected;
    // Include every uppercase letter, negative coordinates, and disconnected tiles.
    for (int i = 0; i < 26; ++i) {
        const auto letter = static_cast<char>('A' + i);
        const Position position{i - 13, 2 * i - 25};
        INFO("letter := " << std::string(1, letter));
        REQUIRE(board.place(position, letter));
        expected[position] = letter;
        check_board(board, expected);
    }
    REQUIRE(board.place({0, 0}, 'A'));
    expected[{0, 0}] = 'A';
    REQUIRE(board.place({100, -100}, 'A'));
    expected[{100, -100}] = 'A';
    check_board(board, expected);
}

TEST_CASE("Board placement overwrites occupied squares without increasing size") {
    auto board = make_board({{{0, 0}, 'A'}, {{-2, 4}, 'C'}});
    REQUIRE(board.place({0, 0}, 'B'));
    check_board(board, {{{0, 0}, 'B'}, {{-2, 4}, 'C'}});
    REQUIRE(board.place({0, 0}, 'B'));
    check_board(board, {{{0, 0}, 'B'}, {{-2, 4}, 'C'}});
    REQUIRE(board.place({0, 0}, 'Z'));
    check_board(board, {{{0, 0}, 'Z'}, {{-2, 4}, 'C'}});
}

TEST_CASE("Board rejects invalid placement without replacing tiles") {
    for (unsigned int byte = 0; byte <= std::numeric_limits<unsigned char>::max(); ++byte) {
        if (byte >= 'A' && byte <= 'Z') {
            continue;
        }
        CAPTURE(byte);
        Board empty{};
        CHECK_FALSE(empty.place({0, 0}, static_cast<char>(byte)));
        check_board(empty, {});
        check_absent(empty, {0, 0});

        const Tiles expected{{{0, 0}, 'A'}, {{-1, 2}, 'Z'}};
        auto populated = make_board(expected);
        CHECK_FALSE(populated.place({0, 0}, static_cast<char>(byte)));
        check_board(populated, expected);
        CHECK_FALSE(populated.place({4, -3}, static_cast<char>(byte)));
        check_board(populated, expected);
        check_absent(populated, {4, -3});
    }
}

TEST_CASE("Board looks up exact positions with unique and repeated letters") {
    const Tiles expected{{{0, 0}, 'A'}, {{0, 1}, 'A'}, {{-3, 5}, 'B'}, {{5, -3}, 'Z'}};
    const auto board = make_board(expected);
    check_board(board, expected);
    for (Position position : {Position{1, 0}, Position{-3, 4}, Position{3, -5}, Position{9, 9}}) {
        check_absent(board, position);
    }
    check_board(board, expected);
}

TEST_CASE("Board removes and returns tiles only once") {
    Board board{};
    CHECK(board.remove({2, -3}) == std::nullopt);
    check_board(board, {});
    REQUIRE(board.place({2, -3}, 'Q'));
    REQUIRE(board.place({0, 0}, 'A'));
    CHECK(board.remove({2, -3}) == std::optional<char>{'Q'});
    check_absent(board, {2, -3});
    check_board(board, {{{0, 0}, 'A'}});
    CHECK(board.remove({2, -3}) == std::nullopt);
    check_board(board, {{{0, 0}, 'A'}});
    CHECK(board.remove({0, 0}) == std::optional<char>{'A'});
    check_board(board, {});
    CHECK(board.remove({0, 0}) == std::nullopt);
    check_board(board, {});

    // A vacated square can be used again, including after an overwrite.
    REQUIRE(board.place({2, -3}, 'A'));
    REQUIRE(board.place({2, -3}, 'Z'));
    CHECK(board.remove({2, -3}) == std::optional<char>{'Z'});
    check_board(board, {});
}

TEST_CASE("Board tiles provides a stable read only view") {
    Board board{};
    const Board& read_only = board;
    const auto& view = read_only.tiles();
    CHECK(view.empty());
    REQUIRE(board.place({-1, 2}, 'A'));
    const Tiles first{{{-1, 2}, 'A'}};
    CHECK(view == first);
    CHECK(&read_only.tiles() == &view);
    check_board(read_only, first);
    CHECK(view == first);
    REQUIRE(board.place({-1, 2}, 'B'));
    const Tiles replaced{{{-1, 2}, 'B'}};
    CHECK(view == replaced);
    CHECK(board.remove({-1, 2}) == std::optional<char>{'B'});
    CHECK(view.empty());
    check_board(read_only, {});
}

TEST_CASE("Board moves a single tile in every direction") {
    const Tiles initial{{{1, -1}, 'Q'}, {{500, 500}, 'Z'}};
    for (Position offset : {Position{1, 0}, Position{-1, 0}, Position{0, 1},
                            Position{0, -1}, Position{-8, 12}, Position{20, -30}}) {
        check_translation(initial, {{1, -1}}, offset);
    }
}

TEST_CASE("Board translates groups to new positions") {
    const Tiles initial{{{0, 0}, 'A'}, {{0, 3}, 'B'}, {{-4, 0}, 'A'}, {{500, 500}, 'Z'}};
    for (Position offset : {Position{1, 0}, Position{-1, 0}, Position{0, 1},
                            Position{0, -1}, Position{20, -30}}) {
        check_translation(initial, {{0, 0}, {0, 3}, {-4, 0}}, offset);
    }
}

TEST_CASE("Board moves overlapping selections independently of source order") {
    for (bool vertical : {false, true}) {
        for (int direction : {-1, 1}) {
            for (bool reverse : {false, true}) {
                CAPTURE(vertical);
                CAPTURE(direction);
                CAPTURE(reverse);
                std::vector<Position> sources;
                Tiles initial{{{50, 50}, 'Z'}};
                for (int i = 0; i < 3; ++i) {
                    const Position source = vertical ? Position{i, 0} : Position{0, i};
                    sources.push_back(source);
                    initial[source] = static_cast<char>('A' + i);
                }
                if (reverse) {
                    std::reverse(sources.begin(), sources.end());
                }
                check_translation(initial, sources, vertical ? Position{direction, 0} : Position{0, direction});
            }
        }
    }
}

TEST_CASE("Board rejects moves colliding with unselected tiles atomically") {
    for (char blocker : {'B', 'X'}) {
        INFO("blocker := " << std::string(1, blocker));
        auto board = make_board({{{0, 0}, 'A'}, {{0, 1}, 'B'}, {{0, 2}, blocker}, {{9, 9}, 'Z'}});
        check_rejected_move(board, {{0, 1}}, {0, 1});
        // The first destination is selected; the second hits the unselected tile.
        check_rejected_move(board, {{0, 0}, {0, 1}}, {0, 1});
        check_rejected_move(board, {{0, 1}, {0, 0}}, {0, 1});
    }
}

TEST_CASE("Board rejects empty duplicate and missing sources atomically") {
    Board empty{};
    check_rejected_move(empty, {}, {1, 0});
    check_rejected_move(empty, {{0, 0}}, {1, 0});
    auto board = make_board({{{0, 0}, 'A'}, {{0, 1}, 'B'}, {{9, 9}, 'Z'}});
    const std::vector<std::vector<Position>> invalid{
        {}, {{0, 0}, {0, 0}}, {{0, 0}, {0, 1}, {0, 0}},
        {{3, 3}}, {{3, 3}, {0, 0}}, {{0, 0}, {3, 3}, {0, 1}}, {{0, 0}, {0, 1}, {3, 3}}};
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        CAPTURE(i);
        check_rejected_move(board, invalid[i], {1, 0});
        check_rejected_move(board, invalid[i], {0, 0});
    }
}

TEST_CASE("Board zero offset leaves valid selections unchanged") {
    const Tiles initial{{{0, 0}, 'A'}, {{0, 1}, 'B'}, {{9, 9}, 'Z'}};
    auto board = make_board(initial);
    // The contract permits success for a valid zero offset, but does not require it.
    static_cast<void>(board.move_tiles({{0, 0}}, {0, 0}));
    check_board(board, initial);
    static_cast<void>(board.move_tiles({{0, 0}, {0, 1}}, {0, 0}));
    check_board(board, initial);
}

TEST_CASE("Board rejects coordinate overflow without changing any tiles") {
    constexpr int high = std::numeric_limits<int>::max();
    constexpr int low = std::numeric_limits<int>::min();
    const std::vector<std::pair<Position, Position>> cases{
        {{high, 5}, {1, 0}}, {{low, 5}, {-1, 0}},
        {{5, high}, {0, 1}}, {{5, low}, {0, -1}}};
    for (const auto& [source, offset] : cases) {
        INFO("source row := " << source.row << ", source col := " << source.col);
        auto board = make_board({{source, 'A'}, {{0, 0}, 'B'}, {{9, 9}, 'Z'}});
        check_rejected_move(board, {source}, offset);
        // A valid source before the overflowing source must remain unchanged too.
        check_rejected_move(board, {{0, 0}, source}, offset);
    }
}

TEST_CASE("Board accepts representable moves at coordinate limits") {
    constexpr int high = std::numeric_limits<int>::max();
    constexpr int low = std::numeric_limits<int>::min();
    SUBCASE("Destination reaches maximum row and minimum column") {
        auto board = make_board({{{high - 1, low + 1}, 'A'}});
        CHECK(board.move_tiles({{high - 1, low + 1}}, {1, -1}));
        check_board(board, {{{high, low}, 'A'}});
        check_absent(board, {high - 1, low + 1});
    }
    SUBCASE("Destination reaches minimum row and maximum column") {
        auto board = make_board({{{low + 1, high - 1}, 'B'}});
        CHECK(board.move_tiles({{low + 1, high - 1}}, {-1, 1}));
        check_board(board, {{{low, high}, 'B'}});
        check_absent(board, {low + 1, high - 1});
    }
}
