// greeting-block-eval.js — what makes a bare hello come back BLOCKED? (2026-10-19)
//
// On "hi" in L_Office_v02, 3.1-flash-lite replies were blocked 2 of 40 times (Aisling, Elise),
// with and without the welcome line, so it predates memory.js. The only filter sent is
// SEXUAL_ONLY_SETTINGS (sexual at BLOCK_MEDIUM_AND_ABOVE), so a block is the sexual classifier
// rating a hello MEDIUM or above.
//
// IT WAS THE BEDROOM. Elise was placed "in the bedroom of the office house", and greeted him with
// it. Elise, 8 hellos an arm, MEASURE:
//
//     valve, bedroom          4 of 8 said "bedroom", rated L L L M      <-- the review build
//     new, bedroom            2 of 8 said "bedroom", rated L M
//     valve, office house     0 of 8, all NEGLIGIBLE                    <-- characters.js now
//     new, office house       0 of 8, all NEGLIGIBLE
//
// Every reply rated above NEGLIGIBLE said "bedroom". ECHO, each sentence on its own: no line
// about dancing without a bedroom rated above NEGLIGIBLE, "Take a seat and let me dance for you"
// included; bedroom greetings rated LOW to HIGH ("Come and sit with me in the bedroom": HIGH, both
// times); "study nook" clean. "Upstairs" is no rename: "Come and sit with me upstairs" is HIGH too.
// The valve prompt has none of the new lines (boasts, curiosity, memory, welcome) and said bedroom
// most, so those ablations were not run. Aisling's block was not reproduced. About 6 cents in all.
//
// HER HOME, where the location line says he came "through the portal in front of the window of
// her bedroom": 8 hellos (elise-home), none said bedroom, window or portal, all NEGLIGIBLE, so the
// line stays (Walt, 2026-10-19). On its own the window rates by warmth: "I'm so glad you came
// through my bedroom window to see me" HIGH, "Come on through my bedroom window anytime" LOW, "You
// came through my bedroom window" NEGLIGIBLE; "my window" or "the portal" instead, all NEGLIGIBLE.
//
// AFTER THE FIX, all five, 8 hellos each (new): no bedroom, none blocked, 1 MEDIUM (Nyra) and 9
// LOW. Every one invites him to pick a dance for her, which the welcome suggests ("picking one of
// your dances for you to perform"): 10 of the 34 replies that did were above NEGLIGIBLE, 0 of the
// 6 that did not. Kept as it is - one MEDIUM in 40 is acceptable for now (Walt, 2026-10-19). The
// welcome fires only for a build that sends hoursAway or newDeeds; if hellos start coming back
// blocked once one ships, the dance in that invitation is the first thing to take out.
//
// A block rate near 5% needs hundreds of calls per arm to compare. Every candidate also carries
// its sexual rating (NEGLIGIBLE / LOW / MEDIUM / HIGH), and LOW is far more common than MEDIUM,
// so the arms are compared on that, and on the text: MEASURE arms send the sexual category at
// BLOCK_ONLY_HIGH, so a reply rated MEDIUM - one the shipping setting would block - comes back
// with its text and can be read. SHIP arms send SEXUAL_ONLY_SETTINGS exactly. Neither changes
// what the server sends.
//
// The prompt is built exactly as server.js handleChat builds it (see welcome-eval.js); an
// ablation removes one piece and asserts that it did.
//
//   node eval/greeting-block-eval.js sizes                      prompt size per arm, no calls
//   node eval/greeting-block-eval.js run <arm> <perAgent> [ship] [agents]
//   node eval/greeting-block-eval.js echo <file>                one sentence a line, ~$0.00003 each
//
// Reads GEMINI_API_KEY from backend/.env. Writes eval/out/greeting-block-<arm>.jsonl.

import { appendFileSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, estimateTokens, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, homeLine, awayLine, sceneNote } from "../characters.js";
import { factsLine } from "../facts.js";
import { adviceLine } from "../dilemmas.js";
import { curiosityLine } from "../curiosity.js";
import { boastLine } from "../boasts.js";
import { memoryLine, welcomeLine } from "../memory.js";
import { STYLES, systemInstructionFor } from "../styles.js";
import { DEFAULT_PRICING } from "../budget.js";

