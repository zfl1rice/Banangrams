import { useLayoutEffect, useRef, useState, useSyncExternalStore } from "react";
import {
  CELL,
  fitBoard,
  same,
  toBoard,
  zoomAt,
  type Camera,
} from "../board/coordinates";
import type { Controller } from "../interaction/controller";
import { movedEnough, type Gesture } from "../interaction/pointer";
import { BoardView } from "./BoardView";
import { RackView } from "./RackView";
import { GameControls } from "./GameControls";
import { GameStatus } from "./GameStatus";
export function GameScreen({
  controller,
  onNewGame,
  mode,
}: {
  controller: Controller;
  onNewGame: () => void;
  mode: string;
}) {
  const state = useSyncExternalStore(
    controller.subscribe,
    controller.getSnapshot,
  );
  const board = useRef<HTMLDivElement>(null),
    rack = useRef<HTMLElement>(null);
  const [camera, setCamera] = useState<Camera>({ x: 0, y: 0, zoom: 1 });
  const [gesture, setGesture] = useState<Gesture | null>(null);
  const gestureRef = useRef<Gesture | null>(null);
  const capture = useRef<Element | null>(null);
  const dispatch = controller.dispatch.bind(controller);
  useLayoutEffect(() => {
    const element = board.current!;
    setCamera(fitBoard([], element.clientWidth, element.clientHeight));
    element.focus();
  }, []);
  const boardPosition = (x: number, y: number) => {
    const rect = board.current!.getBoundingClientRect();
    return toBoard(x - rect.left, y - rect.top, camera);
  };
  function begin(event: React.PointerEvent, rackId?: number, letter?: string) {
    if (event.button !== 0 || gestureRef.current) return;
    const position = boardPosition(event.clientX, event.clientY);
    const tile = state.observation.board.find((t) => same(t, position));
    const kind =
      rackId !== undefined
        ? "rack"
        : event.shiftKey
          ? "rectangle"
          : tile
            ? "board"
            : "pan";
    if (kind === "rack") void dispatch({ type: "selectRack", id: rackId! });
    if (kind === "board") void dispatch({ type: "selectAt", position });
    if (kind === "pan" || kind === "rectangle") void dispatch({ type: "deselect" });
    board.current!.focus({ preventScroll: true });
    const next: Gesture = {
      kind,
      pointerId: event.pointerId,
      startX: event.clientX,
      startY: event.clientY,
      x: event.clientX,
      y: event.clientY,
      moved: false,
      origin: position,
      camera,
      rackId,
      letter,
    };
    gestureRef.current = next;
    setGesture(next);
    capture.current = event.currentTarget;
    event.currentTarget.setPointerCapture(event.pointerId);
    event.preventDefault();
  }
  function cancel() {
    const current = gestureRef.current;
    gestureRef.current = null;
    setGesture(null);
    if (current && capture.current?.hasPointerCapture(current.pointerId))
      capture.current.releasePointerCapture(current.pointerId);
    capture.current = null;
  }
  function abort() {
    if (gestureRef.current) void dispatch({ type: "deselect" });
    cancel();
  }
  function move(event: React.PointerEvent) {
    const current = gestureRef.current;
    if (!current || current.pointerId !== event.pointerId) return;
    const next = {
      ...current,
      x: event.clientX,
      y: event.clientY,
      moved:
        current.moved || movedEnough(current, event.clientX, event.clientY),
    };
    gestureRef.current = next;
    setGesture(next);
    if (next.kind === "pan" && next.moved)
      setCamera({
        ...next.camera,
        x: next.camera.x + next.x - next.startX,
        y: next.camera.y + next.y - next.startY,
      });
  }
  function end(event: React.PointerEvent) {
    const current = gestureRef.current;
    if (!current || current.pointerId !== event.pointerId) return;
    const target = boardPosition(event.clientX, event.clientY);
    const boardRect = board.current!.getBoundingClientRect(),
      rackRect = rack.current!.getBoundingClientRect();
    const within = (r: DOMRect) =>
      event.clientX >= r.left &&
      event.clientX <= r.right &&
      event.clientY >= r.top &&
      event.clientY <= r.bottom;
    if (current.kind === "rectangle") {
      void dispatch({
        type: "selectRectangle",
        from: current.origin,
        to: target,
      });
    } else if (!current.moved && current.kind !== "rack")
      void dispatch({ type: "click", position: target });
    else if (current.moved) {
      if (current.kind === "rack" && within(boardRect))
        void dispatch({
          type: "rackDrop",
          id: current.rackId!,
          position: target,
        });
      if (current.kind === "board") {
        if (within(rackRect)) void dispatch({ type: "returnSelected" });
        else if (within(boardRect))
          void dispatch({
            type: "moveSelected",
            offset: {
              row: target.row - current.origin.row,
              col: target.col - current.origin.col,
            },
          });
        else void dispatch({ type: "deselect" });
      }
    }
    cancel();
  }
  const rect = board.current?.getBoundingClientRect();
  const target = gesture && rect ? boardPosition(gesture.x, gesture.y) : null;
  const onBoard =
    gesture &&
    rect &&
    gesture.x >= rect.left &&
    gesture.x < rect.right &&
    gesture.y >= rect.top &&
    gesture.y < rect.bottom;
  const preview =
    gesture?.moved && target && onBoard
      ? gesture.kind === "rack"
        ? [{ ...target, letter: gesture.letter! }]
        : gesture.kind === "board"
          ? state.observation.board
              .filter((t) => state.selected.some((p) => same(p, t)))
              .map((t) => ({
                ...t,
                row: t.row + target.row - gesture.origin.row,
                col: t.col + target.col - gesture.origin.col,
              }))
          : []
      : [];
  const rectangle =
    gesture?.kind === "rectangle" && rect
      ? {
          left: Math.min(gesture.startX, gesture.x) - rect.left,
          top: Math.min(gesture.startY, gesture.y) - rect.top,
          width: Math.abs(gesture.x - gesture.startX),
          height: Math.abs(gesture.y - gesture.startY),
        }
      : null;
  return (
    <main
      className="game"
      onPointerMove={move}
      onPointerUp={end}
      onPointerCancel={abort}
      onLostPointerCapture={abort}
      onClick={(event) => {
        // Child controls enqueue their action first, so Return selected can
        // consume the region before this clears it for other UI-only clicks.
        if (!board.current?.contains(event.target as Node))
          void dispatch({ type: "deselect" });
      }}
      onKeyDownCapture={(event) => {
        if (event.key === "Escape" && gestureRef.current) {
          cancel();
          event.preventDefault();
        }
      }}
    >
      <header className="topbar">
        <div className="brand">
          <span className="brand-tile">B</span>
          <div>
            <strong>Bananagrams</strong>
            <span>{mode}</span>
          </div>
        </div>
        <div className="pool">
          <strong data-testid="pool-count">{state.observation.poolSize}</strong>
          <span>tiles in pool</span>
        </div>
        <button onClick={onNewGame}>New game</button>
      </header>
      <GameStatus state={state} />
      <GameControls
        state={state}
        controller={controller}
        zoomValue={camera.zoom}
        fit={() =>
          setCamera(
            fitBoard(
              state.observation.board,
              board.current!.clientWidth,
              board.current!.clientHeight,
            ),
          )
        }
        zoom={(factor) =>
          setCamera(
            zoomAt(
              camera,
              camera.zoom * factor,
              board.current!.clientWidth / 2,
              board.current!.clientHeight / 2,
            ),
          )
        }
      />
      <div className="board-wrap">
        <BoardView
          ref={board}
          state={state}
          camera={camera}
          dispatch={dispatch}
          onPointerDown={begin}
          preview={preview}
          rectangle={rectangle}
        />
        <details className="help">
          <summary>Keyboard &amp; mouse</summary>
          <div id="board-instructions">
            <p>Click the board to type letters immediately.</p>
            <dl>
              <dt>Tab</dt>
              <dd>Change typing direction</dd>
              <dt>Arrow keys / Space</dt>
              <dd>Move cursor / step forward</dd>
              <dt>Backspace</dt>
              <dd>Return the selected region; otherwise current tile after navigation, previous tile after typing</dd>
              <dt>Delete</dt>
              <dd>Return current tile</dd>
              <dt>Ctrl or ⌘ Z</dt>
              <dd>Undo last placement</dd>
              <dt>Shift + drag / Shift + arrow keys</dt>
              <dd>Select a region; Shift + arrows extend or shrink it from the starting cursor. Other actions clear it.</dd>
              <dt>Drag empty space</dt>
              <dd>Pan the board</dd>
              <dt>Escape</dt>
              <dd>Cancel selection or cursor</dd>
            </dl>
            <p>Click outside the board to resume normal Tab navigation.</p>
          </div>
        </details>
      </div>
      <RackView
        ref={rack}
        state={state}
        dispatch={dispatch}
        onTileDown={begin}
      />
      {gesture?.moved && gesture.kind === "rack" && !onBoard && (
        <div
          className="tile floating-preview"
          style={{ left: gesture.x - CELL / 2, top: gesture.y - CELL / 2 }}
        >
          {gesture.letter}
        </div>
      )}
    </main>
  );
}
