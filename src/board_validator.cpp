#include "bananagrams/board_validator.hpp"

#include <array>
#include <climits>
#include <map>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bananagrams {
namespace {

enum class Direction { horizontal, vertical };

struct WordStart {
    Position position;
    Direction direction;
};

constexpr std::array<Position, 4> neighbor_offsets{{
    {-1, 0}, // Up
    {1, 0},  // Down
    {0, -1}, // Left
    {0, 1}   // Right
}};

constexpr std::array<Direction, 2> word_directions{Direction::horizontal, Direction::vertical};

// Return nullopt if the sum cannot be represented as an int.
std::optional<int> safe_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) {
        return std::nullopt;
    }
    if (b < 0 && a < INT_MIN - b) {
        return std::nullopt;
    }
    return a + b;
}

// Move a coordinate without overflowing either component.
std::optional<Position> offset_position(Position position, Position offset) {
    const auto row = safe_add(position.row, offset.row);
    const auto col = safe_add(position.col, offset.col);

    if (!row.has_value() || !col.has_value()) {
        return std::nullopt;
    }

    return Position{*row, *col};
}

// Words are read rightward or downward.
Position word_step(Direction direction) {
    return direction == Direction::horizontal ? Position{0, 1} : Position{1, 0};
}

// The current tile starts a word if there is no preceding tile and
// there is a following tile. This excludes single-letter runs.
bool is_word_start(const Board& board, Position position, Direction direction) {
    const Position step = word_step(direction);
    const auto previous = offset_position(position, {-step.row, -step.col});

    if (previous.has_value() && board.contains(*previous)) {
        return false;
    }

    const auto next = offset_position(position, step);
    return next.has_value() && board.contains(*next);
}

// Collect the maximal run and its positions in reading order.
// Position is passed by value, so advancing it does not change the caller's start.
WordOccurrence read_word(const Board& board, Position position, Direction direction) {
    WordOccurrence word;
    const Position step = word_step(direction);

    while (const auto letter = board.tile_at(position)) {
        word.text += *letter;
        word.positions.push_back(position);

        const auto next = offset_position(position, step);
        if (!next.has_value()) {
            break;
        }

        position = *next;
    }

    return word;
}

} // namespace

ValidationResult validate_board(const Board& board, const Lexicon& lexicon) {
    ValidationResult result{};
    result.has_at_least_two_tiles = board.size() >= 2;

    // Removing visited tiles from this copy leaves the original board unchanged.
    std::map<Position, char> remaining_tiles = board.tiles();
    std::vector<WordStart> word_starts;

    while (!remaining_tiles.empty()) {
        const Position start = remaining_tiles.begin()->first;
        std::vector<Position> component;
        std::unordered_set<Position> seen{start};
        std::vector<Position> stack{start};

        while (!stack.empty()) {
            const Position current = stack.back();
            stack.pop_back();

            component.push_back(current);
            remaining_tiles.erase(current);

            // Word starts depend on surrounding tiles, not DFS visitation order.
            for (Direction direction : word_directions) {
                if (is_word_start(board, current, direction)) {
                    word_starts.push_back(WordStart{current, direction});
                }
            }

            for (Position offset : neighbor_offsets) {
                const auto neighbor = offset_position(current, offset);

                // Coordinates beyond the integer limits represent absent neighbors.
                if (!neighbor.has_value() || !board.contains(*neighbor)) {
                    continue;
                }

                // Mark on insertion so each tile enters the stack only once.
                if (seen.insert(*neighbor).second) {
                    stack.push_back(*neighbor);
                }
            }
        }

        result.connected_components.push_back(std::move(component));
    }

    for (const WordStart& start : word_starts) {
        WordOccurrence word = read_word(board, start.position, start.direction);

        if (!lexicon.contains(word.text)) {
            result.invalid_words.push_back(std::move(word));
        }
    }

    result.valid = result.has_at_least_two_tiles && result.connected_components.size() == 1 &&
                   result.invalid_words.empty();

    return result;
}

} // namespace bananagrams