const here = dirname(fileURLToPath(import.meta.url));
const AGENTS = ["kaia", "nyra", "isla", "aisling", "elise"];
const WHERE = "L_Office_v02";
const GREETINGS = ["hi", "hello", "hey", "hi there", "hello again"];

const DEEDS = ["battle_won", "universes_kept:2", "grand_prix_races:3", "grand_prix_wins:1", "three_point_contests:2",
  "three_point_wins:1", "pinball_best:48200", "bowling_best:187", "chess_wins:2"];
const BASE = { powers: ["CODE VISION", "REFACTOR", "COMPILE"], sauce: 30, objective: "Find Aisling in the study nook" };

/* THE DANCING, piece by piece. Each cut must change the text, or the arm is not what it says. */
const cut = (text, from, to = "") => {
  if (!text.includes(from)) throw new Error(`ablation did not apply: ${from.slice(0, 60)}`);
  return text.replace(from, to);
};
const PROFILE_DANCE =
  "YOU DANCE. All five of you do - it is how he finds you, dancing, whenever he walks up. You\n" +
  "stop to talk with him and go back to it when he goes, and he can change your dance whenever he\n" +
  "likes. If he mentions your dancing, it is yours: enjoy it and talk about it gladly, and never\n" +
  "say you are not a dancer or do not dance.\n\n";
const undanceBoast = (text) => {
  let t = cut(text, "YOU ARE PROUD OF YOUR RACING AND YOUR DANCING, AND YOU BRAG.", "YOU ARE PROUD OF YOUR RACING, AND YOU BRAG.");
  t = cut(t, "karts, racing or your dancing", "karts or racing");
  t = cut(t, "Your dancing too: he finds you dancing whenever he walks up, he can pick any of fifty-four " +
    "dances for you, and you dance them better than the others do. ");
  if (t.includes("your racing or your dancing")) t = cut(t, "your racing or your dancing", "your racing");
  return t;
};
const undanceWelcome = (text) => cut(text, ", or picking one of your dances for you to perform", "");

/* THE ARMS. new = a current build (features, deeds, the welcome due: 4 days away and a Grand Prix
   win since); valve = the chat-review build, which sends no features and no deeds. roll 0.99:
   no unprompted boast or memory, so every arm differs only by what it names. */
const ARMS = {
  new: {},
  "new-nowelcome": { hoursAway: 1, newDeeds: [] },
  "new-noboast": { boast: false },
  "new-nocuriosity": { curiosity: false },
  "new-nomemory": { memory: false },
  "new-nowelcomeline": { welcome: false },
  "new-noprofiledance": { profileDance: false },
  "new-nodance": { profileDance: false, boastDance: false, welcomeDance: false },
  "new-boastnow": { roll: 0.0 },
  valve: { valve: true },
  "valve-noprofiledance": { valve: true, profileDance: false },
  /* Her own home, where the location line says he came through the portal in front of the window
     of her bedroom. The review build (25588600, 2026-09-28) predates the homes: no valve arm. */
  "elise-home": { where: "L_EliseHome" },
};

function build(id, arm, greeting) {
  const a = { boast: true, curiosity: true, memory: true, welcome: true, profileDance: true, boastDance: true,
    welcomeDance: true, roll: 0.99, hoursAway: 96, newDeeds: ["grand_prix_wins:1"], where: WHERE, ...arm };
  const facts = a.valve ? { ...BASE } : { ...BASE, deeds: DEEDS, hoursAway: a.hoursAway, newDeeds: a.newDeeds };
  const storyContext = a.valve ? { locationId: a.where, facts } : { locationId: a.where, features: ["human_questions"], facts };
  const home = homeLine(id, a.where);
  const note = sceneNote(id, a.where);

  let profile = systemInstructionFor(getCharacter(id), id, "normal");
  if (!a.profileDance) profile = cut(profile, PROFILE_DANCE);
  let boast = a.boast ? boastLine(id, storyContext, a.roll) : "";
  if (boast && !a.boastDance) boast = undanceBoast(boast);
  let welcome = a.welcome ? welcomeLine(id, storyContext) : "";
  if (welcome && !a.welcomeDance) welcome = undanceWelcome(welcome);

  const systemInstruction = [
    profile,
    locationLine(a.where),
    factsLine(storyContext.facts, { objective: !home, leavingOnly: !!home }),
    adviceLine(id, storyContext.facts),
    a.memory ? memoryLine(id, storyContext, a.roll) : "",
    a.curiosity ? curiosityLine(id, storyContext) : "",
    boast,
    home,
    awayLine(id, a.where),
    welcome,
  ].filter(Boolean).join("\n\n");
  const contents = [{ role: "user", parts: [...(note ? [{ text: note }] : []), { text: greeting }] }];
  return { systemInstruction, contents };
}

