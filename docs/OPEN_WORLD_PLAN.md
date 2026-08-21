# Open World Plan — the powers are the keys

*Drafted 2026-08-20, from Walt's own diagnosis:*

> "I like open worlds. There was one video game that I liked to play in the past, it was
> called Zelda... now that I am bored with the Sibelius game — you watch some nice dancers
> and you play a lot of slot machines to get powers, then you get to the cathedral and play
> another one, big deal — now I miss the open world that I removed. What can I do to make a
> 10GB download worth it? How could I make something as interesting as Zelda was?"

**Nothing here is ratified.** This is the argument for a direction, the arithmetic behind
it, and a one-day experiment that settles it before anyone spends a year. Read it against
`docs/FORESTS_DO_NOT_USE.md`, which is still in force until Walt says otherwise.

---

## 1. The diagnosis is right, and it is not about size

That sentence — *dancers, slot machines, powers, cathedral, another slot machine* — is an
accurate structural description of the shipped game, and it names the defect exactly:

> **Every interaction in the game resolves to the same one.** Press E, play a machine.
> The powers are gated by GAMBLING rather than by each other, so the six most interesting
> verbs in the project are things you *win* and then rarely *use*.

A 10 GB forest does not fix that. It gives the player a larger place to be bored in. The
open world is worth building, but not as a cure for boredom — as the place where the
powers finally become the point.

---

## 2. The size question is the wrong question

**What actually killed forests** (`docs/design/Recipe_01_ModeB_Poplar_Forest.md`, 2026-07-04)
was not size first. It was that **runtime PCG does not run in a cooked build**:

```
[ScheduleComponent] Didn't schedule any task
```

...plus the kit's Runtime Generation mode breaking its own spline interior sampling
(`PCGPartitionGridActor` "Closed Loop"), and unpartitioned runtime generating nothing at
all. Editor success proved nothing — the standing lesson of this project.

**Plan B was a DECK: eight pre-baked levels, `L_Forest_01..08`.** That is what went past
10 GB. Eight baked forests, not one.

The arithmetic that matters:

| | size |
|---|---|
| v0.9.5 shipped build (measured) | **5.5 GB** |
| A deck of 8 baked forests | what pushed it past 10 GB |
| **One** hand-built forest region | a fraction of a deck |

And most of a forest kit's weight is its **asset library**, not the world: dozens of tree
and plant variants, each with full-resolution textures. Six trees, four shrubs, three
ground materials and a rock set make a convincing wood. Culling the unused 80% of a
vendor kit is a normal shipping step, not a compromise.

> ⚠️ Not measured: `Content/EasyBiomes` is **no longer on disk** — the Fab pack appears to
> have been uninstalled despite FORESTS_DO_NOT_USE.md saying to keep the bytes. Re-download
> from Fab before any of this, and measure the folder then. The budget targets below are
> targets, not measurements.

**The reframe: stop trying to make a 10 GB download worth it. Build one region, cull the
kit, and the question dissolves at ~7 GB total.**

---

## 3. "Endless" was never what was good about Zelda

The original Zelda overworld is **128 screens** — tiny by any modern measure, and every
one placed by hand. What is remembered as endless adventure was **authored density**: you
went somewhere because something was visibly there, and when you got there it was worth
the walk.

This is why the Elsewhere rooms — The Flooded Library, The Clockwork Attic, The Starlit
Void — do not satisfy despite being genuinely well written. They are *generated*, and
nothing in them wants anything from the player. A curio with good flavour text is a
collection entry, not a destination.

> 🔒 **PROPOSED: cut "endless", keep "open".** One authored region beats an infinite
> generator, and it is the cheaper of the two to ship. Procedural generation is the
> opposite of the thing that made Zelda good.

---

## 4. The six powers are better Zelda items than Zelda's

This is the asset the project is sitting on. Zelda's items are keys that change what the
world means — the hookshot turns a gap into a door. Sibelius already has six verbs that do
that *and* carry the story, because they are AI assistance made literal
(`docs/NARRATIVE.md`).

They are currently obtained at slot machines and then barely needed again. Invert it:

