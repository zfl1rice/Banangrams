import { describe, expect, it } from "vitest";
import { Controller } from "./controller";
import { keyboardIntent } from "./keyboard";
import { fitBoard, toBoard, zoomAt } from "../board/coordinates";
import { reconcileRack } from "../state/uiState";
import type {
  Action,
  Engine,
  Observation,
  Snapshot,
  Validation,
} from "../engine/protocol";
const observation = (letters = "CAT"): Observation => ({
  board: [],
  rack: Array.from(
    { length: 26 },
    (_, i) => [...letters].filter((c) => c.charCodeAt(0) === i + 65).length,
  ),
  poolSize: 10,
  status: "active",
  winner: null,
  opponents: [],
});
// This transport fixture exercises ordering, not rules: prescribed responses are
// delivered after a delay. Actual rules are covered by C++ and browser tests.
function setup(initial: Observation, responses: Snapshot[]) {
  const calls: Action[] = [];
  const engine: Engine = {
    async act(action) {
      calls.push(action);
      await new Promise((resolve) => setTimeout(resolve, 2));
      const next = responses.shift();
      if (!next) throw new Error("Unexpected action");
      return next;
    },
  };
  return { controller: new Controller(engine, initial), calls };
}
function reply(
  view: Observation,
  success = true,
  validation: Validation | null = null,
  event: Snapshot["result"]["event"] = "none",
): Snapshot {
  return {
    observation: view,
    result: {
      success,
      error: success ? "none" : "invalid_move",
      event,
      validation,
    },
  };
}
describe("ordered interaction controller", () => {
  it("places rapid CAT at successive confirmed positions", async () => {
    const a = observation("AT");
    a.board = [{ row: 0, col: 0, letter: "C" }];
    const b = observation("T");
    b.board = [...a.board, { row: 0, col: 1, letter: "A" }];
    const c = observation("");
    c.board = [...b.board, { row: 0, col: 2, letter: "T" }];
    const { controller, calls } = setup(observation(), [
      reply(a),
      reply(b),
      reply(c),
    ]);
    await Promise.all(
      [..."cat"].map((letter) => controller.dispatch({ type: "type", letter })),
    );
    expect(calls.map((c) => c.type === "place" && c.position.col)).toEqual([
      0, 1, 2,
    ]);
    expect(controller.getSnapshot().cursor).toEqual({ row: 0, col: 3 });
    controller.dispose();
  });
  it("orders clicks, direction changes, arrows and undo behind pending placement", async () => {
    const a = observation("AT");
    a.board = [{ row: 0, col: 0, letter: "C" }];
    const b = observation("T");
    b.board = [...a.board, { row: -2, col: 5, letter: "A" }];
    const { controller, calls } = setup(observation(), [
      reply(a),
      reply(b),
      reply(a),
    ]);
    void controller.dispatch({ type: "type", letter: "C" });
    void controller.dispatch({ type: "click", position: { row: -2, col: 5 } });
    void controller.dispatch({ type: "toggle" });
    void controller.dispatch({ type: "type", letter: "A" });
    void controller.dispatch({ type: "arrow", offset: { row: 0, col: 1 } });
    await controller.dispatch({ type: "undo" });
    expect(calls[1]).toEqual({
      type: "place",
      letter: "A",
      position: { row: -2, col: 5 },
    });
    expect(controller.getSnapshot().cursor).toEqual({ row: -1, col: 6 });
    expect(controller.getSnapshot().history).toHaveLength(1);
    controller.dispose();
  });
  it("crosses matching letters without inventory or action and blocks different/unavailable letters", async () => {
    const initial = observation("T");
    initial.board = [{ row: 0, col: 0, letter: "C" }];
    const { controller, calls } = setup(initial, []);
    await controller.dispatch({ type: "type", letter: "A" });
    expect(controller.getSnapshot().cursor?.col).toBe(0);
    await controller.dispatch({ type: "type", letter: "C" });
    await controller.dispatch({ type: "type", letter: "Z" });
    expect(controller.getSnapshot().cursor?.col).toBe(1);
    expect(controller.getSnapshot().history).toEqual([]);
    expect(calls).toEqual([]);
    controller.dispose();
  });
  it("failed placement preserves cursor, history, and confirmed feedback", async () => {
    const { controller } = setup(observation(), [reply(observation(), false)]);
    await controller.dispatch({ type: "type", letter: "C" });
    expect(controller.getSnapshot().cursor?.col).toBe(0);
    expect(controller.getSnapshot().history).toEqual([]);
    controller.dispose();
  });
  it("peel and dump retain placement history; undo retains draws and cursor", async () => {
    const initial = observation("T");
    initial.board = [
      { row: 0, col: -2, letter: "C" },
      { row: 0, col: -1, letter: "A" },
    ];
    const peeled = observation("E");
    peeled.poolSize = 9;
    peeled.board = [...initial.board, { row: 0, col: 0, letter: "T" }];
    const dumped = { ...peeled, rack: observation("ARR").rack, poolSize: 7 };
    const undone = {
      ...dumped,
      board: initial.board,
      rack: observation("ARRT").rack,
    };
    const { controller } = setup(initial, [
      reply(peeled, true, null, "solo_draw"),
      reply(dumped),
      reply(undone),
    ]);
    await controller.dispatch({ type: "type", letter: "T" });
    await controller.dispatch({
      type: "selectRack",
      id: controller.getSnapshot().rack[0].id,
    });
    await controller.dispatch({ type: "dump" });
    expect(controller.getSnapshot().history).toHaveLength(1);
    await controller.dispatch({ type: "undo" });
    expect(controller.getSnapshot().observation).toEqual(undone);
    expect(controller.getSnapshot().cursor?.col).toBe(1);
    controller.dispose();
  });
  it("keeps invalid placements successful; clears stale reports only on successful edits", async () => {
    const invalid: Validation = {
      valid: false,
      hasAtLeastTwoTiles: false,
      components: [[{ row: 0, col: 0 }]],
      invalidWords: [],
    };
    const placed = observation("");
    placed.board = [{ row: 0, col: 0, letter: "C" }];
    const { controller } = setup(observation("C"), [
      reply(placed, true, invalid),
      reply(placed, false),
      reply(observation("C")),
    ]);
    await controller.dispatch({ type: "type", letter: "C" });
    expect(controller.getSnapshot().cursor?.col).toBe(1);
    expect(controller.getSnapshot().validation).toEqual(invalid);
    await controller.dispatch({ type: "undo" });
    expect(controller.getSnapshot().validation).toEqual(invalid);
    expect(controller.getSnapshot().history).toHaveLength(1);
    await controller.dispatch({ type: "undo" });
    expect(controller.getSnapshot().validation).toBeNull();
    controller.dispose();
  });
  it("uses one atomic move and clears history only on success", async () => {
    const a = observation("AT");
    a.board = [{ row: 0, col: 0, letter: "C" }];
    const b = { ...a, board: [{ row: -1, col: 0, letter: "C" }] };
    const { controller, calls } = setup(observation(), [
      reply(a),
      reply(a, false),
      reply(b),
    ]);
    await controller.dispatch({ type: "type", letter: "C" });
    await controller.dispatch({
      type: "selectAt",
      position: { row: 0, col: 0 },
    });
    await controller.dispatch({
      type: "moveSelected",
      offset: { row: 1, col: 0 },
    });
    expect(controller.getSnapshot().history).toHaveLength(1);
    await controller.dispatch({
      type: "moveSelected",
      offset: { row: -1, col: 0 },
    });
    expect(calls.map((c) => c.type)).toEqual(["place", "move", "move"]);
    expect(controller.getSnapshot().history).toEqual([]);
    controller.dispose();
  });
  it("does not apply responses or queued commands after disposal", async () => {
    const a = observation("AT");
    a.board = [{ row: 0, col: 0, letter: "C" }];
    const { controller, calls } = setup(observation(), [reply(a)]);
    const pending = controller.dispatch({ type: "type", letter: "C" });
    await new Promise((resolve) => setTimeout(resolve, 0));
    const next = controller.dispatch({ type: "type", letter: "A" });
    controller.dispose();
    await Promise.all([pending, next]);
    expect(calls).toHaveLength(1);
    expect(controller.getSnapshot().observation.board).toEqual([]);
  });
  it("finished games allow inspection but no mutations", async () => {
    const initial = observation();
    initial.status = "finished";
    const { controller, calls } = setup(initial, []);
    await controller.dispatch({ type: "type", letter: "C" });
    await controller.dispatch({ type: "undo" });
    await controller.dispatch({ type: "click", position: { row: -1, col: 5 } });
    expect(calls).toEqual([]);
    expect(controller.getSnapshot().cursor).toEqual({ row: -1, col: 5 });
    controller.dispose();
  });
});
it("preserves duplicate tile identities and removes the consumed displayed tile", () => {
  const rack = reconcileRack([], observation("AAC").rack);
  const selected = rack[1];
  const next = reconcileRack(rack, observation("ACE").rack, selected.id);
  expect(next.map((t) => t.letter)).toEqual(["A", "C", "E"]);
  expect(next[0].id).toBe(rack[0].id);
  expect(next.some((t) => t.id === selected.id)).toBe(false);
});
it("converts negative coordinates and preserves zoom anchor", () => {
  const camera = { x: 123, y: -45, zoom: 0.75 };
  expect(toBoard(122, -46, camera)).toEqual({ row: -1, col: -1 });
  const changed = zoomAt(camera, 1.5, 300, 200);
  expect(toBoard(300, 200, changed)).toEqual(toBoard(300, 200, camera));
  expect(fitBoard([], 800, 600)).toEqual({ x: 376, y: 276, zoom: 1 });
});
it("does not reinterpret modified shortcuts as letters", () => {
  const event = {
    key: "a",
    ctrlKey: true,
    metaKey: false,
    altKey: false,
    shiftKey: false,
  };
  expect(keyboardIntent(event)).toBeNull();
  expect(keyboardIntent({ ...event, key: "z" })).toEqual({ type: "undo" });
  expect(keyboardIntent({ ...event, key: "z", shiftKey: true })).toBeNull();
});
