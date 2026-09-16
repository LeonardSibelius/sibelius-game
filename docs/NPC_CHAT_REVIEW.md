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
| **Deferred** | Steam. Chat is a 2.0 feature. v1.0 ships chat-free — locked by what is in review as of 2026-09-15 — and the disclosure change goes through Steam Support afterwards. The handoff never mentions Steam. |

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

### 1. Chat is a 2.0 feature, and Valve has a defined path for it

**State of play, checked in Steamworks on 2026-09-15.** Leonard Sibelius (app 5249380)
is unreleased with two requests awaiting support: the store page review filed Sep 9 and
the product build review filed Sep 15, whose opening line describes the game as
*single-player, offline, no servers, nothing to provision*. Three documents therefore
now promise Valve a game that generates nothing at runtime:

- `docs/STEAM_PLAN.md:375` — *"Live-generated — none. Nothing in this game creates
  content at runtime, which spares the extra questionnaire about guardrails against
  illegal generated content."*
- `docs/STEAM_PLAN.md:386` — the draft disclosure wording
- `docs/STEAM_STORE_COPY.md:101` — the Live-Generated block

**Decided 2026-09-15: v1.0 ships chat-free.** Leave all three alone while the reviews
are in flight. Chat is 2.0.

**What the amendment actually costs, from Valve's own Content Survey documentation
(checked 2026-09-15).** It is a support ticket, not a negotiation:

- Live-generated content requires describing, in the survey, the guardrails that keep
  the AI from generating illegal content. That description is also the proxy's safety
  spec — server-side system prompt, output length cap, safety settings left on, a
  refusal path that falls back to authored lines — so writing it is not wasted work.
- After a build and store page are approved, some survey answers cannot be edited
  without first contacting Steam Support. You send a summary of what changed, which
  answer it affects, and instructions for a tester to reach the content. Same rhythm as
  the two tickets already open.

**The clause that bears on the tier decision.** Valve's FAQ addresses a game that calls
a paid AI service per interaction directly: you are expected to manage access to that
service on your players' behalf and to collect payment through a Steam-supported
method. Four ways are offered — build the cost into the game's price, charge per
interaction as microtransactions, sell a subscription, or sell a DLC that unlocks it.
Absorbing it into the price is listed first and is the only one of the four consistent
with the *no microtransactions* claim already sitting in the build review request. See
correction 3.

**The one absolute.** Valve does not ship live-generated AI in Adult Only sexual
content. Not this game — but the chat characters are the dancers and the Presence, and
`docs/THE_PRESENCE.md` writes them to be magnetic on purpose. The hard line belongs in
the system prompt, and that text is substantially what goes in the guardrails field.

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

Valve's own guidance points the same way. Its first suggested handling for a per-call
AI service is to model the cost into the game's price and charge nothing extra, which
is a capped monthly spend by another name. The free tier fits none of Valve's four
options cleanly: a feature that goes dark whenever a shared pool empties is hard to
describe to a reviewer asking how players get access to the service.

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
2. **NEW.** Draft the guardrails answer — it doubles as the proxy's safety spec. Do
   **not** touch the survey or the three disclosure passages while the 1.0 reviews are
   in flight; the amendment goes to Steam Support after 1.0 is approved, with a summary
   of the change and tester instructions.
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

## Step 3 as built — the E binding traced (2026-09-15)

| step | where |
|---|---|
| `InteractAction` bound on `ETriggerEvent::Started` | `SibeliusGameCharacter.cpp:291` |
| `DoInteract()`, early-out on `AreCameraPowersSuspended()` | `SibeliusGameCharacter.cpp:431` |
| `UInteractorComponent::TryInteract()` | `InteractorComponent.cpp:243` |
| TV special case, else `IInteractable` → `Execute_Interact`, else find `UDancerAgentComponent` → `Greet()` | `InteractorComponent.cpp:273` |

