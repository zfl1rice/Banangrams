import { forwardRef } from "react";
import type { UIState } from "../state/uiState";
import type { Intent } from "../interaction/controller";
type Props = {
  state: UIState;
  dispatch: (intent: Intent) => unknown;
  onTileDown: (event: React.PointerEvent, id: number, letter: string) => void;
};
export const RackView = forwardRef<HTMLElement, Props>(function RackView(
  { state, dispatch, onTileDown },
  ref,
) {
  const finished = state.observation.status === "finished";
  return (
    <section
      className="rack-area"
      ref={ref}
      aria-label="Tile rack"
      data-testid="rack"
    >
      <div className="rack-heading">
        <div>
          <strong>Your rack</strong>
          <span>{state.rack.length} tiles</span>
        </div>
        <div className="rack-actions">
          <button onClick={() => dispatch({ type: "sort" })}>Sort A–Z</button>
          <button onClick={() => dispatch({ type: "shuffle" })}>Shuffle</button>
          <button
            disabled={finished || state.selectedRack === null}
            onClick={() => dispatch({ type: "dump" })}
          >
            Dump selected
          </button>
        </div>
      </div>
      <div className="rack-tiles">
        {state.rack.map((tile) => (
          <button
            key={tile.id}
            data-testid={`rack-${tile.id}`}
            className={`tile rack-tile ${tile.id === state.selectedRack ? "selected" : ""}`}
            disabled={finished}
            aria-pressed={tile.id === state.selectedRack}
            aria-label={`Select ${tile.letter}`}
            onPointerDown={(event) => onTileDown(event, tile.id, tile.letter)}
            onClick={(event) => {
              if (event.detail === 0)
                dispatch({ type: "selectRack", id: tile.id });
            }}
          >
            {tile.letter}
          </button>
        ))}
        {!state.rack.length && (
          <span className="empty-rack">
            {finished
              ? "Every tile found its place."
              : "Make one connected board of valid words to continue."}
          </span>
        )}
      </div>
      <div className="rack-footnote">
        Select a tile to place or dump it. Drop board tiles here to return them.
      </div>
    </section>
  );
});
