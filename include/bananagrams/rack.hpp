#pragma once

#include <array>
#include <cstddef>

namespace bananagrams {

// Unplaced letter counts; display order and visual shuffling belong to the UI.
// Callers keep the total tile count within the game's finite tile supply and
// the range of int. Rack does not check for overflow.
class Rack {
  private:
    std::array<int, 26> CURR_COUNTS = {};
    int NUM_TILES = 0;

  public:
    Rack() = default;

    // Report whether no tiles remain.
    [[nodiscard]] bool empty() const;
    // Return the total tile count, including duplicates.
    [[nodiscard]] std::size_t size() const;
    // Return the count for A-Z; unsupported letters return zero.
    [[nodiscard]] int count(char letter) const;
    // Add one uppercase A-Z tile; invalid letters return false without changes.
    [[nodiscard]] bool add(char letter);
    // Remove one A-Z tile; invalid/unavailable letters return false unchanged.
    [[nodiscard]] bool remove(char letter);
    // Read-only counts indexed by A through Z preserve the rack's invariants.
    [[nodiscard]] const std::array<int, 26>& counts() const;
};

} // namespace bananagrams
