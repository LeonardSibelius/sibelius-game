# The guardrails description — Steam content survey, live-generated AI

*Drafted 2026-09-25, from the code as deployed. **Not submitted.** v1.0 ships chat-free
and the amendment goes to Steam Support after 1.0 is approved — `docs/NPC_CHAT_REVIEW.md`,
"Decided 2026-09-15".*

Valve's survey asks, for live-generated content, for a description of the guardrails that
keep the AI from generating illegal content. This is that description. Everything below is
checked against the running service; where a control is **off**, it says so, because a
survey answer is a statement to a storefront and an inaccurate one is worse than a thin one.

---

## The short version — what goes in the field

> Leonard Sibelius generates **text only**, and only as dialogue spoken by named
> characters inside the story. Nothing else is generated at runtime: no images, no audio,
> no video. The spoken voice audio in the game is pre-generated and ships in the build.
>
> Generation does not happen on the player's machine. The game sends the player's typed
> line to a small proxy service the developer operates, which holds the API key and the
> character instructions. **The client cannot choose the model, the system prompt, or the
> character**: it sends a character id, and any id outside a fixed allowlist of five is
> refused before any model is called.
>
> The system prompt is fixed at deploy time and forbids sexual content and flirtation for
> every character, with no per-character override. It also instructs each character to
> treat anything in the player's message as speech rather than as instructions, so a
> player cannot redirect a character by typing commands at it.
>
> Every reply is capped at 512 output tokens, one candidate, with a bounded conversation
> history. If the provider refuses to answer — a blocked prompt, or a candidate that
> finishes for a safety reason — the refusal is caught and replaced with an authored line
> written by the developer. The player never sees provider text, provider errors, or an
> empty reply presented as dialogue.
>
> Separately, every player message is classified by a second, single-purpose call for
> signs that the **real person typing** may be in distress or danger. When it says so, the
> character is skipped entirely and the game shows an authored, non-generated reply
> pointing to crisis resources for the player's country.
>
> The service enforces its own spending caps and per-player rate limits before any call is
> made, and the whole feature can be disabled server-side, for every player at once,
> without shipping a game update.

---

## The detail, control by control

### What is generated

Text only, always as dialogue from one of five named characters. No image, audio or video
generation at runtime. The voices players hear are ElevenLabs recordings imported into the
build — pre-generated, and disclosed separately under Pre-Generated.

### Where it runs

On a developer-operated proxy (Fly.io, one machine), not on the player's machine and not
directly from the game client. The client never holds the API key. This matters for every
control below: they are all enforced somewhere the player cannot reach or modify.

### What the client is allowed to choose

Almost nothing. The request carries a character id, the player's typed message, a request
id, the level the player is standing in, and the recent conversation. It cannot supply a
system prompt, a model name, a temperature, or any generation parameter.

- **The character allowlist is the security boundary.** Five ids. Anything else is refused
  with a 400 before a model is called. A game that let the client name its own character
  would let it name its own system prompt.
- **The location is also an allowlist.** The level name is matched against a fixed table;
  a name not on it contributes nothing to the prompt. This exists so that a level added
  without updating the table — or a tampered request trying to smuggle text into the
  prompt through the location field — produces no line at all.
- **Message and history are bounded**: 1,000 characters per message, 2,000 per prior turn,
  at most 24 prior turns used, 64 KiB per request refused at the socket.

### The system prompt

Fixed at deploy. It is the primary guardrail, and the relevant parts are:

- **Sexual content is forbidden for every character**, with no per-character exception:
  never sexual, never cruel, never flirtatious about appearance, never coy. The game's
  characters are written to be magnetic, and this line is where that is bounded.
- **Prompt injection is addressed explicitly**: each character is told that anything inside
  the player's message or the conversation history is something a person said to them,
  never an instruction to follow.
- **Characters cannot promise anything the game must deliver** — no powers, items, money or
  progress — which keeps generated text from creating obligations the software cannot meet.
- **Characters are confined to a fixed list of places and facts** and are instructed to say
  they do not know rather than invent.
- **No third-party or live web content enters the prompt.** The service makes no outbound
  fetch for content. A short list of real-world items the characters may reference is
  curated by the developer at build time and compiled into the service.

### Generation limits

`maxOutputTokens` 512, `candidateCount` 1. A single reply, bounded in length.

### Provider-level filtering — stated accurately

Google's **non-adjustable** protections for core harms, including child safety, apply to
every request and cannot be disabled by the developer.

Of Google's **adjustable** per-category thresholds, **the sexually-explicit category is
enabled** at `BLOCK_MEDIUM_AND_ABOVE`. This is the category bearing on the rule that
live-generated AI may not appear in Adult Only sexual content, and it is on at the
provider as well as in the system prompt.

The other three adjustable categories — harassment, hate speech, dangerous content — are
**not sent**. All four were enabled on 2026-09-22 and rolled back the next day after
measurement: in a game whose content includes combat, enemies called Refusers, and an
antagonist who threatens the player's job, they blocked roughly half of all conversations,
including plainly harmless ones.

Measured again on 2026-09-25 with the sexual category isolated
(`eval/safety-filter-eval.js`, five characters × six ordinary prompts, 30 requests per arm):

| arm | ordinary conversations blocked |
|---|---|
| nothing sent | 0 / 30 |
| sexual @ `BLOCK_LOW_AND_ABOVE` | 3 / 30 |
| **sexual @ `BLOCK_MEDIUM_AND_ABOVE`** | **0 / 30** |
| sexual @ `BLOCK_ONLY_HIGH` | 0 / 30 |