| Power | What it is now | What it becomes in a region |
|---|---|---|
| **Code Vision** | reveals true names indoors | the path is *there*; you cannot see it unaided. The scanner, the map, the "something is off about this clearing" |
| **Refactor** | changes properties | the boulder becomes light, the river shallow, the thicket passable |
| **Compile** | builds from gathered parts | a bridge from planks you found — traversal you *make* rather than find |
| **Test-Drive** | branch reality, keep or discard | take the dangerous route and un-take it. A save-scum made diegetic; nothing else does this |
| **Deploy** | makes changes permanent | your edit to the world **persists in the save** — see §5 |
| **Generate** | ask, and it appears | the last resort, and still refused most of the time, which is the joke |

> 🔒 **PROPOSED: the slot machine stops being the toll booth.** It stays exactly where it
> earns its place — the cathedral coda, the Bally story, the machine he finally built. It
> stops standing between the player and every single power. A power is opened by USING the
> previous power to reach the agent who gives it. That is lock-and-key, and it is the
> structure the game currently lacks.

---

## 5. The one thing Zelda does not have: Deploy

**An open world the player permanently rewrites, that persists in their save.**

That is not a feature bolted onto the theme — it *is* the theme. The whole game is about a
man who spent forty years building the warehouse and the reports and was never allowed to
build the machine. Give him terrain he is allowed to edit, and the thesis stops being
something the placards say and becomes something the player does.

The pitch is not "Zelda but with slot machines". It is **an open world you are allowed to
edit**, and the deploy-persistence system for it already exists and is gated by
`BranchSmokeTest`.

---

## 6. The trade loop is nearly already built

Walt wanted "travelling into new towns, doing business with different merchants". Most of
the parts are shipped:

- **Curios** with rarity tiers and flavour text (`Data/ElsewhereCurios.csv`)
- **Sauce** as a unified currency
- **`FSauceShop`** with keyed, persistent, repeat-purchasable offers
- **Refusers** as danger on the road
- **The agents** as inhabitants — the most distinctive thing in the project

The missing piece is one rule: **a merchant pays differently for the same curio depending
on where you are.** That is a trade loop, it gives travel a reason that is not collection,
and it is mostly data plus one price function.

> Deliberately NOT proposed: AI agents fighting each other. It is a large engineering lift
> and it is not what makes the dancers good. They are compelling because they hand you
> something forbidden, not because they could win a fight.

---

## 7. The one-day test — do this before anything else

Do not re-download 10 GB, do not plan a region, do not touch `FORESTS_DO_NOT_USE.md`.
Spend **one day**:

1. One clearing. Existing trees, no new kit, no PCG — hand-place thirty meshes.
2. One path through it that **cannot be taken without Code Vision**.
3. One thing at the end of the path worth the walk.
4. Play it for ten minutes.

**The question it answers:** is using one of *your* powers to read *your* world fun, with
no story, no reward economy, and no dancers? If yes, the direction is real and everything
above is worth costing. If no, you have learned it in a day rather than a year — and you
have learned it about your powers rather than about Zelda's.

Everything else in this document is contingent on that ten minutes.

---

## 8. Sequencing — and the honest warning

**This is a different game, not a feature.** The current build is v0.9.5 with an alpha in
reach and a promotion plan behind it. Retrofitting an open world into a skeleton whose
spine is slot machines is the expensive way to do it.

Recommended order:

1. **Finish the spine and ship 1.0** as it stands.
2. **Run the one-day test** (§7) any time — it costs a day and does not touch the build.
3. **If it passes:** build the region *after* 1.0, with powers-as-keys designed in from
   the first clearing rather than retrofitted.

Also worth saying plainly: being bored of your own game after this many months is normal
and is not the same as the game being bad. The structural critique in §1 is correct and
worth acting on. The boredom is partly just proximity.

---

## What NOT to do

- **Do not revive the deck.** Eight baked forests is what broke the download. One region.
- **Do not attempt runtime PCG again** without first reproducing
  `[ScheduleComponent] Didn't schedule any task` in a COOKED build. Editor success proves
  nothing — that lesson is already paid for.
- **Do not grow the Elsewhere generator into the open world.** Generated rooms are the
  opposite of authored density. Elsewhere is a good curio machine; leave it as one.
- **Do not start by re-downloading EasyBiomes.** Start with §7 and thirty hand-placed
  meshes.
- **Do not remove the cathedral slot machine.** It is the thesis object and the best
  writing in the project points at it. It just stops being a toll booth.
