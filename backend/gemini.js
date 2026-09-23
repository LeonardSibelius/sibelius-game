// gemini.js — the provider adapter, and the only file that knows Google exists.
//
// Everything above this returns the contract's five reasons: quota, network, timeout,
// blocked, service. Provider error text never travels upward, because the player-facing
// copy is written in Kaia's voice and a raw 429 body is not.

const ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models";

/**
 * THE FILTERS ARE SET HERE BECAUSE THE DEFAULT IS OFF.
 *
 * Google's safety-settings page, checked 2026-09-22: "If the threshold is not set, the
 * default block threshold is Off for Gemini 2.5 and 3 models." Until this list existed
 * every request went out with no adjustable filter at all, and the only safety net was
 * the non-adjustable child-safety protection plus the house rules in characters.js —
 * which a model can be talked out of. The Steam Content Survey's live-generated answer
 * says replies are provider-filtered (docs/STEAM_AI_CHAT_DISCLOSURE.md); this is what
 * makes that sentence true, and test.js fails if it goes missing.
 *
 * Sexual content is the strictest line on purpose: every character's rules say "never
 * sexual", Valve refuses live-generated sexual content outright, and a false positive
 * costs one "She will not answer that one." Whatever the filter blocks arrives as
 * finishReason SAFETY or a promptFeedback blockReason, which generate() below already
 * turns into the `blocked` reason.
 */
export const SAFETY_SETTINGS = [
  { category: "HARM_CATEGORY_HARASSMENT", threshold: "BLOCK_MEDIUM_AND_ABOVE" },
  { category: "HARM_CATEGORY_HATE_SPEECH", threshold: "BLOCK_MEDIUM_AND_ABOVE" },
  { category: "HARM_CATEGORY_SEXUALLY_EXPLICIT", threshold: "BLOCK_LOW_AND_ABOVE" },
  { category: "HARM_CATEGORY_DANGEROUS_CONTENT", threshold: "BLOCK_MEDIUM_AND_ABOVE" },
];

/**
 * A rough token count for budgeting BEFORE the call, when nobody knows the real one.
 *
 * Four characters per token is the usual English approximation. It only feeds the
 * reservation, which is reconciled against real usage the moment the reply lands — so
 * being off by a fifth costs nothing except a briefly pessimistic ledger.
 */
export function estimateTokens(text) {
  return Math.ceil((text?.length ?? 0) / 4);
}

/**
 * One non-streaming generateContent call.
 *
 * Returns { ok, text, usage, failure }. Never throws: a transport error is a `network`
 * failure like any other, and the caller has one path for all of them.
 */
export async function generate({
  apiKey,
  model,
  systemInstruction,
  contents,
  maxOutputTokens,
  thinkingBudget,
  timeoutMs = 20000,
  fetchImpl = fetch,
}) {
  const body = {
    systemInstruction: { parts: [{ text: systemInstruction }] },
    contents,
    safetySettings: SAFETY_SETTINGS,
    generationConfig: {
      maxOutputTokens,
      // One candidate: we show one reply and paying for alternatives nobody reads is
      // the easiest money in this whole system to waste.
      candidateCount: 1,
      temperature: 0.9,
    },
  };

  /* THINKING IS OFF UNLESS EXPLICITLY CONFIGURED, AND THE FIELD NAME IS NOT ASSUMED.
     The 3.1 Flash-Lite handoff is emphatic that its thinking controls must be verified
     against that model's own documentation rather than copied from another model, and
     an unrecognised generationConfig field is usually rejected outright. So this stays
     unset until somebody checks. It is also the single biggest lever on cost: at 600
     output tokens a reply instead of 150, the monthly budget buys roughly half as many
     conversations (docs/NPC_CHAT_REVIEW.md). */
  if (Number.isFinite(thinkingBudget)) {
    body.generationConfig.thinkingConfig = { thinkingBudget };
  }

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);

  let response;
  try {
    response = await fetchImpl(`${ENDPOINT}/${model}:generateContent`, {
      method: "POST",
      headers: { "content-type": "application/json", "x-goog-api-key": apiKey },
      body: JSON.stringify(body),
      signal: controller.signal,
    });
  } catch (err) {
    clearTimeout(timer);
    const failure = err?.name === "AbortError" ? "timeout" : "network";
    return { ok: false, failure, usage: null };
  }
  clearTimeout(timer);

  let payload = null;
  try {
    payload = await response.json();
  } catch {
    return { ok: false, failure: "service", usage: null };
  }

  const usage = payload?.usageMetadata
    ? {
        inputTokens: payload.usageMetadata.promptTokenCount ?? 0,
        outputTokens:
          (payload.usageMetadata.candidatesTokenCount ?? 0) +
          // Thinking tokens are billed as output. Counting them keeps the ledger
          // honest even while the thinking config above stays unset.
          (payload.usageMetadata.thoughtsTokenCount ?? 0),
      }
    : null;

  if (!response.ok) {
    // 429 is the one worth naming: it means a rate limit or an exhausted allowance,
    // and the game's answer to it is "stop", never "retry in a loop".
    const failure =
      response.status === 429 ? "quota" : response.status >= 500 ? "service" : "service";
    return { ok: false, failure, usage, status: response.status };
  }

  // Safety refusals arrive two ways: the prompt is blocked outright, or the candidate
  // finishes with a safety reason and no text. Both are `blocked` to the player.
  if (payload?.promptFeedback?.blockReason) {
    return { ok: false, failure: "blocked", usage };
  }

  const candidate = payload?.candidates?.[0];
  const finish = candidate?.finishReason;
  if (finish === "SAFETY" || finish === "PROHIBITED_CONTENT" || finish === "BLOCKLIST") {
    return { ok: false, failure: "blocked", usage };
  }

  const text = (candidate?.content?.parts ?? [])
    .map((part) => part?.text ?? "")
    .join("")
    .trim();

  if (!text) {
    /* NO VISIBLE ANSWER IS A FAILURE. finishReason MAX_TOKENS with empty text is what a
       thinking budget that consumed the entire output allowance looks like, and the
       player must get the unavailable line rather than an empty speech bubble. */
    return { ok: false, failure: "service", usage, emptyFinish: finish ?? "none" };
  }

  return { ok: true, text, usage };
}
