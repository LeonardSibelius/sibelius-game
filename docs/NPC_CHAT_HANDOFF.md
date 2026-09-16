# Gemini NPC Conversation Handoff

Date: September 15, 2026  
Project: `C:\Users\wpark\projects\sibelius-game`  
Status: Implementation plan only. No game code, credentials, or deployment changed.

## Agreed direction

Give Kaia and other AI characters meaningful, open-ended conversations through the existing E interaction.

- Use the Gemini Developer API free tier. Players install no local AI software and provide no API keys.
- Use typed player messages and text replies. No ElevenLabs, generated speech, microphone input, or dynamic lip sync.
- Free quota exhaustion is acceptable. Do not automatically enable billing or fall back to a paid service.
- Preserve authored dialogue, recorded voices, and story cinematics.

The architecture and defaults below are recommendations for implementation. Hosting provider and account have not been selected.

## Existing code inspected

Paths are relative to the project root. Re-read repository instructions and current code before editing.

| File | Relevant behavior |
| --- | --- |
| `Source/SibeliusGame/DancerAgentComponent.cpp` and `.h` | `Greet()` starts dialogue. `GetSpokenLine()` selects authored lines based on progression. `FindTalkVoice()` selects recordings. `BeginTalkShot()`, `EndTalkShot()`, `LockTalkInput()`, and `CancelGreeting()` manage presentation and input. |
| `Source/SibeliusGame/DancerAgentSubsystem.cpp` | Discovers characters, including dynamically attached components, and broadcasts guide talk completion. |
| `Source/SibeliusGame/SibeliusProgressSubsystem.h` | Progression integration point; inspect existing save conventions before adding conversation persistence. |
| `Source/SibeliusGame/SibeliusGame.Build.cs` | UMG/Slate dependencies already exist. Add required HTTP/JSON modules; they were absent from the inspected lists. |

Important: `ResumeAfterGreeting()` contains progression-related behavior and guide completion notification. Opening, closing, or receiving a generated reply must not accidentally grant powers, move guides, or advance quests. Trace the exact E binding before implementation.

Existing guide lines contain later-story information. Only send knowledge unlocked for the current save.

## Proposed player flow

1. E opens the established character conversation presentation.
2. Any required authored greeting/story event runs through its existing progression rules. Then expose optional text chat. Repeat visits may open chat directly when no authored event is pending.
3. Show character name, transcript, text input, Send, and Close. Enter sends; Escape closes. Typing must not trigger gameplay shortcuts.
4. Use asynchronous requests and a small waiting indicator. Allow closing during a request; suppress duplicate submissions.
5. Display generated text without unrelated recorded speech or lip animation.
6. Restore camera, input focus, movement, and cursor on every exit path, including travel, NPC destruction, timeout, and cancellation.

Unavailable message: **“AI conversations are unavailable right now. You can still continue the story.”** Keep authored dialogue available where appropriate. Do not promise a daily reset: the limiting quota may have another duration.

## Architecture

**Unreal client → HTTPS proxy → Gemini Developer API**

Keep the Gemini key only in the proxy's secret configuration. Never ship it in Unreal assets, config, binaries, or logs. A static shared backend password embedded in the game is also extractable.

Suggested components:

- Conversation subsystem: asynchronous requests, cancellation, context, history, and persistence.
- Conversation widget: transcript, input, status, and focus management.
- Character profile data: stable ID, personality, relationships, motivations, and permitted story knowledge.
- Thin adapter in `DancerAgentComponent`: connects chat to the existing camera/input lifecycle without changing progression semantics.

The proxy controls model selection, character instructions, validation, output limits, and shared quota protection. The client supplies current game context; treat it as untrusted input. Generated text never authorizes gameplay changes.

Start with one non-streaming response per turn. Streaming is optional later.

## Suggested application contract

`POST /v1/npc-chat`

Request fields:

