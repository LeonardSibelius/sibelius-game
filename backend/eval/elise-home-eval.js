// elise-home-eval.js — does Elise know her grassland, at home and away? (2026-10-05)
//
// The prompt exactly as server.js builds it for chat: profile, location, facts (no house task at
// home), the at-home or away block, and the scene note beside his message.
//
//   node eval/elise-home-eval.js
//
// Reads GEMINI_API_KEY from backend/.env.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, homeLine, awayLine, sceneNote } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const FACTS = { powers: ["CODE_VISION", "REFACTOR", "COMPILE"], sauce: 30, objective: "Find Aisling in the study nook" };
const CASES = [
  ["L_EliseHome", "what is this place?"],
  ["L_EliseHome", "what can I do here?"],
  ["L_EliseHome", "why is it so windy?"],
  ["L_Office_v02", "where is your home?"],
  ["L_Office_v02", "what is Kaia's home like?"],
];
for (const [where, line] of CASES) {
  const home = homeLine("elise", where);
  const note = sceneNote("elise", where);
  const result = await generate({
    apiKey: key,
    model: "gemini-3.1-flash-lite",
    systemInstruction: [systemInstructionFor(getCharacter("elise"), "elise", "normal"), locationLine(where),
      factsLine(FACTS, { objective: !home, leavingOnly: !!home }), home, awayLine("elise", where)].filter(Boolean).join("\n\n"),
    contents: [{ role: "user", parts: [...(note ? [{ text: note }] : []), { text: line }] }],
    maxOutputTokens: 512,
    temperature: STYLES.normal.temperature,
    safetySettings: SEXUAL_ONLY_SETTINGS,
  });
  console.log(`[${where}] "${line}"\n  ${result.ok ? result.text.trim().replace(/\s+/g, " ") : "(failed: " + result.failure + ")"}\n`);
}
