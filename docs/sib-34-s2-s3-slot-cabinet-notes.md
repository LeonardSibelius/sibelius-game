# SIB-34 S2+S3 — The Slot Cabinet: UMG screen + interactable (predict-bugs ledger)

*Drafted June 11, 2026 (Cowork). S1 (USlotGameModel, pure C++, SlotSmokeTest-gated) already
shipped. S2 = native UMG slot screen presenting the model. S3 = ASlotCabinet interactable at
the cathedral apse. Decisions (Walt, June 11): activation = **E via the shared IInteractable
system** (supersedes the June 10 "P binding" note — no new key, consistent with door/hatch);
economy = **free play, session-only** (start 25,000 credits, resets each session, no save
integration — the coda is a gift, per the Gift par sheet philosophy).*

## Symbol art
The nine sprites are the June 11 vector set (Cowork-drawn SVG → 512px PNG, fully ours,
committable). Imported as `/Game/SlotFactory/SymbolSprites/T_sym_<id>` via
`Tools/Scripts/import_symbol_sprites.py` from `celestial-fortune/src/factory/`.
**Mapping note: `ESlotSymbol::Earth` (the bonus scatter) uses the cyan sparkle art
(`T_sym_scatter`)** — the C++ model calls it Earth, the web game calls it scatter, same role.
(Follow-up idea: draw an actual blue-green Earth vector to match the C++ naming.)

## Architecture (mirrors the established splits)
- `USlotScreenWidget` (native UMG, Journal/GenerateRequest pattern): 5×3 sprite grid,
  CREDITS / BET / WIN / FREE SPINS row, win-line readout, Space = spin, Esc = leave.
  Owns a `USlotGameModel` (seeded once per session). Staggered per-reel reveal via timers.
- `ASlotCabinet` (CathedralDoor pattern): SceneRoot + CabinetMesh (BlockAll so the focus
  trace lands), IInteractable ONLY (deliberately NOT IBranchable — never enters deploy
  saves), prompt "Play the machine [E]". Opens/closes the widget and owns input-mode
  transitions.

## Predict-bugs ledger SC1–SC10 (the gate, written before the code)
| ID | Predicted bug | Guard |
|----|---------------|-------|
| SC1 | Input-mode trap (Ch6 P1's exact bug): GameAndUI leaks WASD; or Esc leaves the player stuck in UIOnly with no cursor | UIOnly + focus widget on open; ONE close path (widget Esc → cabinet's Close handler) restores GameOnly + hides cursor. PIE test: open, close, walk. |
| SC2 | Double-open / re-entrant E while screen is up | Cabinet keeps the widget instance; Interact while open is a no-op. Widget re-shown via AddToViewport, never duplicated. |
| SC3 | Zero-size widget (Ch6 P1) | Stretch anchors + offsets(0) on the root canvas slot — the proven Journal/Generate layout. |
| SC4 | Sprite texture fails to load → invisible/blank cells | Loud per-symbol log on load failure + colored fallback brush; SlotSmokeTest asserts all 9 textures resolve. |
| SC5 | Credits math drift: free spin charges a bet, or win double-credited | Charge ONLY when result.bWasFreeSpin is false (model consumes free spins internally); win credited exactly once from result.TotalWin. |
| SC6 | Spin with insufficient credits → negative balance | Pre-spin guard: refuse when !IsInFreeSpins() && Credits < Bet; SPIN hint greys (text) when broke. |
| SC7 | Reveal timers leak / re-spin mid-reveal double-spins | bRevealing latch blocks Spin; all timers cleared in NativeDestruct and on Close. |
| SC8 | Commandlet prompt assert reads empty (ProcessEvent no-op headless) | FEditorScriptExecutionGuard around Execute_GetInteractionPrompt (the SIB-31 lesson) if/when a placed-cabinet assert is added; v1 keeps commandlet world-free. |
| SC9 | Placeholder cubes (SlotCab_Body) block the focus trace to the new actor | Placement step: ASlotCabinet's mesh REPLACES SlotCab_Body (delete the cube body, keep base; or assign the cube mesh to the cabinet's own component). The cabinet mesh itself is BlockAll. |
| SC10 | Seed reuse makes every session identical | Seed from FDateTime::Now().GetTicks() at first open; determinism remains the smoke test's job (fixed seeds there). |

## Smoke-test bar (extends SlotSmokeTest, stays world-free)
S1 asserts unchanged. New: `USlotScreenWidget` class resolves; all 9
`/Game/SlotFactory/SymbolSprites/T_sym_*` textures load (runs AFTER the import step).
**PIE is the real gate for S2/S3** (visual/UI/input — the standing rule): walk to cabinet →
prompt shows → E opens → Space spins, reels reveal, wins credit → free-spin round shows
counter + ×3 wins → Esc leaves → player walks free.

## Build order
1. Run `import_symbol_sprites.py` (native py, editor open).
2. Build.bat (new classes need no manual wiring — E flows through InteractorComponent).
3. Place ASlotCabinet at the apse (SC9: replace SlotCab_Body), save L_Cathedral.
4. Gates editor-closed: SlotSmokeTest (+ the rest of the sweep untouched).
5. PIE the full loop. Ship: feat branch → main, SIB-34 comment.
