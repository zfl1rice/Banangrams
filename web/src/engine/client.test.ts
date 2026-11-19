import { expect, it } from "vitest";
import { EngineClient } from "./client";
import type { Request, Response, Snapshot } from "./protocol";
class WorkerStub {
  onmessage: ((event: MessageEvent<Response>) => void) | null = null;
  onerror: (() => void) | null = null;
  onmessageerror: (() => void) | null = null;
  requests: Request[] = [];
  terminated = false;
  postMessage(request: Request) {
    this.requests.push(request);
  }
  terminate() {
    this.terminated = true;
  }
  respond(response: Response) {
    this.onmessage?.({ data: response } as MessageEvent<Response>);
  }
}
const snapshot = {
  observation: {
    board: [],
    rack: [],
    opponents: [],
    poolSize: 0,
    status: "active",
    winner: null,
  },
  result: { success: true, error: "none", event: "none", validation: null },
} satisfies Snapshot;
it("matches request IDs and rejects stale-session responses", async () => {
  const worker = new WorkerStub();
  const client = new EngineClient(worker as unknown as Worker);
  let resolved = false;
  const first = client.act({ type: "dump", letter: "A" }).then((value) => {
    resolved = true;
    return value;
  });
  const request = worker.requests[0];
  worker.respond({ ...request, session: "earlier-game", ok: true, snapshot });
  await Promise.resolve();
  expect(resolved).toBe(false);
  worker.respond({ ...request, ok: true, snapshot });
  expect(await first).toEqual(snapshot);
  client.dispose();
  worker.respond({ ...worker.requests.at(-1)!, ok: true, snapshot: null });
});
it("rejects outstanding requests on teardown and terminates after native cleanup", async () => {
  const worker = new WorkerStub();
  const client = new EngineClient(worker as unknown as Worker);
  const pending = client.act({ type: "dump", letter: "A" });
  const rejected = expect(pending).rejects.toThrow("closed");
  client.dispose();
  await rejected;
  expect(worker.requests.at(-1)!.payload.type).toBe("dispose");
  worker.respond({ ...worker.requests[0], ok: true, snapshot });
  worker.respond({ ...worker.requests.at(-1)!, ok: true, snapshot: null });
  await Promise.resolve();
  await Promise.resolve();
  expect(worker.terminated).toBe(true);
  await expect(client.act({ type: "dump", letter: "A" })).rejects.toThrow(
    "closed",
  );
});
it("propagates worker errors instead of leaving actions pending", async () => {
  const worker = new WorkerStub();
  const client = new EngineClient(worker as unknown as Worker);
  const pending = client.act({ type: "dump", letter: "A" });
  const rejected = expect(pending).rejects.toThrow("could not run");
  worker.onerror!();
  await rejected;
  expect(worker.terminated).toBe(true);
});
