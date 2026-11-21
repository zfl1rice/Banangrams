export type Position = { row: number; col: number };
export type Tile = Position & { letter: string };
export type Validation = {
  valid: boolean;
  invalidWords: { text: string; positions: Position[] }[];
  components: Position[][];
  hasAtLeastTwoTiles: boolean;
};
export type Observation = {
  board: Tile[];
  rack: number[];
  poolSize: number;
  status: "active" | "finished";
  winner: number | null;
  opponents: {
    player: number;
    unplacedTileCount: number;
    totalTileCount: number;
    board: Tile[] | null;
  }[];
};
export type Config = { distribution: number[]; rackSize: number; seed: number };
export type Action =
  | { type: "place"; letter: string; position: Position }
  | { type: "return"; positions: Position[] }
  | { type: "validate"; positions: Position[] }
  | { type: "move"; positions: Position[]; offset: Position }
  | { type: "dump"; letter: string };
export type Result = {
  success: boolean;
  error: string;
  event: "none" | "solo_draw" | "peel" | "completed";
  validation: Validation | null;
};
export type Snapshot = { result: Result; observation: Observation };
export type Payload =
  | { type: "init"; config: Config; words: string[]; moduleUrl: string }
  | Action
  | { type: "dispose" };
export type Request = { id: number; session: string; payload: Payload };
export type Response = { id: number; session: string } & (
  | { ok: true; snapshot: Snapshot | null }
  | { ok: false; error: string }
);
export interface Engine {
  act(action: Action): Promise<Snapshot>;
}
