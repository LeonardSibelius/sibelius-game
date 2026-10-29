# THE GIST — an instant, silly Generate (plan, 2026-10-29)

Walt: "Could an entertaining generator be coded for the game that is quick but silly? It would act
like a placeholder for later when everybody is using AI and Steam has to relent." Tripo-style AI
generation takes 10-60 s a model and is live-generated AI content, which the pending Steam chat
review makes awkward. This plan is the opposite: instant, deterministic, no network, no model.

Status: **phase 1 done (2026-10-29)** — the rules, the word lists and their tests. Phases 2-5 to go.

## Phase 1 — what exists

- `Source/SibeliusGame/GistRules.h/.cpp`: engine-free, deterministic. `Build(Vocab, Prompt)` returns
  an `FRecipe`: parts (shape, finish Clay/Gold/Glass, role Body/Eye/Pupil/Feature, colour,
  location, rotation, size in cm), behaviour flags, copies, scale, `bGiant`, and the plaque's
  title / echo / tagline. 40 archetypes, 33 modifiers, 9 quirks, contraption fallback, counts and
  plurals, "made of", "with"/"and"/"wearing", hybrids, the 64-part / 12-copy / 3 m (6 m giant) caps.
- `Content/Data/GistNouns.csv` (238 words) and `GistModifiers.csv` (151 words): staged with the game
  like the catalog CSV. `MakeVocab` parses them and Mrs. Hall's blocklist.
- `Tools/Tests/gist_test.cpp` + `run_gist_tests.ps1`: /W4 /WX, reads the real CSVs. Word lists vs
  code, determinism, every noun, every modifier word, every modifier on every archetype, joins,
  counts, contraptions, quirks, invisible, plaque cap, blocklist (alone, inside a request,
  plural), 5000 fuzzed requests — all pass.
- The blocklist bit twice while testing: "knife" and "fire" are Mrs. Hall's words, so they are not
  gist words (fiery keeps "burning", "flaming", "blazing"...). The builder refuses them anyway.
- The editor target builds with it. UBT compiled GistRules.cpp outside the unity file (adaptive
  build, it is new); a full or packaged build will compile it inside one — watch that in phase 2.

## 1. What is wrong with G today

G opens the request panel; `UGenerateComponent` sends the text to the pure matcher
(`ClassifyGenerateRequest`, GenerateMatcher.h), which scores it against a closed catalog,
`Data/GenerateCatalog.csv` — six rows: tree, plant, lamp, crate, chair, key. Anything else is
`RefusedNoMatch`, and Mrs. Hall says "We don't keep that here, Programmer." So nearly every wish a
player types is refused. The feature that should be the most magical one is mostly a "no".

## 2. The idea in one paragraph

Keep the catalog exactly as it is. When the matcher says **no match**, instead of refusing, the
supply room builds *the gist* of the request out of whatever parts are lying around: a toy-like
assembly of simple shapes, coloured, sized and animated by the words the player typed, with
googly eyes and a brass plaque. "Giant flying toaster" gives a box three times normal size with two
slots and little wings, bobbing in the air, plaque *"Model GFT-4410 — giant flying toaster.
Assembly not required."* Mrs. Hall, grudgingly: "Fine. This is what we had in the back."

The same words always build the same thing, so it feels as if the machine understood.

## 3. Player experience

