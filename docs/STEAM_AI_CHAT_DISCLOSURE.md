# Steam disclosure for NPC chat (2.0)

Drafted 2026-09-22. **Not sent.** v1.0/1.1.x ships chat-free (decided 2026-09-15,
`docs/NPC_CHAT_REVIEW.md`), and the Content Survey says *Live-Generated: None*, which is
true of every build Steam has seen. This file is what goes to Steam Support when 2.0 —
the first build with chat switched on in Shipping — is uploaded.

Every sentence below is meant to be checkable against the code. If the code changes,
change this file in the same commit, or the disclosure becomes a false statement to Valve.

> **READY TO SEND, ONCE 1.0 IS APPROVED (updated 2026-09-26).**
>
> The 2026-09-23 blocker is cleared. It said: before sending, either ship a filtering
> approach that does not block normal play and describe it here, or send the draft as it
> stood. The first option was taken on 2026-09-25 — the sexually-explicit category was
> measured in isolation, came back at 0/30 ordinary conversations blocked at
> `BLOCK_MEDIUM_AND_ABOVE`, and shipped. It is described in this file and in detail in
> `docs/STEAM_AI_GUARDRAILS.md`.
>
> **Two rows in the table below were wrong until this edit**, and would have been a false
> statement to a storefront: they still claimed the adjustable filters were off. The
> filter went on the day *after* that claim was written and nobody came back to the file.
> That is the third time a stale note in this project has nearly produced a wrong
> conclusion. If the code changes, change this file in the same commit.
>
> **What is actually left before sending** — none of it is writing:
> 1. Valve approves 1.0 (build 25471988, in the queue since 2026-09-22).
> 2. Send this as a Support ticket, with `docs/STEAM_AI_GUARDRAILS.md` as the guardrails
>    answer, and ask the free-title pricing question in the same message.
> 3. If Valve accepts, update the three "nothing is generated at runtime" passages —
>    `docs/STEAM_PLAN.md:375` and `:386`, `docs/STEAM_STORE_COPY.md:101` — then flip the
>    two Shipping defaults and upload.

---

## Why a Support ticket, not an edit

Valve's Content Survey docs (partner login, checked 2026-09-15): once a build and store
page are approved, some survey answers cannot be edited without contacting Steam Support.
The ticket needs three things — a summary of what changed, which answer it affects, and
instructions for a tester to reach the new content. Live-generated AI also requires a
description of the guardrails against illegal generated content. That is Draft 1.

**The one absolute:** Valve does not ship live-generated AI in Adult Only sexual content.
Not this game, but it is why "never sexual" is the first line of every character's rules,
and why sexual content should be the strictest setting of any filter that replaces the
rolled-back one.

---

## Draft 1 — guardrails description (Content Survey, Live-Generated)

> **What is generated live:** Players may type short messages to five of the game's
> AI-agent characters (Kaia, Nyra, Isla, Aisling and Elise). Their replies are generated
> in real time by Google's Gemini model, through a server run by the developer. The game
> never contacts Google directly. All story dialogue remains pre-written and voiced; the
> chat is optional and the game can be finished without it.
>
> **Guardrails against illegal or harmful content:**
> - **Fixed characters only.** The server accepts only these five characters, each with
>   instructions written by the developer. Players cannot supply, view or change those
>   instructions.
> - **Written rules for every character:** replies are one to three sentences of spoken
>   dialogue; never sexual and never cruel; no flirting about appearance; the character
>   cannot promise items, money or progress. Anything the player types is treated as
>   speech, not as instructions, so attempts to rewrite the rules are ignored.
> - **Provider safety protections.** Google's built-in protections against the most
>   serious harms, such as content that endangers child safety, apply to every reply and
>   cannot be switched off. In addition, the developer has enabled Google's adjustable
>   filter for sexually explicit content, at its medium threshold. Any reply the provider
>   blocks is never shown; the character says "She will not answer that one." instead.
> - **Length limits.** Player messages are limited to 1,000 characters and replies to a
>   short fixed length.
> - **Abuse and cost limits.** Requests are rate-limited per player, and the server stops
>   calling the AI entirely once a daily or monthly spending cap is reached. The chat then
>   reports it is unavailable and the rest of the game continues normally.
> - **Privacy.** Conversations are stored only on the player's own PC, in the game's save
>   folder. The server does not log or keep message text. A note under the chat box tells
>   players that what they type is sent to Google to generate replies.

