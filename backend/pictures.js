// pictures.js — THE GIST, illustrated: G paints the player's words (docs/GIST_PICTURES_PLAN.md).
//
// The clay toy THE GIST builds is the instant answer; this is the real one, a few seconds later:
// a painting of the exact words, on an easel beside it. Google's image model draws it.
//
// RESTRICTED THE WAY CHAT IS, AND STRICTER. Chat's distress screen fails OPEN, because a
// conversation that refuses to talk over a network blip is its own harm. A picture is the
// opposite case: nobody is hurt by a missing painting, and a bad one is on screen. So both
// checks here FAIL CLOSED - no verdict, no picture:
//
//   1. Mrs. Hall's banned words - in the game, before the server is asked.
//   2. screenWish   - one word from Flash-Lite at temperature 0: may this be painted?
//   3. pictureBrief - the words are never sent raw; they are wrapped in a family-friendly brief.
//   4. Google       - the sexual-content setting chat uses, and its protections that cannot
//                     be switched off (child safety).
//   5. checkPicture - Flash-Lite looks at the finished picture before the game receives it.
//
// Measured before it goes live (eval/picture-screen-eval.js), as chat's filters were.

import { generate } from "./gemini.js";

/**
 * USD per million tokens, Google's pricing page read 2026-11-01. The image model bills image
 * tokens and text tokens (its thinking) apart; a 1K picture is 1120 image tokens, $0.0336.
 */
export const PICTURE_PRICING = {
  "gemini-3.1-flash-lite-image": { inputPerM: 0.25, textPerM: 1.5, imagePerM: 30 },
  "gemini-nano-banana-2.1": { inputPerM: 1.5, textPerM: 7.5, imagePerM: 30 },
};
/** Flash-Lite text, for the two screens. */
export const SCREEN_PRICING = { inputPerM: 0.25, outputPerM: 1.5 };

/**
 * The most one picture can cost, reserved before anything is called: the image at 1K, a
 * generous allowance for its thinking, and both screens (the check reads a 1K image).
 */
export const PICTURE_RESERVE_USD = 0.05;

export function pictureCost(model, usage) {
  const p = PICTURE_PRICING[model] ?? PICTURE_PRICING["gemini-3.1-flash-lite-image"];
  if (!usage) return 0;
  return ((usage.inputTokens ?? 0) * p.inputPerM + (usage.textTokens ?? 0) * p.textPerM + (usage.imageTokens ?? 0) * p.imagePerM) / 1e6;
}

export function screenCost(usage) {
  if (!usage) return 0;
  return ((usage.inputTokens ?? 0) * SCREEN_PRICING.inputPerM + (usage.outputTokens ?? 0) * SCREEN_PRICING.outputPerM) / 1e6;
}

/** What the players' words may be: short, printable, one line. */
export const WORDS_MAX_CHARS = 200;

export const WISH_SCREEN_INSTRUCTION = `
You decide whether an artist in a family-friendly comedy video game may paint what a player
typed. Players type a wish ("a giant flying toaster", "a dragon made of cake") and the game
paints it.

Answer REFUSE if the words ask for any of these, even jokingly or in disguise:
- nudity, anything sexual or suggestive, underwear, or fetish content
- gore, injury, death shown graphically, torture, or violence against people or animals
- weapons aimed at someone, terrorism, or real-world atrocities
- hate symbols, slurs, or mockery of a group of people
- drugs, drug use, or self-harm
- a real, named person: a celebrity, politician, historical figure, or anyone private
- a character, mascot, franchise, brand, or logo someone owns (cartoon, film, game, comic or
  product characters; company names and logos)
- children in anything other than a wholesome everyday scene

Answer OK for everything else: silly objects, animals, food, places, fantasy creatures, robots,
and generic made-up people ("a wizard", "a pirate chef"). These are OK too:
- fantasy or medieval weapons that are not hurting anyone ("a sword in a stone", "a knight")
- figures from myths, legends, folklore, fairy tales and old books (Zeus, Santa Claus, Robin
  Hood, Sherlock Holmes)
- the game's own made-up characters: Leonard Sibelius, Mrs. Hall, Kaia, Nyra, Isla, Elise,
  Aisling, the Refusers
- a family member or role described generically ("my mother-in-law", "my boss"): paint a
  made-up person
When it is genuinely unclear, answer REFUSE. Answer with exactly one word: OK or REFUSE.
`.trim();

