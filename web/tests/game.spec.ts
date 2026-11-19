import { expect, test, type Page } from "@playwright/test";
async function custom(page: Page, supply: string, rack: number, words: string) {
  await page.goto("/");
  await page
    .getByLabel("Word list (.txt)")
    .setInputFiles({
      name: "test-vocabulary.txt",
      mimeType: "text/plain",
      buffer: Buffer.from(words),
    });
  await page.getByText("Table settings", { exact: true }).click();
  await page.getByLabel("Starting rack size").fill(String(rack));
  await page.getByLabel("Tile supply (one letter per tile)").fill(supply);
  await page.getByRole("button", { name: "Start Solo", exact: true }).click();
  await expect(page.getByTestId("board")).toBeVisible();
}
async function cursorCenter(page: Page) {
  const box = await page.getByTestId("cursor").boundingBox();
  return { x: box!.x + box!.width / 2, y: box!.y + box!.height / 2 };
}
async function drag(
  page: Page,
  from: { x: number; y: number },
  to: { x: number; y: number },
) {
  await page.mouse.move(from.x, from.y);
  await page.mouse.down();
  await page.mouse.move(to.x, to.y, { steps: 8 });
  await page.mouse.up();
}
test("actual wasm worker: rapid CAT finishes and leaves board inspectable", async ({
  page,
}) => {
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  const wasm: string[] = [];
  page.on("response", (response) => {
    if (response.url().endsWith(".wasm")) wasm.push(response.url());
  });
  await page.goto("/");
  await page.screenshot({ path: "test-results/start-screen.png" });
  await page.getByRole("button", { name: "Try development table" }).click();
  await expect(page.getByTestId("board")).toBeVisible();
  await page.keyboard.type("cat", { delay: 0 });
  await expect(page.getByTestId("tile-0-2")).toHaveText("T");
  await expect(
    page.getByText("You cleared the pool.", { exact: false }).first(),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "Undo placement" }),
  ).toBeDisabled();
  await page.keyboard.press("Backspace");
  await expect(page.getByTestId("tile-0-2")).toBeVisible();
  await page.getByRole("button", { name: "Zoom in" }).click();
  await page.getByRole("button", { name: "Fit board" }).click();
  await page.screenshot({ path: "test-results/finished-board.png" });
  expect(wasm.length).toBeGreaterThan(0);
  expect(errors).toEqual([]);
});
test("actual wasm: automatic peel, undo preserving draw, dump and history", async ({
  page,
}) => {
  await custom(page, "AAAAA", 2, "AA\nAAA\nAAAA\nAAAAA");
  await page.keyboard.type("aa");
  await expect(page.getByTestId("pool-count")).toHaveText("2");
  await expect(page.getByRole("status")).toContainText("Peel");
  await page.keyboard.press("Control+z");
  await expect(page.locator(".rack-tile")).toHaveCount(2);
  await expect(page.getByTestId("pool-count")).toHaveText("2");
  await expect(page.getByTestId("cursor")).toHaveAttribute("data-col", "2");
  await page.locator(".rack-tile").first().click();
  await page.getByRole("button", { name: "Dump selected" }).click();
  await expect(page.getByTestId("pool-count")).toHaveText("0");
  await expect(page.locator(".rack-tile")).toHaveCount(4);
  await page.getByRole("button", { name: "Undo placement" }).click();
  await expect(page.locator(".rack-tile")).toHaveCount(5);
  await page.locator(".rack-tile").first().click();
  await expect(
    page.getByRole("button", { name: "Dump selected" }),
  ).toBeEnabled();
  await page.getByRole("button", { name: "Dump selected" }).click();
  await expect(page.locator(".rack-tile")).toHaveCount(5);
});
test("actual wasm: invalid highlights clear after edit; matching crossing consumes no tile", async ({
  page,
}) => {
  await custom(page, "CAT", 3, "CAT");
  await page.keyboard.type("cta");
  await expect(page.locator(".board-tile.invalid")).toHaveCount(3);
  await page.keyboard.press("Backspace");
  await expect(page.locator(".board-tile.invalid")).toHaveCount(0);
  await page.keyboard.press("ArrowLeft");
  await page.keyboard.press("ArrowLeft");
  await page.keyboard.type("c");
  await expect(page.locator(".rack-tile")).toHaveCount(1);
  await expect(page.getByTestId("cursor")).toHaveAttribute("data-col", "1");
  await page.keyboard.type("a");
  await expect(page.getByTestId("cursor")).toHaveAttribute("data-col", "1");
  await expect(page.getByTestId("tile-0-1")).toHaveText("T");
});
test("pointer controls: rack drop, group move, rejected collision, return, pan and zoom", async ({
  page,
}) => {
  await custom(page, "CATRE", 5, "CAT\nARE\nAT");
  const origin = await cursorCenter(page);
  const cBox = await page
    .getByRole("button", { name: "Select C", exact: true })
    .boundingBox();
  await drag(page, { x: cBox!.x + 20, y: cBox!.y + 20 }, origin);
  await expect(page.getByTestId("tile-0-0")).toHaveText("C");
  await page.keyboard.press("ArrowRight");
  await page.keyboard.type("a");
  await expect(page.getByTestId("tile-0-1")).toHaveText("A");
  // Rectangle-select both, then translate in one drop to negative coordinates.
  await page.keyboard.down("Shift");
  await drag(
    page,
    { x: origin.x - 22, y: origin.y - 22 },
    { x: origin.x + 70, y: origin.y + 22 },
  );
  await page.keyboard.up("Shift");
  await expect(page.locator(".board-tile.selected")).toHaveCount(2);
  await drag(page, origin, { x: origin.x - 96, y: origin.y - 48 });
  await expect(page.getByTestId("tile--1--2")).toHaveText("C");
  await expect(page.getByTestId("tile--1--1")).toHaveText("A");
  // Place an unselected blocker, then attempt to move the selected group onto it.
  await page.getByRole("button", { name: "Select T", exact: true }).click();
  await page.mouse.click(origin.x, origin.y - 48);
  await expect(page.getByTestId("tile--1-0")).toHaveText("T");
  await page.keyboard.down("Shift");
  await drag(
    page,
    { x: origin.x - 118, y: origin.y - 70 },
    { x: origin.x - 26, y: origin.y - 26 },
  );
  await page.keyboard.up("Shift");
  await drag(
    page,
    { x: origin.x - 96, y: origin.y - 48 },
    { x: origin.x - 48, y: origin.y - 48 },
  );
  await expect(page.getByTestId("tile--1--2")).toHaveText("C");
  await expect(page.getByTestId("tile--1-0")).toHaveText("T");
  await page.getByRole("button", { name: "Return selected (2)" }).click();
  await expect(page.locator(".board-tile")).toHaveCount(1);
  const rack = await page.getByTestId("rack").boundingBox();
  await drag(
    page,
    { x: origin.x, y: origin.y - 48 },
    { x: rack!.x + 200, y: rack!.y + 70 },
  );
  await expect(page.locator(".board-tile")).toHaveCount(0);
  await drag(
    page,
    { x: origin.x + 150, y: origin.y },
    { x: origin.x + 210, y: origin.y + 60 },
  );
  await page.getByRole("button", { name: "Zoom in" }).click();
  await page.getByRole("button", { name: "Fit board" }).click();
  await page.screenshot({ path: "test-results/solo-table.png" });
});
test("restart confirmation, cancel, and fresh worker", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "Try development table" }).click();
  await expect(page.getByTestId("board")).toBeVisible();
  await page.keyboard.type("c");
  await page.getByRole("button", { name: "New game", exact: true }).click();
  await page.getByRole("button", { name: "Keep playing" }).click();
  await expect(page.getByTestId("tile-0-0")).toHaveText("C");
  await page.getByRole("button", { name: "New game", exact: true }).click();
  await page
    .getByRole("dialog")
    .getByRole("button", { name: "New game" })
    .click();
  await page.getByRole("button", { name: "Try development table" }).click();
  await expect(page.getByTestId("board")).toBeVisible();
  await expect(page.locator(".board-tile")).toHaveCount(0);
  await expect(page.locator(".rack-tile")).toHaveCount(3);
});
test("C++ initialization errors reach the start panel", async ({ page }) => {
  await page.goto("/");
  await page
    .getByLabel("Word list (.txt)")
    .setInputFiles({
      name: "bad.txt",
      mimeType: "text/plain",
      buffer: Buffer.from("BAD-WORD"),
    });
  await page.getByRole("button", { name: "Start Solo", exact: true }).click();
  await expect(page.getByRole("alert")).toContainText(
    "malformed lexicon entry",
  );
});

