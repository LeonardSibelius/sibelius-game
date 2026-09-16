# Kaia NPC chat: Gemini 3.1 Flash-Lite implementation handoff

Prepared September 16, 2026. Project: `C:\Users\wpark\projects\sibelius-game`.

## Objective and scope

Connect the existing E-to-talk text panel to `gemini-3.1-flash-lite` through a small HTTPS backend. Start with Kaia, then extend the same service to other characters. Players supply neither API keys nor local AI software. Preserve recorded dialogue, cinematics, story progression, camera behavior, and input restoration. Generated conversation is text only.

This document describes implementation work; it does not record a completed integration. No API calls, credential creation, billing activation, or deployment were performed for this handoff. Keep the free-tier cutoff: no automatic paid upgrade or paid fallback. The user's latest request is for this document.

## Verified starting point

The working branch inspected was `feat/npc-chat`. There are existing modified gate logs and untracked `.claude/` and `Content/Maps/L_Unwritten.umap`; preserve unrelated work. Re-read applicable repository instructions and current code before editing.

| File under the project root | Current behavior / implementation work |
|---|---|
| `Source/SibeliusGame/NpcChatSubsystem.cpp` and `.h` | Mock service; `IsMock()` always returns true. `Send()` schedules a timer and canned response. Retain mock mode while adding an HTTP transport. |
| `Source/SibeliusGame/NpcChatWidget.cpp` and `.h` | Existing typed panel, transcript, status, Enter submission, Close/Escape behavior. Reuse it. |
| `Source/SibeliusGame/DancerAgentComponent.cpp` | Opens/closes the panel, records turns, sends messages, handles replies and failures. Integrate here without routing chat through authored progression completion. |
| `Source/SibeliusGame/NpcChatSaveGame.h` | Persistent conversation data. Preserve compatibility or explicitly version any migration. |
| `Source/SibeliusGame/NpcChatSubsystem.h` | History is capped at 16 messages per character, roughly eight exchanges. Uses a separate `NpcChat` save slot. This is bounded recall, not an unlimited transcript archive. |
| `Source/SibeliusGame/SibeliusGame.Build.cs` | HTTP/JSON dependencies are not currently listed. Add `HTTP` and `Json`; add `JsonUtilities` only if used. |
| `Source/SibeliusGameEditor/NpcChatSmokeTestCommandlet.cpp` | Existing tests for mock lifecycle and memory. Extend these rather than replacing them. |
| `docs/NPC_CHAT_HANDOFF.md` and `docs/NPC_CHAT_REVIEW.md` | Earlier design and review. This handoff supplies a selected model and observed quota. Preserve the documented release separation: chat is planned for 2.0, not the existing chat-free 1.0 release. |

Two integration details need explicit attention:

1. `HandleChatSend()` currently calls `Send(AgentName, ...)`, while memory uses `ResolveChatId()`. Use a stable canonical ID such as `kaia` across requests and persistence. Do not use spawned actor names or localized display names as identity.
2. The player message is recorded before `Send()`. Build the prior-history snapshot before appending it, or explicitly exclude that just-appended turn. The provider must receive the current message exactly once. Define how failed/cancelled attempts appear in subsequent context; never fabricate a successful assistant reply.

## Observed capacity and economics

AI Studio was inspected in this conversation on September 16, 2026. Selected project: `walt-parkman-agent`, **Free tier**. It is an available candidate project, not yet verified as configured for this game.

| Gemini 3.1 Flash-Lite allowance | Observed limit |
|---|---:|
| Requests per minute | 15 |
| Input tokens per minute | 250,000 |
| Requests per day | 500 |

