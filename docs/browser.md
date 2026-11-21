# Solo browser game

The frontend in `web/` is a playable desktop game backed by the existing C++
engine. Selected-region verification reuses the engine's board validator through
the read-only `Game::validate_selection` method. The added
binding target is conditional on Emscripten; native library and CLI targets remain.
Existing CMake edits were preserved. Four pre-existing standalone component
suites (board, rack, lexicon, tile pool) are now also registered with CTest.

## Tools

- CMake 3.24+, Ninja, and a C++20 native compiler.
- Emscripten **4.0.20**, the tested and pinned SDK version. The adapter's exception
  cleanup follows that version's JavaScript exception ABI.
- Node **22.12+** (verified with Node 24.12) and npm. Use `npm ci` with the committed
  `web/package-lock.json`.
- A desktop browser with WebAssembly and module workers. Browser integration was
  verified in installed Microsoft Edge (Chromium). Firefox and Safari have not
  been exercised.

## Native build and tests

Run from a compiler-enabled terminal at the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

There are ten CTest suites, including the existing CLI scaffold smoke test.
The test framework is doctest 2.4.12, fetched with a pinned archive digest.
For offline builds, supply
`-DFETCHCONTENT_SOURCE_DIR_DOCTEST=/path/to/doctest-2.4.12` during configuration.
Release builds use the corresponding `release` presets.

On the original Windows machine, CMake/Ninja and GCC can be enabled for the
current PowerShell session using the paths in the root README's tool section.
Alternatively, use Developer PowerShell for Visual Studio and a separate build
directory when changing compilers.

## Emscripten setup

If an activated 4.0.20 SDK is already available, reuse it. Otherwise install into
the ignored build directory (the download is about 700 MB):

```sh
git clone --depth 1 --branch 4.0.20 https://github.com/emscripten-core/emsdk.git build/emsdk
```

Windows PowerShell:

```powershell
.\build\emsdk\emsdk.bat install 4.0.20
.\build\emsdk\emsdk.bat activate 4.0.20
. .\build\emsdk\emsdk_env.ps1
```

macOS/Linux:

```sh
./build/emsdk/emsdk install 4.0.20
./build/emsdk/emsdk activate 4.0.20
source ./build/emsdk/emsdk_env.sh
```

Keep CMake and Ninja on PATH. These commands configure a separate WebAssembly
build; they do not replace or reuse the native build directory:

```sh
emcmake cmake -S . -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/wasm --target bananagrams_web
```

The output is `web/public/engine/bananagrams.js` and `bananagrams.wasm`.
Generated binaries and the SDK are ignored. Rebuild after changing C++ or binding
sources. Equivalently, from `web/`, run `npm run build:wasm` in the activated SDK
environment.

## Build and play

Shift-drag or use **Shift + arrow keys** to select tiles, then click **Verify selected** to check that region
alone. Horizontal and vertical runs must be in the loaded word list, with at
least two tiles and one connected group. Tiles outside the selection are ignored,
including adjacent tiles. The result names invalid words and highlights problems
until the next board edit. Verification consumes the selection but does not move
tiles, change placement undo history, draw from the pool, or finish the game.

Shift + arrows anchor a rectangle at the cursor's starting square and move its
opposite corner. Repeated presses extend or shrink it in either dimension,
including across empty squares; only occupied squares are selected. Releasing
Shift keeps the region. A plain arrow or another action clears it, and the next
Shift + arrow starts a fresh rectangle at the current cursor.

```sh
cd web
npm ci
npm run dev
```

Open the printed local URL, normally `http://127.0.0.1:5173`. The dev and
production build commands fail early with instructions if the engine assets are
missing. Choose **Try development table** for a three-tile demonstration: type
`CAT` to finish. This is explicitly limited to `CAT`, `ARE`, `AT`, and `QI`.

Choose **Start Solo** to play with the approved `data/lexicon/words.txt` vocabulary,
which is bundled into the frontend at build time and selected by default. Rebuild
the frontend after regenerating the word list to include its latest contents.
You can optionally load a different newline-separated `.txt` vocabulary. The
custom-file loader trims lines, drops blank lines, removes a leading BOM,
and normalizes case. C++ validates entries; words must contain uppercase A–Z and
at least two letters. Duplicates are deduplicated by the engine. Files stay in
the browser and are never uploaded.

Vocabulary policy, review decisions and rebuild instructions are documented in
`data/lexicon/README.md`. There is no approved production tile/rack configuration.
Table settings expose a provisional editable 144-tile distribution
and 21-tile rack. Each letter in the supply field represents one tile. New games
use a new random seed; rack shuffling never touches engine randomness.

Build and preview production assets:

```sh
npm run build
npm run preview
```