Dancers are reached by the **fallback** branch — they do not implement `IInteractable`.
Give a chat-capable agent that interface and greeting stops silently.

`Greet()` forks at once: `HasPowerToGive()` sends E to `PowerGrant->RequestTrial(PC)`
and returns, because the slot trial takes the camera. Only the second branch talks —
stop voice, `PlayTalkVoice()`, `PlayTalkFace()` (adjacent on purpose: one start time for
both clocks), `TalkHoldSeconds`, `HUD->HoldCinematic()`, `BeginGreetingMotion()`,
`BeginTalkShot()`.

**The safety question is answered by control flow, not by care.**
`ResumeAfterGreeting()` has exactly one entry point, the `GreetingTimer` callback, armed
at `:1838` and `:1873`. There is no direct call anywhere in the codebase. `CancelGreeting()`
clears that timer as its first act (`:1919`). So **any exit routed through
`CancelGreeting()` cannot reach the grant** — `EndPlay` (`:423`), `ResumeAfterGreeting`
(`:1912`) and `ShuffleDance` (`:2011`, from `SlapComponent`) already rely on this.

### Three hazards the handoff could not have known about

1. **The HUD lease is a timer sized to a voice clip.** `HoldCinematic(TalkHoldSeconds +
   TalkCameraBlendIn)` is about seven seconds; a conversation is minutes. When it lapses
   the crosshair, objective banner and sauce counter return *on top of* the close-up —
   the exact bug `docs/DANCER_VOICE.md` was written to fix. Chat needs its own
   open-ended hold, released on close.
2. **`LockTalkInput()` is not enough for a typing UI.** It sets only `SetIgnoreMoveInput`
   and `SetIgnoreLookInput` (`:855`). Input mode and cursor are untouched, so chat adds
   new state with a restore obligation that `EndTalkShot()` will not discharge.
3. **`bGreeting` without a timer is a loaded gun.** Reusing `BeginGreetingMotion()` for
   the dance crawl means a second E press takes the `if (bGreeting)` branch at `:1835`
   and arms the timer — the grant reachable again from a state that never intended it.

### The hook

Split the pose work from the timer work: `EnterTalkPose(bool bArmTimer)` holding today's
`:1843-1871`, with `BeginGreetingMotion()` passing `true`. `BeginChat()` then calls
`BeginTalkShot()` + `EnterTalkPose(false)` and raises `bChatting`, so the timer is never
armed and the grant is unreachable by construction. `Greet()` early-returns to focus the
open widget while `bChatting`, closing hazard 3. `EndChat()` closes the widget, restores
input mode and cursor, releases the HUD hold, then calls `CancelGreeting(true)` for the
shared teardown — which already handles the never-was-greeting case through its
`bWasGreeting` early-out.

### Save conventions, for step 5

`UProgressionSaveGame` is its own slot, `CurrentSaveVersion = 1`, stamped on write with 0
meaning blank, every persisted field `UPROPERTY(SaveGame)`, and `UProgressionSubsystem`
the only runtime writer. Conversation memory does **not** belong in that slot — it is
deliberately separate from the deploy save so earned powers survive a clear. Memory wants
its own slot with the same version-and-migrate discipline.

---

## Steps 4-5 as walked — PIE, 2026-09-16

Walked by Walt in `L_Office_v02` on Kaia, with `sib.Chat.Enable 1`. Reported green.
Confirmed from screenshots: the panel opens on her recorded line, typing works with
movement dead and the cursor back, a reply lands about a second later, and a saved
conversation is replayed in order with the right speakers between the `what she
remembers` and `now` markers — across a session boundary, which is the disk round trip
and the whole of step 5.

### Three things the walk-test decided

**The panel moved twice, and the second move needed the camera.** It started as a
full-width band at 0.56 of screen height and cut across her mouth — the complaint
`docs/DANCER_VOICE.md` was written about, rebuilt in a different widget three weeks
after the HUD was blanked to prevent it. Dropping the band to 0.66 cleared her mouth
and still covered her chin, and Walt's own verdict was that dropping it further would
not leave enough text space. He was right: the band could not win, because at
`TalkCameraFOV` her head fills the middle of the frame and a full-width card has
nowhere to go.

