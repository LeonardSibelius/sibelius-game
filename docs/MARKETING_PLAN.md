# MARKETING_PLAN — getting attention for a free alpha

*Written 2026-07-18 at Walt's request ("how would a modern marketing business
put my game out there?"), the day v0.8.0 ALPHA shipped. Companion to
APPEAL_PLAN (reaching strangers with the game) and the promotion mission
(the game + AI-assisted development, evidence over advocacy).*

## The positioning (decide once, reuse everywhere)

At alpha, the product being marketed is THE STORY, and the game is its
proof. One sentence, used everywhere until it's worn smooth:

> **I'm 71. Every system I built in 40 years was retired. So I built my
> memoir as a casino, with an AI — and you can play it free.**

Everything else (mechanics, screenshots, essays) hangs off that sentence.
The three audiences it reaches, in order of receptiveness:
1. **AI-curious builders** (HN, r/ClaudeAI, AI Twitter) — care about the
   "built with AI, reviewed every line" process.
2. **Weird-itch-game people** (itch browse, curator YouTubers, r/indiegames)
   — care about slap-a-demon-into-a-zebra and a casino in a memoir.
3. **General human-interest** (press, Substack, even local news) — care
   about the 71-year-old and the eight letters to former employers.

## The asset checklist (make once, spend forever)

**Revised 2026-07-18 (Walt: "what about YouTube videos?"): video is the
FACTORY, GIFs are its byproducts.** Record devlogs; every hook GIF below
falls out of the footage in one conversion step.