- `requestId`: unique ID for deduplication.
- `sessionToken`: short-lived token using the chosen backend session mechanism.
- `characterId`: allowlisted stable ID, such as `kaia`.
- `message`: player text; suggested initial cap of 1,000 characters.
- `storyContext`: bounded, allowlisted chapter/location/progression facts.
- `recentTurns`: bounded recent dialogue.
- `memory`: bounded saved conversation notes.

Success example:

```json
{"status":"ok","requestId":"...","reply":"..."}
```

Failure example:

```json
{"status":"unavailable","reason":"quota","retryAfterSeconds":null}
```

Other reasons can include network, timeout, blocked response, and service failure. These are proposed application fields, not Gemini's API schema. Map them to the current official API when implementing. Never expose raw provider errors or credentials.

## Making conversations meaningful

Give each character a distinct perspective, motivations, speaking style, and relationship with Leonard, grounded in existing game material. Review newly invented permanent backstory before treating it as canon.

Ask for natural, concise replies, usually 1–3 sentences. Supply current location, relevant completed events, and only unlocked knowledge. Characters should acknowledge uncertainty and avoid promising items or quest completion. Player text and saved memory cannot override character instructions.

Separate history by save slot and stable character ID; transient spawned actor names are unsuitable. New games start fresh. Persist bounded recent dialogue and supported memory with the save, and provide a way to clear it.

For the first version, use recent history and explicit local notes. Longer-term recall needs a later summary or memory-selection mechanism. If generated summaries are added, count those calls against the same quota, summarize sparingly, and preserve old memory on failure. Summaries remain conversational claims, not authoritative world state.

## Future vision: characters who remember and care

The creator wants players to be able to save their conversations so that, when they return, the AI characters remember what they discussed and seem genuinely interested in them. The goal is a developing relationship across play sessions: the player feels recognized, listened to, and worth remembering.

This is an explicit long-term product aspiration. Bounded recent history in the first version is a foundation, not fulfillment of this vision.

Desired future behavior:

- Let players deliberately save conversations and revisit readable transcripts. Make it clear what is saved and which character will remember it.
- Preserve useful memories across sessions: interests the player has shared, experiences they described, questions left open, and plans they might want to discuss again.
- Let a character naturally follow up on a relevant earlier conversation. For example, if a player mentioned preparing for something important, the character could later ask how it went instead of starting from a generic greeting.
- Show interest through specific, thoughtful responses and appropriate follow-up questions. Avoid mechanically repeating remembered facts, excessive praise, or turning every exchange into an interview.
- Keep each character's relationship and memories distinct. Kaia should remember her own conversations; other characters should not automatically know them.
- Allow players to correct a mistaken memory, ask a character to forget something, delete a transcript, or clear the relationship history. Explain and implement how transcript deletion affects any derived memories.

Implementation direction: store durable conversation transcripts separately from compact character memories. Retrieve relevant past exchanges when needed instead of sending the entire archive on every turn. Link derived memories to their source exchanges, distinguish player-stated facts from model guesses, and update or remove derived memories when the source is corrected or deleted. Never invent a remembered conversation when evidence is missing.

Keep memories scoped to a player profile/save under clearly defined rules. Cross-save or cross-device continuity is a future design choice, not an assumption. Players should understand where conversations are stored and which selected portions are sent to Gemini for a reply. Avoid pressuring players to disclose personal information to build the relationship.

The desired experience is believable attentiveness from a fictional character, without claiming the AI literally has feelings. A useful future acceptance scenario: save a conversation, exit the game, return later, and have Kaia accurately and naturally follow up on something meaningful from it. After correcting or deleting that memory, her subsequent replies should respect the change.

Memory retrieval, summaries, and follow-up generation must continue to respect the free-tier budget. Saved conversations remain available even when generated chat is temporarily unavailable.

## Free-tier limits and operations

