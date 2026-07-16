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
| 5 | Stats / daily carousel | 🟡 **Stats page DONE (`f0927b7`)** — RECORDS tab in the M menu: slaps, lifetime sauce, books, curios, chapters, carousel runs/wins/best round/biggest spin. `FProgressionState.LifetimeStats` map + `SibeliusStats::` keys; new stat = one FName + one bump line. Daily carousel (fixed-seed daily run) still open. |
| 6 | Slap juice | 🟡 **STARTED (`e954d02`)** — slapped Gideon now plays his Paragon `Death_Back` anim mid-flight and holds the collapsed pose (skeleton-checked; freeze fallback keeps the taffy fix safe). **Still to do at the same code point** (`USlapComponent::DoSlap`): death voice line (pick from `Gideon_Death_010`–`_050`), `P_Death_Gideon` particle, `MF_DeathFade` corpse dissolve, meatier sound, camera shake (needs a `UCameraShakeBase` subclass), sauce-number popup. All assets already on disk — zero download. |

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

## Milestone: v0.7.3 "worth coming back" (2026-07-14 evening)

Walt: *"the game feels more complete now."* Shipped in one evening session,
each item driven by his live playtest questions:

- **RECORDS tab** (point 5, first half) — lifetime stats in the M menu.
- **Curio treetop beacons** — Walt wandered whole forests without seeing a
  curio; every curio now flies a 60 m mood-tinted light pillar. Findable.
- **Temple blend wired** — the AI temple's book rain now actually fills the
  cauldron (FeedPerBook 0.01, was 0/spectacle-only); completing the Sauce of
  All Knowledge pays a one-time +100 bounty with an emerald ceremony banner.
  The unreachable June stub is now the temple's reason to visit.
- **Carousel room comprehension pass** — HUD 1.8x + phase explainers, black
  marble machine, library piano muted (see the earlier milestone).

Remaining on the plan: daily carousel (5b), slap juice (6), Walt's itch-page
words (3) and GIFs (4).

## Milestone: v0.7.4 "slap dignity" (2026-07-15)

Point 6's first slice shipped same-session: a slapped Gideon collapses in
place playing his Paragon `Death_Back` (no launch; skeleton-checked with the
freeze-and-launch as fallback, so the taffy fix stays protected). Walt called
it good enough to justify a version. Full 14-gate sweep green.

## Milestone: the Wild Refactor + the Menagerie (2026-07-16, unshipped)

Walt's ask ("point at the desk, press R, turn the desk into an animal — same
for Gideon") became the game's funniest system, built and debugged live:

- **R transmutes almost anything** into a random creature from the Menagerie;
  R again restores the original perfectly (hide-and-spawn, never modify).
  Refusers too: Gideon hides behind the zebra and comes back re-possessed.
- **The zoo (16)**: 12 skeletal animals (AfricanAnimalsPack + AnimalVarietyPack,
  kept; tusk part-meshes excluded), rooster/pig/rabbit statues (pruned from the
  2.6 GB farm pack into /Game/FarmKeepers, rest deleted), toy rabbit mascot.
  Auto-scanned from MenagerieFolders; folders in DirectoriesToAlwaysCook.
- **Animals are slappable** (APPEAL-6c): skeletals ragdoll on pack physics
  assets, statues launch rigid. No sauce (R-then-slap would be an infinite
  farm). Creatures never despawn and always answer the R-trace.
- Guard rails: interactables/architecture-by-name/>3.5 m/people never
  transmute; the R-ray hops past invisible interaction boxes.
- **NOT yet shipped to itch** — goes out in v0.7.6 (adds ~0.5 GB of animals;
  weigh against the diet before pushing).

## Shipped: v0.7.7 (2026-07-16, build #1801360)

The temple sauce fountain (Walt's E-pour/C-compile/quiet-recharge ritual,
+40 per bowl, Gideons crash every pour), temple navmesh + resident spawner,
and Greystone as the player's mirror body. 496 MB patch.

## Parked: real mirrors (2026-07-16)

The player is now **Paragon Greystone** (BP_FirstPersonCharacter Mesh + his
AnimBlueprint — Walt's pick over the pale mannequin). But the attic mirror
smears characters into mud: Lumen reflections have no lighting cache for
skinned meshes. A PlanarReflection actor was tried (r.AllowGlobalClipPlane
is now ON in DefaultEngine.ini — the shader recompile is already paid) but
never engaged visibly; deleted, PARKED by Walt ("more trouble than it is
worth"). If revived: likely needs hit-lighting-for-reflections (HW ray
tracing) or a SceneCapture2D mirror material. Walt's carrot: "if the player
is attractive enough, I will put more mirrors in the game."

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