- [ ] **The video stack (Walt's, chosen for him — corrected 2026-07-18,
      Walt's catch: Descript no longer has a free tier)**: OBS Studio
      captures PIE gameplay (one-time setup; agent supplies numbered
      settings). **CapCut desktop (free)** edits — auto-captions,
      transcript-based cutting, and vertical Shorts reframing in one
      tool. Clipchamp (in Windows 11) is the zero-learning fallback.
      Escalation ONLY if tool friction blocks the cadence: one paid month
      of Descript (~$16, text-based editing, then cancel). Auto-captions
      always on (feeds play muted).
      **RULE: AI edits real footage, never generates it** — synthesized
      gameplay would poison the evidence-over-advocacy brand instantly.
- [ ] **Per-release devlog video, 3-6 min** (REVISED 2026-07-18: Walt
      retracted the live-commentary take-one from YouTube; take two is
      SCRIPTED with an AI VOICE reading Walt's words — on-brand for the
      merge thesis, DISCLOSED in the description + YouTube's altered-
      content box). Pipeline: Walt writes the script → agent returns an
      annotated shot list (incl. ToggleDebugCamera console free-cam for
      cinematic b-roll — real footage, drone-style passes) → CapCut
      built-in text-to-speech first, ElevenLabs free tier if quality
      disappoints (paid tier can clone Walt's own voice — the literal
      merge). Guardrail unchanged: anything presented as the game is
      genuine capture. Title pattern: "I'm 71 and I'm building my memoir
      as a casino with an AI — Devlog N: <release name>".
- [ ] **2-3 Shorts cut from each devlog** (45 s, vertical) — Shorts is
      where YouTube discovery happens now.
- [ ] **3-5 hook GIFs cut from the same footage**: the zebra slap; the
      reels slam-stopping on a win; a poker hand with "the house suggests"
      visible; the shrine trial with Gideons crowding in; the kitchen door
      shimmer reveal.
- [ ] **One 60-90 s trailer**: story cards in Walt's voice between the
      best shots. Cut later from the same footage; not a blocker.
- [ ] **Press kit page** (docs/PRESS_KIT.md → later a web page): the one-
      sentence hook, the longer story, 8-10 screenshots, the GIFs, facts
      (free, Windows, ~3.5 GB, alpha, made with Claude), contact. Agent
      drafts; Walt approves.
- [ ] **itch page**: already strong and in Walt's voice. Add the GIFs at
      top (the itch page is the landing page for EVERY channel below).
- [ ] **Capsule/banner art**: needed before any push — pages with default
      itch styling read as abandoned. One good image of the cathedral
      machine or the library floor + title text.

## The channels (in rollout order)

1. **itch devlogs — the drumbeat.** One per release (v0.8.0's is overdue:
   "the library opens its floor"). Devlogs surface in itch feeds; a
   visible update cadence is itself marketing. Agent drafts each from the
   session log; Walt pastes.
2. **Substack — the thesis.** Already live (the assembler essay). Each
   essay links the game; each devlog links the Substack. One voice.
3. **Reddit, in escalating rings.** Feedback subs first (r/DestroyMyGame,
   r/playmygame — brutal, useful, low stakes), then r/IndieDev +
   r/indiegames with the best GIF, then the AI ring (r/ClaudeAI,
   r/aigamedev) with the process story. One sub per day, native copy each
   time, Walt replies personally. Never identical cross-posts.
4. **Show HN — the big swing.** One shot, taken when the itch page has
   GIFs and the game has survived a feedback-sub pass. Title close to:
   "Show HN: I'm 71, spent 40 years on data systems, built my memoir as a
   casino with Claude." HN loves: the age, the honesty about what the AI
   did vs what Walt did, the par-sheet rigor, the memoir letters. Walt
   must be present in comments for the first 3-4 hours.
5. **Curator YouTubers/streamers of weird free games.** 10-20 short
   personal emails ("I'm 71, I made this with an AI, it's free, here's a
   GIF; no obligations"). Free alpha is exactly their beat. Agent drafts
   the pitch + the list; Walt sends from his own address.
6. **Anthropic's community itself.** The story is a made-with-Claude
   showcase Anthropic's devrel would plausibly feature. Email/Discord
   with the press kit. Serves the dual mission directly.
7. **Steam "Coming Soon" page — at beta, not now.** $100 fee, but
   wishlists are the only currency that matters commercially, and a
   Playtest can replace itch alpha distribution later (mirrors GhostCam's
   closed-alpha plan). Decision point: when mechanics stop churning.

## Paste-ready: the feedback-sub post (v0.9.7, written 2026-08-22)

Ring 1 of channel 3 — **r/DestroyMyGame** and **r/playmygame**. These exist for exactly
what Walt asked for: strangers who will try it and say what is wrong. Low stakes, brutal,
useful, and nobody there is offended by an alpha.

**Read each sub's rules first.** r/DestroyMyGame usually requires a video or GIF in the
post itself and is hostile to bare store links; r/playmygame wants a playable link and a
specific question. Post one sub per day, never the same text twice, and reply to every
comment personally — that is the whole value.

**THE ASK MATTERS MORE THAN THE PITCH.** "Try my game" gets nothing. One narrow question
about the first ten minutes gets real answers. For 0.9.7 the question is the one the
whole machine was built to test:

> *Was finding the broken part satisfying, with no quest marker telling you which one?*

---

**Title:** I'm 71, spent 40 years on data systems, and made a game where debugging a
machine is the plot. First ten minutes — does the puzzle land?

**Body:**

Free, Windows, no signup: https://leonardsibelius.itch.io/leonard-sibelius

You play a programmer. Your boss messages you in the first minute: the legacy system
threw again overnight, fix it, and do it by hand — she is not paying a senior developer
to ask a machine.

The legacy system is a real machine in the living room. It runs while you watch. A blank
travels down a row of five stations and lands in REJECT every time, and the housing keeps
a log of the night it spent doing that. Every station has a plaque saying what it does.
One of them is lying, and the only way to see it is a power that shows you what each part
*actually* does. No quest marker, no highlighted object.

**What I want to know:** did you find the broken part by reasoning, or by clicking
everything? And did finding it feel like anything? That is the whole bet of the project
and I cannot judge it myself any more.

Fair warnings: it is an alpha, the art is placeholder in places, and there is more game
past the machine (a cathedral, a slot machine I built to a real par sheet) that you can
ignore for this.

I built it with Claude. Happy to talk about what that was actually like if anyone cares —
including the parts where it was worse than doing it myself.

---

**After a pass through the feedback subs** — and only then — the plan's later rings still
apply: r/IndieDev with the best GIF, then the AI ring, then Show HN as the one big swing.
Do not spend Show HN before the feedback subs have found the obvious problems.

## The cadence (sustainable for one person + one agent)

- **Weekly**: one small post somewhere (a GIF, a hand of poker, a shrine
  letter excerpt). 15 minutes.
- **Per release**: itch devlog + the same content reshaped for one Reddit
  ring. Agent drafts both in the ship session.
- **Monthly-ish**: one Substack essay (the thesis pieces — assembler-mask
  quality). These are the compounding asset; essays get shared for years.
- **Once**: press kit, trailer, Show HN, curator email wave.

## What a marketing business would bill for but Walt should NOT do

- Paid ads for a free alpha (burns money, buys nothing durable).
- Identical copy sprayed across channels (reads as spam, gets removed).
- Arguing the AI-assisted point in comments (evidence over advocacy —
  the mission's standing rule; post receipts, let them argue).
- Chasing TikTok/Shorts unless Walt genuinely enjoys making them (the
  "71-year-old dev" format would work but only sustained, in his voice).

## Measures (so "attention" is a number)

itch: views, downloads, collections (check the itch analytics page per
release). Substack: subscribers. Reddit/HN: not karma — click-throughs
visible as itch view spikes on post days. Later: Steam wishlists, the
only number publishers/press respect.

## Division of labor

**Agent (any session, on request):** devlog drafts, press kit, Show HN
draft, curator pitch + list, Reddit copy per sub, capsule art concepts,
essay editing. **Walt (zero-token, irreplaceable):** GIF capture, the
send button on everything (authenticity is the brand), comment replies
in his own voice, the Substack essays' first drafts (the voice IS the
product).