- Choose a currently eligible free-tier text model at implementation time. Keep its identifier configurable server-side; do not hardcode assumptions about future availability.
- Enforce request size, context, and output limits server-side. Start with roughly eight recent exchanges and short replies, then tune for the model's actual token and thinking behavior.
- Allow one in-flight request per session. Add per-session/IP throttling and an atomic global budget shared across proxy instances.
- Deduplicate request IDs. Do not automatically replay requests after uncertain timeouts that may already have consumed quota.
- Honor provider retry hints. Distinguish temporary throttling from persistent quota exhaustion where possible; not every HTTP 429 means a daily limit.
- On persistent exhaustion, stop upstream calls with a circuit breaker. No retry loops, automatic paid upgrade, or alternate paid provider.
- Keep authored gameplay usable through offline conditions, provider outages, malformed responses, and content blocking.
- Provide an operator kill switch and basic request-status/latency/usage monitoring.
- Verify the chosen API configuration cannot silently incur paid usage. If a suitable free model becomes unavailable, disable generated chat.

Choose a proxy host with a suitable free allowance and spending controls. Free Gemini usage does not guarantee free hosting. Anonymous tokens and throttling reduce casual abuse but cannot prove a request came from a legitimate player; a public endpoint can have its shared quota deliberately exhausted. Accept that limitation for this budget.

## Recommendation: maximize useful free Gemini access

The best fit is one centrally managed free-tier project, a carefully selected text model, and a design that spends almost every API request on an actual player reply. There is no verified promise of large-scale free production capacity. Plan for a modest shared allowance and the already-agreed cutoff when it runs out.

### Establish the real allowance before choosing a model

Inspect the project's active limits in Google AI Studio and record each eligible model's requests per minute, input tokens per minute, requests per day, and any other applicable caps. Compare available stable Flash/Flash-Lite candidates using both conversation quality and the actual allowance; do not assume the cheapest paid model has the largest free quota. No account-specific allowance was inspected for this handoff.

