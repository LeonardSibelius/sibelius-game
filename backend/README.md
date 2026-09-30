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
sib.Chat.ProxyUrl 127.0.0.1:8787/v1/npc-chat
```

**Leave the scheme off.** `//` opens a comment in UE's console parser *and* in
`ConsoleVariables.ini`, so a full URL is truncated to `http:` in both. The client
supplies `http://` for loopback and `https://` for anything else, so a typo cannot
downgrade a remote backend to clear text.

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
| daily cap | $1.50 | this server |
| monthly cap | $45 | this server |
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

## Deployed

**`https://sibelius-npc-chat.fly.dev`** — Fly.io, region `sjc`, one `shared-cpu-1x`
256 MB machine, 1 GB encrypted volume at `/data` with scheduled snapshots. Live since
2026-09-16.

First remote call measured **1,288 ms** end to end against **957 ms** on localhost, so
the extra hop costs roughly 330 ms — and 284 ms of that was the one-time TLS handshake,
so a warm connection lands near 1,050 ms. Inside the 1.5 s p50 target.

```bash
curl https://sibelius-npc-chat.fly.dev/health   # mode, model, budget
flyctl logs                                      # the [chat] ... usd=... line per request
flyctl secrets set CHAT_DISABLED=1               # kill switch
```

## Deploying (Fly.io)

Chosen because this backend wants **one always-on process with a disk**, which is the
cheapest thing to buy and the hardest thing to get from a serverless host. About $3 a
month: ~$2 for a `shared-cpu-1x` 256 MB machine and $0.15/GB for the volume.

`fly.toml` and the `Dockerfile` are in this folder. From here:

```bash
fly launch --no-deploy --copy-config          # reuses fly.toml, does not overwrite it
fly volumes create npcchat_data --size 1 --region sjc
fly secrets set GEMINI_API_KEY=...            # from your shell, never from the repo
fly deploy
```

`fly secrets set` puts the key in the runtime environment and never in an image layer;
`.dockerignore` keeps `.env` out of the build for the same reason. Nothing else needs to
be secret — the model, the caps and the endpoint are all public config in `fly.toml`.

Then point the game at it:

```
sib.Chat.ProxyUrl=sibelius-npc-chat.fly.dev/v1/npc-chat
```

No scheme, as always — and because the host is not loopback, the client dials it over
**https** on its own.

Check it with `fly logs` (the `[chat] … usd=…` line per request) and
`curl https://sibelius-npc-chat.fly.dev/health`.

### Three things about this config that are load-bearing

- **`min_machines_running = 1` and `auto_stop_machines = false`.** A cold start costs
  seconds against a measured p50 of 957 ms. A player watching Kaia's face wait for a
  container is worse than the couple of dollars auto-stop saves.
- **One machine.** The ledger is a file and the dedup table is in memory, so a second
  machine makes the $45 cap advisory. If this ever needs to scale, both move to shared
  state first.
- **The volume.** A restart that reset the month's spend to zero would remove the only
  control that stops at *our* number instead of Google's.

### Staging

The review asks for a separate staging endpoint with a small cap. That is a second app
— `fly launch --name sibelius-npc-chat-staging` with `DAILY_CAP_USD=0.10` — at the same
couple of dollars. Point a test build at it and leave production alone.

The kill switch is `fly secrets set CHAT_DISABLED=1` — it answers unavailable, calls
nobody, and takes effect on the restart that setting a secret triggers.
