// news.js — what the agents know about the world outside the house.
//
// WALT'S BRIEF, 2026-09-25: "I want the player to have the sense that they can be a
// Leonard, empowered by AI progress."
//
// That is an editorial rule, not a topic. The test every item has to pass is:
//
//     DOES THIS SHOW ONE PERSON DOING WHAT USED TO NEED AN INSTITUTION?
//
// Not "is it AI news". Most AI news fails that test, and a lot of it fails the game.
// Measured, not assumed: a harvest of high-scoring Hacker News AI stories on 2026-09-25
// returned court rulings, an NSA procurement story, funding rounds, and two separate
// pieces about AI replacing workers. Piping that into Kaia would have inverted the whole
// point of the game at a 71-year-old who was made obsolete by an industry.
//
// Show HN is the feed that fits, because every Show HN is one person who built a thing.
// `node news-candidates.js` harvests it and pre-filters; a human still picks.
//
// CURATED AT BUILD TIME, NEVER FETCHED AT RUNTIME. The server makes no outbound call for
// this and there is no cache to go stale. That is deliberate: a news feed in the prompt is
// a third party writing into every agent's instructions for every player at once, and the
// injection guard in HOUSE_RULES covers the player's own text, not somebody else's
// headline. A title on a public board is attacker-writable. This constant is not.
//
// NO SOURCES, NO COMPANY NAMES, NO LINKS — on purpose. These are AI agents in a city with
// a spaceport on the lawn; they do not read Hacker News, and "as reported on" would punch
// a hole in the fiction. They simply know things, the way they know anything else. Each
// line is the substance with the citation filed here instead.
//
// REFRESHING: run the harvester, pick three or four that pass the test above, rewrite them
// as one plain sentence each, replace the list, redeploy. Ten minutes, whenever it feels
// stale. Keep the list SHORT — six is plenty, and every line is paid for on every request.

/**
 * Six things the agents know. Each traces to a real Show HN post from the 2026-09-29
 * harvest; the bracketed note is the source, for whoever refreshes this next.
 */
const ITEMS = [
  // Refreshed 2026-09-29 (Walt: "mentioning interesting news items"), from
  // `node news-candidates.js --wide --days 60`. Each line says only what the post says: the
  // 2026-09-25 list had one ("a man who wrote an audio toolkit ... built himself a coding
  // partner") that the agents garbled into "an audio toolkit that fits on a single machine".

  // Show HN: Real-time Solar System with 526k asteroids and all tracked satellites (102pt, 2026-09-29)
  "Someone built a live map of the solar system in a web page: every tracked satellite and more than half a million asteroids, all moving in real time.",

  // Show HN: Galaxium, an experimental WebGPU space explorer (137pt)
  "Someone made a galaxy you can fly through, and it runs in an ordinary web browser.",

  // Show HN: Maple-Preview - Ternary 20B MoE running at 120 tok/s on a iPhone (173pt)
  "A large AI model, twenty billion parameters, now runs quickly on an ordinary phone.",

  // Show HN: Mini-AGI - Dynamic continual learning model trained on 8GB VRAM (277pt)
  "Somebody got a model to keep learning after it was finished, on a graphics card you could buy for playing games.",

  // Show HN: I trained a 125M model to autocomplete piano on-device (598pt)
  "Someone taught a small model to finish a piano melody, and it runs on their own machine.",

  // Show HN: Voice driven murder mystery, Interview AI suspects with your voice (215pt)
  "Someone made a murder mystery you solve out loud, by talking to the suspects until one of them slips.",
];

/**
 * The block appended to every agent's instructions.
 *
 * THE LAST LINE IS THE BRIEF, AND IT IS WITHHELD FROM THE PLAYER ON PURPOSE. A character
 * who says "the things that used to need a building now fit on a desk" is delivering a
 * message; one who simply knows six things like that, and mentions one when it belongs,
 * is a person he is talking to. The difference is the whole game.
 */
export const THE_WORLD_OUTSIDE = `
WHAT IS HAPPENING OUT THERE. A few things you know are true beyond this house. You do not
cite them, you do not name who did them, and you never call them news — you simply know,
the way you know anything else.
${ITEMS.map((s) => `- ${s}`).join("\n")}

WHEN TO USE ONE. When he asks what is new, what is going on out there, or for something
interesting; when he doubts any of this is meant for someone like him; when he wonders what
one person can actually do on their own; or when one of them genuinely connects to what you
are talking about. Say what you think of it, not just that it happened. Never more than one
in a reply. Never as a list. Never to change the subject away from something that matters
to him, and never to cheer him up — if he is struggling, that is the moment to listen, not
to produce an example.

What every one of them has in common is the thing you must NOT say out loud: what used to
take a building full of people now fits on a desk. He is allowed to notice that himself.
`.trim();

/** For the gate, and for anyone checking what the prompt actually costs. */
export function newsItemCount() {
  return ITEMS.length;
}
