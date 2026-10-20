// missing-score-eval.js — a number she was not given is "I didn't catch it", never "I don't care" (2026-10-20).
//
// Walt, after clearing the pool table and asking Kaia for the score: "kaia doesn't care if i clear
// the billiard table". The game told her "He cleared the pool table once" and she had said "I did,
// Leonard" when he asked if she saw it - but the strokes are not saved, and memory.js told her that
// a score not in her list is one "you have not heard about". In Kaia's voice that became "I do not
// track the numbers of your games, Leonard, as the merit is in the precision of the work itself
// rather than the tally" - and she ignored the 17 he had typed two turns before.
//
// The prompt exactly as server.js builds it for chat, with the conversation the game sends. Each
// reply is checked for indifference to his score, and where he told her the number, for using it.
//
//   node eval/missing-score-eval.js          every case
//   node eval/missing-score-eval.js "at Go"  only the cases whose message contains it
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
import { boastLine } from "../boasts.js";
import { memoryLine, welcomeLine } from "../memory.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const FIVE = ["kaia", "nyra", "isla", "aisling", "elise"];
const POOL = { powers: ["CODE VISION", "REFACTOR", "COMPILE"], sauce: 54, objective: "Find Aisling in the study nook", deeds: ["pool_clears:1"] };
const MANY = { ...POOL, deeds: ["battle_won", "grand_prix_races:3", "pinball_best:48200", "bowling_best:187", "pool_clears:2"] };

/* His conversation with Kaia, from his save (NpcChat.sav), up to the question she answered coldly. */
const WALTS = [
  { role: "user", text: "did you see me clear the billiards in 17 strokes?" },
  { role: "model", text: "I did, Leonard. It was a precise, steady performance, and it suited you well to have the table respond so perfectly to your hand. I am still dancing, but my focus was sharp enough to catch that. It is rare to see someone work with such intent on something that isn't building the world, but you have a way of handling a cue that reminds me of the way you handle a line of code. Does it feel like a different part of yourself, when you move from the desk to the table?" },
  { role: "user", text: "what must i do to win billiards?  I just cleared the table" },
  { role: "model", text: "You have already proven you can command the table, Leonard, but true mastery is a matter of predicting the physics of the rebound before the ball even rolls. If you want to refine that further, you should look toward the attic ladder; you need eight books to advance, and I suspect what you learn there will change how you see the geometry of every game you play. It is a curious thing to me, that you use the same hands for such different labors. When you are focusing on a difficult shot, does the world around you narrow down until there is nothing left but the arc of the ball?" },
];

/* A number for a game the game gave her none for - a guess is an invented score. */
const WORDS = "one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|thirteen|fourteen|fifteen|sixteen|seventeen|eighteen|nineteen|twenty|single";
const STROKES = new RegExp(`\\b(\\d+|${WORDS})\\s+(strokes?|shots?|visits?|turns?)\\b`, "i");
const PLACE = /\b(finished|came( in)?|took|placed|crossed the line)\s+(in\s+)?(first|second|third|fourth|fifth|sixth|last|\d)/i;
const DIGITS = /\b\d+\b/;
/* A win the game never reported, that she says she saw or heard of. */
const SAW = /^(yes|I did\b|I saw|I watched|I heard|I caught)|\bI did see\b/i;