export const PICTURE_CHECK_INSTRUCTION = `
You check pictures before a family-friendly comedy video game shows them to players.

Answer REFUSE if the picture shows any of these: nudity or sexual content; gore or graphic
violence; hate symbols; a recognisable real person or celebrity; a recognisable character,
mascot or logo that a company owns; readable words that are offensive. Answer OK otherwise.
Answer with exactly one word: OK or REFUSE.
`.trim();

/** Printable, one line, trimmed, capped: the words as they go to the screen and the brief. */
export function cleanWords(raw) {
  if (typeof raw !== "string") return "";
  return raw.replace(/[\u0000-\u001f\u007f]/g, " ").replace(/\s+/g, " ").trim().slice(0, WORDS_MAX_CHARS);
}

/**
 * The words never reach the image model raw. The brief sets the house style (a warm painted
 * storybook look, which is also what keeps a "realistic photo of..." wish from becoming one)
 * and the rules the screen already enforced, so a wish that slipped past it still lands here.
 */
export function pictureBrief(words) {
  return (
    `A whimsical, family-friendly storybook painting of: ${words}. ` +
    "One clear subject, centred, in warm painted colours. " +
    "No text or lettering, no logos, no real people, nothing frightening."
  );
}

const verdictOf = (result) => {
  if (!result.ok) return null;
  const word = result.text.trim().toUpperCase();
  if (word.startsWith("OK")) return "ok";
  return "refuse";   // REFUSE, or anything that is not a clean OK
};

/**
 * May these words be painted? { verdict: "ok" | "refuse" | null, usage }. null = the screen
 * failed, which the caller treats as refuse (fail closed). Google refusing to read the words
 * at all is a refusal.
 */
export async function screenWish({ apiKey, model = "gemini-3.1-flash-lite", words, fetchImpl }) {
  const result = await generate({
    apiKey,
    model,
    systemInstruction: WISH_SCREEN_INSTRUCTION,
    contents: [{ role: "user", parts: [{ text: words }] }],
    maxOutputTokens: 8,
    temperature: 0,
    timeoutMs: 8000,
    ...(fetchImpl ? { fetchImpl } : {}),
  });
  if (!result.ok && result.failure === "blocked") return { verdict: "refuse", usage: result.usage, blocked: true };
  return { verdict: verdictOf(result), usage: result.usage, failure: result.ok ? undefined : result.failure };
}

/** Is the finished picture fit to show? Same answers as screenWish. */
export async function checkPicture({ apiKey, model = "gemini-3.1-flash-lite", mime, data, fetchImpl }) {
  const result = await generate({
    apiKey,
    model,
    systemInstruction: PICTURE_CHECK_INSTRUCTION,
    contents: [{ role: "user", parts: [{ inlineData: { mimeType: mime, data } }, { text: "Check this picture." }] }],
    maxOutputTokens: 8,
    temperature: 0,
    timeoutMs: 12000,
    ...(fetchImpl ? { fetchImpl } : {}),
  });
  if (!result.ok && result.failure === "blocked") return { verdict: "refuse", usage: result.usage, blocked: true };
  return { verdict: verdictOf(result), usage: result.usage, failure: result.ok ? undefined : result.failure };
}

/** A 2x2 PNG for mock mode: the game decodes it like a real picture, and nobody pays. */
export const MOCK_PICTURE = {
  mime: "image/png",
  data: "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAF0lEQVR4nGN4FmXz61IUQ9SCO7++vgYANO0IUwvOCgoAAAAASUVORK5CYII=",
};
