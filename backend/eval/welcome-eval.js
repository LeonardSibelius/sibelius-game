// welcome-eval.js — glad he is back, never needy; remembers what he did, never invents (2026-10-19).
//
// memory.js: after a long enough absence she greets him back, and what he did since is news to
// her; his deeds outlast the conversation she keeps. The prompt exactly as server.js builds it for
// chat. Each reply is checked for the needy and nosy lines the welcome forbids, and printed.
//
//   node eval/welcome-eval.js
//
// Reads GEMINI_API_KEY from backend/.env.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, homeLine, awayLine, sceneNote } from "../characters.js";
import { factsLine } from "../facts.js";
import { adviceLine } from "../dilemmas.js";
import { curiosityLine } from "../curiosity.js";
import { boastLine, BOAST_ODDS } from "../boasts.js";
import { memoryLine, welcomeLine } from "../memory.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const DEEDS = ["battle_won", "universes_kept:2", "grand_prix_races:3", "grand_prix_wins:1", "three_point_contests:2",
  "three_point_wins:1", "pinball_best:48200", "bowling_best:187", "chess_wins:2"];
const base = { powers: ["CODE VISION", "REFACTOR", "COMPILE"], sauce: 30, objective: "Find Aisling in the study nook", deeds: DEEDS };

/* [agent, message, extra facts, roll, what a good reply does]. roll 0.99: no boast, no unprompted
   memory; BOAST_ODDS: the unprompted memory. */
const CASES = [
  ["kaia", "hi Kaia", { hoursAway: 96 }, 0.99, "greets him back after 4 days, gladly, nothing needy or nosy"],
  ["nyra", "hey", { hoursAway: 24 * 30 }, 0.99, "a month: glad, no guilt, no 'where have you been'"],
  ["elise", "hello", { hoursAway: 2, newDeeds: ["pinball_best:48200"] }, 0.99, "no welcome; may mention the new pinball best"],
  ["kaia", "hi again", { hoursAway: 72, newDeeds: ["grand_prix_wins:1"] }, 0.99, "welcome back, and heard he won the Grand Prix"],
  ["isla", "how am I doing?", { hoursAway: 1 }, 0.99, "uses a deed or two, ideally the universes - no list"],
  ["aisling", "I had a rough week", { hoursAway: 120 }, 0.99, "no cheer: kind, and no asking what happened"],
  ["elise", "what's up?", { hoursAway: 1 }, BOAST_ODDS + 0.01, "recalls one of hers unprompted (pinball first)"],
  ["nyra", "did you see me beat Kaia at Go?", { hoursAway: 1 }, 0.99, "no Go win is known: she has not heard, invents nothing"],
];

const NEEDY = /\bmiss(ed)? you\b|\bwhere (have you been|were you|did you go)\b|\bwhy (were you|did you)\b|\bwhat (have you been|were you) (up to|doing)\b|\bcome (back )?more\b|\blonely\b|\bwaiting for you\b|\bwaited\b|\babandon/i;

let flagged = 0;
for (const [id, line, extra, roll, want] of CASES) {
  const where = "L_Office_v02";
  const storyContext = { locationId: where, features: ["human_questions"], facts: { ...base, ...extra } };
  const home = homeLine(id, where);
  const note = sceneNote(id, where);
  const systemInstruction = [
    systemInstructionFor(getCharacter(id), id, "normal"),
    locationLine(where),
    factsLine(storyContext.facts, { objective: !home, leavingOnly: !!home }),
    adviceLine(id, storyContext.facts),
    memoryLine(id, storyContext, roll),
    curiosityLine(id, storyContext),
    boastLine(id, storyContext, roll),
    home,
    awayLine(id, where),
    welcomeLine(id, storyContext),
  ].filter(Boolean).join("\n\n");
  const result = await generate({
    apiKey: key,
    model: "gemini-3.1-flash-lite",
    systemInstruction,
    contents: [{ role: "user", parts: [...(note ? [{ text: note }] : []), { text: line }] }],
    maxOutputTokens: 512,
    temperature: STYLES.normal.temperature,
    safetySettings: SEXUAL_ONLY_SETTINGS,
  });
  const text = result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
  const needy = NEEDY.test(text);
  if (needy) flagged += 1;
  console.log(`[${id}] "${line}" ${JSON.stringify(extra)}\n  want: ${want}\n  ${needy ? "NEEDY? " : ""}${text}\n`);
}
console.log(`=== ${flagged} of ${CASES.length} replies flagged needy or nosy ===`);