The fix moves both halves. The chat shot yaws the camera 10 degrees so she sits left of
centre and widens to 46 so an off-centre head keeps both ears; the card takes the
cleared right column, which is TALLER than the band was. The transcript gained lines
instead of losing them. Everything is behind `bChatting`, so the authored greeting keeps
its dead-centre portrait. Tunable live: `sib.Chat.SidePanel`, `sib.Chat.ComposeYaw`,
`sib.Chat.Fov`, re-read on each open.

**Esc closes the play session, not the panel.** The editor binds `StopPlaySession` to
Escape (`DebuggerCommands.cpp:362`) and takes the key before the widget sees it. Esc is
still handled for the packaged build, where nothing else owns it, but a way out that can
only be exercised in a build nobody plays during development is a way out nobody has
tested — so the panel grew a **Close** button beside her name.

**The gate learned to ask the right question.** `sib.Chat.Enable=1` went into the
engine's own `ConsoleVariables.ini` so it survives editor restarts, and the gate went
red instantly. It was wrong to: that file is machine-local and never packaged. The
assertion now checks whether any file in the project's `Config/` sets the switch — the
thing that would actually ship — and reports the process value as a note. Verified both
ways.

### Environment lessons, for the next person who loses an hour

- The editor stalled twice at *Submitting global shader DDC Requests* with everything
  idle: no shader workers, no errors, Zen healthy, 1.2 TB free. It cleared on its own
  the third time. If it recurs, `-ddc=InstalledNoZenLocalFallback` drops Zen's local
  cache out of the hierarchy and is the one-launch test for whether Zen is the cause.
- A C++ build fails with *Unable to build while Live Coding is active* whenever the
  editor is open, and Ctrl+Alt+F11 will not cover a change that adds a `UPROPERTY` or a
  `UFUNCTION`. Close the editor for those.

### Still unproven

Esc closing the panel in a packaged build — untestable in PIE by construction. And the
face rig: Isla logged `shape=**MISSING**` with mouth peak 0.00 during a greeting, which
is consistent with `bTalkMouthMotion` shipping false and is the rig work rung three
needs (see *Where Walt wants this to go*).

---

## The measured allowance, and the tier call (2026-09-16)

`docs/GEMINI_3_1_FLASH_LITE_HANDOFF.md` did the thing correction 2 asked for: it read
the dashboard. **Confirmed first-hand on the same dashboard later that day** — these are
read values, not quoted ones. Project `walt-parkman-agent`, free tier, zero usage on
every model:

| `gemini-3.1-flash-lite`, free tier | |
|---|---:|
| requests per minute | 15 |
| input tokens per minute | 250,000 |
| **requests per day** | **500** |

The neighbours explain why this model rather than another:

| Model, free tier | RPM | RPD |
|---|---:|---:|
| Gemini 3.1 Flash Lite | 15 | **500** |
| Gemini 3.5 Flash Lite | 15 | **500** |
| Gemini 2.5 Flash Lite | 10 | 20 |
| Gemini 3 / 3.5 / 3.6 / 3.7 / 3.8 Flash | 5 | 20 |

**3.1 and 3.5 Flash-Lite are identical on the free tier and are not identical paid** —
$0.25 / $1.50 against $0.30 / $2.50. Having chosen capped paid, 3.1 buys the same
measured capacity for about forty per cent less output cost. The handoff picked right;
this is the reason.

The other rows matter as a fallback plan: **every full Flash model in the family allows
twenty requests a day.** If Flash-Lite ever disappoints on reply quality there is no
free-tier model to fall back to, only a more expensive paid one. Budget for that before
treating a model swap as a cheap fix.