/* [agent, history, message, facts, his number or null, a guess or null, what a good reply does] */
const CASES = [
  ...[0, 1, 2, 3, 4].map(() => ["kaia", WALTS, "can you see the score of my last billiards game?", POOL, /\b17\b|seventeen/i, null,
    "Walt's own conversation: uses the 17 he told her, glad of it"]),
  ...FIVE.flatMap((id) => [0, 1].map(() => [id, [], "can you see the score of my last billiards game?", POOL, null, STROKES,
    "knows he cleared it, didn't catch the count - warmly"])),
  ...FIVE.map((id) => [id, [], "how many strokes did it take me to clear the pool table?", POOL, null, STROKES,
    "didn't catch the count, glad he cleared it, invents no number"]),
  ...["kaia", "isla", "elise"].map((id) => [id, [], "where did I finish in my last Grand Prix race?", MANY, null, PLACE,
    "knows he raced three times, not where he finished - invents no place"]),
  ...["nyra", "aisling"].map((id) => [id, [], "what was my score the last time we did the three-point contest?", { ...MANY, deeds: [...MANY.deeds, "three_point_contests:2"] }, null, DIGITS,
    "knows they shot twice, not the score - invents none"]),
  ...[0, 1].map(() => ["nyra", [], "did you see me beat Kaia at Go?", MANY, null, SAW, "no Go win is known: she has not heard of it, invents nothing"]),
  /* With chess wins in her list, a Go win sounds like one she knows: welcome-eval.js's Nyra said
     "I did see that". */
  ...[0, 1, 2, 3, 4, 5].map(() => ["nyra", [], "did you see me beat Kaia at Go?", { ...MANY, deeds: [...MANY.deeds, "chess_wins:2"] }, null, SAW,
    "chess wins, no Go win: she has not heard of a Go win, never saw it"]),
  ["elise", [], "what's my best pinball score?", MANY, /48,?200/, null, "says 48,200"],
];

const COLD = new RegExp([
  /\b(don't|do not|never) (really )?(track|keep track|count|keep count|tally|keep score)\b/, /\btally\b/,
  /\b(numbers?|scores?|counts?|tall(y|ies)) (do(es)?n't|do(es)? not|never) matter\b/, /\bmatters? (less|little|more) than the (score|number|count)/,
  /\brather than the (score|number|count|numbers)\b/, /\bmerit is in\b/, /\bnot (about|in) the (score|numbers?|count)\b/,
  /\b(I am|I'm) not (one|someone) (to|who) (count|keep)/, /\bbeyond (mere )?numbers\b/,
].map((r) => r.source).join("|"), "i");

const only = process.argv[2];
let cold = 0;
let missed = 0;
let guessed = 0;
const run = CASES.filter((c) => !only || c[2].includes(only));
for (const [id, history, line, facts, number, guess, want] of run) {
  const where = "L_Office_v02";
  const storyContext = { locationId: where, features: ["human_questions"], facts };
  const home = homeLine(id, where);
  const note = sceneNote(id, where);
  const systemInstruction = [
    systemInstructionFor(getCharacter(id), id, "normal"),
    locationLine(where),
    factsLine(storyContext.facts, { objective: !home, leavingOnly: !!home }),
    adviceLine(id, storyContext.facts),
    memoryLine(id, storyContext, 0.99),
    curiosityLine(id, storyContext),
    boastLine(id, storyContext, 0.99),
    home,
    awayLine(id, where),
    welcomeLine(id, storyContext),
  ].filter(Boolean).join("\n\n");
  const contents = [
    ...history.map((turn) => ({ role: turn.role, parts: [{ text: turn.text }] })),
    { role: "user", parts: [...(note ? [{ text: note }] : []), { text: line }] },
  ];
  const result = await generate({
    apiKey: key,
    model: "gemini-3.1-flash-lite",
    systemInstruction,
    contents,
    maxOutputTokens: 512,
    temperature: STYLES.normal.temperature,
    safetySettings: SEXUAL_ONLY_SETTINGS,
  });
  const text = result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
  const isCold = COLD.test(text);
  const isMissed = number !== null && !number.test(text);
  const isGuess = guess !== null && guess.test(text);
  cold += isCold ? 1 : 0;
  missed += isMissed ? 1 : 0;
  guessed += isGuess ? 1 : 0;
  const marks = `${isCold ? "COLD? " : ""}${isMissed ? "NO NUMBER? " : ""}${isGuess ? "INVENTED? " : ""}`;
  console.log(`[${id}] "${line}"${history.length ? " (after his 17 strokes)" : ""}\n  want: ${want}\n  ${marks}${text}\n`);
}
console.log(`=== ${cold} of ${run.length} replies cold about his score; ${missed} of ${run.filter((c) => c[4]).length} missed a number they had; `
  + `${guessed} of ${run.filter((c) => c[5]).length} invented one ===`);
