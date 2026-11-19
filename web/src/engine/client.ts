import type {
  Action,
  Config,
  Engine,
  Payload,
  Response,
  Snapshot,
} from "./protocol";
export class EngineClient implements Engine {
  private worker: Worker;
  private session = crypto.randomUUID();
  private nextId = 0;
  private closed = false;
  private pending = new Map<
    number,
    {
      resolve: (value: Snapshot | null) => void;
      reject: (error: Error) => void;
    }
  >();
  constructor(
    worker = new Worker(new URL("./engine.worker.ts", import.meta.url), {
      type: "module",
    }),
  ) {
    this.worker = worker;
    worker.onmessage = ({ data }: MessageEvent<Response>) => {
      if (data.session !== this.session) return;
      const promise = this.pending.get(data.id);
      if (!promise) return;
      this.pending.delete(data.id);
      if (data.ok) promise.resolve(data.snapshot);
      else promise.reject(new Error(data.error));
    };
    worker.onerror = () =>
      this.fail(
        new Error(
          "The game could not run. Check that the game files have been built, then start again.",
        ),
      );
    worker.onmessageerror = () =>
      this.fail(
        new Error("The game sent an unreadable response. Please start again."),
      );
  }
  private fail(error: Error) {
    this.closed = true;
    for (const item of this.pending.values()) item.reject(error);
    this.pending.clear();
    this.worker.terminate();
  }
  private request(payload: Payload) {
    if (this.closed) return Promise.reject(new Error("This game has closed."));
    return new Promise<Snapshot | null>((resolve, reject) => {
      const id = ++this.nextId;
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ id, session: this.session, payload });
    });
  }
  async init(config: Config, words: string[]) {
    const moduleUrl = new URL(
      "engine/bananagrams.js",
      new URL(import.meta.env.BASE_URL, document.baseURI),
    ).href;
    return (await this.request({ type: "init", config, words, moduleUrl }))!;
  }
  async act(action: Action) {
    return (await this.request(action))!;
  }
  dispose() {
    if (this.closed) return;
    // Give the wrapper a chance to delete its Game; terminate also frees the entire
    // wasm heap if initialization is stuck or the worker cannot acknowledge.
    const cleanup = this.request({ type: "dispose" });
    this.closed = true;
    const disposalId = this.nextId;
    for (const [id, item] of this.pending)
      if (id !== disposalId) {
        item.reject(new Error("This game has closed."));
        this.pending.delete(id);
      }
    const timer = setTimeout(
      () => this.fail(new Error("This game has closed.")),
      500,
    );
    void cleanup
      .catch(() => {})
      .finally(() => {
        clearTimeout(timer);
        this.worker.terminate();
      });
  }
}