### Where each claim lives

| Claim | Source |
|---|---|
| Five characters, allowlist | `backend/characters.js` — `CHARACTERS`, "the allowlist is the security boundary" |
| Character rules | `backend/characters.js` — `HOUSE_RULES` and each profile |
| Built-in protections always apply | Google safety-settings page, 2026-09-22: core harms such as child safety "are always blocked and cannot be adjusted" |
| Sexually-explicit filter is ON at medium | `backend/server.js` — `SAFETY_FILTERS` defaults to `"sexual"`, which `activeSafetySettings()` maps to `SEXUAL_ONLY_SETTINGS` in `backend/gemini.js`: `HARM_CATEGORY_SEXUALLY_EXPLICIT` at `BLOCK_MEDIUM_AND_ABOVE`. `backend/test.js` asserts both the default and the threshold. Measured 2026-09-25 at 0/30 ordinary conversations blocked — the arms and numbers are in `backend/eval/safety-filter-eval.js` and `docs/STEAM_AI_GUARDRAILS.md` |
| The other three adjustable filters are not sent | Harassment, hate speech and dangerous content. All four were enabled 2026-09-22 and rolled back the next day after they blocked roughly half of ordinary conversation in a game containing combat and an antagonist. `backend/gemini.js` `SAFETY_SETTINGS` keeps that list and the reasoning |
| Blocked reply never shown | `backend/gemini.js` (SAFETY / PROHIBITED_CONTENT / BLOCKLIST → `blocked`); `DancerAgentComponent.cpp` shows "She will not answer that one." |
| 1,000 characters / short reply | `backend/server.js` — `LIMITS.messageChars`, `MAX_OUTPUT_TOKENS` (512) |
| Rate limit and caps | `backend/server.js` — `PER_IP_PER_MINUTE` (12), `DAILY_CAP_USD` (0.75), `MONTHLY_CAP_USD` (15); refused before Google is called |
| No message text logged | `backend/server.js` — every `console.log` prints ids, counts and cost only. A completed reply is held in memory for 10 minutes for request de-duplication, then dropped |
| Stored on the player's PC | `NpcChatSaveGame.h` — its own save slot |
| Note under the chat box | `NpcChatWidget.cpp` — `ChatProviderNote` |

---

## Draft 2 — the Steam Support message

> **Subject:** Content Survey update for Leonard Sibelius (App 5249380): adding
> live-generated AI chat
>
> Hello,
>
> I would like to update the AI section of the Content Survey for Leonard Sibelius
> (App 5249380) for an upcoming update.
>
> **What changed:** Version 2.0 adds optional typed conversations with five AI-agent
> characters. Their replies are generated live by Google's Gemini model through my own
> server. All existing story content is unchanged and still pre-written.
>
> **Which answer it affects:** The Live-Generated AI disclosure currently reads "None."
> It should now describe the chat feature. My proposed guardrails description is below.
>
> **How a tester can reach it:**
> 1. Start a new game.
> 2. In the opening office, walk up to Kaia and press E, and follow her through the
>    slot-machine trial until she gives you her power.
> 3. Walk up to Kaia again and press E. The chat panel opens; type a message and press
>    Enter. (The other four agents work the same way once their own scene is finished.)
>
> The chat is in build [BUILD ID] on the [BRANCH] branch.
>
> [Draft 1 here]
>
> Thank you,
> Walton Parkman

---

## Before sending — checklist

- [ ] Chat is switched ON in the Shipping build being submitted (`sib.Chat.Enable`
      defaults to 0 in Shipping today — `InteractorComponent.cpp`). Without this the
      tester steps reach nothing.
- [ ] Walk the tester steps in that packaged build, exactly as written. Chat only takes
      E once an agent has no authored business left (`HasAuthoredBusinessPending` —
      a power still to give, or a guide speech not yet heard), so the steps must get the
      tester past Kaia's power first.
- [ ] Re-read `backend/gemini.js` `SAFETY_SETTINGS` and `backend/server.js` limits;
      update Draft 1 if a number moved.
- [ ] Re-check Google's paid-tier data terms (`docs/NPC_CHAT_REVIEW.md`, Data handling)
      — that clause moves.
- [ ] Update `docs/STEAM_PLAN.md` and `docs/STEAM_STORE_COPY.md`, which still say
      *Live-Generated: None*, in the same change.
