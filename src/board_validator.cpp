#include "bananagrams/board_validator.hpp"

#include <climits>
#include <optional>
#include <stdexcept>
#include <array>
#include <unordered_set>
#include <tuple>

namespace bananagrams {
namespace {

// Return the sum, or nullopt if adding would exceed the range of int.
std::optional<int> safe_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) {
        return std::nullopt;
    }
    if (b < 0 && a < INT_MIN - b) {
        return std::nullopt;
    }
    return a + b;
}

} // namespace

// TODO: Report maximal words and connected components with their tile positions.
ValidationResult validate_board(const Board& Board, const Lexicon& lexicon) {
    std::map<Position, char> curr_state = Board.tiles();
    ValidationResult res{};
    res.valid = false;

    if (curr_state.size() < 2) {
        res.has_at_least_two_tiles = false;
    } else {
        res.has_at_least_two_tiles = true;
    }

    constexpr std::array<std::pair<int, int>, 4> dirs = {{
        {-1,  0}, // Up
        { 1,  0}, // Down
        { 0, -1}, // Left
        { 0,  1}  // Right
    }};

    std::vector<std::tuple<Position, int>> word_beginnings{};

    while (curr_state.size() > 0) {
        
        Position starting_point = curr_state.begin()->first;
        std::vector<Position> component;
        std::unordered_set<Position> seen;

        seen.emplace(starting_point);

        std::vector<Position> stack{};
        stack.push_back(starting_point);

        while (stack.size() > 0) {
            Position curr_pos = stack.back();
            component.push_back(curr_pos);
            curr_state.erase(curr_pos);
            stack.pop_back();

            for (const auto& [dx, dy] : dirs) {
                const auto new_row = safe_add(curr_pos.row, dx);
                const auto new_col = safe_add(curr_pos.col, dy);

                const bool neighbor_exists = new_row.has_value() && new_col.has_value() &&
                    Board.contains(Position{*new_row, *new_col});

                if (neighbor_exists) {
                    Position new_pos{*new_row, *new_col};
                    if (seen.count(new_pos) == 0) {
                        stack.push_back(new_pos);
                        seen.emplace(new_pos);
                    }
                } else {
                    if (dx == -1 && dy == 0) {
                        const auto below_row = safe_add(curr_pos.row, 1);
                        if (below_row.has_value() &&
                            Board.contains(Position{*below_row, curr_pos.col})) {
                            word_beginnings.push_back(std::make_tuple(curr_pos, 0));
                        }
                    }
                    if (dx == 0 && dy == -1) {
                        const auto right_col = safe_add(curr_pos.col, 1);
                        if (right_col.has_value() &&
                            Board.contains(Position{curr_pos.row, *right_col})) {
                            word_beginnings.push_back(std::make_tuple(curr_pos, 1));
                        }
                    }
                }
            }
        }
        res.connected_components.push_back(component);
    }

    for (auto& [starting_pos, direction] : word_beginnings) {
        std::string curr_word;
        std::vector<Position> curr_word_locs;
        if (direction == 1) {
            while (Board.contains(starting_pos)) {
                curr_word_locs.push_back(starting_pos);
                curr_word += *Board.tile_at(starting_pos);
                const auto r_col = safe_add(starting_pos.col, 1);
                if (r_col.has_value()) {
                    starting_pos = Position{starting_pos.row, *r_col};
                } else {
                    break;
                }
            }
        } else {
            while (Board.contains(starting_pos)) {
                curr_word_locs.push_back(starting_pos);
                curr_word += *Board.tile_at(starting_pos);
                const auto b_row = safe_add(starting_pos.row, 1);
                if (b_row.has_value()) {
                    starting_pos = Position{*b_row, starting_pos.col};
                } else {
                    break;
                }
            }
        }
        if (!lexicon.contains(curr_word)) {
            res.invalid_words.push_back(WordOccurrence{curr_word, curr_word_locs});
            res.valid = false;
        }

    }

    if (res.connected_components.size() == 1 && res.invalid_words.size() == 0 && res.has_at_least_two_tiles) {
        res.valid = true;
    }
    return res;

}

} // namespace bananagrams