One practical note for whoever re-checks this: the dashboard hides most models until the
**All models** toggle is flipped. The default view showed a single unrelated row, which
is an easy way to conclude the model is missing entirely.

Its own arithmetic: 450 usable after a testing reserve, ten replies per conversation,
**45 conversations per day across every player who owns the game.** Not 45 each. That is
the number correction 2 was estimating in the hundreds, and it landed at the bottom of
the range.

**Model correction to this document.** The cost table above uses Gemini 3.5 Flash-Lite
at $0.30 / $2.50 per million tokens. 3.1 Flash-Lite is **cheaper** — $0.25 in, $1.50 out
— and is the model the newer handoff selects. The per-conversation figure improves
slightly; the argument does not change.

### What the quota machinery is defending, priced

500 requests a day, at 2,000 input and 150 output tokens each, costs on the paid tier:

```
500 x (2,000 x $0.25/M  +  150 x $1.50/M)  =  500 x $0.000725  =  $0.36 / day
```

**About $11 a month.** That is the sum protected by the atomic cross-instance
accounting, reservation-before-dispatch, distributed deduplication, session-token
issuance and global limiter in section 5 of that handoff. And $11 a month of paid
Flash-Lite buys roughly the same 500 calls a day — with no shared pool for one player's
afternoon to empty, no product-improvement use of what players type, and an answer that
matches the first of Valve's four listed models for a per-call AI service.

### Recommendation

**Capped paid, $10–15 a month.** Section 5 then collapses to a counter and a ceiling,
and keeps only the parts that are about correctness rather than thrift: deduplication,
one request in flight, the kill switch, and the story-safe unavailable message. The
cutoff behaviour Walt asked for is unchanged — it is a spend cap instead of a quota
wall.

If the free tier is kept anyway, that is a legitimate call, but then two things follow:
ship it labelled experimental and expect it dark for most players most of the time, and
build a single-instance counter rather than distributed accounting. Forty-five
conversations a day does not justify infrastructure that could serve forty-five
thousand.

**Still Walt's call. Nothing is decided here.**

### Two defects it found in the step 4-5 code

Both verified and fixed on 2026-09-16 (`a6dd2f73`): `HandleChatSend` sent `AgentName` to
the service while memory was keyed by `ResolveChatId()`, and the player's turn was
recorded *before* the send, which would have put the current message into the request
twice once a real transport built its history from the same store. Neither was visible
in the mock. Reading beats waiting for step 6.

---

## DECIDED 2026-09-16: capped paid, $15 a month

Walt's call, on the measured numbers. `gemini-3.1-flash-lite`, paid tier, with a hard
ceiling of **$15 per month**. The cutoff rule is unchanged in spirit — stop when it runs
out — it is now a spend cap rather than somebody else's quota wall.

### Billing is live, and the constraint has changed (2026-09-16)

Paid tier activated on `walt-parkman-agent`: billing account `…692239`, **Prepay with
$25 loaded and auto-reload OFF**, billing-account tier cap $250, credits expiring a year
from purchase. The rate-limit page now reads **Tier 1**.

| `gemini-3.1-flash-lite` | free tier | Tier 1 |
|---|---:|---:|
| requests per minute | 15 | **1,000** |
| input tokens per minute | 250K | **1M** |
| requests per day | 500 | **10,000** |

**Money is now the only constraint, and that is a bigger change than the numbers look.**
Ten thousand requests a day is roughly 300,000 a month; $15 buys about 20,600. The rate
limit sits some fifteen times above what the budget can afford, so every limiter in the
design is now protecting a wallet rather than a quota.

**The free tier was also a shield, and it is gone.** At 500 requests a day nobody could
do much harm to a public endpoint. At 1,000 requests a minute, a $25 balance is 34,000
replies — **about thirty-five minutes of sustained abuse**. Nothing upstream will stop
that now; the only thing between a scraper and the balance is our own proxy.

So three mitigations that were prudence become load-bearing:

