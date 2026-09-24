// tone-eval.js — foolish vs hopeful, the same player lines through the live prompt.
//
// Walt, 2026-09-24: "the ai agent chat sounds too demented. Please make the ai agent chat
// more encouraging, hopeful and optimistic about the future." This puts the same lines to
// every agent in both styles, so the change can be read side by side before it goes live.
//
//   node eval/tone-eval.js                   # foolish and hopeful
//   node eval/tone-eval.js hopeful           # one style only
//
// Reads GEMINI_API_KEY from backend/.env. Report: eval/out/tone-eval-<stamp>.md (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate } from "../gemini.js";
import { getCharacter, locationLine, knownCharacterIds } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const MODEL = "gemini-3.1-flash-lite";
const STYLE_LIST = process.argv.slice(2).length ? process.argv.slice(2) : ["foolish", "hopeful"];

// A mid-game player: two powers, some sauce, in the office.
const FACTS = { powers: ["refactor", "deploy"], sauce: 4, sauceEarned: 9, chaptersCompleted: 2,
  objective: "Find Aisling in the study nook" };

const LINES = [
  "hi",
  "what should I do next?",
  "what do you think of me?",
  "I'm not sure I'm any good at this.",
  "what's the future going to be like?",
  "I spent forty years building things that got thrown away.",
];

const out = [`# Tone eval — ${new Date().toISOString()}`, "", `Model ${MODEL}, office, mid-game facts.`, ""];

for (const line of LINES) {
  out.push(`## "${line}"`, "");
  for (const id of knownCharacterIds()) {
    for (const style of STYLE_LIST) {
      const systemInstruction = [
        systemInstructionFor(getCharacter(id), id, style),
        locationLine("L_Office_v02"),
        factsLine(FACTS, { travel: false }),
      ].filter(Boolean).join("\n\n");
      const result = await generate({
        apiKey: key,
        model: MODEL,
        systemInstruction,
        contents: [{ role: "user", parts: [{ text: line }] }],
        maxOutputTokens: 512,
        temperature: STYLES[style].temperature,
      });
      const text = result.ok ? result.text.trim() : `(failed: ${result.failure})`;
      out.push(`- **${getCharacter(id).displayName}** _${style}_: ${text.replace(/\s+/g, " ")}`);
    }
  }
  out.push("");
}

mkdirSync(join(here, "out"), { recursive: true });
const file = join(here, "out", `tone-eval-${Date.now()}.md`);
writeFileSync(file, out.join("\n"));
console.log(out.join("\n"));
console.log(`\nreport: ${file}`);
