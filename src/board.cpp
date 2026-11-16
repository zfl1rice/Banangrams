#include "bananagrams/board.hpp"

#include <stdexcept>
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
    if (!std::isalpha(static_cast<unsigned char>(l))) {
        return false;
    }
    this->BOARD[p] = l;
    return true;
}
std::optional<char> Board::remove(Position p) {
    auto val = this->BOARD.find(p);

    if (val == this->BOARD.end()) {
        return false;
    }

    this->BOARD.erase(p);
}
// TODO: Support atomic translation, including overlap and overflow checks.
bool Board::move_tiles(const std::vector<Position>& positions, Position offset) {
    int dx = offset.row;
    int dy = offset.col;

    std::unordered_set<Position> starting_positions(positions.begin(), positions.end());

    if (starting_positions.size() != positions.size()) {
        return false;
    }


    for (Position curr_pos: positions) {
        Position new_pos = Position{curr_pos.row + dx, curr_pos.col + dy};

        if (starting_positions.find(new_pos) == starting_positions.end() && contains(new_pos)) {
            return false;
        }

        std::optional<char> val_at_curr_pos = tile_at(curr_pos);

        if (!val_at_curr_pos.has_value()) {
            return false;
        }
    }

    for (Position curr_pos: positions) {
        Position new_pos = Position{curr_pos.row + dx, curr_pos.col + dy};

        remove(curr_pos);

        place(new_pos, *tile_at(curr_pos));
    }

    return true;

}
const std::map<Position, char>& Board::tiles() const {
    std::map<Position, char> res = this->BOARD;
    return res;
}

} // namespace bananagrams
