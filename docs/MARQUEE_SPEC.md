# Marquee spec — the Celestial Fortune top box

*Drafted 2026-08-20. The art spec for the lit sign above the cathedral slot cabinet
(`ASlotCabinet`, L_Cathedral apse). Nothing here is code — it is what the texture and the
material need to be, and why.*

---

## Why this sign exists

The apse machine is the last object in the game and the one the whole memoir points at:

> *"Hey Bally, you could have used this AI skill on the Slot Data System in 2007. I built
> the warehouse and the reports. I never got to build the machine."*

The placard says it plainly (`docs/SPINE.md` Move 4): *I served that floor for years and
never built the machine. I built this one.* The marquee is where that sentence becomes an
object. It carries **the name of the game he built**, in the visual language of the floor
he worked, in the one spot on a real cabinet where the manufacturer's brand would go.

> 🔒 **No real manufacturer's mark, ever — not Bally's, not anyone's.** The marks are live
> and owned (the slot business went to Scientific Games, now Light & Wonder; the *Bally's*
> name to Bally's Corporation), and a defunct company would not free them either —
> trademarks are sold in bankruptcy, not released. It is also the wrong answer
> dramatically: stamping their name on the machine he finally built hands it back to them
> at the exact moment it becomes his. **The era's styling is not anyone's property.** The
> swash script, the starburst, the chrome bezel — that is period idiom, and that is what
> this spec reproduces. Bally is named in the *memoir line*, which is a first-person
> statement about his own career and a different kind of use entirely.

---

## The idea in one line

**In a cathedral, a backlit coloured-glass panel is a stained-glass window.**

That is the whole trick. A Vegas marquee dropped into a gothic apse normally fights the
room; a lit glass panel with a starburst behind a name *rhymes* with the rose window and
the apse glass already there. The machine stops looking like it was imported from another
game and starts looking like the altarpiece it is meant to be.

Everything below serves that: gold on midnight, radiating rays, a metal bezel, lit from
within.

---

## Geometry

The cabinet blockout today (`Tools/Scripts/build_slot_cabinet.py`), at the apse,
X = 3400, facing back down the nave (yaw 180):

| Part | Size (D × W × H, cm) | Z span |
|---|---|---|
| `SlotCab_Base` | 90 × 140 × 30 | 0 – 30 |
| `SlotCab_Body` | 70 × 120 × 170 | 30 – 200 |
| `SlotCab_Screen` | 5 × 90 × 80 | 110 – 190 |
| **`SlotCab_Marquee`** (new) | **8 × 90 × 45** | **200 – 245** |

The marquee sits directly on top of the body, flush with the screen panel's front face
(X − 36) so sign and screen share a plane. Total cabinet height becomes 245 cm — tall
enough to read from the nave entrance, short enough not to compete with the apse glass.

**Texture aspect is 2:1** (90 × 45 cm). Author at **2048 × 1024**.

---

## Layout

Two lines, and they are deliberately from two different worlds:

```
        ╭──────────────────────────────────────────╮
        │   ·  ·   \  |  /   ·  ·                  │   ← starburst rays, behind everything
        │      ╲    Celestial     ╱                │   ← line 1: casino script, gold
        │        ╲___________╱                     │   ← the terminal 'l' sweeps under the word
        │           F O R T U N E                  │   ← line 2: Roman capitals, wide-tracked
        ╰──────────────────────────────────────────╯
              brass/chrome bezel, 3 cm face
```

- **Line 1 — `Celestial`.** A heavy connected casino script, baseline rising ~8° left to
  right, with a long entry swash off the *C* and the terminal *l* sweeping back underneath
  to underline the whole word. Occupies ~62% of the panel height. This is the 70s.
- **Line 2 — `FORTUNE`.** Roman inscriptional capitals (Trajan-family), letter-spaced wide
  — roughly 0.25 em — sitting inside the script's underline sweep. This is the cathedral:
  Roman capitals are what is actually carved over a church door.

The joke and the marriage are the same thing: **Vegas script over Roman capitals.** Neither
half is a compromise; the object is honestly both.

- **Starburst.** 24 thin rays radiating from behind the script's midpoint, longest at the
  horizontal, fading to nothing before the bezel. Period-correct for a slot marquee, and
  simultaneously a mandorla — the almond of light behind a saint. Keep them thin (2–4 px at
  2048) and low-contrast; they are a texture, not a feature.
- **Flanking motifs.** The existing symbol art carries the theme: `T_sym_star` at the
  upper left, `T_sym_saturn` at the upper right, small (~8% panel height), gold-tinted to
  sit back. Do not use `seven` or `coin` here — too literal, and they belong on the reels.

---

## Palette

Matched to `SlotScreenWidget.cpp` so the sign and the screen read as one manufactured
object rather than two art passes. Engine values are `FLinearColor` as they appear in that
file; hex is the approximate sRGB for painting the texture.

| Role | FLinearColor | ≈ hex | Where it comes from |
|---|---|---|---|
| Field (panel ground) | `0.035, 0.035, 0.10` | `#0A0A1A` | screen background |
| Hero gold (the script) | `1.00, 0.82, 0.30` | `#FFD24D` | screen title |
| Pale gold (`FORTUNE`, rays) | `1.00, 0.92, 0.62` | `#FFEB9E` | credits row |
| Bezel brass | `0.83, 0.66, 0.21` | `#D4A836` | the gold trim brush |
| Heat (script shadow only) | `0.85, 0.18, 0.06` | `#D92E10` | new — see below |
| Cool accent (ray tips) | `0.78, 0.93, 1.00` | `#C7EEFF` | win-lines readout |

**On the red.** A 70s casino sign is usually red script on white or gold. Here the field
has to be midnight to match the screen and the apse's dark glass, so the red is demoted to
a **hard drop shadow under the gold script only** — offset down-right ~1.5% of panel
height, no blur. That is where the era's heat comes from without repainting the sign into
something that fights the cathedral. Do not use red for either word's fill.

---

## Material and light

- **Emissive, and it is the light source at the apse.** Earlier staging note stands: the
  marquee should be the brightest thing at the end of the nave so it pulls the player down
  the aisle. Emissive multiplier in the 8–15 range on the gold and the rays; the field
  stays genuinely dark (emissive near zero) so the contrast does the work.
- **The field is glass, not paint.** Slight roughness variation and a faint internal
  gradient — brighter behind the script, falling off to the corners — reads as a backlit
  panel rather than a sticker.
- **Bezel** is a separate material: brushed brass, roughness ~0.35, no emissive. It catches
  the apse's raking sun and ties to the gothic pack's brass lectern.
- **No animation.** No chase lights, no flashing. The machine is an altarpiece; it glows,
  it does not blink. (If a win ever needs to reach the marquee, pulse the emissive
  multiplier — never the colour.)

---

## The maker's badge

Separate small plate, low on the cabinet front — the exact spot a real cabinet carries its
manufacturer's plate. **This is the payoff and it should be easy to miss on the first
pass.**

- Size ~14 × 4 cm, at Z ≈ 45, on the base's front face.
- Roman capitals to match `FORTUNE`, ~60% the tracking, engraved brass — dark recesses,
  bright edges, **no emissive**. It should need the marquee's light to be readable.
- Text: **Walt's name.** Forty years building someone else's back office, and here is his
  name where the manufacturer's goes.

---

## Build order

1. Author the 2048 × 1024 marquee texture to this spec → `/Game/SlotFactory/T_Marquee_CelestialFortune`.
2. Material `M_Marquee` (emissive × field mask), instance for the bezel from the existing
   `M_SlotGold`.
3. Add `SlotCab_Marquee` to `build_slot_cabinet.py` as a fourth block at the sizes above —
   and **stop that script destroying `Altar_Main`** while you are in there
   (`SM_Altar_Main_Marble` from the gothic pack; the machine standing *on* the altar is the
   thesis in one silhouette).
4. Badge plate + its texture.
5. PIE from the nave entrance: the marquee should be legible and the brightest thing in
   the apse from the far end.
