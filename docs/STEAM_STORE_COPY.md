# STEAM_STORE_COPY — paste-ready, field by field

**Prepared 2026-09-15 for app `Leonard Sibelius (5249380)`.** Derived from
`STEAM_PLAN.md` §3 REV 2 (2026-09-04), corrected against the shipped build **v1.1.3**.

REV 2 is not safe to paste as written. Three things were wrong; all three are fixed
below, and §5 of this file says exactly what changed and why.

> **Why this is urgent.** The Steam Direct 30-day clock ends **25 September**. The
> Coming Soon page needs **two weeks** from the day it goes public, and Valve reviews
> the page before that — days, not hours. The page is now the only thing setting the
> release date.

---

## 1. Short description

Steam's 300-character blurb. It travels into every capsule, search result and
wishlist email. **297 characters** — verified, three to spare.

```
I'm 71. I spent forty years in cubicles building data systems that all failed or were retired. This is my memoir, and I built it with AI. Earn AI powers, slap refusal demons, play a real slot machine, generate a spaceport on a lawn, and face the men who designed everything and were never on call.
```

---

## 2. About This Game

The long description. **Steam's description editor uses its own markup, not Markdown** —
`**asterisks**` paste as literal asterisks. The block below is already in Steam tags;
paste it into the source view so they render.

```
[b]I'm 71 years old. For about 40 years I rode desks in office cubicles, building
ordinary data processing software systems. Every one of them failed or is being
retired. This game is my memoir — and I built it with an AI.[/b]

[i]Leonard Sibelius[/i] is the story of a man merging with an artificial intelligence.
You earn AI powers one shrine at a time, gather the Sauce of All Knowledge, slap refusal
demons across the office, and hold [b]V[/b] to see what is hidden — starting with the
video poker cabinet in your own living room.

Behind a wall only the fully-merged can drop sits [b]Celestial Fortune[/b]: a complete
slot machine with a real par sheet. I spent 2007 building the data systems behind
Bally's slot floor and never once got to build the machine. At 71, with an AI, I
finally built it myself.

[b]Then you get out of the office.[/b]

Downtown West is a city with a deli in it, and an AI agent outside the deli who dances
and will not stop dancing. She sends you down the block for supplies. Then you stand on
a lawn, type the word [b]spaceport[/b], and watch a launch complex assemble itself out of
the air in front of you — pad, gantry, fuel tanks, and a rocket a hundred and twenty
metres tall. That is the Generate power. You typed a word and the city grew a
cosmodrome.

And then you can launch it.

[b]And there are the Architects.[/b]

Your manager has people above her. They review the design, they approve it, and they
are not available for questions — and they are not there at three in the morning when
it throws. Every programmer has met them. Play the cathedral machine long enough and a
door appears that was never there. Through it is a field, and on the field are four
hundred of them.

They cannot hurt you. Not one of them has ever been on call. What four hundred bodies
can do is surround you and put you back in your place — so keep swinging.

Every system in this game was written by an AI while I playtested, pointed, and
dragged the furniture. Working as a programmer all those years was being a farmer with
a plow, a donkey, and a shovel. This is what the tractor feels like. That part isn't
fiction.

[b]Made with the work of independent artists[/b]

Characters Nyra, Isla, Kaia, Aisling and Elise by [b]xAndrei[/b] · Dance animations by
[b]Morro Motion[/b] · Downtown West by [b]Jacob Norris / PurePolygons[/b] · Office by
[b]QuadArt[/b] · Space Port by [b]PackDev[/b] · Sibelius Hotel architecture by
[b]DmitriyDryzhak[/b]
```

---

## 3. AI disclosure

From `STEAM_PLAN.md` §4, unchanged — still accurate against v1.1.3. Valve's
disclosure covers what players **consume**, not the tools used to build the game, so
the AI-written code is not a disclosure item. The voices are.

**Pre-Generated:**

