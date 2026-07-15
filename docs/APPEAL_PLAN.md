# APPEAL_PLAN — making Sibelius reach more people

*Written 2026-07-14 by Claude (Fable 5) after the v0.7.1 ship, at Walt's request.
This is the handoff copy for future sessions (any model — Walt may paste this to
Opus when his Fable budget runs out). The six-point consult below is preserved
verbatim from the original conversation; the STATUS table tracks execution.*

---

## The framing (don't lose this)

Stop calling it silly. The games that get remembered on itch.io aren't the
polished ones — they're the **personal** ones. Sibelius has a truth no studio
can fake: it's an autobiographical game about a man merging with an AI,
**actually built by that man working with an AI**, containing a **real slot
machine designed by a real slot-machine designer**. That's not silly; that's
the most honest game premise around. The goal isn't to sand the weirdness
off — it's to remove everything that stops strangers from *reaching* the
weirdness.

## The six points, ordered by what moves the needle most

1. **The 9.7 GB wall** (the biggest one, and it's measurable). Nobody
   downloads 9.7 GB from an unknown free game — they close the tab. Most of
   that weight is cooked content the game barely uses: eight forest variants
   that are the same road in different costumes, plus Fab packs shipping
   wholesale. Cut to the best forests and audit what's actually cooked.

2. **The first five minutes.** Walt got lost in his own game more than once —
   the cauldron, the carousel, the locked powers — and he had the developer on
   call. A stranger has nobody. The fix isn't more journal text; it's one
   guided beat: wake up, one glowing objective ("find the sphere in the
   attic"), one power earned, one demon slapped inside ten minutes. Hook them
   with the slap before asking them to understand the lore.

3. **Tell the true story on the store page.** The pitch that markets itself
   isn't "narrative metroidvania" — it's *"I'm 71. I designed slot machines
   for a living. I built my memoir as a video game with an AI, and my real
   slot machine is hidden at the end of it."* Put that paragraph on the itch
   page, post it as a devlog, and let the Celestial Fortune screenshot carry
   it. People share stories, not features.

4. **Shareable ten seconds.** A GIF of a Gideon getting slapped into the
   treeline is worth a thousand words of description. Two or three GIFs on
   the page — slap, carousel spin, slot win — do the work of a trailer for
   free.

5. **A reason to come back.** The sauce economy gives sessions a spine, but
   there's no scoreboard. Cheap additions: a lifetime stats page in the M menu
   (demons slapped, sauce earned, carousel best run), and maybe a "daily
   carousel" with a fixed stake and bragging rights. Slot-floor wisdom:
   people return for streaks and near-misses, not content.

6. **Juice the slap.** It's the signature verb — a meatier sound, a tiny
   screen shake, a sauce-number popping off the corpse. An afternoon of work
   that makes the whole game feel 20% better.

## STATUS (2026-07-14, end of day)

| # | Item | Status |
|---|------|--------|
| 1 | Download diet | ✅ **SHIPPED as v0.7.2 (2026-07-14)** — forests 8→4 (01/03/06/08; Poplar uncooked), 2K texture cap, dragons + stained glass deleted. **Real numbers: cook 18.6→12.1 GB, archive 10.6→8.1 GB (~7.4 GB download, −24%); existing players patched with 289 MB.** The cap halved characters/props; the holdout is **EasyBiomes (5.98→5.65 GB — foliage MESHES, textures already small; 4 forests = 4 biomes so the pack ships nearly whole).** **Queued follow-up ("two-forest experiment"):** keep 2 forests sharing a biome family, move the 2 orphaned curios into them (anchor placements to the existing curio actors), est. download ≈5 GB. |
| 2 | First five minutes | ✅ **DONE (`d73ed1e`…`355d29f`)** — gold objective banner (readable at 4K, dark backing), state-derived chain (books → find COMPILE → compile key [C] → powers N/6 → cathedral Synthesis guidance; refuser-slap override; gates on the DURABLE Finale.Synthesis claim; silent post-game) + world nameplates. **Bonus shipped in the same arc:** Compile key is now **C** (was B; every text surface updated), **N N = player-facing New Game** (double-press confirm; full wipe + travel home — also the stranger-playtest button), and the finale altar wears **SM_Altar_Main_Marble** (from the cathedral's own pack, zero added download). Remaining nice-to-have: a full fresh-save walkthrough of every beat by a rested human. |
| 3 | Store-page story | 📝 **DRAFTED** in `docs/ITCH_PAGE.md` — Walt edits the words + pastes onto the itch page (zero tokens). |
| 4 | GIFs | 📝 **Shot list drafted** in `docs/ITCH_PAGE.md` — Walt records (Win+Alt+R), zero tokens. |
| 5 | Stats / daily carousel | ❌ not started (stats fields → ProgressionState additive fields; STATUS-menu rows) |
| 6 | Slap juice | ❌ not started (`USlapComponent::DoSlap` is the one code point; camera shake needs a C++ `UCameraShakeBase` subclass) |

## Milestone: the Carousel room stands on its own (2026-07-14 evening, `caae729`)

Walt: *"I am starting to get a better feeling about this game now that I have
HUD help."* — the guided-HUD bet (point 2) is paying off in his own play.

The Carousel of Fates got its comprehension pass, all driven by live playtest
complaints rather than speculation:

- **HUD readable + self-explaining** — CarouselHUD draws everything at 1.8x,
  and every phase carries a dim one-line explainer (chips fill the quota /
  coins buy upgrades / what a win or loss actually pays). A 71-year-old at a
  4K monitor no longer squints.
- **The machine looks like it belongs** — the placeholder checkered cubes now
  wear the cathedral's black marble, so the two fate machines (Carousel +
  Celestial Fortune) read as siblings. Zero download cost.
- **The piano is dead** — the Fab library pack auto-played OrchestralPianoTrack
  on entry via its own level scripting. Muted at the asset (volume 0) and
  force-added past the `Content/Library/` gitignore so a pack reinstall can't
  bring it back.

Carousel + Sauce gates green. Both repos clean and pushed.

## Handoff notes for the next session (read before working)

- **Walt**: 71, learning Unreal, wants small numbered steps, pastes screenshots
  back. Budget-constrained (moving Max → Pro; ration tokens). Never
  `git add -A` (stage exact files). Build (editor CLOSED):
  `& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" SibeliusGameEditor Win64 Development -project="C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -waitmutex`
- **Gates** (headless; run relevant ones after every change; ALL 14 before a
  ship): `-run=<Name>` with UnrealEditor-Cmd; names in
  `Source/SibeliusGameEditor/*Commandlet.h`. Office saves require
  Refactor+Branch minimum.
- **L_Office_v02 rule:** only ever saved by a healthy interactive editor;
  never via the editor's save-on-close prompt (twice corrupted
  URefactorableComponents). Recovery: `git checkout -- <umap>` + gate re-run.
- **Live-editor bridge:** `Tools/ue_bridge/ue_bridge.py` (editor OPEN only).
  `unreal.Rotator()` is POSITIONAL (roll, pitch, yaw) — **always kwargs**.
  Viewport-camera reads/writes may target a viewport that is NOT what Walt
  sees — don't trust them for placement.
- **Placement lesson (learned the hard way 2026-07-14):** never remote-place
  actors in dense unfamiliar levels. Anchor to a trusted existing actor, or
  hand Walt the mouse (Move Object to Camera + End-to-floor works well).
  Prototype big flow changes cheaply and PLAYTEST before wiring them in — the
  cathedral-finale relocation was reverted (`738b286`) after an afternoon.
- **PowerShell + this file:** PowerShell `Get-Content`/`-replace` mangles the
  em-dashes and emoji in these docs (mojibake). Edit docs with the Write/Edit
  tools, never shell regex.
- **Release runbook:** `docs/sib-42-packaging-notes.md` §Release; mirror
  scripts `Tools/Scripts/package_v0*.ps1`; butler push to
  `leonardsibelius/leonard-sibelius:windows --userversion X.Y.Z`. Full 14-gate
  sweep before every ship. **Only cooked maps ship** (`MapsToCook` in
  `Config/DefaultGame.ini`).
- **Ship check ("the banking question"):** work isn't done until
  `git status -sb` shows no ahead/behind on BOTH repos and the itch build is
  verified. Walt will ask.
