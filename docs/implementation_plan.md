# Implementation checklist

This scaffold builds, but gameplay is deliberately unfinished. Replace TODO stubs
yourself and add behavioral tests as each feature becomes real. The cases below
describe outcomes to test, not implementation algorithms.

1. [ ] **Position, Rack, and basic Board operations.** Start with `Rack::empty`
   in `src/rack.cpp`. Position already has signed coordinates and defaulted
   comparisons. Test duplicate rack letters, total counts, missing removals,
   unsupported letters (including lowercase), count limits, occupied squares,
   negative positions, and unchanged state after failure. Board must permit
   temporary invalid words and disconnected arrangements.
2. [ ] **Lexicon and validation.** Test malformed entries, duplicates, exact
   membership, and the tiny fixture. Cover empty and one-tile boards, diagonal
   separation, orthogonal connections, crossing words, maximal runs, ignored
   single-letter runs, and positions in invalid-word/component reports.
3. [ ] **TilePool.** Test custom and empty pools, malformed initial letters,
   exhaustion, zero/oversized draw requests, returned tiles being drawable, and
   tile conservation. A portable replay format may need a defined sampling
   procedure beyond simply recording a seed.
4. [ ] **Solo Game behavior.** Test every configuration check, invalid IDs,
   invalid letters, unavailable rack tiles, atomic transfers, and unchanged
   state on expected failures. A successful action finishes fully before the
   empty-rack check. A valid empty-rack board draws one until the entire pool is
   exhausted; then the player wins. Invalid boards return highlights without
   peeling, finishing, penalties, or undoing successful placements. No-ops do
   not peel. Dump returns the chosen tile first, draws up to three (including
   possible redraw), and works even if the pool was empty. Test finished-match
   rejection and `observe` throwing `out_of_range` for an invalid player.
5. [ ] **Group movement and multiplayer.** Test empty/duplicate/missing sources,
   overlapping selected destinations, unselected collisions, signed coordinate
   overflow, zero offsets, and atomic failure. Test multiplayer configuration,
   shared inventory, exactly enough tiles to peel, too few tiles causing the
   acting player to win, and opponents receiving nothing on a dump. Test
   observations hiding opponent rack letters, pool contents, RNG state, and
   active opponent boards; reveal boards only after completion. Snapshots must
   remain independent of later game changes.
6. [ ] **UI integration.** Keep input controls, rendering, rack display order,
   visual shuffling, and file loading outside the engine. Use snapshots and
   validation positions for display. Defer browser/WebAssembly integration.
7. [ ] **Bot scheduling and Python bindings.** Keep bot policies, RL rewards,
   and training outside the engine. A future scheduler assigns action durations
   and processes scheduled actions in order, using either real-time or
   accelerated simulated time. Strategy and execution speed stay separate.
   Scheduling, timing costs, tie resolution, bots, and bindings are not built yet.

## Future production dictionary

Generate this separately from the top 10,000 American English words. Exclude
proper-noun-only entries, abbreviations, standalone prefixes/suffixes, and words
requiring hyphens or apostrophes. Add validated grammatical inflections,
including irregular forms, and explicitly include QI. No production dictionary
has been sourced or generated. `tests/fixtures/test_words.txt` is only a tiny
test fixture, not a gameplay dictionary; file loading is a later external concern.
