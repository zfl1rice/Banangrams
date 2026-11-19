import type { Intent } from "./controller";
export function keyboardIntent(
  event: Pick<
    KeyboardEvent,
    "key" | "ctrlKey" | "metaKey" | "altKey" | "shiftKey"
  >,
): Intent | null {
  if (event.altKey) return null;
  if (event.ctrlKey || event.metaKey)
    return event.key.toLowerCase() === "z" && !event.shiftKey
      ? { type: "undo" }
      : null;
  const offsets: Record<string, { row: number; col: number }> = {
    ArrowLeft: { row: 0, col: -1 },
    ArrowRight: { row: 0, col: 1 },
    ArrowUp: { row: -1, col: 0 },
    ArrowDown: { row: 1, col: 0 },
  };
  if (offsets[event.key]) return { type: "arrow", offset: offsets[event.key] };
  const keys = {
    Tab: "toggle",
    " ": "space",
    Backspace: "backspace",
    Delete: "delete",
    Escape: "escape",
  } as const;
  if (event.key in keys) return { type: keys[event.key as keyof typeof keys] };
  return /^[a-z]$/i.test(event.key)
    ? { type: "type", letter: event.key.toUpperCase() }
    : null;
}