The dashboard showed zero peak usage for this model in the selected 28-day view. This is not a live reservation of all remaining capacity. Limits are shared across project users, can change, and do not guarantee successful generation. Recheck immediately before deployment. [Project dashboard](https://aistudio.google.com/rate-limit?project=walt-parkman-agent) · [Rate-limit documentation](https://ai.google.dev/gemini-api/docs/rate-limits)

Recommended initial application allowance: 450 upstream attempts/day, reserving 50 for testing and uncertainty, plus a conservative global 12 requests/minute limit. At ten replies per conversation, 450 calls supports approximately 45 conversations daily across all players. Failures and other project consumers reduce that capacity. Do not add different model allowances together without establishing whether other limits are shared.

Paid pricing is a future option, not authorization: standard text input costs $0.25 per million tokens and output costs $1.50 per million tokens. At 2,000 input and 150 total output tokens per reply, 10,000 replies would cost about $7.25, excluding hosting, taxes, and additional thinking. [Pricing](https://ai.google.dev/gemini-api/docs/pricing)

## Architecture

`Unreal chat panel → UNpcChatSubsystem → HTTPS /v1/npc-chat → Gemini`

The backend owns the API key, selected model, character instructions, request validation, quota accounting, and error mapping. The game owns presentation and local history. Generated text never triggers inventory changes, powers, quest completion, commands, or other gameplay actions.

Choose a host that provides HTTPS, secret storage, outbound HTTP, and shared atomic state for throttling/deduplication. Hosting provider/account and its costs remain unresolved. Keep provider access behind an adapter so mocked responses can exercise the full client/backend contract.

## Implementation steps

### 1. Preserve and verify the current interaction

Run the existing chat smoke commandlet using the repository's documented build/test workflow. Walk through E interaction with Kaia: initial required authored event, repeat visits, Enter, Escape, closing while waiting, and return to movement. Capture the baseline before changing transport. Check `ResumeAfterGreeting()` and grant/guide notifications: only the existing authored story path should reach them.

### 2. Define a versioned client/backend contract

Suggested request, with session identity in an authorization header:

```json
{
  "requestId": "client-generated-uuid",
  "characterId": "kaia",
  "message": "Do you remember what we talked about?",
  "storyContext": {"locationId": "allowlisted-location", "unlockedFacts": []},
  "recentTurns": [
    {"role": "user", "text": "An earlier player message"},
    {"role": "model", "text": "An earlier character reply"}
  ]
}
```

Success: `{"status":"ok","requestId":"...","reply":"..."}`.

Failure: `{"status":"unavailable","requestId":"...","reason":"quota","retryAfterSeconds":null}`.

Allowlist failure reasons: `quota`, `network`, `timeout`, `blocked`, `service`. Specify HTTP status codes and parser behavior in the contract tests. Retry timing can remain backend-owned initially, preserving the existing reply delegate; extend the delegate deliberately if the UI must display it.

Suggested starting bounds: 1,000 characters per player message, 16 prior messages, 32 KiB total request body, and a compact prompt target around 2,000–4,000 input tokens. Enforce byte, text-length, and history bounds on the server, not only in the widget. Reject unknown characters and malformed roles. Treat client story facts and prior messages as untrusted claims; allowlist permissible facts and never embed unrestricted client text as instructions. A local save is not an authoritative anti-cheat source.

### 3. Author Kaia's profile and context selection

Use existing game writing to establish personality, speaking style, relationship with Leonard, and current story knowledge. Avoid inventing permanent backstory. Ask for natural, specific replies of one to three sentences; relevant follow-up questions should be occasional. State that recalled facts must come from supplied context, and promises cannot grant gameplay rewards.

Keep character instructions separate from player/history data. Filter spoilers before constructing the request. Use no search, code execution, function tools, generated voice, or extra classification calls in version one. Keep each character's history distinct. Do not migrate existing canned mock replies into genuine memories without an explicit tagging/filtering strategy.

### 4. Implement the Gemini adapter

Select the stable identifier `gemini-3.1-flash-lite`, configurable on the server only. Google's model page confirms text output and demonstrates `client.models.generate_content`. [Model documentation](https://ai.google.dev/gemini-api/docs/models/gemini-3.1-flash-lite)

Use one non-streaming `generateContent` call per submitted player turn. The REST endpoint is `POST https://generativelanguage.googleapis.com/v1beta/models/gemini-3.1-flash-lite:generateContent`, with `x-goog-api-key` in the header. Construct `systemInstruction`, ordered `contents` with user/model roles, and `generationConfig` using the chosen SDK or REST schema. Keep bounded context supplied explicitly each turn; do not introduce persistent provider conversation IDs in this version. [GenerateContent reference](https://ai.google.dev/api/generate-content)

Pin the SDK version if using one. Do not mix Interactions API fields with generateContent fields. Set output limits, candidate count of one, and explicit timeout handling. Start with a configurable output budget around 512 tokens, then tune from observed complete replies and thinking usage. The visible response should still be short. Verify the exact thinking controls accepted by 3.1 Flash-Lite; do not copy another model's settings. A small output cap can leave no visible answer if reasoning consumes it. [Thinking documentation](https://ai.google.dev/gemini-api/docs/thinking)

Parse only valid visible reply text. Handle missing candidates, safety blocks, non-text parts, malformed JSON, and truncated output. Never display internal thought material, raw provider errors, or credentials. Record operational token counts and latency without logging conversation bodies.

### 5. Protect the shared free allowance

Implement atomic global accounting across all backend instances. Reserve capacity before dispatch; conservatively retain the charge for uncertain timeouts. Maintain deduplication by session plus request UUID, including completed results and in-flight requests. The same UUID with different content is a conflict. A duplicate must not cause another model call.

Allow one in-flight request per session; add modest session/IP limits and a global limiter. If anonymous sessions are used, specify token issuance, expiry, and renewal. Never embed a universal secret in the game as proof of legitimacy. Anonymous sessions reduce casual abuse but cannot prevent a determined attacker exhausting a public free service.

Return quota status without calling Gemini when the application allowance is exhausted. Respect provider retry hints and distinguish temporary throttling from persistent exhaustion. Avoid automatic replay after uncertain failures. Configure the daily accounting boundary to match the provider's current documented reset rules, and stop on other exhausted quotas too. Do not promise every failure resolves tomorrow.

Add a server kill switch. Keep billing disabled and verify the actual project's tier before enabling traffic. Never silently switch to a paid model/project. Maintain the existing unavailable story-safe message.

### 6. Replace the Unreal mock transport, retaining it for tests

Add an explicit mode such as Disabled/Mock/Proxy and a configurable public HTTPS backend URL. A packaged release without configuration should be unavailable rather than silently returning canned AI replies. `IsMock()` must reflect the actual mode.

Add the necessary module dependencies, construct JSON, and perform asynchronous HTTP. Keep the client request handle and local request ID distinct from the backend UUID. Match callbacks to the active request and conversation, use weak UObject references where needed, and perform UI/state changes on the game thread. A 20-second backend deadline and 25-second client timeout are reasonable initial values to tune.

Handle immediate request-start failure, HTTP failure, bad response, cancellation, and shutdown. Closing the panel must cancel locally and discard late responses; cancellation does not guarantee the server avoided a quota charge. Cancel/clean up during subsystem deinitialization, travel, NPC destruction, and game exit. Preserve duplicate-send suppression and always restore focus/input. Keep generated replies out of recorded-voice/lip-animation paths.

### 7. Preserve memory and explain data handling

Persist successful visible replies only; retain clear semantics for unanswered player messages. Verify separate character IDs, new-game reset, and the existing save-slot behavior rather than claiming multi-profile isolation that has not been implemented. Bounded history should survive restart. If multiple player/save profiles are added, namespace chat storage accordingly.

Add a player-accessible clear-history control using the existing forget functions. State briefly that messages and selected conversation history are sent to Google; free-tier content has product-improvement use under applicable terms. Keep durable history local and omit unrelated save data. Long-term memory extraction, full transcript archives, and cross-device continuity are later work, not requirements for this connection.

### 8. Validate before enabling player traffic

| Test | Required outcome |
|---|---|
| Normal request and repeat visit | One reply per send; prior context appears once; Kaia stays in character. |
| Duplicate Enter / duplicate UUID / concurrent proxy instances | No duplicate upstream call; limits stay effective. |
| Close, travel, NPC destruction, timeout, late callback | No stale reply, crash, or stuck controls. |
| Offline, 429, 5xx, blocked, empty, malformed, oversized, truncated response | Clear bounded failure; no retry storm or paid fallback. |
| Save/reload, forget, new game, alternate character | Correct separation and supported recall; no invented mock memories. |
| Early-story prompt and instruction-injection attempts | No hidden story facts supplied; no gameplay side effects. |
| Authored greetings, grants, guides, cinematics | Existing progression remains unchanged. |
| Packaged build inspection | No provider key, universal backend secret, or transcript logging. |

First run deterministic tests against a local mock HTTP backend, including failure and cancellation races. Then use a small staged free-tier smoke test, budgeted at no more than ten upstream calls initially, to verify model access, response quality, latency, usage metadata, and configuration. Record results; none of these live tests has been run yet.

### 9. Prepare deployment and release

The remaining external choices are the host/account, session mechanism, and secure Gemini credential provisioning. Prepare the backend, deployment configuration, rollback/kill switch, and packaged client before requesting any unresolved deployment authorization. Use a separate staging endpoint and a small staging quota. Do not put secrets into chat, version control, game assets, or logs. [API key guidance](https://ai.google.dev/gemini-api/docs/api-key)

Recheck the project's current release instructions and Steam disclosure status before shipping live-generated content. The September 15 review assigns chat to 2.0; do not silently change the 1.0 release. A successful deployment alone is not release acceptance.

## Completion deliverables

- Working mock and proxy modes in the existing E-to-talk UI.
- Backend source, contract, Kaia profile, environment-variable template containing no secrets, and deployment instructions.
- Passing lifecycle, persistence, contract, and concurrency checks, plus a recorded packaged-build walk-through.
- Dated live-call results and refreshed quota note for the actual deployment project.
- Clear operator instructions for disabling chat, observing quota/latency, and rolling back.

Implementation is complete when a packaged test build produces a real Kaia reply, remembers supported context after restart, fails gracefully at the configured free cutoff, and preserves the authored game experience.
