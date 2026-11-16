#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>
#include <array>

namespace bananagrams {

// Shared letters with a seeded RNG; small custom pools support future tests.
// A seed alone does not guarantee replay across standard-library implementations.
// All methods, including construction, currently throw std::logic_error.
class TilePool {
  private:
      int SEED;
      std::vector<char> LETTERS;

  public:
    // Accept A-Z letters and a seed; malformed letters will throw invalid_argument.
    TilePool(std::vector<char> initial_letters, std::uint32_t seed);

    // Report whether the pool has no tiles.
    [[nodiscard]] bool empty() const;
    // Return the remaining number of tiles.
    [[nodiscard]] std::size_t size() const;
    // Sample the current pool, including returns; nullopt means empty.
    [[nodiscard]] std::optional<char> draw();
    // Draw at most count tiles; zero or an empty pool returns an empty vector.
    [[nodiscard]] std::vector<char> draw_up_to(std::size_t count);
    // Return A-Z to the pool; unsupported letters return false unchanged.
    [[nodiscard]] bool return_tile(char letter);

  private:
    std::vector<char> letters_;
    std::mt19937 rng_;
};

} // namespace bananagrams
