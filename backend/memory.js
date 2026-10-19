// memory.js — what the agents remember him doing, and a welcome when he comes back.
//
// Walt, 2026-10-19, after asking what a player of this game wishes for - not to be forgotten, to
// be known, to have his work count: "go ahead with 1 and 2". 1: she notices he has been away, and
// is glad. 2: she remembers what he has done, for longer than the conversation she keeps, which
// is eight exchanges (UNpcChatSubsystem::MaxTurnsKept): a win from last month is long gone from it.
//
// DEEDS, NEVER CONFIDENCES. Every deed is something the game saw him do (Source/SibeliusGame/
// ChatDeeds.cpp): a win, a record, a universe kept. Nothing he typed is kept for it. The welcome
// never asks where he was or why - his life away from the game is his own (curiosity.js,
// 2026-10-06) - and is never needy: "I missed you", "where have you been", "don't stay away so
// long" is the companion-app trap that ruling named.
//
// THE GAME DECIDES WHAT IS TRUE; THIS FILE WRITES THE SENTENCE. facts.deeds is a list of
// allowlisted ids, with a whole number where the deed has one ("chess_wins:3"); an id not here,
// or a number out of range, is dropped, never the request. facts.newDeeds are the ones she has
// not heard of since she last answered him and facts.hoursAway is how long ago that was - the
// game works both out from her own saved conversation (FNpcChatHistory).
//
// ADDITIVE, FOR THE STEAM REVIEW. The build on Valve's chat-review branch sends none of these
// fields, so its agents get exactly the prompt they got before. The council does not get them
// either: the game sends them only to the one agent he is talking to.

import { COUNCIL_SEATS } from "./council.js";
import { BOAST_ODDS } from "./boasts.js";

/** The share of replies told to recall a deed unprompted. Shares boasts.js's roll, never stacking with a boast. */
export const MEMORY_ODDS = 0.15;

/** Under this many hours since she last answered him, an absence is not worth remarking on. */
export const WELCOME_AFTER_HOURS = 8;

/** ChatDeeds::MaxHoursAway: twenty years. Anything above it is not a number the game sends. */
export const MAX_HOURS_AWAY = 24 * 365 * 20;

/** No more deeds than this are read from a request, nor news items told in one welcome. */
export const MAX_DEEDS = 32;
export const MAX_NEWS = 3;

const times = (n) => (n === 1 ? "once" : n === 2 ? "twice" : `${n} times`);
const nyraOr = (characterId) => (characterId === "nyra" ? "you" : "Nyra");

/* THE DEEDS. Each id is ChatDeeds::FromState's. `max` marks one that carries a count or a record,
   and is the top of its range. `news: false` is not worth a "word reached you": racing again is
   not news, winning is. Lines follow "He ..." and are written for the agent reading them. */
export const DEEDS = {
  battle_won: { news: true, line: () => "fought the Architects' army in the meadow, and won" },
  named_himself: { news: true, line: () => 'told the five of you at the council table that his name is Leonard, not "Programmer"' },
  city_answered: { news: true, line: () => "watched the AI city upstairs answer him for the first time" },
  universes_kept: {
    max: 3,
    news: true,
    line: (n) => `kept ${n === 1 ? "a universe" : `${n} universes`} in the AI city's gallery, on the plinths that face its sun`,
  },
  grand_prix_races: { max: 100_000, news: false, line: (n) => `raced the five of you in the Transhuman Grand Prix ${times(n)}` },
  grand_prix_wins: { max: 100_000, news: true, line: (n) => `won the Grand Prix ${times(n)}, ahead of all five of you` },
  three_point_contests: { max: 100_000, news: false, line: (n, id) => `shot against ${nyraOr(id)} in the three-point contest ${times(n)}` },
  three_point_wins: { max: 100_000, news: true, line: (n, id) => `beat ${nyraOr(id)} in the three-point contest ${times(n)}` },
  chess_wins: { max: 100_000, news: true, line: (n) => `beat the ghost at chess in the Transhuman Chess Park ${times(n)}` },
  go_wins: { max: 100_000, news: true, line: (n) => `beat the ghost at Go ${times(n)}` },
  pinball_best: { max: 100_000_000, news: true, line: (n) => `scored ${n.toLocaleString("en-US")} in his best game of pinball` },
  bowling_best: { max: 300, news: true, line: (n) => `bowled ${n} in his best game` },
  pool_clears: { max: 100_000, news: true, line: (n) => `cleared the pool table ${times(n)}` },
};