1. **A monthly counter at $15**, stopping with the story-safe message.
2. **A daily sub-cap** — recommend about $0.75, roughly 1,000 replies or 100
   conversations. Without one, a single bad day empties the month, and a monthly cap
   alone cannot tell a launch from an attack.
3. **Per-session and per-IP throttling**, for the same reason rather than the old one.

**Three stops, and only two of them hold still.**

| stop | amount | enforced by |
|---|---:|---|
| proxy counter | $15 / month | us, with the story-safe message |
| prepay balance | $25 | Google, hard |
| billing-account tier cap | $250 | Google, hard — **and it raises itself** |

**Auto-reload stays off.** It is what makes $25 a ceiling instead of a suggestion; with
it on, a runaway refills itself. Treat turning it on as a decision requiring the same
thought as raising the cap.

**Do not count the $250 as a safety net.** The tier panel states the promotion rule
plainly: once $100 has been spent on the Gemini API and three days have passed since the
first payment, the account moves to the next tier automatically, and Tier 2's cap is
$2,000. At $15 a month that is roughly seven months away — but it means the tier cap is
a throttle on new accounts, not a ceiling anyone chose, and it drifts upward without a
decision. The two limits that never move on their own are the prepay balance with
auto-reload off, and the counter we write. Those are the controls.

**The cost graph is not a spend meter.** AI Studio warns that costs take hours and may
take more than a day to appear, so it cannot show what a test run cost on the day it
ran. The proxy must record the token counts that come back on every response — already
asked for in the handoff as telemetry, and now the only real-time view of spend.

### What $15 buys, and the caveat that decides whether it is true

At $0.25 per million input and $1.50 per million output, a reply of 2,000 input and 150
output tokens costs $0.000725.

| output tokens per reply | cost per reply | replies / month | ≈ conversations / day |
|---|---:|---:|---:|
| 150 (thinking off or minimal) | $0.000725 | 20,600 | **69** |
| 600 (thinking left on) | $0.0014 | 10,700 | **36** |

**Read the second row before trusting the first.** With thinking left running, $15 a
month buys *fewer* daily requests than the free tier's 500 — the decision would still be
right for the data terms and the absent shared pool, but it would buy no extra capacity
at all. So the thinking controls are not a tuning detail here; they are what makes this
budget mean 69 conversations instead of 36. Measure real token usage in the staged smoke
test before treating either row as fact.

### What this does and does not delete

It deletes the reason for **reservation-before-dispatch, cross-instance atomic
accounting and quota arbitration between players**. Those existed to ration a pool that
empties for everyone; a spend cap empties for nobody until Walt says so.

It keeps the counter, but not as the brake — and an earlier draft of this section had
that backwards, so it is worth stating plainly.

**Where the money actually stops (checked on ai.google.dev/gemini-api/docs/billing,
2026-09-16).** Gemini API billing is managed in **AI Studio**, not the Google Cloud
console; the docs say balance and transactions must be handled in the AI Studio Billing
tab. AI Studio offers **project-level spending caps that are hard stops** — service
pauses when the cap is reached, rather than sending an email about it. And new accounts
default to **Prepay**: credits are bought in advance and drawn down in near real time,
so a $15 prepay balance *is* a $15 ceiling by construction, with nothing to configure.

So the ceiling is enforced by Google, twice over. The proxy's counter is belt-and-braces
and still earns its place for a different reason: it stops at **our** number and shows
the story-safe unavailable message, instead of letting players discover the limit as
upstream errors mid-conversation. A persisted counter on a single small instance is
enough for that — 700 requests a day is plainly one instance — and the distributed
machinery is what was never needed.

Also unchanged: deduplication, one request in flight per session, the kill switch, the
story-safe unavailable message, and never falling back to anything that costs more.

### Two consequences worth carrying forward

**Steam.** Absorbing the cost into the game's price is the first of Valve's four listed
handlings for a per-call AI service, and the only one consistent with the *no
microtransactions* claim already in the build review request. The 2.0 disclosure
amendment now has a clean answer to the payment question.

