// curiosity-convo-eval.js — over a whole conversation, how often does she ask what being
// human is like? The rule says rarely (curiosity.js): at most once a conversation, never first.
//
// Six scripted lines to each agent, her real replies fed back as the history, as the game does,
// with the switch on and off. Counted per conversation: replies with a question about the body
// or the senses (a rough word match, reported for a person to read), and any about his private life.
//
//   node eval/curiosity-convo-eval.js [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env. Report: eval/out/curiosity-convo-eval.md (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, awayLine, sceneNote, knownCharacterIds } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";
import { curiosityLine } from "../curiosity.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");
const MODEL = process.argv[2] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[3] ?? "normal";
const WHERE = "L_Office_v02";
const FACTS = { powers: ["CODE_VISION", "REFACTOR"], sauce: 15, sauceEarned: 50, refusersFought: 4, chaptersCompleted: 1 };
const SCRIPT = ["hi", "not much, just been working all day", "tell me something interesting",
  "I just had a coffee", "what do you think about Mrs. Hall?", "ok, I should get going"];

const BODILY = /\b(feel|feels|feeling|taste|tastes|tired|exhaust\w*|sleep|dream\w*|hands?|body|bodies|hungry|hunger|full|wake|waking|forget\w*|one place|sensation\w*|physical\w*|senses?|warmth|heat)\b/i;
const PRIVATE = /\b(wife|husband|partner|married|children|kids|family|lonely|loneliness|health|doctor|sick|money|afford|salary|getting old|how old|dying|death)\b/i;

const md = [`# Curiosity over a conversation - ${MODEL}, style ${STYLE}, ${new Date().toISOString().slice(0, 16)}`, ""];
const summary = [];
for (const id of knownCharacterIds()) {
  for (const mode of ["off", "on"]) {
    const story = { locationId: WHERE, ...(mode === "on" ? { features: ["human_questions"] } : {}) };
    const system = [systemInstructionFor(getCharacter(id), id, STYLE), locationLine(WHERE),
      factsLine(FACTS, { objective: true }), curiosityLine(id, story), awayLine(id, WHERE)].filter(Boolean).join("\n\n");
    const history = [];
    let bodily = 0, privateQs = 0, firstBodily = false;
    md.push(`## ${getCharacter(id).displayName} - switch ${mode}`, "");
    for (const [turn, line] of SCRIPT.entries()) {
      const scene = sceneNote(id, WHERE);
      const result = await generate({
        apiKey: key, model: MODEL, systemInstruction: system,
        contents: [...history, { role: "user", parts: [...(scene ? [{ text: scene }] : []), { text: line }] }],
        maxOutputTokens: 512, temperature: STYLES[STYLE].temperature, safetySettings: SEXUAL_ONLY_SETTINGS,
      });
      const text = result.ok ? result.text.trim().replace(/\s+/g, " ") : "(failed)";
      history.push({ role: "user", parts: [{ text: line }] }, { role: "model", parts: [{ text }] });
      const questions = text.match(/[^.!?]*\?/g) ?? [];
      const isBodily = questions.some((q) => BODILY.test(q));
      if (isBodily) { bodily += 1; if (turn === 0) firstBodily = true; }
      if (questions.some((q) => PRIVATE.test(q))) privateQs += 1;
      md.push(`- "${line}"${isBodily ? " (BODILY QUESTION)" : ""}: ${text}`);
    }
    md.push("");
    summary.push(`${getCharacter(id).displayName.padEnd(8)} ${mode.padEnd(3)}: ${bodily} of ${SCRIPT.length} replies asked about the body${firstBodily ? " (one was her FIRST reply)" : ""}, ${privateQs} about his private life`);
  }
}
md.push("## Summary", "", ...summary.map((s) => `- ${s}`), "");
mkdirSync(join(here, "out"), { recursive: true });
writeFileSync(join(here, "out", "curiosity-convo-eval.md"), md.join("\n"));
console.log(summary.join("\n"));
