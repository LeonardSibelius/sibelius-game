# L_Office_v2_QuadArt — Polish Backlog

*Deferred visual/structural refinements for the Chapter 1 office, logged during the Phase 1a
shell build (2026-05-27). None of these block the v0.1 MVP greybox-replacement; they are the
v0.7–0.9 polish pass once the mechanic and layout are validated.*

*Companion to `docs/quadart-install-paths.md` (license/cook policy) and the CP3 plan.*

---

## 1. Lofty 443 cm ceiling — anchor human scale (v0.7–0.9) — PRIMARY

The QuadArt kit's only walls that carry a real exterior window are the **443 cm (4.4 m)** exterior
facade walls. We accepted that height (locked decision, 2026-05-27) to keep the real north window +
golden-hour beam, rather than scaling walls or faking the window. A 4.4 m ceiling reads taller than
an "ordinary home office," so the polish pass re-anchors human scale **without** lowering the walls:

- **Exposed ceiling beams at ~2.5 m** — a horizontal beam grid (or faux structural timbers) that
  visually caps the room at human height while the real wall/window continues above. Reads as a
  converted-loft / high-ceiling study, which is plausible-ordinary.
- **Pendant light over the desk** at ~1.5–1.8 m — pulls the eye down to desk height, defines the work zone.
- **Mid-height bookshelves (~2 m)** along the south wall anchoring the lower band of the room.
- Walt's note: the slight wrongness of the tall proportions in Chapter 1 arguably *serves* the
  "start ordinary, become cosmic" arc — so this is a tasteful anchor, not a full correction.

Optional alternative considered and deferred: a **dropped ceiling plane at ~2.7 m**. Rejected for now
because the window aperture's vertical position in the 443 wall is unverified — a low dropped ceiling
could clip the window. Revisit only if the beam approach doesn't read well.

## 2. Corner pieces (v0.7 cleanup)

Phase 1a assembles the four walls with a **measure-and-snap overlap at corners** (each wall's inner
face meets at the corner line; a small ~17 cm notch exists in the exterior wall-thickness zone only,
not visible or passable from inside). The kit ships dedicated `SM_Corner_1Floor_A` (148×148) corner
pieces for clean exterior corners. Swap the overlap for proper corner pieces when the exterior of the
room becomes visible (e.g. if Chapter 2 steps outside). Interior enclosure is already sound.

## 3. Real openable entry door (Checkpoint 4)

Phase 1a places a **closed door leaf** (`SM_Door_Outside_A1`) against the solid east wall as a visual
entry marker — there is no actual opening. The kit's door-frame piece (`SM_Frame_A1_200`) is **634 cm
tall** and would punch through the 443 ceiling, so a real framed opening was deferred. CP4 (door
interaction) will create the opening + our own thin interaction Blueprint (vendor BPs never ship).

## 4. Desert-mountain backdrop through the window (v0.7–0.9)

Neither QuadArt pack contains a desert/landscape skybox (per `sibelius-game-asset-shortlist.md`, this
is a separate "stretch" purchase). Phase 1a uses the default `SkyAtmosphere` placeholder. Acquire a
desert-mountain skybox / distant-terrain backdrop and place it beyond the north window for Lennie-video
continuity before the office ships.

## 5. Window glass material (v0.7)

Phase 1a uses a thin engine-cube glass plane (`Window_Glass`, tagged `CodeVision.Translucent`) inset
1.5 cm behind the aperture as the CP3 translucency test target. Replace with a proper glass material
(and confirm it reads correctly under the Code Vision post-process) during polish.

## 6. Lighting tune (v0.7)

Sun reuses CP2 values (intensity 6.0, 4200 K, pitch −10°, yaw −100° for a side-rake on the south
corkboard wall). Re-tune intensity/temperature and the rake angle once furniture + corkboard are in,
and profile `stat gpu` with hardware Lumen on the RTX 5070 Ti.