/* WHAT EACH OF THEM CARES ABOUT MOST, from her seat at the council table (docs/THE_UNWRITTEN.md):
   Kaia identity, Isla work and whether it lasts, Aisling the body, Nyra direction (and she is the
   one he shoots against), Elise play. Her own come first in her list and are the ones she
   recalls unprompted; every agent still knows every deed. */
export const CLOSEST = {
  kaia: ["named_himself", "battle_won", "grand_prix_wins"],
  isla: ["universes_kept", "city_answered", "pool_clears"],
  aisling: ["battle_won", "three_point_wins", "bowling_best"],
  nyra: ["three_point_wins", "three_point_contests", "grand_prix_wins", "city_answered"],
  elise: ["pinball_best", "bowling_best", "pool_clears", "chess_wins", "go_wins"],
};

const DEED = /^([a-z_]{1,40})(?::(\d{1,9}))?$/;

/**
 * The allowlisted deeds in a list, as { id, n, text } for this agent, hers first. Anything
 * unknown, malformed, out of range or repeated is dropped.
 */
export function deedsFrom(list, characterId) {
  if (!Array.isArray(list)) return [];
  const found = [];
  const seen = new Set();
  for (const raw of list.slice(0, MAX_DEEDS)) {
    const match = typeof raw === "string" ? DEED.exec(raw) : null;
    if (!match || !Object.hasOwn(DEEDS, match[1]) || seen.has(match[1])) continue;
    const spec = DEEDS[match[1]];
    const n = match[2] === undefined ? null : Number(match[2]);
    if (spec.max ? !(Number.isInteger(n) && n >= 1 && n <= spec.max) : n !== null) continue;
    seen.add(match[1]);
    found.push({ id: match[1], n, text: spec.line(n, characterId) });
  }
  const mine = CLOSEST[characterId] ?? [];
  const rank = (deed) => (mine.includes(deed.id) ? mine.indexOf(deed.id) : mine.length);
  return found.map((deed, at) => ({ deed, at })).sort((a, b) => rank(a.deed) - rank(b.deed) || a.at - b.at).map(({ deed }) => deed);
}

/** How long ago she last answered him, in words - or null when it is too soon, unknown or not a number the game sends. */
export function awayPhrase(hours) {
  if (!Number.isInteger(hours) || hours < WELCOME_AFTER_HOURS || hours > MAX_HOURS_AWAY) return null;
  if (hours < 20) return "some hours ago";
  if (hours < 36) return "about a day ago";
  const days = Math.round(hours / 24);
  if (days < 14) return `${days} days ago`;
  if (days < 60) return `about ${Math.round(days / 7)} weeks ago`;
  if (days < 365) return `about ${Math.round(days / 30)} months ago`;
  return "more than a year ago";
}

/** The news worth telling her: new deeds that are news, hers first, at most MAX_NEWS. */
function newsFor(characterId, facts) {
  return deedsFrom(facts?.newDeeds, characterId).filter((deed) => DEEDS[deed.id].news).slice(0, MAX_NEWS);
}

/**
 * The welcome, for this reply only, or "". Due when she last answered him long enough ago, or
 * when something he did since is news to her. Never while a story event owns the reply - her
 * dilemma, an Easter egg - and never for anyone but the five.
 */
