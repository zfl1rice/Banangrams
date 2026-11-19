import { access } from "node:fs/promises";
try {
  await Promise.all(
    ["bananagrams.js", "bananagrams.wasm"].map((name) =>
      access(new URL(`../public/engine/${name}`, import.meta.url)),
    ),
  );
} catch {
  console.error(
    "Build the C++ browser engine first: activate Emscripten, then run npm run build:wasm. See docs/browser.md.",
  );
  process.exitCode = 1;
}
