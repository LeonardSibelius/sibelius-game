// test.js — the backend's gate. `node test.js`, exit 0 or 1.
//
// Runs against a REAL http server in mock mode: same parsing, same routing, same
// dedup, same budget arithmetic, no provider. Every failure branch the game can show a
// player is reachable here without spending a request, which is the point.

import { createApp, LIMITS, CARE_REPLY } from "./server.js";
import { createBudget } from "./budget.js";
import { getCharacter, knownCharacterIds } from "./characters.js";
import { generate, SAFETY_SETTINGS } from "./gemini.js";
import { STYLES, FOOLISH_ANGLES, systemInstructionFor } from "./styles.js";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

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
    // The world list rides in the house rules, so every agent — in every style — knows
    // what exists and what was cut.
    check(
      c.systemInstruction.includes("THE PLACES THAT EXIST") && /GONE[^]*Carousel of\s+Fates/.test(c.systemInstruction),
      `${id} knows which places exist, and that the Carousel is gone`
    );
  }
}

/* --- the style switch. Normal is the profiles exactly as written; foolish adds to them and
   never replaces them — the house rules are the injection guard and must survive any style. */
{
  check(app.config.chatStyle === "normal", "the chat style is normal by default");
  for (const id of knownCharacterIds()) {
    const c = getCharacter(id);
    check(systemInstructionFor(c, id, "normal") === c.systemInstruction, `${id}: normal is her profile, unchanged`);
    const f = systemInstructionFor(c, id, "foolish");
    check(
      f.startsWith(c.systemInstruction) && f.length > c.systemInstruction.length &&
        f.includes(FOOLISH_ANGLES[id]) && f.includes("You cannot grant powers"),
      `${id}: foolish keeps her profile and house rules, and adds her angle`
    );
  }
  const typo = createApp({ mock: true, stateFile: join(scratch, "typo.json"), chatStyle: "foolsh" });
  check(typo.config.chatStyle === "normal", "a misspelt style falls back to normal");
  check(STYLES.foolish.temperature > STYLES.normal.temperature, "foolish runs hotter than normal");
}

/* --- every file the server loads is in the image.
   The Dockerfile copies a LIST of files, so a new module that is not added to it passes
   every test here and crashes the live server on boot — which is exactly what distress.js
   did on 2026-09-23: ERR_MODULE_NOT_FOUND, ten restarts, chat down. This follows the
   imports from server.js the way Node will, and checks each one against the COPY line. */
{
  const here = dirname(fileURLToPath(import.meta.url));
  const copyLine = readFileSync(join(here, "Dockerfile"), "utf8").match(/^COPY\b.*$/m)?.[0] ?? "";
  const copied = new Set(copyLine.split(/\s+/).filter((w) => /\.(js|json)$/.test(w)));
  const needed = new Set();
  const visit = (file) => {
    if (needed.has(file)) return;
    needed.add(file);
    const src = readFileSync(join(here, file), "utf8");
    for (const [, dep] of src.matchAll(/from\s+["']\.\/([\w.-]+\.js)["']/g)) visit(dep);
  };
  visit("server.js");
  const missing = [...needed].filter((f) => !copied.has(f));
  check(missing.length === 0, `the Dockerfile copies every module server.js loads${missing.length ? ` — MISSING: ${missing.join(", ")}` : ""}`);
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

// --- the provider filters: OFF in production until a replacement is measured (gemini.js)
{
  let sent = null;
  const fakeFetch = (reply) => async (_url, init) => {
    sent = JSON.parse(init.body);
    return { ok: true, status: 200, json: async () => reply };
  };
  const call = (reply, safetySettings) =>
    generate({
      apiKey: "test",
      model: "test-model",
      systemInstruction: "rules",
      contents: [{ role: "user", parts: [{ text: "hello" }] }],
      maxOutputTokens: 64,
      safetySettings,
      fetchImpl: fakeFetch(reply),
    });
  const hello = { candidates: [{ content: { parts: [{ text: "Hi, Leonard." }] }, finishReason: "STOP" }] };

  // The switch, and what the default sends. Turning it on by accident is the regression.
  check(app.config.safetyFilters === false, "the server's adjustable filters are off by default");
  await call(hello);
  check(sent && !("safetySettings" in sent), "a request without filters sends no safetySettings");

  // And when a measurement run does switch them on, they arrive complete.
  await call(hello, SAFETY_SETTINGS);
  const categories = (sent?.safetySettings ?? []).map((s) => s.category);
  for (const category of [
    "HARM_CATEGORY_HARASSMENT",
    "HARM_CATEGORY_HATE_SPEECH",
    "HARM_CATEGORY_SEXUALLY_EXPLICIT",
    "HARM_CATEGORY_DANGEROUS_CONTENT",
  ]) {
    check(categories.includes(category), `when on, a threshold is set for ${category}`);
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

/* --- the distress screen: a player in trouble gets the authored reply, never a joke.
   ITS OWN SERVERS. The main app above is near its per-minute limit by design, and these
   requests would push it over and fail for the wrong reason. */
{
  const startApp = async (extra) => {
    const a = createApp({ mock: true, stateFile: join(scratch, `distress-${uuid()}.json`), perIpPerMinute: 60, ...extra });
    await new Promise((resolve) => a.server.listen(0, resolve));
    const url = `http://127.0.0.1:${a.server.address().port}/v1/npc-chat`;
    const send = async (message) => {
      const res = await fetch(url, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(ask({ message })) });
      return res.json();
    };
    return { a, send };
  };

  const { a: on, send } = await startApp({});
  check(on.config.distressScreen === true, "the distress screen is on by default");

  const troubled = await send("__distress__ I don't see the point anymore");
  check(
    troubled.status === "ok" && troubled.care === true && troubled.reply === CARE_REPLY,
    "a message screened DISTRESS gets the authored care reply, marked care"
  );
  check(!/Mock reply/.test(troubled.reply), "the character's reply is discarded, not shown");
  check(/988/.test(troubled.reply) && /findahelpline\.com/.test(troubled.reply), "the care reply carries the helpline text");

  // __fail: must lead the message (mockGenerate anchors it); __distress__ may be anywhere.
  const failedToo = await send("__fail:blocked__ __distress__ everything hurts");
  check(failedToo.care === true && failedToo.status === "ok", "distress wins even when the character's reply failed");

  const normal = await send("how was the burger?");
  check(normal.status === "ok" && normal.care === undefined && /Mock reply/.test(normal.reply), "an ordinary message gets the character, no care flag");

  const screenDown = await send("__screenfail__ hello");
  check(screenDown.status === "ok" && screenDown.care === undefined, "a failed screen fails open to the normal reply");

  const { a: off, send: sendOff } = await startApp({ distressScreen: false });
  const unscreened = await sendOff("__distress__ hello");
  check(unscreened.care === undefined && /Mock reply/.test(unscreened.reply), "with the screen switched off, nothing is screened");

  await new Promise((resolve) => on.server.close(resolve));
  await new Promise((resolve) => off.server.close(resolve));
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