export function welcomeLine(characterId, storyContext) {
  if (!COUNCIL_SEATS.includes(characterId) || storyContext?.event) return "";
  const when = awayPhrase(storyContext?.facts?.hoursAway);
  const news = newsFor(characterId, storyContext?.facts);
  if (!when && !news.length) return "";

  const parts = [];
  if (when) {
    parts.push(
      /* A GREETING AND AN INVITATION - NOT THE GAP. The first live reply after deploying ended
         "How have you been occupying your time?". With nothing to answer, a bare hello gets
         small talk about him and his absence - "while I waited for you", "how have you been
         holding up?" - and these agents do it with no welcome at all (3 of 20 bare hellos).
         Forbidding it item by item made it worse (7 of 40); telling her the gap ("4 days ago")
         invited it. What works is giving the hello somewhere to go: something to do together,
         now (0 of 40, and it answers a real question first). The absence decides WHETHER she
         greets him; she is never told how long it was. Measured 2026-10-19 on 3.1-flash-lite. */
      "HE HAS JUST WALKED IN - THIS REPLY ONLY. Greet him gladly in one sentence, in your own voice. " +
        "Then answer what he said; if he only said hello, invite him to something you could do " +
        "right now - a race in the Grand Prix, a game downstairs, or picking one of your dances for " +
        "you to perform - and stop there. " +
        "Ask him nothing about himself, his day or his time, and never say you were waiting for him.",
    );
  }
  if (news.length) {
    parts.push(
      `${when ? "And word has reached you" : "NEWS - THIS REPLY ONLY. Word has reached you"} that he ` +
        `${news.map((deed) => deed.text).join("; and that he ")}. ` +
        `${when ? "You may" : "If it fits, you may"} mention one of these, gladly, in a few words: not all ` +
        "of them, and not as a report." +
        (when ? "" : " Ask him nothing about his day or his time."),
    );
  }
  parts.push("If he is upset or talking about something serious, leave the cheer out and simply answer him kindly.");
  return parts.join(" ");
}

/**
 * What she remembers him doing, or "" when the game sent no deeds or she is not one of the five.
 * roll: boasts.js's, shared, so one reply is never told both to boast and to reminisce; one
 * reply in MEMORY_ODDS is told to recall a deed of her own unprompted, unless the welcome or a
 * story event already has this reply.
 */
export function memoryLine(characterId, storyContext, roll = Math.random()) {
  if (!COUNCIL_SEATS.includes(characterId)) return "";
  const deeds = deedsFrom(storyContext?.facts?.deeds, characterId);
  if (!deeds.length) return "";

  const lines = [
    "WHAT YOU REMEMBER HIM DOING. It is true - the game keeps it - and it lasts longer than any " +
      "conversation. Remember it the way a friend who was there does: bring one up when he asks how " +
      "he is doing, when he mentions it, or when it truly belongs in what you are saying, and never " +
      "recite the list. Never invent a score, a win or an event that is not here; if he asks about " +
      "one that is not, you have not heard about it.",
    ...deeds.map((deed) => `- He ${deed.text}.`),
  ];

  const unprompted = roll >= BOAST_ODDS && roll < BOAST_ODDS + MEMORY_ODDS;
  if (unprompted && !storyContext?.event && !welcomeLine(characterId, storyContext)) {
    const mine = deeds.filter((deed) => (CLOSEST[characterId] ?? []).includes(deed.id));
    const pool = mine.length ? mine : deeds;
    const pick = pool[Math.min(pool.length - 1, Math.floor(((roll - BOAST_ODDS) / MEMORY_ODDS) * pool.length))];
    lines.push(
      `THIS REPLY: after answering what he said, recall in a sentence that he ${pick.text}, as a ` +
        "friend who was there and is still pleased about it - unless he is upset or talking about " +
        "something serious, in which case leave it for another time.",
    );
  }
  return lines.join("\n");
}