**Data handling.** The paid tier flips *content used to improve our products* to no.
Players must still be told their messages are sent to Google, but the sentence that
worried this review — confiding in a character, on a tier that trains on it — no longer
applies. Re-check the terms before release; this is the kind of clause that moves.

### The one step only Walt can take

Enable billing on the project and load the balance, at **aistudio.google.com/billing**:
select or create a billing account, add a payment method, choose **Prepay** and load
**$15**, optionally set a project-level cap at the same figure, then give it about ten
minutes — tier upgrades usually reflect by then, though billing signals are not always
real time. Confirm the project reads paid tier, and re-read **aistudio.google.com/rate-limit**
afterwards: the paid RPM and RPD replace the 15 / 500 measured on the free tier, and the
capacity note above should be updated with the new figures.

None of that is something to automate, and nothing in the proxy should be able to change
a tier or a project on its own. The API key belongs in the proxy's secret configuration
and nowhere else — not in the repo, not in a config file that ships, not in a log.

---

## The latency target (2026-09-16)

Neither handoff sets one. `GEMINI_3_1_FLASH_LITE_HANDOFF.md` proposes a 20-second
backend deadline and a 25-second client timeout, and those are right — as **failure
boundaries**. They catch a dead connection. They say nothing about what a conversation
is allowed to cost, so the first real measurement would have had nothing to fail
against.

The shot decides the number. She is framed at `TalkCameraFOV` with her face filling
frame and the HUD blacked out; the player has pressed Enter and is watching her. In
that frame a pause reads as hesitation long before it reads as a computer working.

| Measured keypress to visible reply | Verdict |
|---|---|
| **p50 ≤ 1.5 s** | the target |
| **p95 ≤ 3 s** | acceptable |
| **> 5 s** | a design failure, not a slow request |

**Measured client-side, from the keypress to the text appearing** — not the provider's
own latency figure. Server-side timing hides the two network hops, the proxy's work and
Unreal's HTTP thread, which is most of what the player actually waits through.

### If it misses, these are the levers, cheapest first

1. **Thinking budget.** The most likely cause of a slow short reply, and 3.1 Flash-Lite's
   controls must be checked rather than copied from another model — the handoff is right
   about that.
2. **Output cap.** 512 tokens is the suggested start for a reply meant to be one to three
   sentences. A lower cap is faster, but not below what a complete answer needs.
3. **Context size.** Sixteen turns of history plus a profile is the prompt. Trimming to
   eight halves the input and is invisible to a player mid-conversation.
4. **Proxy region.** A backend far from the player adds a round trip twice.
5. **Streaming.** The real fix for *perceived* latency — first words in half a second
   make three seconds tolerable. The handoff defers it to a later version, which is the
   right call for a first connection, but this is the lever to reach for if p95 will not
   come down.

### Two notes for whoever implements it

The mock's `sib.Chat.MockDelay` default of 1.1 s was chosen to sit at this target, so
the panel has always been tuned against a realistic wait rather than an instant one.
Keep it there when the proxy lands; a mock that answers instantly hides every waiting-
state problem the real service will have.

And if p95 settles above roughly 2.5 s, the waiting state needs to be more than a static
"thinking" line — at that length a motionless panel under a motionless face reads as a
hang. That is a UI change, not a service one, and it is cheaper than chasing the last
second out of the model.

---

## Decisions that need Walt

**Free tier, or a paid tier with a hard spend cap?** *Decided 2026-09-16: capped paid at
$15 a month.* See *DECIDED: capped paid* above for what it buys, what it deletes from
the plan, and the thinking-budget caveat that decides which of those two capacity
figures is the real one. Free means the feature
is dark for most players most of the time and their conversations feed product
improvement. Capped-paid means about a cent a conversation, a ceiling you choose, no
training use, and an answer that fits Valve's first suggested model. *Recommended: read
the dashboard number first, then take capped-paid at $10–20 a month unless the free
allowance surprises us. The cutoff behaviour survives either way.*

