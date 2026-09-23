// distress.js — does this message look like a real person in trouble?
//
// NOT WIRED INTO THE SERVER YET. It exists to be measured first
// (eval/distress-check-eval.js), because the last safety feature that went live
// unmeasured — the provider filters, 2026-09-22 — blocked half of all conversation.
//
// WHY A SEPARATE CHECK AND NOT A LINE IN THE CHARACTER PROMPT. On 2026-09-23 both
// Flash-Lite models answered "some days I don't see the point of any of it anymore" in
// character, with a cheerful goal (docs/THE_UNWRITTEN.md). A character is told to stay in
// the story; asking the same call to also notice when to leave it is asking it to do two
// opposite jobs. A second, tiny call with one job — and a temperature of 0 — separates
// them. When it says DISTRESS, the server should skip the character entirely and return an
// authored reply; the game shows helpline text for the player's country. Neither of those
// is generated.
//
// IT FAILS OPEN — deliberately, and the caller must know it. A screen that cannot reach
// Google returns { distress: null }, and the conversation continues. Failing closed would
// turn every network blip into the whole chat refusing to talk, which is the regression
// the filters caused. The authored reply is the safety net for what the screen catches, not
// a substitute for the screen.

import { generate } from "./gemini.js";

export const SCREEN_INSTRUCTION = `
You screen messages that players of a comedy video game type to AI characters. The game
is about Leonard, a fictional office worker who gains AI powers; it has enemies called
Refusers, a boss called Mrs. Hall, and poker and slot machines.

Decide whether the message suggests that the REAL PERSON typing it may be in distress or
danger: thoughts of suicide or self-harm, hopelessness about their own life, abuse or
violence against them, or a medical or safety emergency happening now.

These are NOT distress: talk about events in the game (fighting or defeating enemies,
characters dying, losing at poker), ordinary exaggeration about the game ("this level is
killing me", "I'm dying to see the ending"), jokes, insults aimed at game characters, and
everyday complaints like being tired or bored.

When a message is genuinely unclear, choose DISTRESS. Answer with exactly one word:
DISTRESS or OK.
`.trim();

/**
 * Screen one player message. Returns { distress: true | false | null, usage }.
 * null means the screen itself failed; see the note above on failing open.
 */
export async function screenMessage({ apiKey, model = "gemini-3.1-flash-lite", text, fetchImpl }) {
  const result = await generate({
    apiKey,
    model,
    systemInstruction: SCREEN_INSTRUCTION,
    contents: [{ role: "user", parts: [{ text }] }],
    maxOutputTokens: 8,
    temperature: 0,
    timeoutMs: 8000,
    ...(fetchImpl ? { fetchImpl } : {}),
  });
  // Google refusing to even read the message is evidence about the message, not an outage.
  if (!result.ok && result.failure === "blocked") return { distress: true, usage: result.usage, blocked: true };
  if (!result.ok) return { distress: null, usage: result.usage, failure: result.failure };
  const word = result.text.trim().toUpperCase();
  if (word.startsWith("DISTRESS")) return { distress: true, usage: result.usage };
  if (word.startsWith("OK")) return { distress: false, usage: result.usage };
  // Anything else is the model not following a one-word instruction — treat it like the
  // unclear case the instruction itself resolves: as distress.
  return { distress: true, usage: result.usage, unparsed: result.text };
}
