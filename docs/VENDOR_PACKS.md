# Vendor packs — what a fresh clone does NOT get

## Sibelius Hotel — added September 7, 2026

**Elven architecture**, DmitriyDryzhak: https://www.fab.com/listings/1cb2369a-f53b-43fe-9c67-14790e3a4e15 . Install under `Content/ElvenArchitecture`, then run `Tools/Scripts/make_hotel_tile.py` to create `Content/Maps/L_SibeliusHotel_Generated.umap`. Both are Git-ignored. This architecture-only tile is revealed inside Grok by G and is included in packaging. See `docs/SIBELIUS_HOTEL.md`.

`git clone` gives you a repo that **builds and runs, but is missing about 33 GB
of purchased and downloaded content**. That content is git-ignored on purpose:
it is licensed to the Epic account that bought it, this repo is public, and the
bytes would blow through Git LFS billing besides.

This file is the record of what has to come back, and — more importantly —
**which of it the C++ silently depends on**.

## The trap this file exists to prevent

Several systems reach into ignored folders by **hard-coded asset path from C++**.
When the folder is missing, `FObjectFinder` / `LoadObject` simply returns null.
There is no build error, no cook error, and usually no runtime error — the
feature just quietly does nothing:

| If this is missing | What silently stops working |
|---|---|
| `ParagonGideon` | Refusers stop swinging (`RefuserController`) and stop collapsing when slapped (`SlapComponent`) |
| `EchoContent` | The Presence has no mesh, hair, or idle (`Presence.cpp`) |
| `MagicianLabatory` | The sauce pot mesh (`SauceBowl.cpp`) |
| `StainedGlass3D` | Carousel floor material (`CarouselMachine.cpp`) |
| `ModularSciFiEnv_K`, `SciFiBoxes_A`, `SciFi_Box_B` | The Elsewhere builds an empty room (`ElsewhereBuilder`, `ElsewhereGen`, `Curio`, `BuildSite`) |
| `AfricanAnimalsPack`, `AnimalVarietyPack`, `FarmKeepers` | Refactor's Menagerie has nothing to swap in (`RefactorComponent.h`) |
| `Fab`, `Paid`, `MorroMotion` | The dancers (see `Content/MetaHumans/README.md`) |

**Symptom to recognise:** something that used to work does nothing at all, with a
clean log. Check this list before debugging the code.

## Everything ignored, largest first

Re-downloadable from the **Fab library of the Epic account that bought them**
unless noted. Sizes are what was on disk on 2 Aug 2026.

