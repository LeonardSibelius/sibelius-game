# NEXT_SHRINE_NOTES — the Refactor temple (consult, 2026-07-15)

*Walt asked, at the end of the v0.7.4 session: what should the next
temple/shrine be, and what use can R (Refactor) be put to? Claude's advice,
recorded for the next session (Fable or Opus).*

## The pitch: the Temple of the Second Draft

R is the most under-used verb — it toggles office furniture and gates the
save, while every other power has a home. The next temple should be BUILT on
R: a place where the path to the shrine doesn't exist until the player
refactors it into existence.

- Collapsed beam → staircase. Wall → doorway. Rubble → bridge across a gap.
- Memoir theme: every refactorable is a memory with two versions — the way it
  was, and the way he remembers it.
- **Zero new systems.** `URefactorableComponent` already does everything; the
  temple is level design only (place object, assign its two states). Walt can
  hand-place with the mouse — cheap in tokens, done in the editor.
- One optional dead-end: sauce or a curio behind a refactorable bookcase,
  teaching "when stuck, try R on everything."
- Echo in the forests afterward: one refactorable log-bridge on a road to a
  curio beacon makes R a game-wide verb for free.

## Runner-up (later): the Slot Workshop

`Content/SlotFactory/L_SymbolStudio` already exists on disk — a shrine about
DESIGNING machines rather than playing them is the most autobiographical room
possible, and the natural home for the daily carousel (APPEAL point 5b).
Needs more design thought; the Refactor temple is buildable now.

## Practical notes

- Kit meshes for the two-state props can come from packs already shipped
  (brutalist/sci-fi kits) — zero download cost, per the diet rule.
- Placement lesson applies (see APPEAL_PLAN handoff): anchor to trusted
  actors or hand Walt the mouse; never remote-place in dense levels.
- A new shrine implies which power it grants or celebrates — if all six are
  placed, this one can instead be a PILGRIMAGE site (sauce/curio reward),
  or the shrine where R gets an upgrade (more refactorables per level?).
