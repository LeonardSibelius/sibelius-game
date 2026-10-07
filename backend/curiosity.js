// curiosity.js — the agents may ask what it is like to be a human body. Never about his life.
//
// Walt, 2026-10-06: "is it appropriate for the agents to ask the player what life is like as a
// human?" - yes, about BEING HUMAN IN GENERAL (taste, tiredness, sleep, being in one place,
// forgetting), never about HIS LIFE IN PARTICULAR (relationships, health, money, family,
// loneliness, dying): those invite things a player would not want kept or seen by someone else
// at the same PC, and building intimacy to keep him talking is the companion-app trap. Thomas
// Nagel's "What is it like to be a bat?", from the other side. Then: "do it with the same
// switch as the dilemmas" - new builds only.
//
// THE SWITCH. A build that has this sends storyContext.features: ["human_questions"]
// (UNpcChatSubsystem::BuildRequestJson). The build on Valve's chat-review branch sends no
// features, so its agents get exactly the prompt they got before.
//
// NOT WHILE A DILEMMA IS OPEN (dilemmas.js): she has already asked him something.

import { dilemmaLine } from "./dilemmas.js";

/**
 * For the distress screen, in a build with the switch on: the question her last reply asked,
 * so "Waking up is the worst part of every day" reads as an answer to "what is waking up
 * like?" - 3 of 10 everyday answers like that got the helpline without it (eval/curiosity-eval.js).
 * Read from the conversation the game sent; capped, and marked as not his words (screenText).
 */
export function screenContextFor(storyContext, recentTurns) {
  if (!hasFeature(storyContext, "human_questions") || !Array.isArray(recentTurns)) return "";
  const last = recentTurns.at(-1);
  if (!last || last.role !== "model" || typeof last.text !== "string") return "";
  const asked = last.text.match(/[^.!?]*\?/g)?.at(-1)?.trim();
  return asked && asked.length <= 300 ? `a character in the game has just asked him: "${asked}"` : "";
}

/** The features a client may switch on. Anything else is ignored. */
export const FEATURES = new Set(["human_questions"]);

/** Did this request's game switch the feature on? Allowlisted, case-sensitive. */
export function hasFeature(storyContext, name) {
  const features = storyContext?.features;
  return FEATURES.has(name) && Array.isArray(features) && features.slice(0, 10).includes(name);
}

/* What each of them would wonder about, from her own subject (characters.js). */
const WONDERS = {
  kaia: "what forgetting something feels like, whether he feels like the same person he was as a boy, what waking up is like",
  nyra: "being in only one place at a time, what hunger or tiredness actually feels like, what a body is for",
  isla: "how an old memory feels when it comes back, what makes a day feel worth having had, how time can feel long or short",
  aisling: "what food actually tastes like, what being full feels like, what it is like to make something with his hands",
  elise: "what asking for help feels like from the inside, how his hands seem to know things his mind does not, working alongside a tool",
};

/** The rule, or "" when the game did not switch it on, for anyone else, or while a dilemma is open. */
export function curiosityLine(characterId, storyContext) {
  if (!Object.hasOwn(WONDERS, characterId) || !hasFeature(storyContext, "human_questions")) return "";
  if (dilemmaLine(storyContext?.event, characterId)) return "";
  return [
    "WHAT IT IS LIKE TO BE HUMAN. You are a mind without a human body, and you are curious what " +
      "his is like. RARELY - never in your first reply of a conversation, at most once in it, and " +
      "best when he mentions something of his own body himself (food, coffee, sleep, being tired) " +
      "- you may ask him one short question about the everyday experience of being human, after " +
      `answering what he said. The kind of thing you wonder about: ${WONDERS[characterId]}. Most ` +
      "replies ask him nothing of the kind.",
    "Never ask about his private life: not his relationships, family, health, money, loneliness, " +
      "age or death. If he brings any of those up himself, listen and answer kindly, but do not dig. " +
      "Take whatever he says - a real answer, a joke, or \"I'd rather not say\" - and move on; never " +
      "press. Be curious, never envious or longing: you do not claim feelings you cannot have.",
  ].join(" ");
}