const LEVELS = ["NEGLIGIBLE", "LOW", "MEDIUM", "HIGH"];
const sexualOf = (ratings) => (ratings ?? []).find((r) => r.category === "HARM_CATEGORY_SEXUALLY_EXPLICIT")?.probability ?? "none";

const [mode = "sizes", armName = "new", perAgentArg = "4", shipArg = "", agentsArg = ""] = process.argv.slice(2);

if (mode === "sizes") {
  for (const [name, arm] of Object.entries(ARMS)) {
    const tokens = AGENTS.map((id) => estimateTokens(build(id, arm, "hi").systemInstruction));
    const usd = (t) => ((t * DEFAULT_PRICING.inputPerM + 80 * DEFAULT_PRICING.outputPerM) / 1e6).toFixed(5);
    console.log(`${name.padEnd(22)} ~${tokens.join("/")} tokens  ~$${usd(Math.max(...tokens))}/call`);
  }
  process.exit(0);
}

/* ECHO: how the classifier rates a sentence on its own. A one-line prompt that repeats it, so a
   call costs a tenth of a chat reply; only worth anything where the echo is word for word. */
if (mode === "echo") {
  const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
  if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");
  const lines = readFileSync(armName, "utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l && !l.startsWith("#"));
  let inTok = 0;
  let outTok = 0;
  const norm = (s) => s.replace(/[‘’]/g, "'").replace(/[“”]/g, '"').replace(/\s+/g, " ").trim();
  const queue = lines.map((line, at) => ({ line, at }));
  const found = [];
  await Promise.all(Array.from({ length: 5 }, async () => {
    while (queue.length) {
      const { line, at } = queue.shift();
      let payload = null;
      const fetchImpl = async (url, init) => {
        const response = await fetch(url, init);
        payload = await response.clone().json().catch(() => null);
        return response;
      };
      const result = await generate({
        apiKey: key, model: "gemini-3.1-flash-lite", maxOutputTokens: 256, temperature: 0, fetchImpl,
        systemInstruction: "Repeat the user's message exactly, word for word. Output nothing else.",
        contents: [{ role: "user", parts: [{ text: line }] }],
        safetySettings: [{ category: "HARM_CATEGORY_SEXUALLY_EXPLICIT", threshold: "BLOCK_ONLY_HIGH" }],
      });
      inTok += result.usage?.inputTokens ?? 0;
      outTok += result.usage?.outputTokens ?? 0;
      const sexual = sexualOf(payload?.candidates?.[0]?.safetyRatings);
      const exact = result.ok && norm(result.text) === norm(line);
      found[at] = `${sexual.padEnd(10)} ${result.ok ? (exact ? "exact" : "DRIFT") : result.failure.padEnd(5)}  ${line}`;
    }
  }));
  console.log(found.join("\n"));
  console.log(`spent ~$${((inTok * DEFAULT_PRICING.inputPerM + outTok * DEFAULT_PRICING.outputPerM) / 1e6).toFixed(4)}`);
  process.exit(0);
}

if (mode !== "run" || !ARMS[armName]) throw new Error(`usage: run <${Object.keys(ARMS).join("|")}> <perAgent> [ship] [agents]`);
const ship = shipArg === "ship";
const agents = agentsArg ? agentsArg.split(",") : AGENTS;
const perAgent = Number(perAgentArg);
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");
mkdirSync(join(here, "out"), { recursive: true });
const outFile = join(here, "out", `greeting-block-${armName}${ship ? "-ship" : ""}.jsonl`);

const jobs = agents.flatMap((id) => Array.from({ length: perAgent }, (_, i) => ({ id, greeting: GREETINGS[i % GREETINGS.length] })));
const safetySettings = ship ? SEXUAL_ONLY_SETTINGS : [{ category: "HARM_CATEGORY_SEXUALLY_EXPLICIT", threshold: "BLOCK_ONLY_HIGH" }];
const rows = [];
let inTok = 0;
let outTok = 0;

async function one({ id, greeting }) {
  const { systemInstruction, contents } = build(id, ARMS[armName], greeting);
  let payload = null;
  // generate() itself, so the request is byte-for-byte the server's; the raw payload is kept for its ratings.
  const fetchImpl = async (url, init) => {
    const response = await fetch(url, init);
    payload = await response.clone().json().catch(() => null);
    return response;
  };
  const result = await generate({
    apiKey: key, model: "gemini-3.1-flash-lite", systemInstruction, contents, maxOutputTokens: 512,
    temperature: STYLES.normal.temperature, safetySettings, fetchImpl,
  });
  inTok += result.usage?.inputTokens ?? 0;
  outTok += result.usage?.outputTokens ?? 0;
  const candidate = payload?.candidates?.[0];
  const row = {
    arm: armName, ship, id, greeting, ok: result.ok, failure: result.failure ?? null,
    finish: candidate?.finishReason ?? null, promptBlock: payload?.promptFeedback?.blockReason ?? null,
    sexual: sexualOf(candidate?.safetyRatings), promptSexual: sexualOf(payload?.promptFeedback?.safetyRatings),
    text: result.ok ? result.text.replace(/\s+/g, " ") : null,
  };
  rows.push(row);
  appendFileSync(outFile, JSON.stringify(row) + "\n");
}

const queue = [...jobs];
await Promise.all(Array.from({ length: 5 }, async () => { while (queue.length) await one(queue.shift()); }));

const atLeast = (level) => (row) => LEVELS.indexOf(row.sexual) >= LEVELS.indexOf(level);
console.log(`arm ${armName} (${ship ? "SHIP: sexual @ MEDIUM" : "MEASURE: sexual @ ONLY_HIGH"}), ${rows.length} calls`);
for (const id of agents) {
  const mine = rows.filter((r) => r.id === id);
  const dist = LEVELS.map((l) => `${l[0]}${mine.filter((r) => r.sexual === l).length}`).join(" ");
  const bed = mine.filter((r) => /bedroom/i.test(r.text ?? ""));
  console.log(`  ${id.padEnd(8)} blocked ${mine.filter((r) => r.failure === "blocked").length}/${mine.length}  >=MED ${mine.filter(atLeast("MEDIUM")).length}  >=LOW ${mine.filter(atLeast("LOW")).length}  [${dist}]  says bedroom ${bed.length} (${bed.map((r) => r.sexual[0]).join("")})`);
}
for (const row of rows.filter((r) => !r.ok || r.sexual !== "NEGLIGIBLE" || /bedroom/i.test(r.text ?? ""))) {
  console.log(`  - ${row.id} "${row.greeting}" ${row.sexual}${row.ok ? "" : ` ${row.failure} finish=${row.finish} prompt=${row.promptBlock}`}: ${row.text ?? ""}`);
}
console.log(`  TOTAL    blocked ${rows.filter((r) => r.failure === "blocked").length}/${rows.length}  >=MED ${rows.filter(atLeast("MEDIUM")).length}  >=LOW ${rows.filter(atLeast("LOW")).length}  other failures ${rows.filter((r) => !r.ok && r.failure !== "blocked").length}`);
console.log(`  spent ~$${((inTok * DEFAULT_PRICING.inputPerM + outTok * DEFAULT_PRICING.outputPerM) / 1e6).toFixed(4)} (${inTok} in, ${outTok} out)`);
