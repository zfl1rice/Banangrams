#pragma once

#include "bananagrams/action_result.hpp"
#include "bananagrams/game_config.hpp"
#include "bananagrams/lexicon.hpp"
#include "bananagrams/observation.hpp"
#include "bananagrams/player_state.hpp"
#include "bananagrams/tile_pool.hpp"

#include <optional>
#include <vector>

namespace bananagrams {

// Owns inventory and rules; actions are sequential and the engine never sleeps.
// All methods currently throw logic_error (construction also reaches TODO dependencies).
// Future expected action failures return codes and leave all state unchanged.
class Game {
  public:
    // Validate config (invalid_argument on failure), build the pool, and deal racks.
    Game(GameConfig config, Lexicon lexicon);

    // Transfer an available A-Z rack tile to a free square; failure changes nothing.
    [[nodiscard]] ActionResult place_tile(PlayerId player, char letter, Position position);
    // Return selected board tiles to the rack atomically; reject empty/duplicate/missing sources.
    [[nodiscard]] ActionResult return_tiles(PlayerId player,
                                            const std::vector<Position>& positions);
    // Move a valid selection atomically using Board's movement contract.
    [[nodiscard]] ActionResult move_tiles(PlayerId player, const std::vector<Position>& positions,
                                          Position offset);
    // Return one rack tile first, then draw up to 3, possibly that same tile again.
    // Allowed with an empty pool; opponents draw nothing. Failures change nothing.
    [[nodiscard]] ActionResult dump(PlayerId player, char letter);
    // Return an independent privacy-filtered snapshot; invalid IDs throw out_of_range.
    [[nodiscard]] PlayerObservation observe(PlayerId player) const;
    // Return whether the match is active or finished.
    [[nodiscard]] MatchStatus status() const;
    // Return the winning player ID only after completion; otherwise nullopt.
    [[nodiscard]] std::optional<PlayerId> winner() const;

  private:
    // After an entire state-changing action, check the acting player's empty rack.
    // No-ops never trigger checks/peels. Invalid boards report highlights without penalty.
    // Solo: valid board draws 1 if possible, otherwise finishes (clear the entire pool).
    // Multiplayer: valid board peels 1 per player when enough remain; otherwise actor wins.
    GameConfig config_;
    std::vector<PlayerState> players_;
    TilePool pool_;
    Lexicon lexicon_;
    MatchStatus status_{MatchStatus::active};
    std::optional<PlayerId> winner_;
};

} // namespace bananagrams
