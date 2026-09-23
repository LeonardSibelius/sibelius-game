// council-opening-eval.js — do the later council speakers repeat the first one's answer?
//
// Walt, 2026-09-23: asked "what are you trying to decide on?", four of five agents opened
// "We are deciding whether…". The answer-first rule (styles.js) told every speaker to answer
// the question, so every speaker did. This runs real council turns through the live prompt
// (councilInstruction) and counts later speakers whose opening echoes the first speaker's.
//
//   node eval/council-opening-eval.js
//
// Reads GEMINI_API_KEY from backend/.env. Five questions x five speakers = 25 small calls.
// Report goes to eval/out/ (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate } from "../gemini.js";
import { councilInstruction, speakingOrder, stripOwnName } from "../council.js";
import { STYLES } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const MODEL = "gemini-3.1-flash-lite";
const STYLE = "foolish";

// Direct questions of fact are the case that triggered it; one opinion question for contrast.
const QUESTIONS = [
  "what are you trying to decide on?",
  "where am i?",
  "who gave me the Refactor power?",
  "what should I do next?",
  "what do you all think of my week?",
];

const words = (text) => text.toLowerCase().replace(/[^a-z' ]/g, " ").split(/\s+/).filter(Boolean);
/** An echo: the same first three words as the first speaker, or the stock "we are …" opening. */
const echoes = (first, text) => {
  const a = words(first).slice(0, 3).join(" ");
  const b = words(text).slice(0, 3).join(" ");
  return b === a || /^we (are|re) (deciding|trying)/.test(words(text).join(" "));
};

const report = [];
let later = 0;
let echoed = 0;

for (const [qi, question] of QUESTIONS.entries()) {
  const order = speakingOrder(`opening-eval-${qi}`);
  const soFar = [];
  for (const id of order) {
    const result = await generate({
      apiKey: key,
      model: MODEL,
      systemInstruction: councilInstruction({ id, style: STYLE, locationId: "L_Office_v02", history: [], soFar }),
      contents: [{ role: "user", parts: [{ text: question }] }],
      maxOutputTokens: 512,
      temperature: STYLES[STYLE].temperature,
    });
    const text = result.ok ? stripOwnName(id, result.text) : `(failed: ${result.failure})`;
    if (soFar.length) {
      later += 1;
      if (echoes(soFar[0].text, text)) echoed += 1;
    }
    soFar.push({ speaker: id, text });
  }
  report.push({ question, turn: soFar });
  process.stdout.write(`\n> ${question}\n`);
  for (const line of soFar) process.stdout.write(`  ${line.speaker}: ${line.text.slice(0, 110)}\n`);
}

const summary = `later speakers echoing the first speaker's opening: ${echoed}/${later}`;
console.log(`\n${summary}`);

mkdirSync(join(here, "out"), { recursive: true });
const file = join(here, "out", `council-opening-${Date.now()}.json`);
writeFileSync(file, JSON.stringify({ model: MODEL, style: STYLE, summary, report }, null, 2));
console.log(`report: ${file}`);
