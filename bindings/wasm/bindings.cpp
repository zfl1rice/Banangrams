#include "bananagrams/game.hpp"
#include <emscripten/bind.h>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using namespace bananagrams;
using emscripten::val;

int integer(const val& value) {
    const double number = value.as<double>();
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Expected a signed 32-bit integer");
    }
    return static_cast<int>(number);
}
Position position(const val& value) { return {integer(value["row"]), integer(value["col"])}; }
val serialize(Position p) {
    auto out = val::object(); out.set("row", p.row); out.set("col", p.col); return out;
}
val serialize(const Board& board) {
    auto out = val::array();
    for (const auto& [p, letter] : board.tiles()) {
        auto tile = serialize(p); tile.set("letter", std::string(1, letter)); out.call<void>("push", tile);
    }
    return out;
}
std::vector<Position> positions(const val& values) {
    std::vector<Position> out;
    for (unsigned i = 0; i < values["length"].as<unsigned>(); ++i) out.push_back(position(values[i]));
    return out;
}
val serialize(const std::vector<Position>& values) {
    auto out = val::array();
    for (const auto p : values) out.call<void>("push", serialize(p));
    return out;
}
val serialize(const ValidationResult& report) {
    auto out = val::object(); auto words = val::array(); auto components = val::array();
    for (const auto& word : report.invalid_words) {
        auto item = val::object(); item.set("text", word.text); item.set("positions", serialize(word.positions));
        words.call<void>("push", item);
    }
    for (const auto& component : report.connected_components) components.call<void>("push", serialize(component));
    out.set("valid", report.valid); out.set("invalidWords", words); out.set("components", components);
    out.set("hasAtLeastTwoTiles", report.has_at_least_two_tiles); return out;
}
const char* errors[] = {"none", "invalid_player", "invalid_letter", "unavailable_rack_tile",
    "occupied_destination", "invalid_selection", "missing_board_tile", "coordinate_overflow",
    "inventory_overflow", "finished_match", "invalid_move"};
const char* events[] = {"none", "solo_draw", "peel", "completed"};

// Only this wrapper is bound. All returned values are ordinary JS snapshots;
// temporary native observations die here and never cross the worker boundary.
class BrowserGame {
    std::unique_ptr<Game> game_;
public:
    BrowserGame(const val& config, const val& words) {
        std::array<int, 26> counts{};
        if (config["distribution"]["length"].as<unsigned>() != counts.size())
            throw std::invalid_argument("Expected 26 tile counts");
        for (unsigned i = 0; i < counts.size(); ++i) counts[i] = integer(config["distribution"][i]);
        const int rack = integer(config["rackSize"]);
        if (rack <= 0) throw std::invalid_argument("Starting rack must be positive");
        std::vector<std::string> vocabulary;
        for (unsigned i = 0; i < words["length"].as<unsigned>(); ++i) vocabulary.push_back(words[i].as<std::string>());
        if (vocabulary.empty()) throw std::invalid_argument("The word list is empty");
        game_ = std::make_unique<Game>(GameConfig{GameMode::solo, 1, static_cast<std::size_t>(rack),
            counts, config["seed"].as<std::uint32_t>()}, Lexicon{vocabulary});
    }
    val observe() const {
        const auto observation = game_->observe(0);
        auto out = val::object(); auto rack = val::array(); auto opponents = val::array();
        for (int count : observation.rack.counts()) rack.call<void>("push", count);
        for (const auto& opponent : observation.opponents) {
            auto item = val::object(); item.set("player", static_cast<double>(opponent.player));
            item.set("unplacedTileCount", static_cast<double>(opponent.unplaced_tile_count));
            item.set("totalTileCount", static_cast<double>(opponent.total_tile_count));
            item.set("board", opponent.revealed_board ? serialize(*opponent.revealed_board) : val::null());
            opponents.call<void>("push", item);
        }
        out.set("board", serialize(observation.board)); out.set("rack", rack); out.set("opponents", opponents);
        out.set("poolSize", static_cast<double>(observation.pool_size));
        out.set("status", observation.status == MatchStatus::finished ? "finished" : "active");
        out.set("winner", observation.winner ? val(static_cast<double>(*observation.winner)) : val::null());
        return out;
    }
    val act(const val& command) {
        const auto type = command["type"].as<std::string>();
        ActionResult result;
        if (type == "place" || type == "dump") {
            const auto letter = command["letter"].as<std::string>();
            if (letter.size() != 1) throw std::invalid_argument("Expected one letter");
            result = type == "place" ? game_->place_tile(0, letter[0], position(command["position"]))
                                      : game_->dump(0, letter[0]);
        } else if (type == "return") result = game_->return_tiles(0, positions(command["positions"]));
        else if (type == "move") result = game_->move_tiles(0, positions(command["positions"]), position(command["offset"]));
        else if (type == "validate") result = game_->validate_selection(0, positions(command["positions"]));
        else throw std::invalid_argument("Unknown action");
        auto out = val::object(); auto action = val::object();
        action.set("success", result.success); action.set("error", errors[static_cast<int>(result.error)]);
        action.set("event", events[static_cast<int>(result.event)]);
        action.set("validation", result.validation ? serialize(*result.validation) : val::null());
        out.set("result", action); out.set("observation", observe()); return out;
    }
};
}
EMSCRIPTEN_BINDINGS(bananagrams_browser) {
    emscripten::class_<BrowserGame>("BrowserGame")
        .constructor<const val&, const val&>().function("observe", &BrowserGame::observe).function("act", &BrowserGame::act);
}
