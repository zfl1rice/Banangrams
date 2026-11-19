import { useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { EngineClient } from "./engine/client";
import { Controller } from "./interaction/controller";
import { GameScreen } from "./components/GameScreen";
import "./styles/game.css";

// The repository has no production distribution; this is an explicit editable
// starting preset, not an engine default or a claim of an approved vocabulary.
const counts = [
  13, 3, 3, 6, 18, 3, 4, 3, 12, 2, 2, 5, 3, 8, 11, 3, 2, 9, 6, 9, 6, 3, 3, 2, 3,
  2,
];
const defaultSupply = counts
  .map((n, i) => String.fromCharCode(65 + i).repeat(n))
  .join("");
function App() {
  const [controller, setController] = useState<Controller | null>(null);
  const [phase, setPhase] = useState<"start" | "loading" | "playing">("start");
  const [error, setError] = useState("");
  const [words, setWords] = useState<string[]>([]),
    [filename, setFilename] = useState("");
  const [supply, setSupply] = useState(defaultSupply),
    [rackSize, setRackSize] = useState(21);
  const [mode, setMode] = useState(""),
    [confirm, setConfirm] = useState(false);
  const session = useRef(0),
    client = useRef<EngineClient | null>(null),
    liveController = useRef<Controller | null>(null);
  const stop = () => {
    session.current++;
    liveController.current?.dispose();
    liveController.current = null;
    client.current?.dispose();
    client.current = null;
  };
  useEffect(() => () => stop(), []);
  async function start(development: boolean) {
    stop();
    const generation = session.current;
    setError("");
    setPhase("loading");
    try {
      const vocabulary = development ? ["CAT", "ARE", "AT", "QI"] : words;
      const letters = development
        ? "CAT"
        : supply.toUpperCase().replace(/\s/g, "");
      if (!/^[A-Z]+$/.test(letters))
        throw new Error("The tile supply must contain only letters A–Z.");
      const distribution = Array.from(
        { length: 26 },
        (_, i) => [...letters].filter((c) => c.charCodeAt(0) === i + 65).length,
      );
      const engine = new EngineClient();
      client.current = engine;
      const { observation } = await engine.init(
        {
          distribution,
          rackSize: development ? 3 : rackSize,
          seed: crypto.getRandomValues(new Uint32Array(1))[0],
        },
        vocabulary,
      );
      if (session.current !== generation) {
        engine.dispose();
        return;
      }
      const next = new Controller(engine, observation);
      liveController.current = next;
      setController(next);
      setMode(
        development ? "Development · 4-word vocabulary" : `Solo · ${filename}`,
      );
      setPhase("playing");
    } catch (cause) {
      if (session.current !== generation) return;
      client.current?.dispose();
      client.current = null;
      setError(cause instanceof Error ? cause.message : String(cause));
      setPhase("start");
    }
  }
  function restart() {
    stop();
    setController(null);
    setConfirm(false);
    setPhase("start");
  }
  if (phase === "playing" && controller)
    return (
      <>
        <GameScreen
          controller={controller}
          mode={mode}
          onNewGame={() =>
            controller.getSnapshot().observation.status === "active"
              ? setConfirm(true)
              : restart()
          }
        />
        {confirm && (
          <Confirm onCancel={() => setConfirm(false)} onConfirm={restart} />
        )}
      </>
    );
  return (
    <main className="start-screen">
      <div className="start-brand">
        <span className="brand-tile">B</span> THE SOLO TABLE
      </div>
      <section className="start-card">
        <div className="start-intro">
          <span className="eyebrow">A LITTLE SPACE FOR WORDS</span>
          <h1>
            One tile.
            <br />
            Endless possibilities.
          </h1>
          <p>
            Build a connected crossword, use every letter, and keep going until
            the pool is empty.
          </p>
          <div className="sample-word" aria-hidden="true">
            {"PLAY".split("").map((c) => (
              <span key={c} className="tile">
                {c}
              </span>
            ))}
          </div>
          <p className="quiet">No clock. No opponents. Just your next word.</p>
        </div>
        <div className="start-options">
          <h2>Make yourself a table</h2>
          <div className="vocabulary-note">
            <strong>Bring your word list</strong>
            <p>
              The approved vocabulary is not included yet. Load a
              newline-separated word list you trust, or try the clearly limited
              development table.
            </p>
          </div>
          <label className="file-label">
            Word list (.txt)
            <input
              type="file"
              accept=".txt,text/plain"
              disabled={phase === "loading"}
              onChange={async (event) => {
                const file = event.target.files?.[0];
                if (!file) return;
                setWords([]);
                setFilename("");
                setError("");
                try {
                  const entries = (await file.text())
                    .replace(/^\uFEFF/, "")
                    .split(/\r?\n/)
                    .map((word) => word.trim().toUpperCase())
                    .filter(Boolean);
                  setWords(entries);
                  setFilename(file.name);
                } catch {
                  setError("That word list could not be read.");
                }
              }}
            />
          </label>
          {filename && (
            <p className="file-summary">
              {filename} · {words.length.toLocaleString()} entries
            </p>
          )}
          <details className="setup">
            <summary>Table settings</summary>
            <p>
              No production configuration is defined in the engine. This
              editable preset has 144 tiles and starts with 21.
            </p>
            <label>
              Starting rack size
              <input
                type="number"
                min="1"
                max="1000"
                value={rackSize}
                onChange={(e) => setRackSize(Number(e.target.value))}
              />
            </label>
            <label>
              Tile supply (one letter per tile)
              <textarea
                value={supply}
                onChange={(e) => setSupply(e.target.value)}
                rows={4}
              />
            </label>
          </details>
          <button
            className="primary"
            disabled={phase === "loading" || !words.length}
            onClick={() => void start(false)}
          >
            Start Solo
          </button>
          <div className="dev-option">
            <button
              disabled={phase === "loading"}
              onClick={() => void start(true)}
            >
              Try development table
            </button>
            <span>3 tiles · CAT, ARE, AT, QI only</span>
          </div>
          {phase === "loading" && (
            <div role="status" className="loading">
              Preparing your tiles and vocabulary…
              <button onClick={restart}>Cancel</button>
            </div>
          )}
          {error && (
            <p role="alert" className="error">
              Could not start: {error}
            </p>
          )}
        </div>
      </section>
      <footer className="start-footer">
        BANANAGRAMS <span>A quiet place to play with words.</span>
      </footer>
    </main>
  );
}
function Confirm({
  onCancel,
  onConfirm,
}: {
  onCancel: () => void;
  onConfirm: () => void;
}) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const dialog = ref.current!;
    dialog.showModal();
    return () => dialog.close();
  }, []);
  return (
    <dialog
      ref={ref}
      onCancel={(event) => {
        event.preventDefault();
        onCancel();
      }}
      aria-labelledby="new-game-title"
    >
      <h2 id="new-game-title">Start a new table?</h2>
      <p>Your current board and progress will be lost.</p>
      <div className="dialog-actions">
        <button autoFocus onClick={onCancel}>
          Keep playing
        </button>
        <button className="primary" onClick={onConfirm}>
          New game
        </button>
      </div>
    </dialog>
  );
}
createRoot(document.getElementById("root")!).render(<App />);
