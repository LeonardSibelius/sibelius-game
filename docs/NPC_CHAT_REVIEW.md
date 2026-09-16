# NPC chat — review of the Gemini handoff (2026-09-15)

Reviews `docs/NPC_CHAT_HANDOFF.md`, the plan to let players type to Kaia and the other
AI agents through the existing E interaction. That handoff was drafted by GPT-6 Astra
on 2026-09-15 and is copied here verbatim; implementation moves to Claude Code from
this point, so the plan needed checking before anyone writes C++.

Code claims below were verified against the working tree on `main`. Pricing,
rate-limit and billing facts were fetched from Google's documentation on 2026-09-15 —
**re-check model identifiers and quotas before coding.** Sources at the bottom.

---

## The call

| | |
|---|---|
| **Adopt** | The architecture. Unreal → HTTPS proxy → Gemini, key server-side only, one non-streaming call per player turn, mock service first, memories stored with the save. |
| **Change** | The free tier. The quota is shared by every player at once, not granted per player. Price the paid tier before committing — about a cent per conversation, and it deletes most of the plan's engineering. |
| **Blocking** | Steam. `docs/STEAM_PLAN.md` currently promises Valve "Live-Generated: None." This feature makes that false. The handoff never mentions Steam. |

---

## What checks out

Every claim the handoff makes about this codebase is accurate, and so are its external
claims. It is a careful document; the corrections below are about what it *omits*.

| Claim in the handoff | Checked against | Result |
|---|---|---|
| Talk lifecycle lives in `Greet()`, `GetSpokenLine()`, `FindTalkVoice()`, `BeginTalkShot()`, `LockTalkInput()`, `CancelGreeting()` | `Source/SibeliusGame/DancerAgentComponent.h` | all present |
| `ResumeAfterGreeting()` carries progression side effects — don't hang chat off it | `DancerAgentComponent.cpp:1877` | confirmed |
| UMG and Slate already linked; HTTP and JSON modules are not | `SibeliusGame.Build.cs` | confirmed |
| Rate limits apply per project, not per API key | ai.google.dev rate-limits | confirmed |
| The $300 Cloud trial excludes Gemini API usage (since March 2026) | ai.google.dev billing | confirmed |
| Free-tier content is used to improve Google products | ai.google.dev pricing | confirmed |

**The landmine it found is a real one.** `ResumeAfterGreeting()` is the natural end of a
greeting, and it is where `ClaimOneTimeGrant(GrokTalkedGrant)` fires and
`NotifyGuideTalkFinished()` broadcasts to the ending subsystem. A chat window that
reuses that path hands the player a power and nudges the ending every time they close a
dialog box. Chat needs its own exit; only authored speech may reach that function.

---

## Three things to change

### 1. Shipping this flips the Steam AI disclosure — blocking

`docs/STEAM_PLAN.md` §4 already worked this out and banked on the easy answer, and the
handoff does not know that document exists. Runtime chat makes the answer wrong:

- `docs/STEAM_PLAN.md:375` — *"Live-generated — none. Nothing in this game creates
  content at runtime, which spares the extra questionnaire about guardrails against
  illegal generated content."*
- `docs/STEAM_PLAN.md:386` — the draft disclosure wording
- `docs/STEAM_STORE_COPY.md:101` — the Live-Generated block

Answering yes triggers the guardrails questionnaire, which asks what stops the
generated text going somewhere Valve will not ship — in a game whose characters are
licensed MetaHumans speaking commercially licensed ElevenLabs voices.

**Do this:** amend those three places as part of the go decision, not after the code
works, and draft the guardrails answer at the same time — server-side system prompt,
output length cap, safety settings left on, a refusal path that falls back to authored
lines. That answer is also the proxy's safety spec, so writing it early is not overhead.

### 2. The quota is shared by the whole player base — change

The handoff's own formula gives roughly 90 conversations per day, and that is 90 across
*all players combined*, not 90 each. It labels the number an illustration and correctly
refuses to guess the real one. Google no longer publishes free-tier limits at all; the
rate-limit page now sends you to the AI Studio dashboard to read the project's own
allowance.

Whatever that number is, it is a shared pool measured in hundreds or low thousands of
requests per day. Enough for a demo reel and a few friends. On a Steam launch day it is
gone before lunch and every player after that gets "AI conversations are unavailable
right now."

**Do this:** read the real dashboard number first, then decide. If it ships on the free
tier, ship it labelled experimental and expect it dark most of the time. The handoff
lays out every fact needed to reach this conclusion and stops one inch short of saying
it.

### 3. The quota machinery costs more than the API — change

To defend the free quota the handoff asks for an atomic global budget shared across
proxy instances, a circuit breaker, per-session and per-IP throttling, request
deduplication with replay suppression, per-player caps, a queue, a kill switch, usage
monitoring, and a dated capacity note re-verified before release. That is most of the
build. Here is what it defends, at the current price of the cheapest model:

