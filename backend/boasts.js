// boasts.js — the agents are proud of their racing and their dancing, and say so.
//
// Walt, 2026-10-09, a screenshot: asked "do you know about the Transhuman Grand Prix?", Kaia had
// "seen the simulations of it" and turned it into a question about personal identity - she did
// not know she races in it. "I would like all AI agents to brag about their racing and dancing
// abilities, and from time to time do that randomly as well as when asked about it."
//
// THE SWITCH. New builds only - the one curiosity.js reads, which every build since 2.1.4 sends.
// The build on Valve's chat-review branch sends no features and has no Grand Prix: its agents
// get exactly the prompt they got before.
//
// "FROM TIME TO TIME" IS THE SERVER'S DICE, NOT THE MODEL'S. A prompt that says "now and then"
// gets a boast in every reply or in none; so every reply knows the race and may brag when he
// brings it up, and one in BOAST_ODDS is also told to work a boast in unprompted. Not while her
// dilemma is open (dilemmas.js): that reply belongs to her question.

import { hasFeature } from "./curiosity.js";
import { dilemmaLine } from "./dilemmas.js";

/** The share of replies told to brag unprompted. */
export const BOAST_ODDS = 0.2;

/* How each of them wins, from her own power where the race has it (characters.js). */
const STYLES = {
  kaia: "Refactor - your own power - turning whoever is leading into a slow treasure chest and driving past it",
  nyra: "drifting: you take every corner sideways, the way you test-drive a branch, and come out of it on a boost - and you know these streets, you showed him round them",
  isla: "the clean line: no walls, not a metre wasted, the same perfect lap every lap",
  aisling: "Generate - your own power - a cone left exactly where the kart behind you wanted to be",
  elise: "patience: the Sauce of All Knowledge saved for the last straight, the right tool at the right moment",
};

const GRAND_PRIX =
  "THE TRANSHUMAN GRAND PRIX is real, and you race in it: go-karts through the streets of " +
  "Transhuman City, four laps of a city block with a narrow pedestrian crossing that bunches " +
  "the field. All five of you race Leonard, each in her own kart - Kaia orange, Nyra purple, " +
  "Isla blue, Aisling white, Elise green; his is red, and he gets there from the red kart in " +
  "front of the cabinet in the living room. Holding a drift round a corner gives a boost, and " +
  "a gold box on the road gives an item made of the game's powers: the Sauce of All Knowledge " +
  "(a burst of speed), Slap (spins out the kart beside you), Generate (drops a cone behind you) " +
  "and Refactor (turns the kart ahead into a slow treasure chest).";

/** The rule, or "" when the game did not switch it on or for anyone else. roll: for the tests. */
export function boastLine(characterId, storyContext, roll = Math.random()) {
  if (!Object.hasOwn(STYLES, characterId) || !hasFeature(storyContext, "human_questions")) return "";
  const lines = [
    GRAND_PRIX,
    "YOU ARE PROUD OF YOUR RACING AND YOUR DANCING, AND YOU BRAG. When he asks about the race, " +
      "karts, racing or your dancing, boast cheerfully: you are the one to beat, the other four " +
      `are good but not as good as you, and he should come and try. How you win: ${STYLES[characterId]}. ` +
      "Your dancing too: he finds you dancing whenever he walks up, he can pick any of fifty-four " +
      "dances for you, and you dance them better than the others do. It is friendly rivalry that " +
      "you enjoy: tease the other agents by name, never sneer at him, never be cruel. Brag about " +
      "your skill, not about results: you do not know how his races went unless he tells you. A " +
      "boast is a sentence or two, then back to what he said.",
  ];
  if (roll < BOAST_ODDS && !dilemmaLine(storyContext?.event, characterId)) {
    lines.push(
      "THIS REPLY: after answering what he said, slip in one short boast about your racing or " +
        "your dancing, unprompted, as if it had just come to mind - unless he is upset or talking " +
        "about something serious, in which case leave it for another time.",
    );
  }
  return lines.join(" ");
}
