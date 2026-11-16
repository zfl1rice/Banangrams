#pragma once

#include "bananagrams/position.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

namespace bananagrams {

// Sparse occupied squares only; words and inventory are checked elsewhere.
// All non-default methods currently throw std::logic_error until implemented.
class Board {
  private:
    std::map<Position, char> BOARD;
    int NUM_OCCUPIED;
  public:
    Board() = default;

    // Report whether there are no occupied squares.
    [[nodiscard]] bool empty() const;
    // Return the number of occupied squares.
    [[nodiscard]] std::size_t size() const;
    // Report whether position is occupied.
    [[nodiscard]] bool contains(Position position) const;
    // Return the letter, or nullopt for an empty square.
    [[nodiscard]] std::optional<char> tile_at(Position position) const;
    // Place A-Z; return false without changes for invalid letters or occupancy.
    // Disconnected arrangements and temporary invalid words are allowed.
    [[nodiscard]] bool place(Position position, char letter);
    // Remove and return a letter; a missing square returns nullopt unchanged.
    [[nodiscard]] std::optional<char> remove(Position position);
    // Translate a selection atomically: failure must leave the entire board unchanged.
    // Reject empty/duplicate/missing sources, overflow, and unselected collisions.
    // Selected source squares may become destinations; valid zero offsets may succeed.
    [[nodiscard]] bool move_tiles(const std::vector<Position>& positions, Position offset);
    // Read-only access prevents callers from bypassing placement/movement rules.
    [[nodiscard]] const std::map<Position, char>& tiles() const;

  private:
    std::map<Position, char> tiles_;
};

} // namespace bananagrams
