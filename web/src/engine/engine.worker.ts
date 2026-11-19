/// <reference lib="webworker" />
import type { Request, Response, Snapshot } from "./protocol";
import { invoke, loadEngine, type BoundGame } from "./wasmAdapter";
const scope = self as unknown as DedicatedWorkerGlobalScope;
let game: BoundGame | undefined;
let module: Awaited<ReturnType<typeof loadEngine>> | undefined;
let chain = Promise.resolve();
scope.onmessage = ({ data }: MessageEvent<Request>) => {
  // Serializing here also protects initialization if a caller sends early input.
  chain = chain.then(async () => {
    const { id, session, payload } = data;
    let response: Response;
    try {
      let snapshot: Snapshot | null = null;
      if (payload.type === "dispose") {
        game?.delete();
        game = undefined;
      } else if (payload.type === "init") {
        game?.delete();
        game = undefined;
        module = await loadEngine(payload.moduleUrl);
        const loaded = module;
        game = invoke(
          loaded,
          () => new loaded.BrowserGame(payload.config, payload.words),
        );
        snapshot = {
          observation: invoke(loaded, () => game!.observe()),
          result: {
            success: true,
            error: "none",
            event: "none",
            validation: null,
          },
        };
      } else {
        if (!game) throw new Error("The game is not ready.");
        snapshot = invoke(module!, () => game!.act(payload));
      }
      response = { id, session, ok: true, snapshot };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      response = { id, session, ok: false, error: message };
    }
    scope.postMessage(response);
  });
};
