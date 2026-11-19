import type { Observation, Position, Validation } from "../engine/protocol";
export type RackTile = { id: number; letter: string };
export type UIState = {
  observation: Observation;
  cursor: Position | null;
  vertical: boolean;
  selected: Position[];
  rack: RackTile[];
  selectedRack: number | null;
  history: Position[];
  validation: Validation | null;
  notice: string;
  conflict: Position | null;
  busy: boolean;
};
let nextTile = 0;
export function reconcileRack(
  previous: RackTile[],
  counts: number[],
  consumed?: number,
): RackTile[] {
  const remaining = [...counts];
  const rack = previous.filter((tile) => {
    if (tile.id === consumed) return false;
    const index = tile.letter.charCodeAt(0) - 65;
    if (remaining[index] <= 0) return false;
    remaining[index]--;
    return true;
  });
  remaining.forEach((count, index) => {
    for (let i = 0; i < count; i++)
      rack.push({ id: ++nextTile, letter: String.fromCharCode(65 + index) });
  });
  return rack;
}
