# Bananagrams C++ engine skeleton

This is a learning scaffold for a native solo/multiplayer engine. The build,
CLI, and trivial value-type smoke tests work. **Gameplay is not implemented.**
Every nontrivial method throws `std::logic_error("TODO: ...")`, including basic
Rack/Board queries and accessors. Constructors for Lexicon, TilePool, and Game
are unfinished too. Passing smoke tests do not establish gameplay correctness.

## Required tools

- A C++20 compiler: recent GCC, Clang, or Visual Studio C++ Build Tools/MSVC.
- CMake 3.24 or newer, Ninja, and Git.
- Network access on the first test-enabled configure to download **doctest
  2.4.12**, pinned by version and SHA-256. This is the only fetched dependency.
- Optional clang-format for the supplied `.clang-format` style.

Use a compiler-enabled terminal. On Windows with MSVC, open **Developer
PowerShell for Visual Studio** or **x64 Native Tools Command Prompt**. Install
the Desktop development with C++ workload and C++ CMake tools if absent. For
MSYS2 GCC, put its `ucrt64/bin` directory on PATH so the compiler and runtime
DLLs can be found. Choose one compiler per build directory.

On the setup machine, these existing tools can be enabled for the current
PowerShell session (paths are machine-specific; no permanent PATH edit needed):

```powershell
$cmakeBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$ninjaBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
$env:PATH = "C:\msys64\ucrt64\bin;$cmakeBin;$ninjaBin;$env:PATH"
```

## Configure, build, and test

From the repository root, run:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release
```

If CMake does not choose your intended compiler, use, for example,
`cmake --preset debug -DCMAKE_CXX_COMPILER=g++` on the first configure.
Run the CLI with `./build/debug/bananagrams_cli` on Unix or
`.\build\debug\bananagrams_cli.exe` in Windows PowerShell. It prints that this
is a skeleton and exits successfully without invoking gameplay.

For a different compiler, use a separate build directory:

```sh
cmake --preset debug -B build/other-debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/other-debug
ctest --test-dir build/other-debug --output-on-failure
```

The native `bananagrams_engine` library (alias `bananagrams::engine`) contains
all six stub source files. Both executables link to it. Project targets use
C++20 without compiler extensions, plus `/W4 /permissive-` on MSVC or
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` on GCC/Clang.

To build the library and CLI without fetching the test framework:

```sh
cmake --preset debug -B build/no-tests -DBUILD_TESTING=OFF
cmake --build build/no-tests
```

For an offline test build, extract the pinned doctest release locally and add
`-DFETCHCONTENT_SOURCE_DIR_DOCTEST=/absolute/path/to/doctest-2.4.12` to configure.

## Optional sanitizers

On a GCC/Clang toolchain with sanitizer runtimes installed:

```sh
cmake --preset debug -B build/sanitized -DBANANAGRAMS_ENABLE_ASAN=ON -DBANANAGRAMS_ENABLE_UBSAN=ON
cmake --build build/sanitized
ctest --test-dir build/sanitized --output-on-failure
```

With MSVC, request only `BANANAGRAMS_ENABLE_ASAN`; UBSan is not supported by
this configuration. CMake disables conflicting MSVC runtime checks and
incremental linking for ASan. Ensure the sanitizer runtime is on PATH when
running executables. Configure checks compile/link support and fails clearly
if the selected compiler lacks the requested runtime. MinGW installations may
lack these runtimes; use ordinary builds or another toolchain in that case.
Sanitizers become more useful as gameplay is implemented.

For MSVC ASan, run these in a Visual Studio developer terminal:

```sh
cmake --preset debug -B build/msvc-asan -DCMAKE_CXX_COMPILER=cl -DBANANAGRAMS_ENABLE_ASAN=ON
cmake --build build/msvc-asan
ctest --test-dir build/msvc-asan --output-on-failure
```

## Editor tooling

Ninja presets generate `build/debug/compile_commands.json` and
`build/release/compile_commands.json`. Point clangd at one with
`--compile-commands-dir=build/debug`, or configure your editor's C++ tooling to
use that file. Visual Studio's CMake integration can also use the presets.
Format a file with `clang-format -i src/rack.cpp`. Build products and local
editor configuration are ignored by Git.

## Project layout and contracts

- `include/bananagrams/`: self-contained public headers and simple value types.
- `src/`: linkable throwing TODO definitions, including the first Rack/Board exercises.
- `apps/cli/`: minimal scaffold executable.
- `tests/`: doctest smoke checks and a tiny, non-production word fixture.
- `docs/implementation_plan.md`: ordered learning milestones and future behavior tests.

Start with **`Rack::empty()` in `src/rack.cpp`**. Add behavioral tests as you
replace each TODO. Existing tests cover Position comparisons/map keys and
explicit GameConfig storage; they neither fake gameplay nor assert TODO throws
as desirable behavior. The CLI smoke check verifies its skeleton message.

Headers describe the intended behavior after implementation. Use uppercase
A-Z only. Unsupported-character mutations must fail without changing state.
Board/Rack/TilePool mutations use bool/optional results; Game uses ActionResult
and explicit ActionError codes. Expected action failures leave the complete
game unchanged. Invalid construction inputs will throw `std::invalid_argument`;
invalid observation IDs will throw `std::out_of_range`. These future errors
are distinct from today's `std::logic_error` TODO markers. Standard allocation
failures may still throw; no throwing stub is marked `noexcept`.

Board stores occupied squares only, permits signed coordinates, and does not
know about racks, dictionaries, or completion. Read-only access protects its
invariants. Game owns inventory transfers and completion rules. A successful
placement may produce an invalid board and still succeed. Finish an entire
state-changing action before checking the acting player's empty rack and board;
no-op actions do not trigger a peel. Invalid boards get validation highlights
without penalties. Solo draws one until the whole pool is cleared, then finishes.
Multiplayer peels one for everyone if enough remain; otherwise the acting
player wins. Dump returns one rack tile before drawing up to three, even from
an initially empty pool; the returned tile can be drawn again.

Observations are independent value snapshots, hiding opponent rack letters,
hidden pool contents, and RNG state; opponent boards appear only after the
match ends. Engine state and rules stay separate from rendering, controls,
policies, rewards, and training. No WebAssembly, browser UI, Python bindings,
bots, scheduling, timing costs, or tie resolution are implemented. The engine
applies actions sequentially and never sleeps. A future scheduler will support
real-time and accelerated simulated time with strategy separate from speed.

The production dictionary and production tile/rack sizes are deliberately
unspecified. See the implementation plan for the future dictionary criteria.
A seed does not promise identical draws across standard-library implementations;
portable replay may later require an explicitly defined sampling procedure.

## Setup verification

Verified on Windows with the existing tools; no installation was needed:

- GCC 15.2.0: Debug and Release configured, compiled all six engine sources,
  and passed both CTest checks in each build.
- MSVC 19.50: Debug with AddressSanitizer configured, built, and passed both checks.
- All 11 public headers compiled individually with GCC in C++20 mode.
- Both standard presets generated `compile_commands.json`; Release exercised
  the actual pinned dependency download and digest verification.
- The installed MinGW GCC lacks `libasan` and `libubsan`. Its sanitizer configure
  probe correctly failed. UBSan execution could not be verified on this machine.
- CMake 4.1.1 emits a non-fatal deprecation notice from doctest 2.4.12's own
  CMake minimum-version declaration. Project compilation completed without warnings.

These checks cover the scaffold and trivial value types only. They do not
exercise gameplay, inventory, validation, randomness, or completion rules.
