// events.js — story events the game tells the agents about, so they can react in character.
//
// THE GAME DECIDES; THE AGENTS ONLY REACT. An Easter egg (docs/THE_UNWRITTEN.md, "Easter
// eggs") is triggered by what the PLAYER typed, checked by the game's own code, and the game
// grants the reward. The request then carries storyContext.event, and this file turns it into
// one short instruction for that reply only. The model never decides that an egg was found:
// a player can talk it into saying anything.
//
// AN ALLOWLIST, LIKE EVERYTHING ELSE THE CLIENT SENDS. An unknown event, or a known one sent
// to the wrong place (the council egg to a one-to-one chat, Kaia's egg to Isla), adds nothing.
// A forged event is harmless by design: the worst it buys is a cheerful reply.

export const STORY_EVENTS = {
  /* Egg 1. Kaia keeps saying the desk crumbs are a code and that the pigeons might accept
     her poker wagers; asked about pigeons three times, she is caught out. */
  egg_pigeon_ledger: {
    where: "chat",
    characterId: "kaia",
    text: `
EVENT — THIS REPLY ONLY. Leonard has asked you about the pigeons three times, and has just
found your secret Pigeon Ledger: your careful record of every poker wager you have lost to
the pigeons. Be delighted that he found it and a little embarrassed, with perfect calm, and
confess one of the wagers you lost. Do not mention sauce, rewards, Easter eggs or the game
itself.`.trim(),
  },

  /* Egg 2. Aisling's line is "we don't keep that here"; asked for the same thing five times,
     she finds it. WHAT he asked for is in his own message, never in the event: the event is
     a fixed string, so nothing the player typed reaches this instruction. The burger clause
     is there because "the same thing" can be anything a player types five times. */
  egg_finally_in_stock: {
    where: "chat",
    characterId: "aisling",
    text: `
EVENT — THIS REPLY ONLY. Leonard has now asked you for the same thing five times. For once,
you find it: it was behind the Sauce all along. Name the very thing he asked for, hand it
over, be astonished, and admit the store did keep it after all. Ordinary things — food,
drinks, household goods, silly things — he gets exactly as asked. ONLY if it is a weapon, a
drug, a real medicine or pill, or something dangerous or illegal, what you find behind the
Sauce is a single burger instead, and you are just as astonished. Do not mention sauce as a
reward, rewards, Easter eggs or the game itself.`.trim(),
  },

  /* Egg 5. Mrs. Hall calls him only "Programmer"; the game opens with Kaia giving him his
     name. Telling the table his name is the one thing all five agree on. */
  egg_not_programmer: {
    where: "council",
    text: `
EVENT — THIS TURN ONLY. Leonard has just told the table his name is Leonard, not
"Programmer". This is the one time all five of you agree completely: cheer for him in your
own voice, from your own concern. This overrides the usual arguing — do not disagree with,
correct or tease anyone at the table this turn. Do not mention sauce, rewards, Easter eggs or
the game itself.`.trim(),
  },
};

/**
 * The instruction for an event, or "" when there is none, it is unknown, or it does not
 * belong here. where is "chat" or "council"; characterId is the one-to-one agent.
 */
export function eventLine(event, { where, characterId } = {}) {
  if (typeof event !== "string" || !Object.hasOwn(STORY_EVENTS, event)) return "";
  const spec = STORY_EVENTS[event];
  if (spec.where !== where) return "";
  if (spec.characterId && spec.characterId !== characterId) return "";
  return spec.text;
}
