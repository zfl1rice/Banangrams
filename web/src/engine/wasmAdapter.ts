import type { Action, Config, Observation, Snapshot } from "./protocol";
export interface BoundGame {
  observe(): Observation;
  act(action: Action): Snapshot;
  delete(): void;
}
interface Module {
  BrowserGame: new (config: Config, words: string[]) => BoundGame;
  getExceptionMessage(error: number): string[];
  incrementExceptionRefcount(error: number): void;
  decrementExceptionRefcount(error: number): void;
  stackSave(): number;
  stackRestore(stack: number): void;
}
export function invoke<T>(module: Module, call: () => T): T {
  const stack = module.stackSave();
  try {
    return call();
  } catch (error) {
    module.stackRestore(stack);
    if (typeof error !== "number") throw error;
    // Emscripten 4's JS exception ABI starts with refcount zero. JS catches
    // must release the exception themselves, as no C++ catch runs here.
    module.incrementExceptionRefcount(error);
    try {
      throw new Error(module.getExceptionMessage(error).join(": "));
    } finally {
      module.decrementExceptionRefcount(error);
    }
  }
}
export async function loadEngine(moduleUrl: string): Promise<Module> {
  // The generated ES module stays next to its wasm; URLs are relative to the app,
  // not the hashed worker chunk, so nested production paths also work.
  const { default: createModule } = await import(/* @vite-ignore */ moduleUrl);
  return createModule({
    locateFile: (name: string) => new URL(name, moduleUrl).href,
  });
}
