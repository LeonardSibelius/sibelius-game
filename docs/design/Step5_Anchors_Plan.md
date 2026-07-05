# Build Order #5 — Anchors on Terrain (PLAN, banked for next session)

Project: Leonard Sibelius (UE 5.7) · Level: L_Elsewhere_Dev · Branch: feat/forest-elsewhere
Status: planned, not started. Follows completed #4 (lighting preset per recipe).

---

## Core concept

**Anchors are an authored fixed rig — the world's stage marks.** The *framing* (where the
door is, what the arrival view composes around) is hand-authored and stays put across every
generation, exactly like the spline regions. The seed/recipe decides only *what fills each
mark* — which hero, which Shinbi pose, which surprise. This directly serves the art rules:

- #3 the door frames a postcard — arrival view always composed (framing = authored)
- #4 exactly one hero per world
- #6 Shinbi is the figure in the landscape — sightline, mid-distance
- #7 surprise = the wrong thing in the right place, integrated into the ground
- #8 small palette, big repetition (same marks, different content)

One actor, **BP_WorldAnchors**, holds named **Scene Components** as sockets. It lives in
L_Elsewhere_Dev and persists across generations (like the splines). The Conductor reads its
sockets at generation time and places content on them.

## Anchor set (Scene Components on BP_WorldAnchors)

| Socket | Purpose |
|---|---|
| Door | Player arrival transform. Its **forward vector = the sightline.** |
| LookTarget | (optional) the point the postcard composes around; hero sits at/near it. |
| Hero | The one focal object (rule #4), on the sightline, framed by the door. |
| Shinbi_A / Shinbi_B / Shinbi_C | Three figures, mid-distance, on/near the sightline (rule #6). |
| Surprise | The surprise object, integrated into the ground (rule #7). |

Keeping them as sockets on one actor means you can nudge the whole rig as a unit and the
Conductor has one clean thing to read.

## Recipe additions (PDA_WorldRecipe)

- **HeroMesh** (Static Mesh ref) *or* **HeroPoolTag** — which hero to place. (Worksheet Block B
  item 4 is still "TBD"; this is where it lands. Candidate already spotted:
  `SM_Poplar_01_Field_Trunk`, the open-grown field poplar.)
- `ShinbiPoseTag` and `SurprisePoolTag` already exist as stub fields — used in #6/#7.

## Step 5 scope (this milestone only)

1. Create **BP_WorldAnchors** (Actor) with the Scene Components above.
2. Place it in L_Elsewhere_Dev. **Hand-compose Door + LookTarget + Hero** to frame a postcard,
   using the current world as reference — this is the authored arrival, take time on it.
3. Add **HeroMesh** to the recipe; set Recipe #1's hero.
4. Wire the Conductor: on generation, **spawn/position the hero mesh at the Hero socket**,
   reading `Recipe.HeroMesh`. **Clear the previous hero first** (same discipline as the biome
   cleanup — avoid duplicate heroes stacking up each run).
5. Verify: pilot the camera to the Door socket, look along its forward — hero should sit framed
   and composed. Re-roll a couple of seeds: the hero stays framed while the flora reshuffles
   around it.

## Deferred to #6 / #7 (noted so we design compatibly now)

- **#6 Shinbi ×3** at Shinbi_A/B/C, using `ShinbiPoseTag`.
- **#7 Surprise** at Surprise socket, using `SurprisePoolTag`, integrated into ground.
- **Clearing carve:** foliage will try to grow over the hero/door. Need a clearing mask
  (PCG exclusion volume or similar) around the anchors, sized by `ClearingRadiusMin/Max`
  from the recipe. Flag for the #7 integration pass.

## Open design questions to decide at the start of next session

1. **Hero source:** single mesh ref, or a pool the seed picks one from? (Rule #4 = exactly one
   hero per world — but it *could* vary between worlds.)
2. **Anchor positions:** fixed authored transforms (recommended — the postcard is authored), or
   seed-jittered within a radius for variety? Start fixed.
3. **Door ↔ PlayerStart:** for now a marker socket; later tie to the real arrival/portal.
4. **Clearing mask mechanism:** PCG exclusion vs. manual clearing volume vs. runtime foliage mask.

## Risks / lessons carried from #4

- **Cross-level reference gotcha:** if any anchor or Conductor variable ends up pointing at an
  actor in a *sublevel* (the way SunLight/HeightFog pointed into EB_LightingDaytime), mark that
  variable **Transient** or the persistent level won't save. Keep BP_WorldAnchors and the hero
  in L_Elsewhere_Dev itself to avoid this.
- **Clean up spawned content each run** (like we did for orphan PCGStampChild actors) so heroes
  don't accumulate.
- **Hero must sit on landscape height:** hand-set Z (terrain is fixed) or line-trace down onto
  the landscape when placing.

## First actions next session

1. Decide the four open questions above (5 min).
2. Create BP_WorldAnchors + sockets.
3. Hand-place the rig and compose the arrival postcard.
4. Add HeroMesh to the recipe, pick Recipe #1's hero, wire the Conductor to place it.