- Instant: the parts are computed in microseconds and appear through G's existing build reveal
  (ABuildSite's reveal phases), so it looks made, not popped in.
- Readable silliness: chunky primitives in a matte toy palette, googly eyes whose pupils turn
  toward the player, a plaque on a small plinth with the player's own words (capped at 40 chars).
- Mrs. Hall lines for the gist outcome (new rows in Data/MrsHallLines.csv, reason `Gist`), text
  first; recorded clips later only if Walt wants them (same pre-generated voice disclosure as
  her existing lines).
- Costs 1 from the existing per-session Generate budget (10), like a catalog item.
- Branch / Test-Drive / Deploy keep working because the result is still an ABuildSite (§6).

## 4. The rules (pure, tested — `GistRules.h`)

Engine-free C++ like DragonsWagerRules.h / LanternBurstSim: input = normalized tokens (reuse the
matcher's tokenizer), output = a **recipe**: up to 64 parts `{shape, colour, local transform}` plus
behaviours (hover, spin, bob, wobble, dance) and a plaque name. No `FMath::Rand`: a small PRNG
seeded by an FNV-1a hash of the normalized prompt, so tests and save/reload are exact.

**Nouns (~40), each a tiny hand-built recipe of 4-12 parts.** Starter set: cat, dog, bird, fish,
dragon, horse, duck, snake, robot, person, car, boat, rocket, train, plane, toaster, cake, pizza,
tree, house, castle, tower, bridge, chair, table, bed, lamp, hat, crown, sword, guitar, piano,
cup, teapot, clock, phone, computer, moon, star, mountain. Synonyms map onto these in data
(`Data/GistNouns.csv`: word -> archetype), so "kitten", "kitty" and "cat" share one recipe.

**Modifiers (~30), applied in order to the recipe:**
- size: giant / huge / enormous (x2.5, capped), big (x1.6), small (x0.6), tiny / mini (x0.35)
- material/colour: golden (the wager's gold), glass (lantern glass), crystal, red/blue/green/...
  (palette), rainbow (per-part hues), invisible (only the eyes and plaque remain), wooden, stone
- motion: flying / floating (hover + bob), spinning, dancing (hop + turn), bouncy, wobbly, sleepy
  (slow sway, closed-eye lids)
- mood: angry (red tint + slanted brows), happy / sad (mouth arc), scary (extra eyes)
- count: two ... ten, many, army, herd (copies in a ring, capped at 12)
- joins: "X made of Y" (Y's colour/material onto X's shape), "X with Y" (Y's signature part
  attached on top — "cat with a hat"), two bare nouns "cat car" (body of one, head of the other)

**Unknown words still build.** No known noun -> a seeded *contraption*: a stack of 3-7 random
primitives in palette colours, sometimes a tiny hat, always eyes; plaque *"Interpretation 7 of ∞"*.
"My mother-in-law" gets a wobbling tower of spheres in a hat.

**Safety.** The existing Mrs. Hall blocklist runs first and still refuses (`RefusedUnsafe`); a
blocked word never reaches the builder and never appears on a plaque. Everything the gist can
draw is a coloured primitive, so the only player text in the world is the capped plaque echo.

**Limits.** At most 64 parts and 12 copies; every result fits a 3 m cube (6 m when giant), sits
on the floor at the trace point, gets one simple box collision for placement and the camera.

## 5. Parts and assets

All cheap, all already cooked or engine content:
- Engine BasicShapes: cube, sphere, cylinder, cone (+ squashed/stretched variants by transform).
- One new material, `/Game/Gist/M_GistClay`: matte, per-instance colour from custom data 0-2
  (the M_WagerGem pattern, roughness ~0.65, metallic 0), made by a `make_gist_assets.py` that only
  creates missing assets. Eyes: white and black spheres in the same material.
- Reused looks: `/Game/DragonsWager/M_WagerGold` for "golden", `/Game/LanternBurst/M_LanternOrb`
  for "glass"/"glowing".
- Optional signature props later: catalog meshes (lamp, crate, chair, key), the wager's crown,
  GemShader gems.
- Sounds: TableSounds chimes for the reveal; nothing new needed.

The package script gets a check for M_GistClay (named only by the new class, so a missing asset
would draw grey checkerboard toys).

## 6. Fitting into Generate

1. `UGenerateComponent::SubmitRequest`: unchanged up to the matcher. `Resolved` -> catalog spawn as
   now. `RefusedUnsafe` and `RefusedOverBudget` -> refusals as now. `RefusedNoMatch` (and, if Walt
   agrees, `RefusedAmbiguous`) -> **gist**: charge 1 budget, build the recipe, spawn.
2. `AGistSite : ABuildSite` (the ASpaceport precedent shows a class-spawning generated site):
   one ISM per primitive shape in M_GistClay, the plaque, eye components; behaviours in Tick;
   `MarkGenerated("gist")`; its reveal plays like any generated site.
3. Save/reload: the deployed-site record (BranchTypes.h, `bGenerated` / `GenerateEntryId` /
   `GenerateTransform`) gains `FString GistPrompt`. Re-spawn on load sees EntryId `gist`, spawns an
   AGistSite at the saved transform and rebuilds the recipe from the prompt — deterministic, so
   it comes back identical. Old saves are unaffected (empty prompt, catalog ids only).
4. The Generate smoke test checks the pure matcher only, which does not change, so it stays green.
   A new pure test covers the gist rules; a short editor check covers spawn + save + reload.

## 7. The seam for real AI later

Put the choice behind one interface: a request goes in, a result comes out — a catalog entry, a
gist recipe, or (later) an external mesh. When Steam relents, a Tripo-style backend plugs in as a
third result type. The gist does not have to go away: it becomes the **instant placeholder** while
the real model cooks for 10-60 s, then the real one replaces it with the same reveal. "Silly now,
real later" without changing how G feels.

## 8. Steam and itch

No model, no network, deterministic code: nothing here is AI-generated content, so the Steam
disclosures and the pending chat review are untouched. Ships to itch and (once chat is accepted)
Steam like any other update.

## 9. Work plan

| Phase | What | Done when |
|---|---|---|
| 1 | `GistRules.h` + `Data/GistNouns.csv` + `Data/GistModifiers.csv`; 40 nouns, 30 modifiers, contraption fallback, plaque names | pure tests pass at /W4 /WX: determinism, every vocab word builds, part/copy/size caps, unknown words build, blocklist never reaches the builder, plaque cap |
| 2 | `AGistSite`, M_GistClay via `make_gist_assets.py`, SubmitRequest routing, Mrs. Hall gist lines | in PIE: ten typed wishes build instantly; budget charged; refusals unchanged for blocked words |
| 3 | Save/reload (`GistPrompt`), Branch/Test-Drive/Deploy | a gist survives save, quit, load identically; discard on a failed branch removes it |
| 4 | Polish: pupils track the player, wobble/bob tuning, chimes, plaque styling; screenshot pass | Walt plays it and laughs at least once |
| 5 | Package check for M_GistClay, release as the next 2.4.x | cook verified, itch push |

Estimate: phases 1-2 one session, 3-4 a second, 5 is the usual release routine.

## 10. Decisions (Walt, 2026-10-29: "Yes, go with yours")

1. Voice: **Mrs. Hall, grudging** — "Fine. This is what we had in the back."
2. **Ambiguous requests become gists too** (a hybrid of the tied entries, e.g. "chair lamp"),
   instead of the RefusedAmbiguous line.
3. **Keep the 10-per-session budget**; a gist costs 1, like a catalog item.
4. Name on the plaque: **THE GIST**.

Next: phase 1 in a new session.
