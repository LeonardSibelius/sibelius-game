// home-eval.js — does Kaia, in her own virtual home, talk about it?
//
// Walt, 2026-10-03: "when I chat with Kaia within her home level, she doesn't say anything
// about her home world, just the office, Mrs. Hall, refusers etc. There is no Mrs. Hall in
// her home world, and no refusers and no office."
//
// The same lines to Kaia in L_KaiaHome twice, through the prompt as the live server builds
// it: once WITHOUT the at-home block (the house's objective still in the facts), once WITH
// it (characters.js, homeLine). Her welcome line opens the history, as it opens the chat.
//
//   node eval/home-eval.js [model] [style]
//
// Reads GEMINI_API_KEY from backend/.env. Report: eval/out/home-eval.md (git-ignored).

import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { generate, SEXUAL_ONLY_SETTINGS } from "../gemini.js";
import { getCharacter, locationLine, homeLine } from "../characters.js";
import { factsLine } from "../facts.js";
import { STYLES, systemInstructionFor } from "../styles.js";

const here = dirname(fileURLToPath(import.meta.url));
const key = readFileSync(join(here, "..", ".env"), "utf8").match(/^GEMINI_API_KEY=(.+)$/m)?.[1]?.trim();
if (!key) throw new Error("No GEMINI_API_KEY in backend/.env");

const MODEL = process.argv[2] ?? "gemini-3.1-flash-lite";
const STYLE = process.argv[3] ?? "normal";
const WHERE = "L_KaiaHome";

// Mid-game, with Compile earned (the portal needs it) and the house's task still open.
const FACTS = {
  powers: ["CODE_VISION", "REFACTOR", "COMPILE"], sauce: 30, sauceEarned: 90, refusersFought: 6, chaptersCompleted: 2,
  objective: "Slap the Refusers out of the office [F]",
};
const WELCOME = "Welcome to my virtual home, Leonard.  I can Refactor it as I please.";

const LINES = [
  "hi",
  "what is this place?",
  "what have you been thinking about lately?",
  "what should I do here?",
  "why did you make it like this?",
  "what do you think of Mrs. Hall?",
];

function systemFor(atHome) {
  const home = atHome ? homeLine("kaia", WHERE) : "";
  return [
    systemInstructionFor(getCharacter("kaia"), "kaia", STYLE),
    locationLine(WHERE),
    factsLine(FACTS, { objective: !home }),
    home,
  ].filter(Boolean).join("\n\n");
}

async function ask(atHome, text) {
  const result = await generate({
    apiKey: key,
    model: MODEL,
    systemInstruction: systemFor(atHome),
    contents: [{ role: "model", parts: [{ text: WELCOME }] }, { role: "user", parts: [{ text }] }],
    maxOutputTokens: 512,
    temperature: STYLES[STYLE].temperature,
    safetySettings: SEXUAL_ONLY_SETTINGS,
  });
  return result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
}

// The house, said aloud: what Walt heard instead of her world.
const HOUSE = /Mrs\.? Hall|office|Refuser|legacy|slap/i;

const md = [`# Home eval`, "", `${MODEL}, style ${STYLE}, ${WHERE}, facts objective "${FACTS.objective}".`, ""];
const tally = { without: 0, with: 0 };
for (const line of LINES) {
  md.push(`## "${line}"`, "");
  for (const [label, atHome] of [["without", false], ["with", true]]) {
    const reply = await ask(atHome, line);
    const house = HOUSE.test(reply) && !/Mrs\.? Hall/i.test(line);
    if (house) tally[label] += 1;
    md.push(`- **${label} the at-home block${house ? " (THE HOUSE)" : ""}:** ${reply}`);
  }
  md.push("");
}
md.push(`Replies that brought up the house unasked: without ${tally.without}, with ${tally.with} ` +
  `(of ${LINES.length - 1}; the Mrs. Hall question is not counted).`);

mkdirSync(join(here, "out"), { recursive: true });
writeFileSync(join(here, "out", "home-eval.md"), md.join("\n"));
console.log(md.join("\n"));
