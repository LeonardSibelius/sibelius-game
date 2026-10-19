# STEAM_STORE_COPY — paste-ready, field by field

> **FOR 2.3.0, USE `STEAM_RELEASE_2.3.0.md` (2026-10-11, moved through 2.2.0-2.2.9 and then
> to 2.3.0 on 2026-10-19).** It has the store text rewritten for everything since 2.1.0 (the agents who talk
> back and their worlds, the dance menu, the Grand Prix, the chess coach, Go, the laptop,
> Freakbook, R on the agents), the Live-Generated
> disclosure as the chat server stands
> now, storage at 10 GB, and the release-day steps. §1-§4 below are the copy for the builds before
> it; §5-§6 (why the copy reads as it does, the capsules) still apply.

**Prepared 2026-09-15 for app `Leonard Sibelius (5249380)`.** Derived from
`STEAM_PLAN.md` §3 REV 2 (2026-09-04), corrected against the shipped build **v1.1.3**.

REV 2 is not safe to paste as written. Three things were wrong; all three are fixed
below, and §5 of this file says exactly what changed and why.

> **Updated 2026-09-27 and 2026-09-28 for the next upload:** the living room now has a
> pinball machine, a pool table, a door to the Transhuman Bowling Alley and a door to the
> Transhuman Chess Park, and the poker cabinet is no longer hidden. One sentence of the
> long description, the credits, the storage note and three notes changed; §5b and §5c
> list them. **The AI disclosure did not change.** None of this applies to the build Valve
> already holds (v1.1.4): paste it with the next upload.

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
demons across the office, and hold [b]V[/b] to see what is hidden. And when you need a
break, your own living room has a video poker cabinet, a 1960s pinball machine, a pool
table, a door to the Transhuman Bowling Alley, and another to a park where a ghost will
play you at chess.

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
[b]DmitriyDryzhak[/b] · Pinball machine by [b]Speedbump Games[/b] · Pool table by
[b]PijayArt[/b] · Bowling alley by [b]PixelForgeESP[/b] · Chess set by [b]Tomas Dvorak[/b]
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

