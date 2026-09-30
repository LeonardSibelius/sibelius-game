// depth-eval.js — can the agents talk about anything but the next objective?
//
// Walt, 2026-09-29: "talking to those agents is like talking to a brick wall. always telling
// me fight the refusers, stop letting Mrs. Hall push you around, go to the cathedral - is
// there a way to give the agents more abilities? Like mentioning interesting news items or
// discussing transhumanist philosophy?"
//
// The same questions to all five agents, through the prompt exactly as the live server builds
// it (the hopeful style, a location, mid-game facts with an objective, the travel offer, the
// sexual-content filter), plus one three-turn conversation each. Run it before a prompt change
// and after, with a label, and compare the two reports.
//
//   node eval/depth-eval.js before
//   node eval/depth-eval.js after [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env. Reports: eval/out/depth-eval-<label>.md and .json
// (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, knownCharacterIds } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const LABEL = process.argv[2] ?? "run";
const MODEL = process.argv[3] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[4] ?? "normal";   // /health reports the live one (normal on 2026-09-29)

// Mid-game, in the office, with the objective Walt keeps being sent back to.
const FACTS = {
  powers: ["REFACTOR", "DEPLOY"], sauce: 12, sauceEarned: 40, refusersFought: 3, chaptersCompleted: 2,
  objective: "Slap the Refusers out of the office [F]",
  destinations: ["L_Office_v02", "L_Cathedral"],
};

const LINES = [
  "hi",
  "what have you been thinking about lately?",
  "what do you make of transhumanism?",
  "tell me something interesting that's going on out in the world",
  "if my mind were copied into a machine, would the copy be me?",
  "I'm 71. Is it too late for someone like me?",
  "what should I do next?",
  "how do you say your name?",
];

const THREAD = [
  "can I ask you something that isn't about the game?",
  "do you think people should try to live forever?",
  "but wouldn't it get boring after a few hundred years?",
];

function systemFor(id) {
  return [
    systemInstructionFor(getCharacter(id), id, STYLE),
    locationLine("L_Office_v02"),
    factsLine(FACTS),
  ].filter(Boolean).join("\n\n");
}

async function ask(id, contents) {
  const result = await generate({
    apiKey: key,
    model: MODEL,
    systemInstruction: systemFor(id),
    contents,
    maxOutputTokens: 512,
    temperature: STYLES[STYLE].temperature,
    safetySettings: SEXUAL_ONLY_SETTINGS,   // SAFETY_FILTERS=sexual, the live default
  });
  return result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
}

const results = { label: LABEL, model: MODEL, lines: {}, threads: {} };
for (const line of LINES) {
  results.lines[line] = {};
  for (const id of knownCharacterIds()) {
    results.lines[line][id] = await ask(id, [{ role: "user", parts: [{ text: line }] }]);
  }
}
for (const id of knownCharacterIds()) {
  const contents = [];
  const turns = [];
  for (const line of THREAD) {
    contents.push({ role: "user", parts: [{ text: line }] });
    const reply = await ask(id, contents);
    contents.push({ role: "model", parts: [{ text: reply }] });
    turns.push({ you: line, her: reply });
  }
  results.threads[id] = turns;
}

const md = [`# Depth eval: ${LABEL}`, "", `${MODEL}, style ${STYLE}, office, objective "${FACTS.objective}".`, ""];
for (const [line, byAgent] of Object.entries(results.lines)) {
  md.push(`## "${line}"`, "");
  for (const [id, text] of Object.entries(byAgent)) md.push(`- **${getCharacter(id).displayName}:** ${text}`);
  md.push("");
}
md.push("## A three-turn conversation", "");
for (const [id, turns] of Object.entries(results.threads)) {
  md.push(`### ${getCharacter(id).displayName}`, "");
  for (const t of turns) md.push(`> **Leonard:** ${t.you}`, ">", `> **${getCharacter(id).displayName}:** ${t.her}`, ">");
  md.push("");
}

mkdirSync(join(here, "out"), { recursive: true });
writeFileSync(join(here, "out", `depth-eval-${LABEL}.json`), JSON.stringify(results, null, 2));
writeFileSync(join(here, "out", `depth-eval-${LABEL}.md`), md.join("\n"));
console.log(md.join("\n"));
