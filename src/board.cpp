#include "bananagrams/board.hpp"

#include <stdexcept>
#include <climits>
#include <cctype>
#include <unordered_set>

namespace bananagrams {

// TODO: Implement these basic board exercises before group movement.
bool Board::empty() const {
    return this->NUM_OCCUPIED == 0;
}
std::size_t Board::size() const {
    return this->NUM_OCCUPIED;
}
bool Board::contains(Position p) const {
    auto val = this->BOARD.find(p);
    if (val == this->BOARD.end()) {
        return false;
    }
    return true;
}
std::optional<char> Board::tile_at(Position p) const {
    auto val = this->BOARD.find(p);

    if (val == this->BOARD.end()) {
        return std::nullopt;
    }
    return static_cast<unsigned char>(val->second);
}
bool Board::place(Position p, char l) {
    if (!(l >= 'A' && l <= 'Z')) {
        return false;
    }
    if (!this->BOARD.contains(p)) {
        this->NUM_OCCUPIED ++;
    }
    this->BOARD[p] = l;
    return true;
}
std::optional<char> Board::remove(Position p) {
    auto val = this->BOARD.find(p);

    if (val == this->BOARD.end()) {
        return std::nullopt;
    }
    this->NUM_OCCUPIED --;
    char res = val->second;
    this->BOARD.erase(p);
    return res;
}

std::optional<int> Board::safe_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) {
        return std::nullopt; // Positive overflow
    }
    if (b < 0 && a < INT_MIN - b) {
        return std::nullopt; // Negative underflow
    }
    return a + b;
}

// TODO: Support atomic translation, including overlap and overflow checks.
bool Board::move_tiles(const std::vector<Position>& positions, Position offset) {
    int dx = offset.row;
    int dy = offset.col;

    std::unordered_set<Position> starting_positions(positions.begin(), positions.end());

    if (starting_positions.size() != positions.size() || positions.size() == 0) {
        return false;
    }

    std::map<Position, char> valid_moves{};

    for (Position curr_pos: positions) {
        const auto new_row = safe_add(curr_pos.row, dx);
        const auto new_col = safe_add(curr_pos.col, dy);
        if (!new_row.has_value() || !new_col.has_value()) {
            return false;
        }
        Position new_pos = Position{*new_row, *new_col};

        if (starting_positions.find(new_pos) == starting_positions.end() && contains(new_pos)) {
            return false;
        }

        std::optional<char> val_at_curr_pos = tile_at(curr_pos);

        if (!val_at_curr_pos.has_value()) {
            return false;
        }

        valid_moves[new_pos] = *val_at_curr_pos;
    }

    for (Position curr_pos: positions) {
        // Both coordinates were checked for every source before any mutation.
        Position new_pos = Position{*safe_add(curr_pos.row, dx), *safe_add(curr_pos.col, dy)};

        if (!valid_moves.contains(curr_pos)){
            remove(curr_pos);
        }

        place(new_pos, valid_moves[new_pos]);
    }

    return true;

}
const std::map<Position, char>& Board::tiles() const {
    return this->BOARD;
}

} // namespace bananagrams
