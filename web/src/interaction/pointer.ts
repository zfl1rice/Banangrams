import type { Camera } from "../board/coordinates";
import type { Position } from "../engine/protocol";
export type Gesture = {
  pointerId: number;
  kind: "pan" | "rectangle" | "board" | "rack";
  startX: number;
  startY: number;
  x: number;
  y: number;
  moved: boolean;
  origin: Position;
  camera: Camera;
  rackId?: number;
  letter?: string;
};
export const movedEnough = (gesture: Gesture, x: number, y: number) =>
  Math.hypot(x - gesture.startX, y - gesture.startY) >= 5;