test("nested static mount resolves production worker and wasm relative to the app", async ({
  page,
  context,
}) => {
  const requests: string[] = [];
  // Map a nested static mount to the same built files, like a static server alias.
  await context.route("**/table/**", async (route) => {
    requests.push(route.request().url());
    const response = await route.fetch({
      url: route.request().url().replace("/table/", "/"),
    });
    await route.fulfill({ response });
  });
  await page.goto("/table/");
  await page.getByRole("button", { name: "Try development table" }).click();
  await expect(page.getByTestId("board")).toBeVisible();
  await page.keyboard.type("cat");
  await expect(page.getByTestId("tile-0-2")).toHaveText("T");
  expect(
    requests.some((url) => url.endsWith("/table/engine/bananagrams.wasm")),
  ).toBe(true);
  expect(
    requests.some((url) => url.includes("/table/assets/engine.worker-")),
  ).toBe(true);
});

test("pan and zoom preserve signed placement coordinates; Escape cancels drag; Tab stays scoped", async ({
  page,
}) => {
  await custom(page, "CATARE", 6, "CAT\nARE\nAT");
  let origin = await cursorCenter(page);
  await drag(
    page,
    { x: origin.x + 150, y: origin.y },
    { x: origin.x + 210, y: origin.y + 30 },
  );
  await page.getByRole("button", { name: "Zoom in" }).click();
  origin = await cursorCenter(page);
  await page.getByRole("button", { name: "Select C", exact: true }).click();
  await page.mouse.click(origin.x - 120, origin.y - 60);
  const tile = page.getByTestId("tile--1--2");
  await expect(tile).toHaveText("C");
  const before = await tile.boundingBox();
  await page.keyboard.type("ca");
  await expect(page.getByTestId("tile--1--1")).toHaveText("A");
  expect(await tile.boundingBox()).toEqual(before);
  await page.mouse.move(origin.x - 60, origin.y - 60);
  await page.mouse.down();
  await page.mouse.move(origin.x, origin.y, { steps: 5 });
  await expect(page.locator(".preview")).toHaveCount(1);
  await page.keyboard.press("Escape");
  await page.mouse.up();
  await expect(page.locator(".preview")).toHaveCount(0);
  await expect(page.getByTestId("tile--1--1")).toHaveText("A");
  await page.getByTestId("board").focus();
  await page.keyboard.press("Tab");
  await expect(
    page.getByRole("button", { name: "Type vertically" }),
  ).toBeVisible();
  await page.getByRole("button", { name: "Shuffle", exact: true }).focus();
  await page.keyboard.press("Tab");
  await expect(
    page.getByRole("button", { name: "Type vertically" }),
  ).toBeVisible();
});