**Rechecked 2026-09-27 for the living-room games — nothing to add.** All three purchased
packs showed *"Generated with AI: No"* on their Fab listings before they were bought
(Speedbump Games' Retro Pinball Machines, PijayArt's Classic Billiards Table,
PixelForgeESP's Retro Bowling Alley). Their sounds are synthesised in code at runtime
(`Source/SibeliusGame/TableSounds.cpp`), not recordings and not AI. The bowling pin and
the BOWLING door sign are drawn by `Tools/Scripts/make_bowling_art.py` from regulation
dimensions. The physics that moves the balls and pins is code, not generated content.

**Rechecked 2026-09-28 for the chess park — nothing to add.** Tomas Dvorak's free Chess
Set showed *"Generated with AI: No"* on Fab before it was downloaded. The CHESS door sign is
a render of that set's own knight by `Tools/Scripts/make_chess_sign.py`; the two chess
sounds are synthesised like the others. **The ghost's moves are not generated content:**
they are chosen by a classic chess search (`Source/SibeliusGame/ChessEngine.cpp`,
alpha-beta over legal moves, written for this game), the same kind of opponent chess
programs have had since the 1970s. No model, no network, nothing produced that was not
already a legal move on the board. The park is a copy of the game's own city level.

The Live-Generated answer above is for the builds Valve has now. It changes only if the
AI chat amendment is accepted — see `STEAM_AI_CHAT_DISCLOSURE.md`, a separate ticket.

---

## 4. System requirements

`STEAM_PLAN.md` says to state these conservatively, because there is one machine in
this project and no way to test a minimum spec on it. A refund from someone whose
machine could not run it costs more than the sale.

**Minimum** — conservative, and **untested**. Walt's call, 2026-09-15: a plausible
UE5 floor rather than the dev machine, on the reasoning that it can be raised if
refunds come in, whereas buyers lost to an inflated minimum never show up as data.

```
OS:          Windows 10 64-bit
Processor:   Intel Core i5-8400 / AMD Ryzen 5 2600
Memory:      16 GB RAM
Graphics:    NVIDIA GeForce GTX 1060 6 GB / AMD Radeon RX 580 8 GB
DirectX:     Version 12
Storage:     8 GB available space
```

**Recommended** — measured from the machine it was built and tested on, not
estimated:

```
OS:          Windows 11 64-bit
Processor:   Intel Core i7-14700KF
Memory:      32 GB RAM
Graphics:    NVIDIA GeForce RTX 5070 Ti
DirectX:     Version 12
Storage:     8 GB available space
```

**Re-measured 2026-09-28: 8 GB still holds.** The chat Shipping build with the pinball,
pool table and bowling alley in it measured **7.83 GB** (the old note guessed 9). The
chess park adds the chess set's textures - tens of MB, not a gigabyte - and reuses the
city's art. Measure the next package once more before pasting; change both rows only if
it passes 8 GB.

**The 5 GB figure in `STEAM_PLAN.md` §3 is stale** — it predates the city, the
spaceport and the hotel. 8 GB is the measured number, rounded up from 7.8 GB.

**The minimum row is the one to revisit.** Nothing below the i7-14700KF / RTX 5070 Ti
has ever run this build, so the GTX 1060 line is an inference from what UE 5.7 games
of this size generally need. If early refunds cite performance, raise it — that is
cheaper than the reverse mistake.

---

## 4b. Content survey — gambling

Walt's call, 2026-09-15: **declare it.** The game contains simulated gambling —
*Celestial Fortune*, a five-reel slot machine with a published par sheet, and a video
poker cabinet in the living room. Both are played with in-game Sauce. **No real
money is wagered and nothing pays out in real currency.**

**Still the answer after 2026-09-27 and 2026-09-28.** The pinball, pool table, bowling
alley and chess table are free play: skill games, no Sauce, nothing wagered. The slot machine and poker are unchanged.

The reasoning: under-declaring is the expensive failure, because it surfaces *after*
launch, when the announcement has already been spent. Declaring simulated gambling
costs almost nothing in visibility. The slot machine is also a headline feature in
the store blurb, so a reviewer checking the claim would find it immediately.

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

## 5b. What changed on 2026-09-27, and why

**The poker line described a hidden cabinet that is no longer hidden.** Walt made the
poker cabinet plainly visible ("two hidden doors are enough") and the game's Code Vision
lesson moved to the legacy machine. The long description said *"hold V to see what is
hidden — starting with the video poker cabinet"*, which would now send a player looking
for a secret that is standing in the open. The V clause stays; the living-room sentence
replaces the poker clause and names the three new games in the same breath.

**Placement:** in the first paragraph of the game, not after the Architects — the living
room is where the game starts, and the Architects stay last (REV 2's rule).

**Credits:** Speedbump Games, PijayArt and PixelForgeESP, added to the artists' line the
way the Hotel credit was.

**The short description is unchanged.** It is 297 of 300 characters and every word is
spent; the games belong in the long description.

---

## 5c. What changed on 2026-09-28, and why

**Chess.** A CHESS door beside the bowling door leads to the Transhuman Chess Park, where
one of the city's ghosts plays you. The living-room sentence gains one clause for it, in
the same place and for the same reason as the other games (§5b). *"A ghost will play you"*
is what the player meets; the engine behind it is described in §3's recheck, so a reviewer
asking *"is the opponent AI?"* finds the answer: a chess program, not generative AI.

**Credits:** Tomas Dvorak, for the chess set. His Standard License does not require it;
the rest of the line credits every artist whose work is on screen, so he is credited too.

**The storage note was stale.** It predicted 9 GB; the package measured 7.83 GB. Fixed.

---

## 6. What still blocks the page

Nothing, as of 2026-09-15. Copy is done and so is the art.

### Capsules — use `C:\STEAM\capsules_v2\`, and only that folder

There are three generations on disk and they are **not** interchangeable:

| folder | date | title treatment | use it? |
|---|---|---|---|
| `capsules` | 3 Sep | none — raw MRQ renders | source material only |
| `capsules_final` | 3 Sep | thin, small, letterspaced | **no — fails at 120×45** |
| `capsules_v2` | 9 Sep | large bold condensed, stacked | **yes** |

The 3 September set sets the title in light letterspaced caps roughly 20 px tall on a
920 px canvas. Shrunk to the 120×45 thumbnail Steam generates, it disappears — the
exact failure `STEAM_PLAN.md` §5 warns about. `capsules_v2` fixes it, and
`PROOF_small_at_120x45.png` in that folder is the test.

`capsules_v2` now holds the complete set:

| asset | file | origin |
|---|---|---|
| Header 920×430 | `library_header_920x430.jpg` | 9 Sep |
| Small 462×174 | `small_capsule_462x174.jpg` | 9 Sep |
| **Main 1232×706** | `main_capsule_1232x706.jpg` | **15 Sep** |
| **Vertical 748×896** | `vertical_capsule_748x896.jpg` | **15 Sep** |
| Library capsule 600×900 | `library_capsule_600x900.jpg` | 9 Sep |
| Library hero 3840×1240 | `library_hero_3840x1240.jpg` | 9 Sep |
| Library logo 1280×720 | `library_logo.png` | 9 Sep |

**How the two new ones were made, in case they need redoing.** Both are crops of
`C:\STEAM\Kaia1.jpg` (2541×1305) graded with `eq=brightness=-0.05:saturation=1.02`
and `vignette=angle=PI/5`, with `library_logo.png` composited on top — the logo is the
title lockup on a real alpha channel, ink bounds `crop=955:480:163:71`, so the
lettering is the genuine artwork rather than a guessed font. Main crops
`2276:1305:132:0`; vertical crops `1090:1305:700:0` to centre the face.

**One flaw worth knowing about:** `library_hero_3840x1240.jpg` has the game's HUD
crosshair visible in the middle of the sky. Valve does not forbid it, but it reads as
unintentional. Re-shoot that frame with the HUD hidden if there is time.

### Screenshots — done

Eight, 16:9, all over 1920×1080, at `C:\STEAM\screenshots_final\`.

**One question the form will ask that is not answered anywhere yet:** the content
survey asks about gambling. This game has a slot machine with a published par sheet
and a video poker cabinet, both played with in-game Sauce and no real money. Decide
how to answer before you are in the form.
