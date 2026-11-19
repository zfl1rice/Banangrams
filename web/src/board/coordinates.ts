import type { Position } from "../engine/protocol";
export const CELL = 48;
export type Camera = { x: number; y: number; zoom: number };
export const key = (p: Position) => `${p.row},${p.col}`;
export const same = (a: Position, b: Position) =>
  a.row === b.row && a.col === b.col;
export function toBoard(x: number, y: number, camera: Camera): Position {
  // Floor, rather than truncate, keeps negative squares aligned with the grid.
  return {
    row: Math.floor((y - camera.y) / (CELL * camera.zoom)),
    col: Math.floor((x - camera.x) / (CELL * camera.zoom)),
  };
}
export function fitBoard(
  tiles: Position[],
  width: number,
  height: number,
): Camera {
  if (!tiles.length)
    return { x: width / 2 - CELL / 2, y: height / 2 - CELL / 2, zoom: 1 };
  const rows = tiles.map((p) => p.row),
    cols = tiles.map((p) => p.col);
  const minRow = Math.min(...rows),
    maxRow = Math.max(...rows),
    minCol = Math.min(...cols),
    maxCol = Math.max(...cols);
  const zoom = Math.min(
    1.4,
    Math.max(
      0.25,
      Math.min(
        (width - 144) / ((maxCol - minCol + 1) * CELL),
        (height - 144) / ((maxRow - minRow + 1) * CELL),
      ),
    ),
  );
  return {
    zoom,
    x: width / 2 - ((minCol + maxCol + 1) * CELL * zoom) / 2,
    y: height / 2 - ((minRow + maxRow + 1) * CELL * zoom) / 2,
  };
}
export function zoomAt(
  camera: Camera,
  zoom: number,
  x: number,
  y: number,
): Camera {
  const next = Math.max(0.25, Math.min(2, zoom));
  return {
    zoom: next,
    x: x - ((x - camera.x) * next) / camera.zoom,
    y: y - ((y - camera.y) * next) / camera.zoom,
  };
}