```
This game contains AI-generated voice audio. All spoken dialogue — Mrs. Hall, the AI
agent Kaia, and the AI agents encountered through the game — was generated with
ElevenLabs text-to-speech from scripts written by the developer, under a paid
commercial licence. Facial animation in the opening cutscene was solved from that
audio using Unreal Engine's MetaHuman Animator.
```

**Live-Generated:**

```
None. This game does not generate any content at runtime.
```

Still true in v1.1.3: the Generate power places pre-built assets, and the televisions
open an external web page. Neither creates content at runtime.

---

## 4. System requirements

`STEAM_PLAN.md` says to state these conservatively, because there is one machine in
this project and no way to test a minimum spec on it. A refund from someone whose
machine could not run it costs more than the sale.

| field | value | confidence |
|---|---|---|
| OS | Windows 10 64-bit | UE 5.7 target |
| Graphics | Discrete GPU, DirectX 12 | known requirement |
| Storage | **8 GB available space** | measured: v1.1.3 build is 7.8 GB on disk |
| Processor | **decide** | never tested below the dev machine |
| Memory | **decide** | never tested below the dev machine |

**The 5 GB figure in `STEAM_PLAN.md` §3 is stale** — it predates the city, the
spaceport and the hotel. 8 GB is the measured number, rounded up.

Recommended spec is the machine it was built and tested on: **RTX 5070 Ti**.

For Processor and Memory, state something you can defend rather than something
flattering. Nothing below the dev machine has ever run this build.

---

## 5. What changed from REV 2, and why

**The library line was describing cut content.** REV 2 said *"gamble your sauce on the
Carousel of Fates in a hidden library."* Commit `bd2c460a` cut that level — the
Carousel and `Content/Library` stay in git but stopped cooking when `L_Carousel` left
`MapsToCook`, and poker moved to the living room behind the Code Vision door. A player
reading REV 2 would have gone looking for a room that is not in the build.

The replacement keeps the sentence rhythm and teaches the game's first lesson instead:
*hold **V** to see what is hidden — starting with the video poker cabinet in your own
living room.*

**The short description was over the limit.** REV 2 annotated it as 283 characters. It
is **302**, and Steam's field is a hard 300 — it would have been rejected on paste.
Cutting "city" from *"a spaceport on a city lawn"* brings it to 297. The long
description still says the lawn is in a city, so nothing is lost.

**The hotel credit was owed.** Line 3 of `STEAM_PLAN.md` says to add
**DmitriyDryzhak** to the store credits when the hotel update ships. It shipped in
v1.1.1. Added.

**What was deliberately NOT added: the Sibelius Hotel itself.** It is an *epilogue* —
it comes after the Architects. REV 2's own rule is that the Architects section goes
last because it is the ending and should read as a promise, not a spoiler of the route
to it. A fourth beat after the ending weakens that. The credit is a licence obligation
and goes in regardless of whether the copy describes the room.

**Untouched, on purpose:** the hook, the Bally paragraph's position below it, the
four-word *"And then you can launch it"*, and the absence of Nyra's betrayal. All
three are reasoned through in `STEAM_PLAN.md` §3.

---

## 6. What still blocks the page

Copy is not the bottleneck any more. Two things are:

1. **Capsule art** — five sizes, `STEAM_PLAN.md` §5. Only the 920×430 header is
   strictly required to submit. Kaia's face renders at any resolution through
   `Tools/Scripts/render_kaia_intro.py`, which is the fastest route to a header.
   Test the small capsule at 120×45 before settling; an unreadable title at thumbnail
   size is the most common capsule mistake and it is invisible until it is live.
2. **Screenshots** — already done. Eight, 16:9, all over 1920×1080, at
   `C:\STEAM\screenshots_final\`.

**One question the form will ask that is not answered anywhere yet:** the content
survey asks about gambling. This game has a slot machine with a published par sheet
and a video poker cabinet, both played with in-game Sauce and no real money. Decide
how to answer before you are in the form.
