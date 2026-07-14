# APPEAL_PLAN â€” making Sibelius reach more people

*Written 2026-07-14 by Claude (Fable 5) after the v0.7.1 ship, at Walt's request.
This is the handoff copy for future sessions (any model â€” Walt may paste this to
Opus when his Fable budget runs out). The six-point consult below is preserved
verbatim from the original conversation; the STATUS table tracks execution.*

---

## The framing (don't lose this)

Stop calling it silly. The games that get remembered on itch.io aren't the
polished ones â€” they're the **personal** ones. Sibelius has a truth no studio
can fake: it's an autobiographical game about a man merging with an AI,
**actually built by that man working with an AI**, containing a **real slot
machine designed by a real slot-machine designer**. That's not silly; that's
the most honest game premise around. The goal isn't to sand the weirdness
off â€” it's to remove everything that stops strangers from *reaching* the
weirdness.

## The six points, ordered by what moves the needle most

1. **The 9.7 GB wall** (the biggest one, and it's measurable). Nobody
   downloads 9.7 GB from an unknown free game â€” they close the tab. Most of
   that weight is cooked content the game barely uses: eight forest variants
   that are the same road in different costumes, plus Fab packs shipping
   wholesale. Cut to the best forests and audit what's actually cooked.

2. **The first five minutes.** Walt got lost in his own game more than once â€”
   the cauldron, the carousel, the locked powers â€” and he had the developer on
   call. A stranger has nobody. The fix isn't more journal text; it's one
   guided beat: wake up, one glowing objective ("find the sphere in the
   attic"), one power earned, one demon slapped inside ten minutes. Hook them
   with the slap before asking them to understand the lore.

3. **Tell the true story on the store page.** The pitch that markets itself
   isn't "narrative metroidvania" â€” it's *"I'm 71. I designed slot machines
   for a living. I built my memoir as a video game with an AI, and my real
   slot machine is hidden at the end of it."* Put that paragraph on the itch
   page, post it as a devlog, and let the Celestial Fortune screenshot carry
   it. People share stories, not features.

4. **Shareable ten seconds.** A GIF of a Gideon getting slapped into the
   treeline is worth a thousand words of description. Two or three GIFs on
   the page â€” slap, carousel spin, slot win â€” do the work of a trailer for
   free.

5. **A reason to come back.** The sauce economy gives sessions a spine, but
   there's no scoreboard. Cheap additions: a lifetime stats page in the M menu
   (demons slapped, sauce earned, carousel best run), and maybe a "daily
   carousel" with a fixed stake and bragging rights. Slot-floor wisdom:
   people return for streaks and near-misses, not content.

6. **Juice the slap.** It's the signature verb â€” a meatier sound, a tiny
   screen shake, a sauce-number popping off the corpse. An afternoon of work
   that makes the whole game feel 20% better.

## STATUS (2026-07-14)

| # | Item | Status |
|---|------|--------|
| 1 | Download diet | âœ… **SHIPPED as v0.7.2 (2026-07-14)** â€” forests 8â†’4 (01/03/06/08; Poplar uncooked), 2K texture cap, dragons + stained glass deleted. **Real numbers: cook 18.6â†’12.1 GB, archive 10.6â†’8.1 GB (~7.4 GB download, âˆ’24%).** The cap halved characters/props; the holdout is **EasyBiomes (5.98â†’5.65 GB â€” foliage MESHES, textures already small; 4 forests = 4 biomes so the pack ships nearly whole).** **Queued follow-up ("two-forest experiment"):** keep 2 forests sharing a biome family, move the 2 orphaned curios into them (anchor placements to the existing curio actors), est. download â‰ˆ5 GB. |
| 2 | First five minutes | âŒ not started |
| 3 | Store-page story | âŒ not started (pure writing; Walt approves the words) |
| 4 | GIFs | âŒ not started (capture in PIE or packaged build) |
| 5 | Stats / daily carousel | âŒ not started (stats fields â†’ own SaveGame slot or ProgressionState additive fields) |
| 6 | Slap juice | âŒ not started (`USlapComponent::DoSlap` is the one code point) |

Small extra spotted by Walt: **the forest levels have no indicator of which
forest you're in** â€” a small HUD line (SibeliusHUD, level-name â†’ friendly name)
would help. Cheap, unclaimed.

## Handoff notes for the next session (read before working)

- **Read `docs/NEXT_SESSION_PROMPT.md`-style intro in this repo? No â€” this
  project's working agreement lives in the sibling GhostCam repo habitually;
  the essentials:** Walt is 71, wants small numbered steps, pastes screenshots
  back. Never `git add -A` (stage exact files). Build (editor CLOSED for
  reflection changes):
  `& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" SibeliusGameEditor Win64 Development -project="C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -waitmutex`
- **Gates** (headless, run relevant ones after every change; ALL after office
  saves): `-run=<Name>` with UnrealEditor-Cmd; names in
  `Source/SibeliusGameEditor/*Commandlet.h`. 14 gates; office saves require
  Refactor+Branch minimum.
- **L_Office_v02 rule:** only ever saved by a healthy interactive editor;
  never via the editor's save-on-close prompt (twice corrupted
  URefactorableComponents). Recovery: `git checkout -- <umap>` + gate re-run.
- **Live-editor bridge:** `Tools/ue_bridge/ue_bridge.py` (editor OPEN only).
  `unreal.Rotator()` is POSITIONAL (roll, pitch, yaw) â€” **always kwargs**.
  Viewport-camera reads/writes may target a viewport that is NOT what Walt
  sees â€” don't trust them for placement.
- **Placement lesson (learned the hard way 2026-07-14):** never remote-place
  actors in dense unfamiliar levels. Anchor to a trusted existing actor, or
  hand Walt the mouse (Move Object to Camera + End-to-floor works well).
  Prototype big flow changes cheaply and PLAYTEST before wiring them in â€” the
  cathedral-finale relocation was reverted (`738b286`) after an afternoon.
- **Release runbook:** `docs/sib-42-packaging-notes.md` Â§Release; mirror
  scripts `Tools/Scripts/package_v0*.ps1`; butler push to
  `leonardsibelius/leonard-sibelius:windows --userversion X.Y.Z`. Full 14-gate
  sweep before every ship. **Only cooked maps ship** (`MapsToCook` in
  `Config/DefaultGame.ini`).
- **Ship check ("the banking question"):** work isn't done until
  `git status -sb` shows no ahead/behind on BOTH repos and the itch build is
  verified. Walt will ask.
