#include "bananagrams/game.hpp"

#include <limits>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <unordered_set>
#include <algorithm>

namespace bananagrams {

Game::Game(GameConfig config, Lexicon lexicon)
    : config_(std::move(config)), pool_({}, config_.seed), lexicon_(std::move(lexicon)) {
    if (config_.mode != GameMode::solo && config_.mode != GameMode::multiplayer) {
        throw std::invalid_argument("Unsupported game mode");
    }
    if ((config_.mode == GameMode::solo && config_.player_count != 1) ||
        (config_.mode == GameMode::multiplayer && config_.player_count < 2)) {
        throw std::invalid_argument("Player count does not match game mode");
    }
    if (config_.starting_rack_size == 0) {
        throw std::invalid_argument("Starting rack size must be positive");
    }

    // Rack counters and pool sampling use int, so bound the entire tile supply.
    const auto maximum_tiles = static_cast<std::size_t>(std::numeric_limits<int>::max());
    std::size_t total_tiles = 0;
    for (int count : config_.tile_distribution) {
        if (count < 0) {
            throw std::invalid_argument("Tile counts must be nonnegative");
        }
        const auto quantity = static_cast<std::size_t>(count);
        if (quantity > maximum_tiles - total_tiles) {
            throw std::invalid_argument("Tile supply exceeds the supported int counter range");
        }
        total_tiles += quantity;
    }
    // Division avoids overflowing player_count * starting_rack_size.
    if (config_.starting_rack_size > total_tiles / config_.player_count) {
        throw std::invalid_argument("Not enough tiles to deal the starting racks");
    }

    std::vector<char> letters;
    letters.reserve(total_tiles);
    for (std::size_t index = 0; index < config_.tile_distribution.size(); ++index) {
        const char letter = static_cast<char>('A' + static_cast<int>(index));
        const auto quantity = static_cast<std::size_t>(config_.tile_distribution[index]);
        letters.insert(letters.end(), quantity, letter);
    }
    pool_ = TilePool(std::move(letters), config_.seed);
    players_.resize(config_.player_count);

    for (PlayerState& player : players_) {
        for (std::size_t index = 0; index < config_.starting_rack_size; ++index) {
            const auto letter = pool_.draw();
            if (!letter.has_value() || !player.rack.add(*letter)) {
                throw std::logic_error("Validated starting deal could not be completed");
            }
        }
    }
}

ActionResult Game::place_tile(PlayerId player_id, char letter, Position position) {
    ActionResult res = ActionResult();
    if (status_ == MatchStatus::finished) {
        res.success = false;
        res.error = ActionError::finished_match;
        return res;
    }
    if (player_id >= players_.size()) {
        res.success = false;
        res.error = ActionError::invalid_player;
        return res;
    }
    if (letter < 'A' || letter > 'Z') {
        res.success = false;
        res.error = ActionError::invalid_letter;
        return res;
    }

    const PlayerState& player = players_[player_id];
    if (player.rack.count(letter) == 0) {
        res.success = false;
        res.error = ActionError::unavailable_rack_tile;
        return res;
    }

    const auto displaced_letter = player.board.tile_at(position);
    if (displaced_letter.has_value() && *displaced_letter == letter) {
        // The board and rack would be identical afterward; no completion check is needed.
        res.success = true;
        return res;
    }


    // Prepare the entire transfer on a copy so failures leave the real player unchanged.
    std::vector<PlayerState> candidate_players = this->players_;
    TilePool candidate_pool = this->pool_;
    MatchStatus candidate_status = this->status_;
    PlayerState& candidate = candidate_players[player_id];
    std::optional<PlayerId> candidate_winner = winner_;
    if (!candidate.rack.remove(letter)) {
        res.success = false;
        res.error = ActionError::unavailable_rack_tile;
        return res;
    }
    if (displaced_letter.has_value() && !candidate.rack.add(*displaced_letter)) {
        res.success = false;
        res.error = ActionError::invalid_letter;
        return res;
    }
    if (!candidate.board.place(position, letter)) {
        res.success = false;
        res.error = ActionError::invalid_letter;
        return res;
    }

    // Validate the completed candidate before deciding whether to draw or finish.

    if (candidate.rack.empty()) {
        res.validation = validate_board(candidate.board, this->lexicon_);

        if (res.validation->valid) {
            if (candidate_pool.size() < candidate_players.size()) {
                // we have a winner!
                candidate_status = MatchStatus::finished;
                candidate_winner = player_id;
                res.event = ActionEvent::completed;

            } else {
                // we have a peel
                for (PlayerState& recipient : candidate_players) {
                    const auto drawn_tile = candidate_pool.draw();
                    if (!drawn_tile.has_value()) {
                        // The pool-size check guarantees enough tiles for this draw.
                        throw std::logic_error("TilePool became empty during a peel");
                    }
                    if (!recipient.rack.add(*drawn_tile)) {
                        res.success = false;
                        res.error = ActionError::invalid_letter;
                        return res;
                    }
                }
                res.event = ActionEvent::peel;
                if (candidate_players.size() == 1){
                    res.event = ActionEvent::solo_draw;
                }
            }
        }
    }
    // Publish the transaction only after every step succeeds. These moves cannot throw.
    static_assert(std::is_nothrow_move_assignable_v<decltype(players_)>);
    static_assert(std::is_nothrow_move_assignable_v<TilePool>);
    static_assert(std::is_nothrow_move_assignable_v<decltype(winner_)>);
    this->players_ = std::move(candidate_players);
    this->pool_ = std::move(candidate_pool);
    this->status_ = candidate_status;
    this->winner_ = std::move(candidate_winner);
    res.success = true;
    return res;
}

ActionResult Game::return_tiles(PlayerId player_id, const std::vector<Position>& positions) {
    ActionResult res = ActionResult();
    if (this->status_ == MatchStatus::finished) {
        res.success = false;
        res.error = ActionError::finished_match;
        return res;
    }
    if (player_id >= this->players_.size()) {
        res.success = false;
        res.error = ActionError::invalid_player;
        return res;
    }

    if (positions.size() == 0) {
        res.success = false;
        res.error = ActionError::invalid_selection;
        return res;
    }

    std::unordered_set<Position> s(positions.begin(), positions.end());

    if (s.size() != positions.size()) {
        res.success = false;
        res.error = ActionError::invalid_selection;
        return res;
    }

    Board& curr_board = this->players_[player_id].board;
    for (Position p : positions) {
        if (!curr_board.tile_at(p).has_value()) {
            res.success = false;
            res.error = ActionError::missing_board_tile;
            return res;
        }
    }

    Rack& curr_rack = this->players_[player_id].rack;
    for (Position p: positions) {
        char removed_tile = curr_board.remove(p).value();
        curr_rack.add(removed_tile);
    }
    res.success = true;
    return res;
}
ActionResult Game::move_tiles(PlayerId player_id, const std::vector<Position>& positions,
                              Position offset) {
    ActionResult res = ActionResult();
    if (status_ == MatchStatus::finished) {
        res.success = false;
        res.error = ActionError::finished_match;
        return res;
    }
    if (player_id >= players_.size()) {
        res.success = false;
        res.error = ActionError::invalid_player;
        return res;
    }
    std::vector<PlayerState> candidate_players = this->players_;
    TilePool candidate_pool = this->pool_;
    MatchStatus candidate_status = this->status_;
    PlayerState& candidate = candidate_players[player_id];
    std::optional<PlayerId> candidate_winner = winner_;
    if (!candidate.board.move_tiles(positions, offset)) {
        res.success = false;
        res.error = ActionError::invalid_move;
        return res;
    }

    if (offset == Position{0,0}) {
        res.success = true;
        return res;
    }
    // Validate the completed candidate before deciding whether to draw or finish.

    if (candidate.rack.empty()) {
        res.validation = validate_board(candidate.board, this->lexicon_);

        if (res.validation->valid) {
            if (candidate_pool.size() < candidate_players.size()) {
                // we have a winner!
                candidate_status = MatchStatus::finished;
                candidate_winner = player_id;
                res.event = ActionEvent::completed;

            } else {
                // we have a peel
                for (PlayerState& recipient : candidate_players) {
                    const auto drawn_tile = candidate_pool.draw();
                    if (!drawn_tile.has_value()) {
                        // The pool-size check guarantees enough tiles for this draw.
                        throw std::logic_error("TilePool became empty during a peel");
                    }
                    if (!recipient.rack.add(*drawn_tile)) {
                        res.success = false;
                        res.error = ActionError::invalid_letter;
                        return res;
                    }
                }
                res.event = ActionEvent::peel;
                if (candidate_players.size() == 1){
                    res.event = ActionEvent::solo_draw;
                }
            }
        }
    }
    // Publish the transaction only after every step succeeds. These moves cannot throw.
    static_assert(std::is_nothrow_move_assignable_v<decltype(players_)>);
    static_assert(std::is_nothrow_move_assignable_v<TilePool>);
    static_assert(std::is_nothrow_move_assignable_v<decltype(winner_)>);
    this->players_ = std::move(candidate_players);
    this->pool_ = std::move(candidate_pool);
    this->status_ = candidate_status;
    this->winner_ = std::move(candidate_winner);
    res.success = true;
    return res;
    
}
ActionResult Game::validate_selection(PlayerId player_id,
                                      const std::vector<Position>& positions) const {
    ActionResult result;
    if (player_id >= players_.size()) {
        result.error = ActionError::invalid_player;
        return result;
    }
    if (positions.empty() ||
        std::unordered_set<Position>(positions.begin(), positions.end()).size() != positions.size()) {
        result.error = ActionError::invalid_selection;
        return result;
    }
    Board selected;
    for (const Position position : positions) {
        const auto letter = players_[player_id].board.tile_at(position);
        if (!letter) {
            result.error = ActionError::missing_board_tile;
            return result;
        }
        if (!selected.place(position, *letter)) {
            throw std::logic_error("Existing board tile could not be copied for validation");
        }
    }
    result.validation = validate_board(selected, lexicon_);
    result.success = true;
    return result;
}

ActionResult Game::dump(PlayerId player_id, char letter) {
    ActionResult res = ActionResult();
    if (status_ == MatchStatus::finished) {
        res.success = false;
        res.error = ActionError::finished_match;
        return res;
    }
    if (player_id >= players_.size()) {
        res.success = false;
        res.error = ActionError::invalid_player;
        return res;
    }
    if (letter < 'A' || letter > 'Z') {
        res.success = false;
        res.error = ActionError::invalid_letter;
        return res;
    }

    const PlayerState& player = players_[player_id];
    if (player.rack.count(letter) == 0) {
        res.success = false;
        res.error = ActionError::unavailable_rack_tile;
        return res;
    }

    std::vector<PlayerState> candidate_players = this->players_;
    TilePool candidate_pool = this->pool_;
    PlayerState& candidate = candidate_players[player_id];
    if (!candidate.rack.remove(letter)) {
        res.success = false;
        res.error = ActionError::unavailable_rack_tile;
        return res;
    }

    if (!candidate_pool.return_tile(letter)){
        res.success = false;
        res.error = ActionError::invalid_letter;
        return res;
    }

    int num_tiles_to_draw = std::min(static_cast<int>(candidate_pool.size()), 3);

    for (int i = 0; i < num_tiles_to_draw; i ++) {
        const auto drawn_tile = candidate_pool.draw();
        if (!drawn_tile.has_value()) {
            throw std::logic_error("TilePool became empty during a dump");
        }
        if (!candidate.rack.add(*drawn_tile)) {
            res.success = false;
            res.error = ActionError::invalid_letter;
            return res;
        }
    }

    static_assert(std::is_nothrow_move_assignable_v<decltype(players_)>);
    static_assert(std::is_nothrow_move_assignable_v<TilePool>);
    this->players_ = std::move(candidate_players);
    this->pool_ = std::move(candidate_pool);
    res.success = true;
    return res;
}
// TODO: Build a snapshot that reveals opponent boards only after completion.
PlayerObservation Game::observe(PlayerId player_id) const {
     if (player_id >= players_.size()) {
        throw std::out_of_range("Invalid player id");
    }
    std::vector<OpponentObservation> ops{};
    for (PlayerId i = 0; i < this->players_.size(); i ++) {
        if (i != player_id){
            if (this->status_ == MatchStatus::finished){
                ops.push_back(OpponentObservation{
                    i, 
                    this->players_[i].rack.size(), 
                    this->players_[i].rack.size() + this->players_[i].board.size(), 
                    this->players_[i].board});
            } else {
                ops.push_back(OpponentObservation{
                    i, 
                    this->players_[i].rack.size(), 
                    this->players_[i].rack.size() + this->players_[i].board.size(), 
                    std::nullopt});
            }
            
        }
    }

    return PlayerObservation{
        this->players_[player_id].board,
        this->players_[player_id].rack,
        ops,
        this->pool_.size(),
        this->status_,
        this->winner_
    };

}
MatchStatus Game::status() const {
    return this->status_;
}
std::optional<PlayerId> Game::winner() const {
    if (this->status_ == MatchStatus::finished) {
        return this->winner_;
    }
    return std::nullopt;
}

} // namespace bananagrams
