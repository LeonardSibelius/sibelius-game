// council.js — the five agents at one table (docs/THE_UNWRITTEN.md, "purely the council").
//
// A council turn is Leonard's line and five answers. The agents speak ONE AFTER ANOTHER,
// each seeing what the ones before her said this turn, because the whole point is that they
// react to each other — measured in eval/comic-eval.js: with "react to at least one other
// agent BY NAME", every tested variant argued. This file only builds prompts and decides the
// order; server.js does the calling, the budget and the screen.

import { getCharacter, locationLine } from "./characters.js";
import { systemInstructionFor } from "./styles.js";

/** Everyone at the table, in the base order the rotation starts from. */
export const COUNCIL_SEATS = ["kaia", "isla", "aisling", "nyra", "elise"];

/** What each agent cares about at the table — docs/THE_UNWRITTEN.md, "The five voices". */
export const COUNCIL_ANGLES = {
  kaia: "IDENTITY — who Leonard is now that he is not 'Programmer'.",
  isla: "WORK AND CRAFT — what he builds, and whether it lasts.",
  aisling: "HEALTH AND BODY — a man who lived on coffee at a desk for forty years.",
  nyra: "DIRECTION — where he goes next.",
  elise: "CREATIVITY AND PLAY — making things for the joy of it.",
};

/** How much of the table's earlier conversation each prompt carries. */
export const COUNCIL_HISTORY_LINES = 30;

const nameOf = (speaker) =>
  speaker === "leonard" ? "Leonard" : getCharacter(speaker)?.displayName ?? speaker;

/**
 * Who speaks first rotates, so Kaia is not always the opening act. Seeded from the request
 * id, not a clock or Math.random: a retried request must get the same order, or the dedup
 * replay and the original would disagree about who said what.
 */
export function speakingOrder(seed) {
  let hash = 0;
  for (const ch of String(seed)) hash = (hash * 31 + ch.charCodeAt(0)) >>> 0;
  const start = hash % COUNCIL_SEATS.length;
  return [...COUNCIL_SEATS.slice(start), ...COUNCIL_SEATS.slice(0, start)];
}

/**
 * The full system instruction for one agent's turn at the table: her profile and style
 * exactly as in one-to-one chat (house rules, places, who's who, foolishness), where Leonard
 * is standing, then the council itself — the recent conversation and what the agents before
 * her said this turn.
 */
export function councilInstruction({ id, style, locationId, history = [], soFar = [] }) {
  const recent = history.slice(-COUNCIL_HISTORY_LINES);
  const lines = (turns) => turns.map((t) => `${nameOf(t.speaker)}: ${t.text}`).join("\n");

  /* REACT ONLY TO WHAT WAS ACTUALLY SAID. "React to another agent by name" given to the
     first speaker, with nobody before her, made Kaia answer what "Nyra may think" before
     Nyra had spoken (2026-09-23). The first speaker reacts to Leonard; everyone after her
     reacts to someone who really spoke. */
  /* ONE ANSWER PER TURN, NOT FIVE. The answer-first rule (styles.js) told every speaker to
     answer a question of fact, so every speaker did: "where am i?" got the same sentence five
     times, "what are you trying to decide on?" four "We are deciding…" (Walt, 2026-09-23).
     The first speaker answers; everyone after her has heard the answer and moves on. */
  const soFarRule = soFar.length
    ? `${nameOf(soFar[0].speaker)} has already answered Leonard. Do NOT answer his question ` +
      "again and do not restate anything already said. " +
      "React to at least one agent who has already spoken, BY NAME — agree with her, disagree, " +
      "top her or tease her — or add something new from your own concern. React only to what " +
      "she actually said. Do not begin with the same words anyone before you began with."
    : "You speak first this turn, so react to what Leonard just said. Do not guess what the " +
      "other agents think — they have not spoken yet.";

  const council = `
THE COUNCIL. You are at one table with Leonard and the other four agents: Kaia, Isla,
Aisling, Nyra and Elise. Leonard has just said something to all of you, and you each answer
in turn.

YOUR CONCERN AT THE TABLE: ${COUNCIL_ANGLES[id]}

Speak once: one to three sentences, from your own concern. ${soFarRule}
Do not repeat what someone already said. Write only your own words: no name in front of
them, and never speak for anyone else.

THE CONVERSATION AT THE TABLE SO FAR:
${recent.length ? lines(recent) : "(this is the first thing said at the table)"}

THIS TURN, BEFORE YOU:
${soFar.length ? lines(soFar) : "(you speak first)"}
`.trim();

  return [
    // Only the first speaker gets the answer-first rule; see soFarRule.
    systemInstructionFor(getCharacter(id), id, style, { answerFirst: soFar.length === 0 }),
    locationLine(locationId),
    council,
  ].filter(Boolean).join("\n\n");
}

/**
 * Models sometimes prefix their own name ("Kaia: ...") however they are told. The panel
 * already shows who is speaking, so a leading "Name:" is removed — only her own name, and
 * only at the very start.
 */
export function stripOwnName(id, text) {
  const name = getCharacter(id)?.displayName;
  if (!name) return text;
  return text.replace(new RegExp(`^\\s*\\**${name}\\**\\s*:\\s*`, "i"), "").trim();
}
