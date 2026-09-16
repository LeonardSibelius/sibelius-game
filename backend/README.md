# NPC chat backend

The proxy between the game and Gemini. It owns the API key, the model, the character
instructions and the spend cap; the game owns presentation and local memory. Nothing the
model says is allowed to change gameplay.

Design and decisions live in `docs/NPC_CHAT_REVIEW.md`. This file is how to run it.

**Zero dependencies.** Node's own `http` and `fetch` are enough for one endpoint, and a
process holding an API key is the last place to want a supply chain.

---

## Run it

```bash
cd backend
cp .env.example .env        # then put the key in .env, which is gitignored
node --env-file=.env server.js
```

Mock mode calls nobody and answers with canned lines — that is how the Unreal client's
failure, cancellation and race behaviour gets exercised without spending a request:

```bash
CHAT_MOCK=1 node server.js
```

Then point the game at it:

```
sib.Chat.Mode 2
sib.Chat.ProxyUrl http://127.0.0.1:8787/v1/npc-chat
```

## Test it

```bash
node test.js
```

Twenty-two assertions against a real HTTP server in mock mode: validation, dedup,
conflict, every failure reason, and the budget arithmetic. Exit 0 or 1.

---

## The contract

`POST /v1/npc-chat`

```json
{
  "requestId": "uuid",
  "characterId": "kaia",
  "message": "do you remember me?",
  "storyContext": { "locationId": "/Game/L_Office_v02", "unlockedFacts": [] },
  "recentTurns": [{ "role": "user", "text": "..." }, { "role": "model", "text": "..." }]
}
```

Success is `{"status":"ok","requestId":"...","reply":"..."}`. Everything else is
`{"status":"unavailable","reason":"quota|network|timeout|blocked|service"}`. Those five
reasons are the entire vocabulary — the game prints one sentence per reason, in Kaia's
voice, and provider error text never reaches a player.

`GET /health` reports mode, model and budget, and deliberately carries no credential.

Limits enforced server-side, not just in the widget: 32 KiB body, 1,000 characters per
message, 16 prior turns, 2,000 characters per turn.

## What stops the spending

Three ceilings, smallest first:

| stop | default | enforced by |
|---|---|---|
| daily cap | $0.75 | this server |
| monthly cap | $15 | this server |
| prepay balance | $25 | Google, hard |

The daily cap exists because a monthly one cannot tell a launch day from an attack: one
bad afternoon would empty the month. Cost is **reserved before dispatch** at the worst
case and **reconciled** against the real token counts when the reply lands, so a burst
cannot spend past the cap and discover it afterwards.

**The AI Studio cost graph is not a spend meter** — it lags by up to a day. The
`[chat] … usd=…` line this server logs per request is the only same-day view.

## Known limits, stated rather than discovered

- **Single instance.** The ledger and the dedup table are in one process, with the ledger
  on disk. Run two copies and the accounting becomes advisory. At ~1,000 requests a day
  one small instance is the right size; if that ever changes, both need shared state.
- **No session identity yet.** Throttling is per-IP with one request in flight per
  caller. Anonymous session tokens are future work, and they would reduce casual abuse
  rather than prevent a determined attacker — a public endpoint's allowance can always
  be drained by someone willing to.
- **Thinking is unset on purpose.** `THINKING_BUDGET` stays empty until the controls for
  this model are verified in its own documentation; an unrecognised `generationConfig`
  field is usually rejected outright. It is also the largest single lever on cost.
- **Kaia's profile is a first draft.** `characters.js` is assembled from the game's own
  writing but has not been through Walt's ear, and invented backstory is not canon until
  it has.

## Deploying

Unresolved, deliberately. It needs HTTPS, secret storage, outbound HTTP and a writable
path for the ledger. Whatever host is chosen: keep the key in that host's secret store,
never in the repo or the game; use a separate staging endpoint with a small cap; and
verify a packaged client carries no key and no universal backend secret before release.

The kill switch is `CHAT_DISABLED=1` — it answers unavailable and calls nobody.
