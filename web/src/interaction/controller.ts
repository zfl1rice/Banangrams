import { same } from "../board/coordinates";
import type { Action, Engine, Observation, Position } from "../engine/protocol";
import { reconcileRack, type UIState } from "../state/uiState";
export type Intent =
  | { type: "type"; letter: string }
  | { type: "click"; position: Position }
  | { type: "cursor"; position: Position | null }
  | { type: "arrow"; offset: Position }
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
        | "sort"
        | "shuffle";
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
  private step(position: Position, distance = 1): Position {
    return {
      row: position.row + (this.state.vertical ? distance : 0),
      col: position.col + (this.state.vertical ? 0 : distance),
    };
  }
  private async act(action: Action, consumed?: number): Promise<boolean> {
    if (this.state.observation.status === "finished") return false;
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
        this.update({ validation: result.validation });
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
    switch (intent.type) {
      case "cursor":
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
        const p = state.cursor ?? { row: 0, col: 0 };
        this.update({
          cursor: {
            row: p.row + intent.offset.row,
            col: p.col + intent.offset.col,
          },
        });
        return;
      }
      case "space":
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
          selected: state.observation.board.filter(
            (p) =>
              p.row >= Math.min(intent.from.row, intent.to.row) &&
              p.row <= Math.max(intent.from.row, intent.to.row) &&
              p.col >= Math.min(intent.from.col, intent.to.col) &&
              p.col <= Math.max(intent.from.col, intent.to.col),
          ),
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
          if (existing.letter === letter)
            this.update({ cursor: this.step(state.cursor) });
          else
            this.notify(
              `There is already ${existing.letter} here.`,
              state.cursor,
            );
        } else if (await this.place(letter, state.cursor))
          this.update({ cursor: this.step(state.cursor) });
        return;
      }
      case "click": {
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
        if (!state.cursor) return;
        const previous = this.step(state.cursor, -1);
        if (!this.tile(previous) || (await this.remove([previous])))
          this.update({ cursor: previous });
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
        if (!intent.offset.row && !intent.offset.col) return;
        if (
          await this.act({
            type: "move",
            positions: state.selected,
            offset: intent.offset,
          })
        ) {
          this.update({
            history: [],
            selected: state.selected.map((p) => ({
              row: p.row + intent.offset.row,
              col: p.col + intent.offset.col,
            })),
          });
        }
        return;
    }
  }
}
