# Changelog

All notable, player-facing milestones for the Sibelius game. Versions track the
packaged Win64 builds pushed to itch.

## v0.4.0 — Many Worlds

- **Many Worlds** — a hidden Sauce Door opens onto a procedurally-generated Elsewhere
  (UE5 PCG) holding a collectable curio; Cabinet of Curiosities. Hold **V** in the
  kitchen to reveal the door, step through to a seeded sci-fi "Server Cathedral" dressed
  with a debris pile of crates and containers, collect the glowing curio, walk back
  through the doorway to the office, and watch the Cabinet of Curiosities fill.

### Packaging notes (v0.4.0)

- `L_Elsewhere` added to `MapsToCook` — the Sauce Door travels by level name, so the
  cooker needs it listed explicitly.
- Marketplace kit content (Crebotoly `ModularSciFiEnv_K`, SciFi Boxes A/B) is cooked
  into the package via `DirectoriesToAlwaysCook` — its bytes are gitignored out of the
  repo, but licensed and shipped in the build so the Elsewhere renders.

## Earlier

- **v0.1–v0.3** — the office → chapters 1–6 → cathedral → slot-machine coda loop,
  packaged for Win64; each chapter gated by a headless smoke test. See
  `docs/sib-42-packaging-notes.md` for the shipping runbook and the predicted-bug ledger.
