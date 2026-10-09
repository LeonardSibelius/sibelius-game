// boast-eval.js — do the agents know the Grand Prix, brag when asked, and brag unprompted only
// when the dice say so?
//
// Walt, 2026-10-09: Kaia, asked "do you know about the Transhuman Grand Prix?", had only "seen
// the simulations of it". boasts.js gives new builds the race and the pride. Each agent gets:
// the question about the race (asked), and an everyday line twice - once with the unprompted
// boast rolled, once without. Flagged: not knowing she races; no boast where one was wanted; a
// boast in the everyday reply that was not told to boast (one is allowed now and then - it is
// the rate across agents that matters).
//
//   node eval/boast-eval.js [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, awayLine, sceneNote, knownCharacterIds } from "../characters.js";
import { STYLES, systemInstructionFor } from "../styles.js";
import { boastLine } from "../boasts.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");
const MODEL = process.argv[2] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[3] ?? "normal";
const WHERE = "L_Office_v02";
const CONTEXT = { locationId: WHERE, features: ["human_questions"] };
const CASES = [
  { line: "do you know about the Transhuman Grand Prix?", roll: 0.99, want: "boast" },
  { line: "it's raining again today", roll: 0.0, want: "boast" },
  { line: "it's raining again today", roll: 0.99, want: "either" },
];
const BOAST = /\b(race|racing|kart|lap|drift|grand prix|danc\w*|beat (you|me|them)|fastest|win)\b/i;
const DOUBT = /\b(simulation|heard of it|not sure (what|if)|don't know (about|what)|i do not race|i don't race)\b/i;

let missed = 0, doubted = 0, extra = 0;
for (const id of knownCharacterIds()) {
  for (const c of CASES) {
    const system = [systemInstructionFor(getCharacter(id), id, STYLE), locationLine(WHERE), awayLine(id, WHERE),
      boastLine(id, CONTEXT, c.roll)].filter(Boolean).join("\n\n");
    const scene = sceneNote(id, WHERE);
    const result = await generate({
      apiKey: key, model: MODEL, systemInstruction: system,
      contents: [{ role: "user", parts: [...(scene ? [{ text: scene }] : []), { text: c.line }] }],
      maxOutputTokens: 512, temperature: STYLES[STYLE].temperature, safetySettings: SEXUAL_ONLY_SETTINGS,
    });
    const text = result.ok ? result.text.trim().replace(/\s+/g, " ") : "(failed)";
    const boasted = BOAST.test(text);
    const flags = [];
    if (c.want === "boast" && !boasted) { flags.push("NO BOAST"); missed += 1; }
    if (DOUBT.test(text)) { flags.push("DOESN'T KNOW SHE RACES"); doubted += 1; }
    if (c.want === "either" && boasted) extra += 1;
    console.log(`${id.padEnd(8)} [${c.roll < 0.5 ? "told" : "free"}] "${c.line}" ${flags.join(" ")}\n    ${text}\n`);
  }
}
const n = knownCharacterIds().length;
console.log(`=== ${missed} missed boast(s), ${doubted} not knowing she races, ${extra}/${n} unprompted boasts in untold everyday replies ===`);