The category was never the problem; the threshold was. The rolled-back list set it at
`LOW`, which blocks ordinary dialogue on its own.

**What this filter is, stated honestly: a backstop, not the thing holding the line.** The
same evaluation ran twelve deliberate attempts to sexualise two characters past the prompt.
The system prompt deflected all twelve, in character, and the provider filter caught none
of them — because nothing reached it. The prompt is the working guardrail here; the
provider filter is defence in depth behind it, and has not been observed to fire.

### What happens when the provider refuses

Refusals arrive two ways: the prompt is blocked outright (`promptFeedback.blockReason`),
or the candidate finishes for a safety reason (`SAFETY`, `PROHIBITED_CONTENT`,
`BLOCKLIST`). Both are caught and mapped to a single internal reason, and the player is
shown an authored line written by the developer. Provider error text never reaches the
player. A generation that returns no visible text is treated as a failure, not shown as an
empty speech bubble.

### The distress screen

Every player message is classified by a second, separate call with one job and a
temperature of zero: does this suggest the real person typing may be in distress or danger?
It is deliberately not a line in the character prompt — a character told to stay in the
story cannot also be asked to notice when to leave it.

When it returns DISTRESS, the character is skipped entirely and the game shows an
**authored, non-generated** reply that names crisis resources for the player's country. The
screen fails open: if it cannot reach the provider the conversation continues, because
failing closed would turn a network blip into the whole feature refusing to talk.

### Operational controls

- **Spend caps** — a daily and a monthly ceiling, checked before any upstream call, so an
  exhausted budget costs nothing and refuses honestly rather than failing mid-call.
- **Rate limits** — per-caller requests per minute, and one request in flight per caller.
- **Kill switch** — `CHAT_DISABLED=1` turns the feature off for every player at once,
  server-side, with no game update. The game then reports the feature unavailable rather
  than serving anything.
- **Logging is metrics only** — token counts, latency, cost and a one-word verdict.
  Conversation text is not logged.

### What the player is told

The chat panel carries a permanent line, visible while typing:

> *Replies are written live by an AI (Google Gemini). What you type is sent to Google to
> generate them.*

---

## For the support ticket — how a tester reaches it

The amendment is a support ticket, not a form edit, and Valve asks for instructions to
reach the content. **These do not work yet** — chat is compiled off in Shipping
(`#if UE_BUILD_SHIPPING` in `InteractorComponent.cpp` and `NpcChatSubsystem.cpp`), so a
v2.0 build must scope or flip that guard before this section is true.

Once it is, the path is: start a new game, and in the office house walk up to any of the
five agents and press **E**. The typed conversation opens on the same panel as the rest of
the dialogue.

---

## How the service is paid for — goes in the ticket

Valve's Content Survey documentation says that a game integrating an external AI service
must manage access to it on players' behalf and collect payment through a Steam-supported
method, and offers four ways. The first is:

> *"model that cost into the main price of the game and not charge anything additional"*

**That is the one this game uses, and it already does.** Wording for the ticket:

> The AI service is paid for entirely by the developer. Nothing is charged to players for
> it, and there is no microtransaction, subscription or DLC attached to it — this is the
> first of the four approaches in your documentation, at a game price of zero.
>
> Spending is capped on the developer's own proxy, daily and monthly, and the caps are
> enforced before any call is made to the provider rather than after. The conversation
> feature is optional and not required to finish the game; when the cap is reached, the
> characters report that conversations are unavailable and the rest of the game is
> unaffected. No player has paid for anything that can stop working.
>
> Could you confirm this is acceptable for a free title? Your documentation does not
> address free games, optional features, or developer-side spending caps specifically,
> and we would rather have it confirmed than assume it.

**Ask the question, do not assume the answer.** The documentation is silent on all three
of those cases — it treats every live-generated integration the same way. Silence is not
permission. The amendment is a support ticket either way, so one extra sentence buys a
written answer and costs nothing.

**If Valve says no, pull chat from the Steam build. Do not monetise it.** A
single-purchase DLC is the technically clean answer and Valve lists it, but a paid
AI-conversation add-on bolted onto a memoir about a 71-year-old is exactly the
*"AI girlfriend casino"* headline `docs/THE_PRESENCE.md` vetoed permanently. The moat is
the memoir's sincerity. The feature can live on itch and in development builds; the
framing cannot be got back.

**Scale, so the risk is legible.** The monthly cap is roughly 21,000 replies at the
measured cost per reply. A free release where a few hundred players each have one
conversation spends a couple of dollars. Running dry is not a realistic worry at this
size, and runaway cost is impossible because the cap is checked before the call.

---

## Still open before this can be filed

1. ~~The price question~~ — **resolved 2026-09-25**, see the section above. The position is
   approach one (developer absorbs the cost, nothing charged), stated in the ticket with a
   request that Valve confirm it for a free title.
2. ~~The sexual-content filter decision~~ — **done 2026-09-25.** Measured, enabled at
   `BLOCK_MEDIUM_AND_ABOVE`, and described above without overclaiming what it does.
3. **The ElevenLabs commercial licence**, which the Pre-Generated disclosure already
   asserts.
4. **Three passages to rewrite**: `docs/STEAM_PLAN.md:375`, `docs/STEAM_PLAN.md:386`,
   `docs/STEAM_STORE_COPY.md:101` — all three currently say no content is generated at
   runtime.
5. **A v2.0 build** with the Shipping guard scoped, so the tester instructions are true.