Serve the complete `web/dist/` directory over HTTP, including `assets/` and
`engine/`. Do not open `index.html` as a local file. Vite uses a relative base;
the worker resolves the generated module relative to the app, and the module
loads its sibling `.wasm`. Both root and nested static mount paths are supported.
No backend, cross-origin isolation headers, network vocabulary request, hosting
registration, or deployment is needed.

## Controls

- Click a board square and type A–Z; each successful placement advances the cursor.
  Matching board letters advance without consuming a tile. Conflicts do not advance.
- Tab switches horizontal/vertical typing while the board has focus. Arrow keys
  move the cursor; Space steps forward. Click another control to leave board focus.
- With a selected region, Backspace returns all selected tiles to the rack in one
  action, clears the selection and leaves the cursor in place.
  Otherwise, after clicking a square or navigating with arrow keys, Backspace returns the
  current tile and leaves the cursor in place. An empty square does nothing.
  After typing or stepping forward with Space, Backspace returns the preceding
  tile and moves back only when a tile is there. Delete always returns the current
  tile and leaves the cursor in place.
- Ctrl+Z / Cmd+Z returns the latest placed tile. Undo is not a rollback: draws,
  peels, dumps, and the pool remain as they are. Manual moves/removals clear history.
- Click a rack tile then an empty square, or drag it onto the board. Sort and
  Shuffle affect display order only. Dump selected works even with an empty pool.
- A region selection lasts for the next action only. Unmodified arrow keys, typing (even an
  unsuccessful attempt), direction changes, clicks, pan/zoom and other controls
  clear it. Moving or returning a group consumes the selection, even on failure.
- Drag a board tile to move it or return it to the rack. Shift-drag a rectangle,
  then drag any selected tile to move the whole group atomically. Return selected
  sends the entire selection through the engine's return action.
- Drag empty space to pan. Zoom controls and Fit board change only the camera.
- Escape cancels a drag/selection or clears the cursor. Pointer cancellation
  drops the preview without sending a gameplay action.

Validation runs when the C++ rules require it. Invalid words turn red; disconnected
sections have dashed outlines. Successful board edits clear stale feedback and
display any new report. Peels are automatic. Completion leaves the board visible
and allows inspection, pan and zoom while disabling gameplay changes.

## Architecture and lifetime

- `components/`: React rendering and pointer gesture previews.
- `interaction/controller.ts`: a single ordered intent queue, including clicks,
  direction changes, selections, removals, and undo. Cursor-dependent commands
  read the latest confirmed state when executed; rapid typing cannot reuse a stale
  cursor. Camera motion and previews are immediate.
- `state/uiState.ts`: rack identity reconciliation and frontend state. Consumed
  duplicate tiles are removed by their displayed identity; new draws are appended.
- `engine/client.ts`: typed messages, request IDs, session isolation, rejection of
  outstanding requests on teardown, worker errors, and disposal acknowledgement.
- `engine/engine.worker.ts`: owns the sole bound game and serializes initialization
  and actions. Responses include the result and complete player observation.
- `bindings/wasm/`: constructs the existing Game from explicit config and lexicon;
  exposes place, return, atomic move, dump, and observation. Serializes C++ values
  to plain JS objects within the worker, with no live bound values in messages.

The worker calls `.delete()` on the bound wrapper before normal termination. A
500 ms teardown fallback terminates unresponsive initialization and releases the
whole worker heap. JavaScript catches restore the wasm stack and release C++
exception storage. Session IDs and disposed-controller guards prevent old games
from updating a new game's UI. Opponent observation fields are retained without
fabricating opponents in solo mode.

The engine's existing placement API supports replacement. Browser placement
controls intentionally offer only empty-square placement; matching keyboard
crossings advance locally and conflicting letters show feedback. This is input
behavior, not a change to the C++ replacement contract.

## Tests

From `web/`:

```sh
npm test
npm run build
npx playwright install chromium
npm run test:browser
```

The browser suite launches and stops its own production preview server. To use
installed Edge instead of downloading Chromium:

```powershell
$env:PLAYWRIGHT_CHANNEL = 'msedge'
npm run test:browser
```

On macOS/Linux, use `PLAYWRIGHT_CHANNEL=msedge npm run test:browser` if Edge is
installed. Screenshots and failures are written under ignored `web/test-results/`.

Focused tests exercise ordering, matching crossings, failure handling, undo after
peel and dump, validation lifetime, group commands, duplicate rack identities,
coordinate math, worker errors, and stale-response isolation. Browser tests use
the actual production worker and wasm with tiny explicit test distributions;
they cover typing to completion, peeling, dumping from a depleted pool, undo,
invalid highlights, pointer placement/movement/return, restart, and C++ errors.

No save/resume, mobile-specific controls, bots, Python bindings, multiplayer UI,
or backend is included. A page reload discards the current game.
