# Changelog

All notable, player-facing milestones for the Sibelius game. Versions track the
packaged Win64 builds pushed to itch.

Versions are `x.y.z` for a release and `x.y.z.N` for a fix to an already-shipped build
(no new content) — see `docs/sib-42-packaging-notes.md`. A `.N` fix is a line under its
parent release heading, never a heading of its own.

## v0.9.6 — The First Ticket

- **The opening job is the legacy system.** New Game still starts in the living room.
  Mrs. Hall still hands you the ticket. The gold objective now sends you to the machine
  that has been rejecting overnight, not to poker through the glass. Hold **V**, find
  the part that is lying, take Refactor from Kaia upstairs, **R** it. The next piece
  lands in ACCEPT. She notices you did not do it by hand.
- **The fix is readable.** After Refactor, GRADER's true name agrees with its plaque.
  Reloading keeps the job done even without Deploy (you do not have that verb yet).
- **It looks like equipment, not a debug sketch.** The row is Crebotoly crates already
  in the project. Poker, the other agents, and the cathedral are still there — they
  open after the ticket.
- **Elise gives COMPILE in the bedroom.** The library alcove sphere is only the attic
  key. Walk into it — you do not need COMPILE. The cyan pole that hung down the
  stairwell is gone. The attic ladder ghost shows once you have COMPILE; **C** builds
  it if you have the books.

## v0.9.5 — She notices everything, and the machine gets its name

