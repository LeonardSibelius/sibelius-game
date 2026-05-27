# QuadArt Asset Packs — Install Paths & Compliance Reference

*Source-of-truth for where the QuadArt packs live on disk, how `.gitignore` protects
them, and which assets may ship. Read this before authoring `L_Office_v2_QuadArt` or
running any cook/package.*

*Created 2026-05-27 (Walt + Claude Code) on the CyberPowerPC, UE 5.7.4, branch `feat/v0.1-mvp`.*

---

## What was installed

Two QuadArt packs, purchased on Fab at **Standard tier** on 2026-05-27 ($74.99 + $69.99 = $144.98),
added to the project via Epic Games Launcher → Fab Library → "Add to Project".

| Pack | Fab listing | Install path | Size | `.uasset` count |
|------|-------------|--------------|------|-----------------|
| House Furniture | `fab.com/listings/7990c054-90fb-4b34-948e-55f963f67a6c` | `Content/HouseFurniture/` | 3,809.8 MB | 1,578 |
| Modular House | `fab.com/listings/308a9d58-2f19-4c75-b433-86545a1d7bf8` | `Content/ModularHouses/` | 4,124.5 MB | 758 |

**Note the folder name `ModularHouses/` is plural** — Fab sanitized the pack title "Modular House"
to a plural folder. The over-inclusive `.gitignore` written before import already covered it.

## License & permission

- **Written AI-use permission on file**: `quadarthelp@gmail.com`, 2026-05-26 ("Sure, feel free")
  covering BOTH packs, recorded in `walt-cowork-memory/vendor-permissions/2026-05-26-quadart-house-furniture-AND-modular-house-YES.md`.
- **Scope**: Chapter 1 environment of the Sibelius Game ONLY. Reuse elsewhere (cathedral / other
  chapters / other projects) requires a fresh request to QuadArt.
- **Four binding prohibitions**: (a) static-mesh use only in the packaged Steam build;
  (b) no QuadArt mesh/texture/material fed into AI training; (c) not used as input to any generative
  AI system; (d) no derivative works. See the permission record for the canonical wording.

## `.gitignore` protection (paid assets — NEVER push to the public repo)

These packs are **gitignored** and must never enter git history. Each developer installs them
locally from their own Fab purchase. Verified matches:

```
git check-ignore -v Content/HouseFurniture  -> .gitignore:57:Content/HouseFurniture/
git check-ignore -v Content/ModularHouses   -> .gitignore:62:Content/ModularHouses/
```

`.gitattributes` LFS-tracks `*.uasset`/`*.umap` repo-wide, so `.gitignore` is the ONLY thing
preventing these ~8 GB of binaries from being staged and pushed. Do not weaken these rules.

## Cook / ship policy — STATIC MESHES ONLY (LC-3)

QuadArt delivers most furniture and structure **as Blueprint actors, not raw static meshes**
(e.g. `BP_BookShelf_A`, `BP_WorkingTable_A1`, `BP_Door_Garage_A`). Per Walt's directive and the
permission's "static-mesh use only" term, **vendor Blueprints do not ship**. We compose the level
from the raw `SM_*` static meshes in `Meshes/` and write our own thin interaction Blueprints from
scratch (door/window/garage) in CP4.

### ✅ MAY ship (static-mesh use)
| Folder | HouseFurniture | ModularHouses | What it is |
|--------|---------------:|--------------:|------------|
| `Meshes/`    | 449 | 353 | Raw `SM_*` static meshes (incl. `Proxy_House_*` LOD proxies) — **use these** |
| `Materials/` | 412 | 176 | Materials/instances the meshes reference (ship as mesh dependencies) |
| `Textures/`  | 649 | 166 | Textures the materials reference (ship as mesh dependencies) |

### ❌ MUST NOT ship (cook deny-list)
| Folder / asset | HouseFurniture | ModularHouses | Why excluded |
|----------------|---------------:|--------------:|--------------|
| `Blueprints/`           | 61 | 44 | Vendor BP logic (furniture wrappers, doors, `BP_Demo_*`, `Interactive_BP_Interface`) — not static-mesh use |
| `Maps/` (demo levels)   | 4  | 4  | Vendor demo maps; not our content |
| `FoliageType/`          | 3  | 3  | Foliage actor types (we place static meshes directly) |
| `Animation/`            | —  | 8  | Not static-mesh use |
| `Sounds/`               | —  | 3  | Not static-mesh use |
| `NewLevelSequence.uasset` (root) | — | 1 | Vendor sequence; not our content |

### Pre-cook audit (run before any package/ship)
Cooking in UE is reference-driven: only assets referenced by cooked maps (plus any folders added to
"Additional Asset Directories to Cook") get packaged. The guard is therefore:

1. **Never** add a QuadArt folder to *Project Settings → Packaging → Additional Asset Directories to Cook*.
2. **Never** reference a QuadArt `Blueprints/`, `Animation/`, `Sounds/`, `Maps/`, `FoliageType/`, or
   `NewLevelSequence` asset from any of our maps/Blueprints.
3. After a cook, scan the cook log / `AssetRegistry.bin` for any path under the deny-list folders:
   ```
   # any hit here is an LC-3 violation that must be removed before ship
   Select-String -Path <CookLog> -Pattern 'HouseFurniture/(Blueprints|Maps|FoliageType)|ModularHouses/(Blueprints|Animation|Sounds|Maps|FoliageType)|ModularHouses/NewLevelSequence'
   ```

As of 2026-05-27 no cook has run and **no map references any QuadArt asset yet**, so the simulated
QuadArt cook footprint is empty (trivially compliant). This becomes load-bearing once
`L_Office_v2_QuadArt` references the static meshes.

## Integrity baseline (LC-4)

`docs/quadart-checksums-baseline.txt` records a SHA-256 of every QuadArt `.uasset` plus a single
manifest digest. Source assets must stay byte-identical (no derivative works). Re-run the same scan
to verify; any changed line or digest is an LC-4 violation. Hidden-door / level edits must be done
by *placement of separate meshes*, never by modifying a QuadArt source asset.
