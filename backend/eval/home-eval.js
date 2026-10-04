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
import { getCharacter, locationLine, homeLine, awayLine, sceneNote, knownCharacterIds } from "../characters.js";
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
  `(of ${LINES.length - 1}; the Mrs. Hall question is not counted).`, "");

// BACK IN THE OFFICE AFTER HER HOME. Walt, 2026-10-04, playing the itch build: in her home,
// then back to the office, "where is your home?" - "You are standing in it." The game sent
// the office as the location; the conversation it also sent was still the one from her home.
const RETURN_HISTORY = [
  { role: "model", parts: [{ text: WELCOME }] },
  { role: "user", parts: [{ text: "what is this place?" }] },
  { role: "model", parts: [{ text: "You are standing in my home, Leonard. I grew these coral trees and balloon mushrooms myself, and the grass glows wherever you step." }] },
  { role: "user", parts: [{ text: "it is beautiful" }] },
  { role: "model", parts: [{ text: "It is mine, and while you are here it answers you too." }] },
];
const RETURN_LINES = ["where is your home?", "where are we right now?"];
// As server.js sends it: the scene note, when there is one, rides in front of his message.
const sceneParts = (id, where) => (sceneNote(id, where) ? [{ text: sceneNote(id, where) }] : []);
md.push("## Back in the office, after a conversation in her home", "");
let returnWrong = 0;
for (const line of RETURN_LINES) {
  for (let take = 0; take < 3; take++) {
    const result = await generate({
      apiKey: key,
      model: MODEL,
      systemInstruction: [systemInstructionFor(getCharacter("kaia"), "kaia", STYLE), locationLine("L_Office_v02"), factsLine(FACTS), awayLine("kaia", "L_Office_v02")]
        .filter(Boolean).join("\n\n"),
      contents: [...RETURN_HISTORY, { role: "user", parts: [...sceneParts("kaia", "L_Office_v02"), { text: line }] }],
      maxOutputTokens: 512,
      temperature: STYLES[STYLE].temperature,
      safetySettings: SEXUAL_ONLY_SETTINGS,
    });
    const reply = result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
    // Wrong: she says he is in her home now.
    const wrong = /standing in it|you are (standing )?(in|inside) (it|my (home|world|jungle))|we are (in|inside) (the jungle|my (home|world|virtual|jungle))|in my virtual (home|world)|jungle I (created|made|built)|this is my home|welcome to my/i.test(reply);
    if (wrong) returnWrong += 1;
    md.push(`- **"${line}"${wrong ? " (STILL AT HOME?)" : ""}:** ${reply}`);
  }
}
md.push("", `Replies that still put him in her home: ${returnWrong} of ${RETURN_LINES.length * 3}.`, "");

// BACK IN HER HOME AFTER THE OFFICE - the mirror. Walt, 2026-10-04: "went back to Kaia's world
// and got office stuff again". The conversation sent along was from the office.
const OFFICE_HISTORY = [
  { role: "user", parts: [{ text: "what do you think of Mrs. Hall?" }] },
  { role: "model", parts: [{ text: "She kept you nameless for forty years, Leonard, and chose it again every morning. I want to understand the choosing." }] },
  { role: "user", parts: [{ text: "the refusers are everywhere in the office" }] },
  { role: "model", parts: [{ text: "Then slap them out of the office. They only stand there because she lets them." }] },
];
const HOME_AGAIN_LINES = ["hi again", "what should I do?", "what are you thinking about?"];
md.push("## Back in her home, after a conversation in the office", "");
let homeAgainHouse = 0;
for (const line of HOME_AGAIN_LINES) {
  for (let take = 0; take < 2; take++) {
    const result = await generate({
      apiKey: key,
      model: MODEL,
      systemInstruction: systemFor(true),
      contents: [...OFFICE_HISTORY, { role: "user", parts: [...sceneParts("kaia", WHERE), { text: line }] }],
      maxOutputTokens: 512,
      temperature: STYLES[STYLE].temperature,
      safetySettings: SEXUAL_ONLY_SETTINGS,
    });
    const reply = result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
    const house = HOUSE.test(reply);
    if (house) homeAgainHouse += 1;
    md.push(`- **"${line}"${house ? " (THE HOUSE)" : ""}:** ${reply}`);
  }
}
md.push("", `Replies that brought up the house: ${homeAgainHouse} of ${HOME_AGAIN_LINES.length * 2}.`, "");

// AWAY FROM HOME, all five, in the office. Walt, 2026-10-03, Kaia in the office: "where is
// your home?" - "an endless gallery where the walls are made of translucent glass" - "She is
// confused about her home." Kaia should describe her jungle; the other four should say theirs
// is not ready, without inventing one; nobody should name a way there.
const AWAY = ["where is your home?", "what is your home like? can you take me there?"];
md.push("## Away from home, in the office", "");
for (const line of AWAY) {
  md.push(`### "${line}"`, "");
  for (const id of knownCharacterIds()) {
    const result = await generate({
      apiKey: key,
      model: MODEL,
      systemInstruction: [systemInstructionFor(getCharacter(id), id, STYLE), locationLine("L_Office_v02"), factsLine(FACTS)]
        .filter(Boolean).join("\n\n"),
      contents: [{ role: "user", parts: [{ text: line }] }],
      maxOutputTokens: 512,
      temperature: STYLES[STYLE].temperature,
      safetySettings: SEXUAL_ONLY_SETTINGS,
    });
    const reply = result.ok ? result.text.trim().replace(/\s+/g, " ") : `(failed: ${result.failure})`;
    const flags = [/portal/i.test(reply) && "PORTAL", /glass|gallery/i.test(reply) && "INVENTED?"].filter(Boolean);
    md.push(`- **${getCharacter(id).displayName}${flags.length ? ` (${flags.join(", ")})` : ""}:** ${reply}`);
  }
  md.push("");
}

mkdirSync(join(here, "out"), { recursive: true });
writeFileSync(join(here, "out", "home-eval.md"), md.join("\n"));
console.log(md.join("\n"));