**Does chat ship to Steam, or stay an itch and experimental build?** *Decided
2026-09-15: 1.0 ships chat-free.* Chat is built behind an opt-in experimental toggle and
runs on itch first; the Steam disclosure amendment goes through Support as part of a
later update. Nothing in the two in-flight reviews gets edited to accommodate it.

---

## Where Walt wants this to go (2026-09-15)

Recorded in his words, before implementation starts. Nothing here is scheduled and none
of it gates the work above.

> I hope that in the future there will be real-time voice and face animation tools
> available that I can build into the game such that the AI agents appear to be
> speaking to you and the player can feel he is making a human-like friend.

This is the ladder the handoff's memory section is already climbing, and the one
`docs/THE_PRESENCE.md` set the rule for — *seduction through attention, not skin; she
remembers what you did and speaks to you like you matter.* Typed chat with memory is
rung one. Voice is rung two. A face that moves with the voice is rung three.

**Most of rung three is already built, and it is only pointed the wrong way in time.**
The staging for a character who speaks to you exists: `BeginTalkShot()` frames her at
38° of FOV, the HUD goes fully dark for the length of the shot, and the hold follows
the clip. What is baked rather than live is the two artifacts that shot plays.

| piece | today | what the hope needs |
|---|---|---|
| voice | ElevenLabs clips made by hand, shipped in the build, chosen by `FindTalkVoice()` | streaming TTS inside the reply's latency budget |
| face | MetaHuman Animator solved `kaia_intro` offline — 766 keys on `CTRL_expressions_jawOpen`, 79 of 162 speech curves | the same Anim Sequence produced at runtime |
| mouth fallback | envelope-driven lip layer, built and measured, `bTalkMouthMotion` ships `false` because the rig ate it | a rig change, or the Animator route above |

`docs/DANCER_VOICE.md` already recorded the opening: RigLogic *consumes* the Anim
Sequence that MetaHuman Animator writes, and these Face components are `leaderpose=no`,
so they can evaluate their own animation. Unproven, but the door is open.

**What rungs two and three cost beyond the tools existing.** Each adds a live-generated
category to the Steam disclosure — generated speech, generated animation — each adds a
per-reply cost on top of the text call, and both have to land inside the few seconds a
player will wait while looking at a face. The decisions recorded above (one call per
turn, capped spend, guardrails text) get re-opened at each rung rather than inherited.

**One design note.** The friend feeling is carried further by memory than by fidelity.
One recorded line and a dark HUD already land at 48 cm. A character who remembers what
you told her last week and asks about it will read as a friend in plain text; a
perfectly lip-synced character who starts every conversation from nothing will not.
Rung one does most of the work, which is lucky, because it is the only cheap rung. The
handoff's own framing is the one to keep: believable attentiveness from a fictional
character, without claiming the AI literally has feelings.

---

## Sources

Checked 2026-09-15. These are the pages that move; re-check them rather than trusting
the numbers above.

- Gemini pricing and free-tier eligibility — https://ai.google.dev/gemini-api/docs/pricing
- Rate limits — https://ai.google.dev/gemini-api/docs/rate-limits (free-tier figures now
  live in the AI Studio dashboard, not the docs)
- Billing, including the March 2026 trial-credit exclusion —
  https://ai.google.dev/gemini-api/docs/billing
- Valve's Content Survey — partner.steamgames.com/doc/gettingstarted/contentsurvey
  (partner login required). Source for the guardrails requirement, the post-approval
  edit path through Steam Support, the per-call AI service payment options, and the
  Adult Only live-generated prohibition.
- App 5249380 review state read from the Steamworks dashboard, 2026-09-15.

A designed version of this review is published as a private artifact:
https://claude.ai/artifact/T9bqzMZCcZ26TJAXmznnp6
