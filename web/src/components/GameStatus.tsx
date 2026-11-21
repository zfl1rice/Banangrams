import type { UIState } from "../state/uiState";
export function GameStatus({ state }: { state: UIState }) {
  const report = state.validation;
  return (
    <>
      <div className="status-strip" aria-live="polite">
        {report?.valid && state.validationScope === "selection" ? (
          <>
            <strong>Selected region is valid.</strong>
            <span>All selected tiles form connected, valid words.</span>
          </>
        ) : state.observation.status === "finished" && state.validationScope !== "selection" ? (
          <>
            <strong>Bananagrams!</strong> You cleared the pool. Your final board
            is yours to explore.
          </>
        ) : report && !report.valid ? (
          <>
            <strong>{state.validationScope === "selection" ? "Selected region is not valid." : "Keep arranging"}</strong>
            {report.invalidWords.length > 0 && (
              <span>
                Check {report.invalidWords.map((word) => word.text).join(", ")}.
              </span>
            )}
            {report.components.length > 1 && (
              <span>
                Connect the {report.components.length} outlined sections.
              </span>
            )}
            {!report.hasAtLeastTwoTiles && (
              <span>A word needs at least two tiles.</span>
            )}
          </>
        ) : (
          <>
            <strong>Solo table</strong>
            <span>Use every tile. Connect your words. Clear the pool.</span>
          </>
        )}
      </div>
      {state.notice && (
        <div className="toast" role="status">
          {state.notice}
        </div>
      )}
    </>
  );
}
