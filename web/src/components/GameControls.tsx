import type { Controller } from "../interaction/controller";
import type { UIState } from "../state/uiState";
export function GameControls({
  state,
  controller,
  zoom,
  fit,
  zoomValue,
}: {
  state: UIState;
  controller: Controller;
  zoom: (factor: number) => void;
  fit: () => void;
  zoomValue: number;
}) {
  const finished = state.observation.status === "finished";
  return (
    <div className="board-tools">
      <div className="tool-group">
        <button aria-label="Zoom out" onClick={() => zoom(0.8)}>
          −
        </button>
        <span>{Math.round(zoomValue * 100)}%</span>
        <button aria-label="Zoom in" onClick={() => zoom(1.25)}>
          +
        </button>
        <button onClick={fit}>Fit board</button>
      </div>
      <div className="tool-group">
        <button
          disabled={!state.selected.length || state.busy}
          onClick={() => controller.dispatch({ type: "verifySelected" })}
          title="Check only the selected tiles as a connected set of words"
        >
          Verify selected
          {state.selected.length ? ` (${state.selected.length})` : ""}
        </button>
        <button onClick={() => controller.dispatch({ type: "toggle" })}>
          {state.vertical ? "Type vertically" : "Type horizontally"}
        </button>
        <button
          disabled={finished || !state.history.length}
          onClick={() => controller.dispatch({ type: "undo" })}
        >
          Undo placement
        </button>
        <button
          disabled={finished || !state.selected.length}
          onClick={() => controller.dispatch({ type: "returnSelected" })}
        >
          Return selected
          {state.selected.length ? ` (${state.selected.length})` : ""}
        </button>
      </div>
    </div>
  );
}