- **Mrs. Hall reacts to every power you take, not just the first.** She was written to
  notice Code Vision and then went quiet for the other five — the code was wired for all
  six the whole time, and the lines were simply missing. Now she gets worse as you get
  stronger: irritated at Refactor, suspicious at Compile ("Nothing in my shop builds
  itself, Programmer. What are you using?"), a warning at Test-Drive, a threat at Deploy,
  and at Generate the closest she comes to saying it outright — *"somebody makes them, and
  somebody is paid for it, and that was supposed to be you."*
- **A fourth AI agent, and the last office sphere is gone.** Aisling hands over Generate
  in the study nook. Kaia gives Refactor, Isla gives Deploy, Nyra gives Test-Drive.
  Compile keeps its sphere on purpose: it is the payoff of the twelve-book hunt, and a
  reward you walked to is the opposite of an ambush.
- **The cathedral machine looks like a machine.** The last object in the game was a
  square-footprint cube. It is now an upright cabinet with a lit marquee reading
  **CELESTIAL FORTUNE** in gold — the name of the thing he finally got to build, in the
  spot on a real cabinet where the manufacturer's name goes.
- **The altar is back at the apse.** A build script had deleted it to make room for a
  placeholder years of releases ago and nothing ever put it back. A slot machine standing
  at a cathedral altar is the whole ending in one silhouette.


## v0.9.4 — Somebody wants something from you

- **You have a boss now, from the first minute.** Mrs. Hall messages you in the living
  room: the legacy system threw overnight, fix it, and do it by hand — she is not paying a
  senior developer to ask a machine. Press **V** a few seconds later and she notices.
- **The AI agents hand over the powers.** No more floating spheres on poles. Walk up to a
  dancer, press **E**, and she offers you what your employer forbade — *"[E] ask Kaia for
  REFACTOR"*. They introduce themselves as AI agents; now they act like it.
- **Nothing ambushes you any more.** Shrines used to open a staked slot machine the moment
  you walked within a metre — including through a floor, from the staircase below. A power
  is something you ask for.
- **The journal keeps your record.** Every message you have earned, collected under
  *WHAT I WOULD TELL THEM*, re-readable whenever you like.
- **The ending says what it was for.** Complete the Synthesis and the walls come down, Mrs.
  Hall says her last word, and forty years of messages to former employers read back in
  order — 1988 to 2022 — a few paces from a slot machine you built yourself.
- **In the cathedral, E means the machine.** The old wooden door's prompt is gone; **O**
  takes you home from anywhere, as it always did.
- The living-room book glows properly again, the service panel's columns line up, and the
  memo card grows to fit whatever she has to say.

## v0.9.3 — First five minutes: living room, Vision, poker

- **New Game starts in the living room**, looking down the hall, poker glass behind you.
- **Opening banner:** Vision **[V]** shows hidden doors; after you use it, poker is through the glass. No COMPILE scavenger hunt.
- **50 sauce** on a fresh save, so you can sit at Jacks or Better immediately.
- **Glowing book on the coffee table** — **E** collects it (+5 sauce). Upstairs is yours to find; the HUD does not nag you there.
- **Video poker** is a standing ace-of-spades deck. After you leave the machine, **[O] back to office** is on the library panel.
- HUD sauce / Status lines sit on a **dark chip** so they read on wood.
- **Many Worlds** is out of the journal. Forest assets stay on disk, unused.

## v0.9.2 — She celebrates when you say hello

- **Talk to a dancer and she greets you.** Press **E** and she stops dancing,
  plays a short celebration, then picks the same dance back up. The line is
  unchanged: *"Hi. I am AI Agent … Wanna Fight?"*
- **F mid-greeting** still changes her dance (and unfreezes her).

## v0.9.1 — The game finally talks to you

- **Interaction prompts are visible.** Every door, machine, bowl, curio, cabinet and
  dancer has always had something to say when you looked at it. None of it has ever
  reached a downloaded copy of this game — the text was drawn by a debug facility the
  engine strips out of release builds. It draws on the HUD now.
- **So is everything else you were meant to read.** Sauce pickups and totals, chapter
  completions, the Carousel's payout and its refusal, the alarm when the Refusers come,
  why a branched door will not let you through, the Cabinet of Curiosities score,
  Test-Drive and Generate feedback.
- **"Press Q again to quit" now appears.** Until this build the first press did nothing
  visible and the second closed the game.
- Messages stack up to three, fade on their own, and repeat presses refresh a line in
  place instead of printing it three times.

## v0.9.0 — The machine keeps its own books

- **The slot machine now counts.** Press **T** at the machine, then **M**, and it shows
  what it actually did — spins, coin in, coin out, biggest win — in two columns.
  **SESSION** is this sitting; **LIFETIME** never resets. Those are the trade's real
  names, soft meters and hard meters, and on a real cabinet the law is what stops anyone
  clearing the second one.
- **And it tells you whether the difference means anything.** Every measured figure sits
  beside the par it is judged against, and underneath, the range of returns that count as
  *normal* for the number of spins you have actually played. A losing session almost
  always turns out to be inside it.
- **Under fifty spins it refuses to answer.** Not a limitation — the point. At a few dozen
  spins the honest range is so wide it tells you nothing, and knowing *that* is worth more
  than a number would be. Pinning this machine down to within one percentage point takes a
  few hundred thousand spins.
- **A good run gets the same verdict as a bad one.** Nothing here will call a machine loose
  or broken when it is neither.
- **The help page explains all of it** — hard and soft meters, why free spins add to coin
  out but never to coin in, and what a real casino floor does with these numbers when a
  machine drifts.
- **The panel's columns line up now.** The body reads in a monospace face, which fixes the
  figures on every page, not only the new one.

## v0.8.9 — Open the machine and change its mind

- **The slot machine has a service panel.** Press **E** to play it, then **T** to open the
  technician's panel — four dials that rewrite the machine's mathematics while you watch.
  Turn one and the return, the hit rate, the volatility and the bonus rhythm all move
  together, live.
- **It explains itself.** Every number is repeated in plain English — *"bet 100 credits
  over a long evening and about 96 come back"*, *"wins are frequent and small, your
  credits drift"* — and **H** opens a page on what a par sheet, RTP, hit frequency and
  volatility actually are. Written for someone who has never been in a casino.
- **The house has rules.** Build something too generous and no casino will run it; too
  mean and no regulator will license it. Outside the band the machine simply refuses to
  spin — and tells you which dial to turn, and how far, to bring it back.
- **Your machine is still yours tomorrow.** The dials are saved.
- **A real payout bug, fixed.** A line with wilds paid the first combination it found
  rather than the best one — three Wilds were worth 100 but paid out as five Stars for 30.
  Machines pay their best reading now. The house edge shifted accordingly.
- **The cathedral apse is clear.** The orbiting symbol cards were moved out of the way of
  the message on the wall, and the plinth now says what it is.

## v0.8.8 — The dancers are AI Agents

- **Talk to a dancer.** Walk up to any of them and press **E**: *"Hi. I am AI Agent
  Nyra. Wanna Fight? (I don't really fight, I just dance)"*
- **Press F and she changes her dance** — one of ten motion-capture routines, picked at
  random. F is the fight key, which is the joke: she isn't going to fight you.
- **No sniper aim required.** A dancer's collision shape doesn't travel with her
  animation, so the livelier dances used to swallow key presses — Nyra could take seven.
  The game now looks for a dancer near the middle of your screen instead of demanding a
  pixel-perfect crosshair.
- **A reticle you can actually see** — bigger, outlined so it reads against bright walls
  as well as dark ones, and it scales with your resolution instead of shrinking to a
  speck on a 4K display.
- **F is "Fight", not "Slap"** everywhere it's named — the menu, the journal, and the
  cauldron's upgrade (now **Fighting Strength**).

## v0.8.7 — The Refusers mean it now

- **Refusers turn to face you and attack.** They used to walk up, stop a metre
  away, and stand there. Now they track you as they close and swing when they
  reach you.
- **Which makes Test-Drive worth having.** Branching freezes Refusers — and now
  that they're actually swinging, freezing one mid-swing is a rescue rather than
  a shrug. Press **6** to branch, **7** to merge back.
- **The altar tells you which key.** The Synthesis now reads *"show me DEPLOY
  [0]"* instead of naming a power and leaving you to hunt the keyboard for it.

## v0.8.6 — A dancer at the altar

- **Finish the Synthesis and a dancer comes out to dance.** Show all six powers at
  the cathedral altar and she arrives at the top of the steps — and she's still
  there every time you come back.
- **The AI Temple pair are brighter.** Their key lights were too dim for a room lit
  by torches, so Aisling and Elise read as people rather than shadows.

## v0.8.5 — The office fills up

- **Two more dancers in the office** — Nyra and Isla, joining Kaia. Five dancers
  across the game now.
- **You start facing the room, not the desk.** The office spawn point pointed at the
  computer monitor; it now looks into the room, tilted slightly down.

## v0.8.4 — The dancers get their own light

- **Every dancer now carries her own key light**, so faces read as skin rather than
  waxwork wherever they're standing. MetaHuman skin needs directional light to look
  alive, and a room lit by torches and small point lights gives it nothing to work
  with.

## v0.8.3 — Kaia, and they dance

- **Aisling and Elise now dance** at the AI Temple instead of standing still, on
  retargeted motion-capture, and they've moved to a better-lit spot where they
  look like people rather than waxworks.
- **Kaia** — a third dancer, in the hallway outside the office.

> Expect a larger download: each MetaHuman adds roughly 600 MB.

## v0.8.2 — Two at the throne

- **Aisling and Elise** now stand either side of The Presence in the AI Temple —
  two MetaHuman figures in matching pink, breathing, and solid enough to walk
  into rather than through.

## v0.8.1 — The Presence, poker, and plain words

- **The Presence** — the AI embodied, a figure at the throne in the AI Temple who
  greets you as you approach, with her own subtitle channel so the cauldron can't
  talk over her.
- **Video poker — Jacks or Better** — a second machine behind its own kitchen door,
  with a genuine 52-card deck, a HOW TO PLAY panel, and a house that suggests which
  cards to hold.
- **The Carousel speaks plain words.** The old number dump was unreadable; the HUD
  now sits on a dialog panel and shop offers explain what they do.
- **The slot machine got its showmanship** — spinning reels, win presentation, sound.
- Shrine stake retuned twice (750 → 1200 → 2250) to land near a 62% win per attempt.
- The Cabinet of Curiosities was cut from the office kitchen.
- Fixes: **E belongs to the machine you stand at** (the dead-E bug, where two machines
  starved each other of the same key), and collapsed Refusers settle on real furniture
  instead of sinking through it.

> v0.8.0 shipped this same content as a **Development** build, which leaked
> on-screen debug text to players. v0.8.1 is the Shipping re-ship — the first
> Shipping-config release — plus the slap fall-lane fix.

## v0.7.2 — The download diet

- **A much smaller download** — four forests instead of eight, the stained glass and
  throne dragons cut, textures capped at 2K.
- **Celestial Fortune moved into the Carousel of Fates library**, so both machines
  share one room.
- **Arrival doorsteps** — two doors now land you at two distinct spots in the library.

## v0.7.1 — Shinbi settles down

- Companion cloth fixes: **Shinbi's ribbons no longer flail**. Damping applied as a
  repeating heartbeat rather than a one-shot that raced the mesh's own setup.

## v0.7.0 — Shinbi and the road battles

- **A companion.** Shinbi follows you through the Elsewhere and slaps Refusers.
- **Road battles in all eight forests**, with navmesh that follows you into the woods.
- **The slap stopped stretching** — Refusers now ragdoll cleanly with rigid knockback;
  the taffy effect was the cloth sim, reset at ragdoll start.

## v0.6.0 — The Carousel of Fates

- **The Carousel of Fates** — a fate machine in a library tower, reached through the
  kitchen's second secret door, its arrival framed as a shot: bookshelves, carpet,
  machine dead ahead.
- **E is the one verb** — the same key starts a staked run or pulls the lever.
- **The fate-glyph ring crowns the machine**, the same nine glyphs from the opening.
- **The temple blend pays** — +100 sauce, once, on completion.
- **Purchases announce themselves** — "BLENDED: `<item>` (−N sauce)".
- Launch hint: **"M for Status, J for Journal."**
- Fixes: a crosshair in the Carousel room, solid panel backgrounds behind the menu and
  shop, and a can't-afford message that stays put.

## v0.5.4 — A deck of eight forests

- **Eight baked forests, shuffled** — the Many Worlds door deals a different one each
  visit, with Shinbi waiting on the road. Replaced the live-generation approach, which
  couldn't finish before the player walked in.

## v0.5.3 — Reseed fix

- The forest genuinely varies per visit now — the random seed range was so wide that
  the row rotation landed on the same value nearly every time.

## v0.5.2 — No two alike

- **The kitchen door opens onto a composed forest**, freshly seeded on every entry.
- **Composed arrival** — you step through facing the postcard: the boat and three
  Shinbi down the sightline.

## v0.5.1 — The world takes shape

- **World Conductor** — one seed regenerates all four Elsewhere biome regions.
- Per-recipe lighting, a framed hero mesh that survives seed re-rolls, Shinbi anchors,
  and a marooned sailboat on the road.

## v0.5.0 — Travel

- **The Sauce door opens on a poplar forest**, with **O** to return to the office from
  any level and the drop-curio loop closing.
- **Travel transitions** — fade, shimmer, and a loading screen with a watchdog so fast
  loads don't strand you behind it.

## v0.4.2 — Return-door polish

- The Elsewhere's **return door no longer reads sideways** — its facing is a tunable
  `ReturnDoorRotation` on the builder, finalized by eye in PIE.
- Added the **"THE WAY HOME"** sign over the return door, matching the kitchen Sauce
  Door's "Many Worlds" plaque.

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