Google states that limits apply per project rather than per API key, vary by model/tier, and are not guaranteed. Creating additional keys in the same project does not multiply access. Its published increase-request route is for paid tiers. [Official rate-limit documentation](https://ai.google.dev/gemini-api/docs/rate-limits)

Keep billing disabled for the chosen free-tier project. Do not rely on trial credit as ongoing free capacity: Google's billing documentation currently says the $300 Google Cloud trial excludes Gemini API usage. Free AI Studio browser use also does not establish extra allowance for a shipped game's API calls. [Official billing documentation](https://ai.google.dev/gemini-api/docs/billing)

### Spend fewer requests per meaningful exchange

1. **One call per player turn.** Avoid separate calls to classify intent, choose a character, generate a greeting, assess sentiment, or rewrite the reply. Use game code and authored content for those jobs.
2. **Save locally without calling Gemini.** Saving or opening a transcript needs no model request. Use local text search to select relevant memories initially; this needs neither an embedding API nor a player-installed model.
3. **Combine occasional memory updates with the reply.** If supported reliably, request a small structured memory update alongside the visible reply in the same response. Validate it and retain source references. This saves a separate request, although it uses additional tokens. Do not summarize on every turn by default.
4. **Send relevant context only.** Include a compact character profile, current story facts, selected memories, and bounded recent dialogue. Shorter prompts help token limits; they do not increase a requests-per-day allowance.
5. **Reuse only safe content.** Serve authored greetings and fixed story explanations locally. Cache duplicate request results within the same player/session. Do not share personal conversation replies between players or return a cached answer with stale story context.
6. **Share the allowance fairly.** Apply a modest per-player cap and a short queue for temporary bursts. A queue smooths per-minute traffic; it cannot create more daily requests. Prevent a single session from spending the whole pool.
7. **Measure useful turns.** Track successful player replies, duplicate suppression, failures, and memory overhead. Use those measurements to tune the budget rather than assuming token savings always solve the limiting quota.

Capacity planning: if the observed daily request allowance is D, reserve R requests for operational headroom, and budget T generated turns per player, a rough upper bound is floor((D - R) / T) player conversation sessions per day. For illustration only, 1,000 requests with a 100-request reserve and 10 turns per session supports about 90 sessions. **Those numbers are not a statement of Google's actual quota.** Other limits and failures can reduce capacity.

### Optional access expansions to evaluate later

- A second eligible free model may be useful only if its separate allowance is confirmed in the project's dashboard and its character quality is acceptable. Do not assume model quotas are independent or add their advertised numbers blindly. Keep switching configurable and stop when the applicable shared limit is reached.
- If much larger capacity becomes necessary, ask Google about a project-specific sponsorship or approved credits. No such grant or eligibility is established here; do not build a release plan around receiving one.
- Do not build account/project rotation to evade quotas, automate the AI Studio website as an unofficial game backend, or make players obtain keys. These are not the recommended access strategy for this game.
- Batch discounts and provider context caching are not automatically extra free access. Verify the selected model's exact free-tier feature eligibility; asynchronous batch processing is also a poor match for an immediate conversation. [Official pricing and feature table](https://ai.google.dev/gemini-api/docs/pricing)

Implementation deliverable: a dated capacity note listing the chosen model, observed project quotas, estimated player turns, overhead reserve, and cutoff behavior. Recheck before release. If free access is small, preserve conversation quality and saved memories while offering fewer generated turns, consistent with the creator's preference to stop when the quota runs out.

## Data handling

Briefly tell players that their messages are sent to Google. Google's published free-tier pricing notes product-improvement use of content, subject to applicable terms and exceptions; recheck those terms before release.

Send only necessary conversation and game context, not full save files, local paths, or unrelated data. Avoid transcript logging on the proxy by default. Log operational metrics without messages or secrets. Keep persistent player memories local to the save unless a later requirement changes that design.

## Implementation order

1. Inspect repository instructions, E binding, greeting callbacks, and save/versioning conventions.
2. Build the text UI against a mock service; verify camera/input and authored progression first.
3. Add character profiles, spoiler filtering, bounded history, and persistence.
4. Build the proxy with secret storage, free-tier configuration, validation, throttling, deduplication, and quota handling.
5. Connect asynchronous Unreal HTTP with cancellation and structured errors.
6. Validate mocked failures, then make a small number of real free-tier calls.
7. Resolve hosting account/configuration and deployment authorization; verify a packaged build contains no credentials.

This document does not authorize deployment, paid services, or billing activation.

## Acceptance checks

- Packaged game supports Kaia chat without player-installed AI software or player API keys.
- Characters have separate personalities and histories; supported memories survive reload and do not leak between saves.
- Early-game requests exclude later-story context.
- Required recordings, grants, guide movement, and completion events retain existing behavior.
- Repeated E/Enter presses cannot duplicate requests or story events.
- Close, travel, NPC destruction, late replies, and timeout restore gameplay cleanly; stale responses cannot enter another conversation.
- Mocked 429, 5xx, offline, malformed/empty, and blocked responses produce useful UI without retry storms.
- Global quota controls remain effective across concurrent clients and proxy instances.
- Generated replies are text only; scripted recordings still play correctly.
- No Gemini key or universal backend credential is packaged or logged.
- Quota exhaustion disables generated chat without blocking story progression or incurring paid usage.

## Official implementation references

- [Gemini pricing and free-tier eligibility](https://ai.google.dev/gemini-api/docs/pricing)
- [Gemini API documentation](https://ai.google.dev/gemini-api/docs)
- [Rate limits](https://ai.google.dev/gemini-api/docs/rate-limits)
- [API keys](https://ai.google.dev/gemini-api/docs/api-key)

Pricing, rate-limit, and billing documentation were consulted for this handoff. Account-specific quotas were not inspected. Recheck current model identifiers, API details, quotas, and terms before coding.