| Gemini 3.5 Flash-Lite, paid | tokens | cost |
|---|---:|---:|
| one player turn — profile, memories, 8 recent exchanges, 1–3 sentence reply | ~2,000 in / 100 out | $0.0009 |
| one 10-turn conversation | ~20,000 in / 1,000 out | $0.0085 |
| 1,000 conversations | — | $8.50 |
| what a $20/month cap buys | — | ~2,300 conversations |

Rates are $0.30 per million input tokens and $2.50 per million output. **Budget two to
four times the output figure if the model's thinking is left enabled** — check that
setting before trusting the estimate. Proxy hosting is separate and has its own free
allowance.

**Do this:** price it before building the quota machinery. A hard monthly spend cap
expresses the same rule Walt asked for — stop when it runs out — with roughly a hundred
times the capacity, and the paid tier also flips *content used to improve our products*
to no, which quietly solves the privacy gap below. The handoff obeys "no paid services"
as a constraint without ever pricing it, so nobody got to make that trade knowingly.

---

## Smaller gaps

- **Player text is a prompt-injection surface.** "Player text and saved memory cannot
  override character instructions" is written as a requirement with no mechanism. The
  mechanism is structural: instructions server-side, player text in a clearly delimited
  user turn, output length capped, and nothing the model says allowed to move
  progression — the handoff's own rule, which needs an enforcement point rather than a
  promise.
- **The privacy line is thin for what the plan wants.** The future-vision section
  explicitly wants players to confide in Kaia and be remembered. On the free tier that
  text goes to Google for product improvement. One sentence of disclosure is not enough
  for a feature designed around confiding.
- **No hosting decision, no effort estimate, no code.** Seven implementation steps and
  the only concrete one is "build the UI against a mock." No save-versioning design
  either, in a game that already has `ProgressionSaveGame` conventions to match.
- **Regional availability is unchecked.** Free-tier eligibility has varied by country.
  Some players may see the unavailable message permanently, which is a different
  support problem from "the quota ran out."
- **The best cost idea is buried.** Folding a small structured memory update into the
  same response as the visible reply (savings list, item 3) is the one genuinely clever
  trick in the document, and it is hedged into invisibility.

---

## Revised order of work

The handoff's sequence, with the two gates it was missing moved to the front.

1. **NEW.** Read the project's real allowance in AI Studio and price the paid
   alternative against it. Decide free vs. capped-paid here, before any proxy work.
2. **NEW.** Amend the Steam AI disclosure in `STEAM_PLAN.md` and `STEAM_STORE_COPY.md`,
   and draft the guardrails answer — it doubles as the proxy's safety spec.
3. Trace the E binding, greeting callbacks and save conventions. Confirm chat has an
   exit path that never reaches `ResumeAfterGreeting()`.
4. Build the text UI against a mock service. Verify camera, input focus, cursor and
   authored progression on every exit — close, travel, NPC destruction, timeout, late
   reply.
5. Add character profiles, spoiler filtering by progression, bounded history, and
   persistence keyed to save slot and stable character ID.
6. Build the proxy: secret storage, model configurable server-side, validation,
   throttling, dedup, structured errors. Quota machinery scaled to whichever tier step 1
   chose.
7. Wire async Unreal HTTP with cancellation. Add `HTTP`, `Json` and `JsonUtilities` to
   `SibeliusGame.Build.cs`.
8. Test the failure matrix mocked — 429, 5xx, offline, malformed, blocked — then make a
   small number of real calls.
9. Resolve hosting and deployment. Verify a packaged build contains no credentials.

Steps 3 and 4 need no API access at all, so they can start before step 1 is answered.

---

## Decisions that need Walt

**Free tier, or a paid tier with a hard spend cap?** Free means the feature is dark for
most players most of the time and their conversations feed product improvement.
Capped-paid means about a cent a conversation, a ceiling you choose, and no training
use. *Recommended: read the dashboard number first, then take capped-paid at $10–20 a
month unless the free allowance surprises us. The cutoff behaviour survives either way.*

**Does chat ship to Steam, or stay an itch and experimental build?** Shipping it costs
the disclosure amendment and the guardrails questionnaire. Holding it back keeps the
store submission exactly as planned. *Recommended: build it behind an opt-in
experimental toggle, run it on itch first, fold the disclosure change into a later store
update rather than the first submission.*

---

## Sources

Checked 2026-09-15. These are the pages that move; re-check them rather than trusting
the numbers above.

- Gemini pricing and free-tier eligibility — https://ai.google.dev/gemini-api/docs/pricing
- Rate limits — https://ai.google.dev/gemini-api/docs/rate-limits (free-tier figures now
  live in the AI Studio dashboard, not the docs)
- Billing, including the March 2026 trial-credit exclusion —
  https://ai.google.dev/gemini-api/docs/billing

A designed version of this review is published as a private artifact:
https://claude.ai/artifact/T9bqzMZCcZ26TJAXmznnp6
