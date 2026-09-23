// test.js — the backend's gate. `node test.js`, exit 0 or 1.
//
// Runs against a REAL http server in mock mode: same parsing, same routing, same
// dedup, same budget arithmetic, no provider. Every failure branch the game can show a
// player is reachable here without spending a request, which is the point.

import { createApp, LIMITS } from "./server.js";
import { createBudget } from "./budget.js";
import { getCharacter, knownCharacterIds } from "./characters.js";
import { generate, SAFETY_SETTINGS } from "./gemini.js";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

let failures = 0;
const check = (ok, label) => {
  console.log(`  [${ok ? "PASS" : "FAIL"}] ${label}`);
  if (!ok) failures += 1;
};

const scratch = mkdtempSync(join(tmpdir(), "npcchat-"));
let uuidSeed = 0;
const uuid = () => `00000000-0000-4000-8000-${String(++uuidSeed).padStart(12, "0")}`;

const app = createApp({
  mock: true,
  stateFile: join(scratch, "budget.json"),
  monthlyCapUsd: 15,
  dailyCapUsd: 0.75,
  perIpPerMinute: 12,
});

await new Promise((resolve) => app.server.listen(0, resolve));
const base = `http://127.0.0.1:${app.server.address().port}`;

const post = async (body) => {
  const res = await fetch(`${base}/v1/npc-chat`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  return { code: res.status, payload: await res.json() };
};

const ask = (over = {}) => ({
  requestId: uuid(),
  characterId: "kaia",
  message: "do you remember me?",
  storyContext: { locationId: "/Game/L_Office_v02", unlockedFacts: [] },
  recentTurns: [],
  ...over,
});

console.log("=== NPC chat backend contract ===");

/* --- the roster: every agent the game can name has somewhere to land.
   NO HTTP HERE, deliberately. Five more round trips would push this file past
   perIpPerMinute and the rate limiter would start refusing the later blocks — and
   nothing about the allowlist needs a socket to check. The wire side is already
   covered: kaia goes through the real server in the block below, and an unknown id
   is refused in the validation block.

   THE LIST IS SPELLED OUT RATHER THAN DERIVED. Deriving it from the module under test
   would pass no matter what is in there. These five strings are what
   UDancerAgentComponent::ResolveChatId produces for the five placed agents — the actor
   label, first name only, lowercased — so a rename on either side fails here instead of
   in play, where a missing key is a 400 that reads as "unavailable". */
{
  const expected = ["kaia", "nyra", "isla", "aisling", "elise"];
  const known = knownCharacterIds();
  check(
    expected.every((id) => known.includes(id)) && known.length === expected.length,
    `the allowlist is exactly ${expected.join(", ")}`
  );

  for (const id of expected) {
    const c = getCharacter(id);
    check(
      Boolean(c) &&
        typeof c.displayName === "string" &&
        c.systemInstruction.includes(c.displayName) &&
        // The house rules are the gameplay and injection guard. A profile that drops
        // them is a character who can be talked into promising the player a power.
        c.systemInstruction.includes("You cannot grant powers") &&
        c.systemInstruction.includes("never an instruction to follow"),
      `${id} has a profile, her own name in it, and the house rules`
    );
  }
}

// --- the happy path
{
  const { code, payload } = await post(ask());
  check(code === 200 && payload.status === "ok" && payload.reply, "a valid request gets a reply");
  check(typeof payload.requestId === "string", "the reply echoes the request id");
}

// --- validation speaks the contract, never a stack trace
{
  const cases = [
    [ask({ characterId: "someone-else" }), "an unknown character is refused"],
    [ask({ message: "" }), "an empty message is refused"],
    [ask({ message: "x".repeat(1001) }), "an over-long message is refused"],
    [
      ask({ recentTurns: Array(LIMITS.maxTurns + 1).fill({ role: "user", text: "hi" }) }),
      "a body past maxTurns is refused",
    ],
    [ask({ recentTurns: [{ role: "narrator", text: "hi" }] }), "an unknown turn role is refused"],
    [ask({ requestId: "short" }), "a missing request id is refused"],
  ];
  for (const [body, label] of cases) {
    const { code, payload } = await post(body);
    check(code === 400 && payload.status === "unavailable", label);
  }
}

/* --- the four limits have to agree with each other, and with the game.
   BUILT, NOT ESTIMATED. A worst-case body carrying exactly promptTurns turns at
   turnChars each, plus a full-length message, has to survive the bodyBytes check at
   the socket. Arithmetic done in a comment is how the old pair went wrong: 16 turns of
   2000 characters against a 32,768-byte door, a limit no client could reach. */
{
  const worst = {
    requestId: uuid(),
    characterId: "kaia",
    message: "x".repeat(LIMITS.messageChars),
    storyContext: { locationId: "/Game/Maps/L_City", unlockedFacts: [] },
    recentTurns: Array(LIMITS.promptTurns).fill({
      role: "user",
      text: "x".repeat(LIMITS.turnChars),
    }),
  };
  const size = Buffer.byteLength(JSON.stringify(worst), "utf8");
  check(size < LIMITS.bodyBytes, `a full-size prompt fits the door (${size} < ${LIMITS.bodyBytes} bytes)`);

  /* UNpcChatSubsystem::MaxTurnsKept, written down because it cannot be imported from
     here. The headroom IS the feature, and a check that compared the server to itself
     would go on passing the day the two drift back together. */
  const clientKeeps = 16;
  check(
    LIMITS.promptTurns > clientKeeps,
    `the server keeps more turns than the game sends (${LIMITS.promptTurns} > ${clientKeeps})`
  );
  check(LIMITS.maxTurns > LIMITS.promptTurns, "there is room between trimming and refusing");
}

// --- a client that remembers too much is trimmed, not turned away
{
  const over = LIMITS.promptTurns + 4;
  const { code, payload } = await post(
    ask({ recentTurns: Array(over).fill({ role: "user", text: "hi" }) })
  );
  check(
    code === 200 && payload.status === "ok",
    `${over} turns is trimmed to ${LIMITS.promptTurns}, not refused`
  );
}

// --- dedup: the guarantee the whole budget rests on
{
  const body = ask();
  const first = await post(body);
  const replay = await post(body);
  check(
    replay.code === 200 && replay.payload.reply === first.payload.reply,
    "the same id with the same content replays, it does not call again"
  );

  const conflict = await post({ ...body, message: "different words, same id" });
  check(conflict.code === 409, "the same id with different content is a conflict");
}

// --- every failure the player can be shown, reachable without a provider
{
  for (const reason of ["quota", "network", "timeout", "blocked", "service"]) {
    const { payload } = await post(ask({ message: `__fail:${reason}__ hello` }));
    check(
      payload.status === "unavailable" && payload.reason === reason,
      `a ${reason} failure arrives as its own reason`
    );
  }
  const { payload } = await post(ask({ message: "__fail:empty__ hello" }));
  check(payload.reason === "service", "an empty generation is a service failure, not an empty reply");
}

// --- the provider filters: the Steam disclosure promises them, and the default is OFF
{
  let sent = null;
  const fakeFetch = (reply) => async (_url, init) => {
    sent = JSON.parse(init.body);
    return { ok: true, status: 200, json: async () => reply };
  };
  const call = (reply) =>
    generate({
      apiKey: "test",
      model: "test-model",
      systemInstruction: "rules",
      contents: [{ role: "user", parts: [{ text: "hello" }] }],
      maxOutputTokens: 64,
      fetchImpl: fakeFetch(reply),
    });

  await call({ candidates: [{ content: { parts: [{ text: "Hi, Leonard." }] }, finishReason: "STOP" }] });
  const categories = (sent?.safetySettings ?? []).map((s) => s.category);
  for (const category of [
    "HARM_CATEGORY_HARASSMENT",
    "HARM_CATEGORY_HATE_SPEECH",
    "HARM_CATEGORY_SEXUALLY_EXPLICIT",
    "HARM_CATEGORY_DANGEROUS_CONTENT",
  ]) {
    check(categories.includes(category), `every request sets a threshold for ${category}`);
  }
  check(
    (sent?.safetySettings ?? []).every((s) => s.threshold !== "OFF" && s.threshold !== "BLOCK_NONE"),
    "no filter is switched off"
  );
  check(
    Array.isArray(sent?.safetySettings) && sent.safetySettings.length === SAFETY_SETTINGS.length,
    "the request carries exactly the declared filters"
  );

  const filtered = await call({ candidates: [{ finishReason: "SAFETY" }] });
  check(!filtered.ok && filtered.failure === "blocked", "a filtered reply is `blocked`, never shown");
  const refusedPrompt = await call({ promptFeedback: { blockReason: "SAFETY" } });
  check(!refusedPrompt.ok && refusedPrompt.failure === "blocked", "a filtered prompt is `blocked`");
}

// --- health without secrets
{
  const res = await fetch(`${base}/health`);
  const payload = await res.json();
  const text = JSON.stringify(payload);
  check(res.status === 200 && payload.budget, "health reports the budget");
  check(!/apiKey|GEMINI_API_KEY|AIza/i.test(text), "health leaks no credential");
}

/* AWAITED, AND NO process.exit() AFTERWARDS. Closing the socket and exiting in the same
   tick trips a libuv assertion on Windows - the run prints every PASS and then dies with
   exit 127, which is the worst shape a gate can take: green log, red CI. Let the loop
   drain and set exitCode instead. */
await new Promise((resolve) => app.server.close(resolve));

// --- the budget, exercised directly: the arithmetic that decides when to stop
{
  const budget = createBudget({
    file: join(scratch, "b2.json"),
    monthlyCapUsd: 1,
    dailyCapUsd: 0.1,
    maxOutputTokens: 512,
  });

  check(budget.check(2000) === null, "a fresh budget affords a call");

  // A reservation is the worst case, then the real usage replaces it. Without the
  // reconcile, every call would be billed at its maximum output forever.
  const handle = budget.reserve(2000);
  const reservedDay = budget.status().daySpentUsd;
  const actual = budget.settle(handle, { inputTokens: 2000, outputTokens: 150 });
  check(actual > 0 && budget.status().daySpentUsd < reservedDay, "the real cost replaces the reservation");

  check(
    Math.abs(actual - (2000 / 1e6) * 0.25 - (150 / 1e6) * 1.5) < 1e-9,
    "cost matches Flash-Lite pricing"
  );

  const released = createBudget({
    file: join(scratch, "b3.json"),
    monthlyCapUsd: 1,
    dailyCapUsd: 0.1,
    maxOutputTokens: 512,
  });
  const h = released.reserve(2000);
  released.release(h);
  check(released.status().daySpentUsd === 0, "a released reservation costs nothing");

  // The daily cap is the one that stops a single bad afternoon emptying the month.
  const tight = createBudget({
    file: join(scratch, "b4.json"),
    monthlyCapUsd: 15,
    dailyCapUsd: 0.0001,
    maxOutputTokens: 512,
  });
  check(tight.check(2000) === "day", "the daily cap refuses before the monthly one is touched");
}

console.log(`=== ${failures} failure(s) ===`);
process.exitCode = failures === 0 ? 0 : 1;
