import { same } from "../board/coordinates";
import type { Action, Engine, Observation, Position } from "../engine/protocol";
import { reconcileRack, type UIState } from "../state/uiState";
export type Intent =
  | { type: "type"; letter: string }
  | { type: "click"; position: Position }
  | { type: "cursor"; position: Position | null }
  | { type: "arrow"; offset: Position }
  | { type: "extendSelection"; offset: Position }
  | {
      type:
        | "toggle"
        | "space"
        | "backspace"
        | "delete"
        | "escape"
        | "undo"
        | "dump"
        | "returnSelected"
        | "verifySelected"
        | "sort"
        | "shuffle"
        | "deselect";
    }
  | { type: "selectRack"; id: number }
  | { type: "select"; positions: Position[] }
  | { type: "selectRectangle"; from: Position; to: Position }
  | { type: "selectAt"; position: Position }
  | { type: "rackDrop"; id: number; position: Position }
  | { type: "moveSelected"; offset: Position };
export class Controller {
  private state: UIState;
  private listeners = new Set<() => void>();
  private chain = Promise.resolve();
  private alive = true;
  // Navigation targets the current square; typing targets the preceding square.
  private backspaceAtCursor = false;
  private selectionAnchor: Position | null = null;
  private timer?: ReturnType<typeof setTimeout>;
  constructor(
    private engine: Engine,
    observation: Observation,
  ) {
    this.state = {
      observation,
      cursor: { row: 0, col: 0 },
      vertical: false,
      selected: [],
      rack: reconcileRack([], observation.rack),
      selectedRack: null,
      history: [],
      validation: null,
      validationScope: "board",
      notice: "",
      conflict: null,
      busy: false,
    };
  }
  getSnapshot = () => this.state;
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  private update(patch: Partial<UIState>) {
    if (!this.alive) return;
    this.state = { ...this.state, ...patch };
    this.listeners.forEach((listener) => listener());
  }
  private notify(notice: string, conflict: Position | null = null) {
    clearTimeout(this.timer);
    this.update({ notice, conflict });
    this.timer = setTimeout(
      () => this.update({ notice: "", conflict: null }),
      2800,
    );
  }
  dispose() {
    this.alive = false;
    clearTimeout(this.timer);
    this.listeners.clear();
  }
  dispatch(intent: Intent): Promise<void> {
    // Resolve cursor, rack selection and history only when this intent reaches the
    // front. Clicks and direction changes share the same queue as engine actions.
    this.chain = this.chain
      .then(async () => {
        if (this.alive) await this.handle(intent);
      })
      .catch((error) => {
        if (this.alive)
          this.notify(error instanceof Error ? error.message : String(error));
      });
    return this.chain;
  }
  private tile(position: Position) {
    return this.state.observation.board.find((tile) => same(tile, position));
  }
  private rectangle(from: Position, to: Position) {
    return this.state.observation.board.filter(
      (p) =>
        p.row >= Math.min(from.row, to.row) &&
        p.row <= Math.max(from.row, to.row) &&
        p.col >= Math.min(from.col, to.col) &&
        p.col <= Math.max(from.col, to.col),
    );
  }
  private step(position: Position, distance = 1): Position {
    return {
      row: position.row + (this.state.vertical ? distance : 0),
      col: position.col + (this.state.vertical ? 0 : distance),
    };
  }
  private async act(action: Action, consumed?: number): Promise<boolean> {
    if (this.state.observation.status === "finished" && action.type !== "validate") return false;
    this.update({ busy: true });
    try {
      const { observation, result } = await this.engine.act(action);
      if (!this.alive) return false;
      const rack = reconcileRack(
        this.state.rack,
        observation.rack,
        result.success ? consumed : undefined,
      );
      this.update({
        observation,
        rack,
        selectedRack: rack.some((t) => t.id === this.state.selectedRack)
          ? this.state.selectedRack
          : null,
        selected: this.state.selected.filter((p) =>
          observation.board.some((t) => same(t, p)),
        ),
      });
      if (!result.success) {
        this.notify(
          result.error === "invalid_move"
            ? "Those tiles cannot move there."
            : result.error.replaceAll("_", " "),
        );
        return false;
      }
      if (action.type !== "dump")
        this.update({
          validation: result.validation,
          validationScope: action.type === "validate" ? "selection" : "board",
        });
      if (result.event === "solo_draw" || result.event === "peel")
        this.notify("Peel! A new tile is in your rack.");
      if (result.event === "completed")
        this.notify("Bananagrams! You cleared the pool.");
      return true;
    } finally {
      this.update({ busy: false });
    }
  }
  private async place(letter: string, position: Position, id?: number) {
    if (this.tile(position)) {
      this.notify("That square already has a tile.", position);
      return false;
    }
    const tile =
      id === undefined
        ? this.state.rack.find((t) => t.letter === letter)
        : this.state.rack.find((t) => t.id === id);
    if (!tile) {
      this.notify(`No ${letter} in your rack.`, position);
      return false;
    }
    if (!(await this.act({ type: "place", letter, position }, tile.id)))
      return false;
    this.update({ history: [...this.state.history, { ...position }] });
    return true;
  }
  private async remove(positions: Position[], undo = false) {
    if (!positions.length || !(await this.act({ type: "return", positions })))
      return false;
    this.update({ history: undo ? this.state.history.slice(0, -1) : [] });
    return true;
  }
  private async handle(intent: Intent) {
    const state = this.state;
    if (intent.type !== "extendSelection") this.selectionAnchor = null;
    // A selection lasts for one action. Keep this snapshot for actions that
    // consume the region, but clear its highlight even if that action fails.
    if (state.selected.length && !["select", "selectRectangle", "selectAt", "extendSelection"].includes(intent.type))
      this.update({ selected: [] });
    switch (intent.type) {
      case "deselect":
        return;
      case "verifySelected":
        if (state.selected.length)
          await this.act({ type: "validate", positions: state.selected });
        return;
      case "cursor":
        this.backspaceAtCursor = true;
        this.update({ cursor: intent.position });
        return;
      case "toggle":
        this.update({ vertical: !state.vertical });
        return;
      case "escape":
        this.update({
          selected: [],
          selectedRack: null,
          cursor:
            state.selected.length || state.selectedRack !== null
              ? state.cursor
              : null,
        });
        return;
      case "arrow": {
        this.backspaceAtCursor = true;
        const p = state.cursor ?? { row: 0, col: 0 };
        this.update({
          cursor: {
            row: p.row + intent.offset.row,
            col: p.col + intent.offset.col,
          },
        });
        return;
      }
      case "extendSelection": {
        const from = state.cursor ?? { row: 0, col: 0 };
        this.selectionAnchor ??= from;
        const cursor = {
          row: from.row + intent.offset.row,
          col: from.col + intent.offset.col,
        };
        this.backspaceAtCursor = true;
        this.update({
          cursor,
          selected: this.rectangle(this.selectionAnchor, cursor),
          selectedRack: null,
        });
        return;
      }
      case "space":
        this.backspaceAtCursor = false;
        if (state.cursor) this.update({ cursor: this.step(state.cursor) });
        return;
      case "select":
        this.update({
          selected: intent.positions.filter((p) => this.tile(p)),
          selectedRack: null,
        });
        return;
      case "selectRectangle":
        this.update({
          selected: this.rectangle(intent.from, intent.to),
          selectedRack: null,
        });
        return;
      case "selectAt":
        this.update({
          selected: state.selected.some((p) => same(p, intent.position))
            ? state.selected
            : this.tile(intent.position)
              ? [intent.position]
              : [],
          selectedRack: null,
        });
        return;
      case "selectRack":
        this.update({
          selectedRack: state.rack.some((t) => t.id === intent.id)
            ? intent.id
            : null,
          selected: [],
        });
        return;
      case "sort":
        this.update({
          rack: [...state.rack].sort((a, b) =>
            a.letter.localeCompare(b.letter),
          ),
        });
        return;
      case "shuffle": {
        const rack = [...state.rack];
        for (let i = rack.length - 1; i > 0; i--) {
          const j = Math.floor(Math.random() * (i + 1));
          [rack[i], rack[j]] = [rack[j], rack[i]];
        }
        this.update({ rack });
        return;
      }
    }
    if (state.observation.status === "finished") {
      if (intent.type === "click") this.update({ cursor: intent.position });
      return;
    }
    switch (intent.type) {
      case "type": {
        if (!state.cursor) return;
        const letter = intent.letter.toUpperCase(),
          existing = this.tile(state.cursor);
        if (existing) {
          if (existing.letter === letter) {
            this.backspaceAtCursor = false;
            this.update({ cursor: this.step(state.cursor) });
          } else
            this.notify(
              `There is already ${existing.letter} here.`,
              state.cursor,
            );
        } else if (await this.place(letter, state.cursor)) {
          this.backspaceAtCursor = false;
          this.update({ cursor: this.step(state.cursor) });
        }
        return;
      }
      case "click": {
        this.backspaceAtCursor = true;
        const selected = state.rack.find((t) => t.id === state.selectedRack);
        if (selected && !this.tile(intent.position)) {
          if (await this.place(selected.letter, intent.position, selected.id))
            this.update({ selectedRack: null });
        }
        this.update({ cursor: intent.position });
        return;
      }
      case "rackDrop": {
        const tile = state.rack.find((t) => t.id === intent.id);
        if (tile && (await this.place(tile.letter, intent.position, tile.id)))
          this.update({ selectedRack: null });
        return;
      }
      case "backspace": {
        if (state.selected.length) {
          await this.remove(state.selected);
          return;
        }
        if (!state.cursor) return;
        const target = this.backspaceAtCursor
          ? state.cursor
          : this.step(state.cursor, -1);
        if (this.tile(target) && (await this.remove([target])))
          this.update({ cursor: target });
        return;
      }
      case "delete":
        if (state.cursor && this.tile(state.cursor))
          await this.remove([state.cursor]);
        return;
      case "undo": {
        const last = state.history.at(-1);
        if (last) await this.remove([last], true);
        return;
      }
      case "returnSelected":
        await this.remove(state.selected);
        return;
      case "dump": {
        const tile = state.rack.find((t) => t.id === state.selectedRack);
        if (tile)
          await this.act({ type: "dump", letter: tile.letter }, tile.id);
        return;
      }
      case "moveSelected":
        if (!state.selected.length || (!intent.offset.row && !intent.offset.col)) return;
        if (
          await this.act({
            type: "move",
            positions: state.selected,
            offset: intent.offset,
          })
        ) {
          this.update({
            history: [],
          });
        }
        return;
    }
  }
}
