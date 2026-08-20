# Forests — purchased, parked, do not use

**Walt, 2026-08-19.** Many Worlds is out of the game. Do not put it back.

## What happened

Runtime procedurally generated forests were the plan. They do not work in a
cooked UE 5.7 build with the EasyBiomes PCG kit (generation does not schedule;
see `docs/design/Recipe_01_ModeB_Poplar_Forest.md`). Baking a deck of forests
instead made the download climb past 10 GB. Walt cut the feature (2026-07-16).
The kitchen door is not a world-deck anymore.

## Do not delete these

Walt paid for the forest kit. The bytes stay on disk. Do **not** `git rm`,
Fab-uninstall, or "clean up" the following:

| What | Where |
|---|---|
| EasyBiomes Broadleaf Poplar Forest (Fab, paid) | `Content/EasyBiomes/` (gitignored) |
| Paragon Shinbi (forest watchers) | `Content/ParagonShinbi/` (gitignored) |
| Forest workbench / leftover maps | `Content/Maps/L_Elsewhere_Dev.umap`, `L_Elsewhere.umap` |
| Homemade forest ground materials | `Content/Forest/` |
| Any leftover `L_Forest_*` / `L_Poplar_*` levels | if they reappear on disk |

## Do not use them

- Do not wire the Sauce Door (or any door) to a forest level.
- Do not add `/Game/EasyBiomes` to `DirectoriesToAlwaysCook`.
- Do not open the forests in PIE as if they were part of the game.
- Do not bake more forest worlds "just one more card."
- Do not revive Many Worlds, the shuffled deck, or runtime PCG forests in
  README, itch copy, or a "while we're here" session.

Code that still mentions forests (`ASauceDoor` deck, `ElsewhereSmokeTest`,
PCG builder) is leftover. Leave it unless it ships content. The
`ElsewhereSmokeTest` already notes the deck is gone and only guards return-to-office.

## If you are an agent

The player-facing game is the office, the attic, the cathedral, the machines.
Forests are a paid archive. Ask Walt before touching any of the paths above.