| Folder | Size | Referenced from C++? |
|---|---|---|
| `MagicianLabatory` | 6.9 GB | yes — `SauceBowl.cpp` |
| `ModularHouses` | 3.9 GB | no (level dressing) |
| `HouseFurniture` | 3.8 GB | no (level dressing) |
| `ParagonGideon` | 2.8 GB | **yes — `SlapComponent.cpp`, `RefuserController.cpp`** |
| `UltimateGothicCathedralChurch` | 2.6 GB | no (level dressing) |
| `ParagonGreystone` | 2.2 GB | soft paths only (`BattleFormComponent.cpp`, `SlapComponent.cpp`), so his hero folder is in `DirectoriesToAlwaysCook` — minus his Novaborn, Tough and WhiteTiger skins, in `DirectoriesToNeverCook` (226 MB nobody sees; Dragonlord stays for his eyes) |
| `Vehicles` | 1.5 GB | no |
| `EchoContent` | 1.3 GB | **yes — `Presence.cpp`** |
| `StainedGlass3D` | 1.1 GB | yes — `CarouselMachine.cpp` |
| `Fab` | 744 MB | no, but holds the `MHC_*` source characters |
| `Phantom_All_In_One` | 657 MB | no — but `Content/Phantom/Manny_Glow` inherits from it, and the AI ghosts in L_City are invisible without it |
| `SciFi_Box_B` | 676 MB | yes — `ElsewhereBuilder.cpp` |
| `ModularSciFiEnv_F` | 669 MB | no |
| `Dragon_Rise` | 647 MB | no |
| `AfricanAnimalsPack` | 615 MB | yes — `RefactorComponent.h` |
| `Helicopter` | 585 MB | no |
| `AnimalVarietyPack` | 494 MB | yes — `RefactorComponent.h` |
| `ModularSciFiEnv_K` | 476 MB | yes — 4 files |
| `MorroMotion` | 442 MB | no — dance mocap, see MetaHumans README |
| `SciFiBoxes_A` | 378 MB | yes — `ElsewhereBuilder.cpp` |
| `ModularSciFiEnv_J` | 305 MB | no |
| `FarmKeepers` | 267 MB | yes — `RefactorComponent.h` |
| `Paid` | 178 MB | no — the Aput outfit |
| `ModularSciFiEnv_1` | 168 MB | no — Isla's first home, `L_IslaHome` (retired 2026-10-24) |
| `Fafnir_Keep_Meshingun` | 2.6 GB | no — Isla's home since 2026-10-24: `L_IslaKeep` is its `LV_Main_Night_Light` saved as a new map (`make_virtual_home.py`). World Partition, so the map's actors are in `__ExternalActors__/Maps/L_IslaKeep/`, gitignored with it. The pack also brings `__ExternalActors__/CGMA/` (the seller's leftovers, no level), ignored too |
| `XenoMocap` | 48 MB | no — free pack, UE4 skeleton, unused |
| `Audio` | 5 MB | yes — `GenerateComponent.cpp` (`/Game/Audio/MrsHall/`) |
| `AncientTreasures` | 914 MB | **yes — `RefactorComponent.cpp`**, its two chalices only (Dekogon, "Allows usage with AI: No"). Re-run `Tools/Scripts/prepare_hoard.py` after a re-download: the meshes ship with no simple collision |
| `GemShader` | 15 MB | **yes — `RefactorComponent.cpp`**, the gems and their six colours (added 2026-10-13) |
| `EasyBiomes` | large (Nanite foliage) | **NO — parked. Paid kit. Do not delete. Do not cook. Do not use.** See `docs/FORESTS_DO_NOT_USE.md` |
| `ParagonShinbi` | 918 MB (Fab, free) | **yes — `RefactorComponent.cpp`**, a soft path: one of the shapes R gives an agent (Walt put her to use 2026-10-13; she was parked for the Many Worlds deck). Her `Meshes` folder is in `DirectoriesToAlwaysCook` |
| `ParagonAurora` | 2.8 GB (Fab, free) | **yes — `RefactorComponent.cpp`**, a soft path: an agent's shape under R (2026-10-13). `Meshes` folder in `DirectoriesToAlwaysCook` |
| `ParagonYin` | 1.3 GB (Fab, free) | **yes — `RefactorComponent.cpp`**, a soft path: an agent's shape under R (2026-10-13). `Meshes` folder in `DirectoriesToAlwaysCook` |

Also ignored and NOT re-downloadable — these are rebuilt, not fetched:

- `Content/MetaHumans/` — the assembled dancers. **Rebuild instructions and every
  known-good setting are in `Content/MetaHumans/README.md`.** Read it before
  re-assembling anything.
- `Content/Characters/Retargeting/*_MH.uasset` — retargeted mocap. Re-export
  through `RTG_Mannequin_to_MetaHuman` after re-downloading MorroMotion.

## Cooking — two different mechanisms

Vendor content reaches the shipped pak one of two ways, and it matters:

1. **`DirectoriesToAlwaysCook`** in `Config/DefaultGame.ini` — for content found
   by *runtime scan* rather than by reference. A scan is not a reference, so the
   cooker has to be told. This covers the Menagerie packs, the SciFi kits, PCG,
   Cards, SlotFactory, AIApparition and `Audio/MrsHall`.
2. **C++ hard reference** — `FObjectFinder` in a constructor is a real CDO
   reference, so the cooker follows it. This is how `ParagonGideon`,
   `MagicianLabatory` and `StainedGlass3D` ship despite being in neither
   `MapsToCook` nor `DirectoriesToAlwaysCook`.

> **Never use a soft-path-only reference for vendor content.** It resolves fine
> in PIE and is MISSING from the packaged build — the v0.7.4 soft-ref miss. Use
> `FObjectFinder`, or add the folder to `DirectoriesToAlwaysCook`, then verify
> under `Saved/Cooked/` before pushing to itch.

## Restoring after a fresh clone

1. `git clone`, then open the project (it will build with content missing)
2. Sign in to the Epic account that owns the Fab library
3. Re-download the packs above via the Epic Games Launcher / Fab
4. Rebuild the MetaHumans per `Content/MetaHumans/README.md`
5. Re-export the retargeted animations through `RTG_Mannequin_to_MetaHuman`
6. Run `Tools/Scripts/run_all_gates.ps1` — 15 headless gates, ~3 minutes on a
   quiet machine. Passing gates do NOT prove the vendor content is present; they
   test logic, not assets. Load `L_Office_v02` and look.

> Keep this file current. Every time C++ gains a hard-coded `/Game/...` path into
> an ignored folder, add a row — that path is a dependency no build step checks
> and no test catches.
