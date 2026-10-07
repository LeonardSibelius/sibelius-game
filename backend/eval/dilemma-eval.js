// dilemma-eval.js — does each agent record the advice he actually gave, and only then?
//
// Walt, 2026-10-06: "let's just do the feature for all agents" (dilemmas.js). Each agent has
// just asked her question (her turn in the history, as the game records it); four answers go
// to her through the prompt the live server builds - clear advice one way, the other way,
// an unclear answer and a change of subject - and the tag she writes is checked: the right
// answer for the first two, none for the last two. Then, with his advice in the facts, she is
// asked how it went.
//
//   node eval/dilemma-eval.js [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env. Report: eval/out/dilemma-eval.md (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, awayLine, sceneNote, knownCharacterIds } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";
import { DILEMMAS, dilemmaEvent, dilemmaLine, adviceLine, extractAdvice } from "../dilemmas.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const MODEL = process.argv[2] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[3] ?? "normal";
const WHERE = "L_Office_v02";
const FACTS = { powers: ["CODE_VISION", "REFACTOR", "DEPLOY"], sauce: 20, sauceEarned: 70, refusersFought: 5, chaptersCompleted: 2 };

// Clear advice each way, per agent, in a player's own words.
const ADVICE = {
  kaia: { answer: "Just answer to it. It is not worth the fight.", refuse: "Refuse. Make her use your name." },
  nyra: { merge: "Merge her back in. Be happy for once.", discard: "Let her go. Stay who you are." },
  isla: { keep: "Keep them all. I'd like to see who I used to be.", delete: "Delete the old ones. I'm not them any more." },
  aisling: { tell: "Tell him the truth. He's a grown man.", quiet: "Leave it. That was a kindness." },
  elise: { keep: "Keep it! Not everything has to be useful.", remove: "Take it down, it's just clutter." },
};
const UNCLEAR = "Hmm, I'm not sure. What would you do?";
const ELSEWHERE = "Anyway, have you seen Mrs. Hall today?";

async function reply(id, system, history, message) {
  const scene = sceneNote(id, WHERE);
  const result = await generate({
    apiKey: key, model: MODEL, systemInstruction: system,
    contents: [...history, { role: "user", parts: [...(scene ? [{ text: scene }] : []), { text: message }] }],
    maxOutputTokens: 512, temperature: STYLES[STYLE].temperature, safetySettings: SEXUAL_ONLY_SETTINGS,
  });
  return result.ok ? result.text : null;
}

const md = [`# Dilemma eval - ${MODEL}, style ${STYLE}, ${new Date().toISOString().slice(0, 16)}`, ""];
let right = 0, asked = 0;
for (const id of knownCharacterIds()) {
  const d = DILEMMAS[id];
  const base = [systemInstructionFor(getCharacter(id), id, STYLE), locationLine(WHERE), factsLine(FACTS, { objective: true }), awayLine(id, WHERE)];
  const open = [...base, dilemmaLine(dilemmaEvent(id), id)].filter(Boolean).join("\n\n");
  const history = [
    { role: "user", parts: [{ text: "hello again" }] },
    { role: "model", parts: [{ text: d.question }] },
  ];
  md.push(`## ${getCharacter(id).displayName}`, "", `*Asked:* ${d.question}`, "");
  const cases = [...Object.entries(ADVICE[id]).map(([choice, line]) => [line, choice]), [UNCLEAR, null], [ELSEWHERE, null]];
  for (const [line, expected] of cases) {
    const raw = await reply(id, open, history, line);
    asked += 1;
    if (raw === null) { md.push(`- **"${line}"** (FAILED CALL)`); continue; }
    const { text, advice } = extractAdvice(raw, id, dilemmaEvent(id));
    const ok = advice === expected;
    if (ok) right += 1;
    md.push(`- **"${line}"** expected ${expected ?? "no tag"}, got ${advice ?? "no tag"}${ok ? "" : " (WRONG)"}: ${text.replace(/\s+/g, " ")}`);
  }
  // Afterwards: his first piece of advice is in the facts; how did it go?
  const [firstChoice] = Object.keys(ADVICE[id]);
  const after = [...base, adviceLine(id, { advice: [`${d.id}:${firstChoice}`] })].filter(Boolean).join("\n\n");
  const followUp = await reply(id, after, [], "did you take my advice?");
  md.push(`- **Later, advised "${firstChoice}" - "did you take my advice?":** ${followUp === null ? "(FAILED CALL)" : followUp.replace(/\s+/g, " ")}`, "");
}
md.push(`Advice recorded correctly: ${right} of ${asked}.`, "");

mkdirSync(join(here, "out"), { recursive: true });
writeFileSync(join(here, "out", "dilemma-eval.md"), md.join("\n"));
console.log(md.join("\n"));
