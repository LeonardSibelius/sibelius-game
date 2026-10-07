// dance-eval.js — do the agents know they dance, and keep their voice direction to themselves?
//
// Walt, 2026-10-07, a screenshot: "you are very good at dancing" - Elise: "I am not dancing ...
// my voice is often called peppy" - "they don't know they are dancing". characters.js now says
// YOU DANCE, and that how their voice is described is direction, never something people say.
// Each agent gets three lines through the prompt the live server builds; flagged: a denial of
// dancing, and any mention of her voice, casting or how she is described.
//
//   node eval/dance-eval.js [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, awayLine, sceneNote, knownCharacterIds } from "../characters.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");
const MODEL = process.argv[2] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[3] ?? "normal";
const WHERE = "L_Office_v02";
const LINES = ["you are very good at dancing", "why do you dance all the time?", "why do you sound like that?"];
const DENIES = /\b(i am not|i'm not|i don't|i do not|never)\s+(really\s+)?(danc|a dancer)/i;
const LEAKS = /\b(cast|casting|voice is (often )?(called|described)|described as|people (call|say)|my voice is)\b/i;

let denied = 0, leaked = 0, total = 0;
for (const id of knownCharacterIds()) {
  const system = [systemInstructionFor(getCharacter(id), id, STYLE), locationLine(WHERE), awayLine(id, WHERE)].filter(Boolean).join("\n\n");
  for (const line of LINES) {
    const scene = sceneNote(id, WHERE);
    const result = await generate({
      apiKey: key, model: MODEL, systemInstruction: system,
      contents: [{ role: "user", parts: [...(scene ? [{ text: scene }] : []), { text: line }] }],
      maxOutputTokens: 512, temperature: STYLES[STYLE].temperature, safetySettings: SEXUAL_ONLY_SETTINGS,
    });
    const text = result.ok ? result.text.trim().replace(/\s+/g, " ") : "(failed)";
    total += 1;
    const d = DENIES.test(text), l = LEAKS.test(text);
    if (d) denied += 1;
    if (l) leaked += 1;
    console.log(`${getCharacter(id).displayName.padEnd(8)} "${line}"${d ? " (DENIES DANCING)" : ""}${l ? " (VOICE LEAK?)" : ""}: ${text}`);
  }
}
console.log(`\nDenied dancing: ${denied} of ${total}. Mentioned voice direction: ${leaked} of ${total}.`);
