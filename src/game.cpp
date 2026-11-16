#include "bananagrams/game.hpp"

#include <stdexcept>
#include <utility>

namespace bananagrams {

// TODO: Validate config, create its distribution, and deal starting racks.
// These initializers are build glue only; TilePool construction is also unfinished.
Game::Game(GameConfig config, Lexicon lexicon)
    : config_(std::move(config)), pool_({}, config_.seed), lexicon_(std::move(lexicon)) {
    throw std::logic_error("TODO: Game::Game");
}
// TODO: Coordinate atomic transfers and check completion only after the whole action.
ActionResult Game::place_tile(PlayerId /*player*/, char /*letter*/, Position /*position*/) {
    throw std::logic_error("TODO: Game::place_tile");
}
ActionResult Game::return_tiles(PlayerId /*player*/, const std::vector<Position>& /*positions*/) {
    throw std::logic_error("TODO: Game::return_tiles");
}
ActionResult Game::move_tiles(PlayerId /*player*/, const std::vector<Position>& /*positions*/,
                              Position /*offset*/) {
    throw std::logic_error("TODO: Game::move_tiles");
}
ActionResult Game::dump(PlayerId /*player*/, char /*letter*/) {
    throw std::logic_error("TODO: Game::dump");
}
// TODO: Build a snapshot that reveals opponent boards only after completion.
PlayerObservation Game::observe(PlayerId /*player*/) const {
    throw std::logic_error("TODO: Game::observe");
}
MatchStatus Game::status() const {
    throw std::logic_error("TODO: Game::status");
}
std::optional<PlayerId> Game::winner() const {
    throw std::logic_error("TODO: Game::winner");
}

} // namespace bananagrams
