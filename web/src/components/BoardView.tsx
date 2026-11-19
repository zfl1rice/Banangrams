import { forwardRef } from "react";
import { CELL, key, type Camera } from "../board/coordinates";
import type { Position, Tile } from "../engine/protocol";
import type { UIState } from "../state/uiState";
import { keyboardIntent } from "../interaction/keyboard";
import type { Intent } from "../interaction/controller";
type Props = {
  state: UIState;
  camera: Camera;
  dispatch: (intent: Intent) => unknown;
  onPointerDown: React.PointerEventHandler<HTMLDivElement>;
  preview: Tile[];
  rectangle: {
    left: number;
    top: number;
    width: number;
    height: number;
  } | null;
};
const at = (p: Position) => ({ left: p.col * CELL, top: p.row * CELL });
export const BoardView = forwardRef<HTMLDivElement, Props>(function BoardView(
  { state, camera, dispatch, onPointerDown, preview, rectangle },
  ref,
) {
  const invalid = new Set(
    state.validation?.invalidWords.flatMap((w) => w.positions.map(key)),
  );
  const disconnected = new Set(
    (state.validation?.components.length ?? 0) > 1
      ? state.validation!.components.flat().map(key)
      : [],
  );
  const selected = new Set(state.selected.map(key));
  return (
    <div
      ref={ref}
      className="board"
      data-testid="board"
      tabIndex={0}
      role="application"
      aria-label="Bananagrams board"
      aria-describedby="board-instructions"
      style={{
        backgroundSize: `${CELL * camera.zoom}px ${CELL * camera.zoom}px`,
        backgroundPosition: `${camera.x}px ${camera.y}px`,
      }}
      onPointerDown={onPointerDown}
      onKeyDown={(event) => {
        if (
          event.target !== event.currentTarget ||
          event.nativeEvent.isComposing
        )
          return;
        const intent = keyboardIntent(event.nativeEvent);
        if (intent) {
          event.preventDefault();
          dispatch(intent);
        }
      }}
    >
      <div
        className="board-world"
        style={{
          transform: `translate(${camera.x}px, ${camera.y}px) scale(${camera.zoom})`,
        }}
      >
        <div className="origin-mark" style={at({ row: 0, col: 0 })}>
          +
        </div>
        {state.observation.board.map((tile) => (
          <div
            key={key(tile)}
            data-testid={`tile-${tile.row}-${tile.col}`}
            aria-label={`${tile.letter}, row ${tile.row}, column ${tile.col}`}
            className={`tile board-tile ${selected.has(key(tile)) ? "selected" : ""} ${invalid.has(key(tile)) ? "invalid" : ""} ${disconnected.has(key(tile)) ? "disconnected" : ""}`}
            style={at(tile)}
          >
            {tile.letter}
          </div>
        ))}
        {state.cursor && (
          <div
            data-testid="cursor"
            data-row={state.cursor.row}
            data-col={state.cursor.col}
            className={`cursor ${state.vertical ? "vertical" : ""} ${state.conflict ? "conflict" : ""}`}
            style={at(state.cursor)}
          >
            <span>{state.vertical ? "↓" : "→"}</span>
          </div>
        )}
        {preview.map((tile) => (
          <div
            key={`preview-${key(tile)}`}
            className="tile board-tile preview"
            style={at(tile)}
          >
            {tile.letter}
          </div>
        ))}
      </div>
      {rectangle && <div className="selection-rectangle" style={rectangle} />}
      {!state.observation.board.length && (
        <div className="board-hint">
          Your next word starts here.
          <span>Click a square and type, or drag a tile.</span>
        </div>
      )}
      <div className="board-caption">
        {state.cursor
          ? `ROW ${state.cursor.row} · COL ${state.cursor.col}`
          : "CLICK A SQUARE TO BEGIN"}
        <span>{state.vertical ? "Vertical" : "Horizontal"}</span>
      </div>
    </div>
  );
});
