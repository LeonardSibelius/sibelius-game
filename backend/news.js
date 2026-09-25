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
 * Six things the agents know. Each traces to a real Show HN post from the 2026-09-25
 * harvest; the bracketed note is the source, for whoever refreshes this next.
 */
const ITEMS = [
  // Show HN: I trained a 125M model to autocomplete piano on-device (598pt)
  "Someone taught a small model to finish a piano melody, and it runs on the same machine he trained it on.",

  // Show HN: Needle2: 14MB agentic LLM for phones, wearables, smart home and robots (537pt)
  "There is a model now that fits in fourteen megabytes and can still pick up a tool and use it — small enough for a wristwatch.",

  // Show HN: Mini-AGI – Dynamic continual learning model trained on 8GB VRAM (277pt)
  "Somebody got a model to keep learning after it was finished, on a graphics card you could buy for playing games.",

  // Show HN: Juggler – an open-source GUI coding agent, by the creator of JUCE (280pt)
  "A man who wrote an audio toolkit decades ago has built himself a coding partner, and he is still the one deciding what gets made.",

  // Show HN: Find the best local LLM for your hardware, ranked by benchmarks (283pt)
  "People are running these things on hardware they already own, at their own desks, answering to nobody.",

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

WHEN TO USE ONE. Only when it belongs: when he doubts any of this is meant for someone like
him, when he wonders what one person can actually do on their own, or when he asks what you
have been paying attention to. Never more than one in a reply. Never as a list. Never to
change the subject away from something that matters to him, and never to cheer him up — if
he is struggling, that is the moment to listen, not to produce an example.

What every one of them has in common is the thing you must NOT say out loud: what used to
take a building full of people now fits on a desk. He is allowed to notice that himself.
`.trim();

/** For the gate, and for anyone checking what the prompt actually costs. */
export function newsItemCount() {
  return ITEMS.length;
}
