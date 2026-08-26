# Changelog

All notable, player-facing milestones for the Sibelius game. Versions track the
packaged Win64 builds pushed to itch.

Versions are `x.y.z` for a release and `x.y.z.N` for a fix to an already-shipped build
(no new content) — see `docs/sib-42-packaging-notes.md`. A `.N` fix is a line under its
parent release heading, never a heading of its own.

The seven Unreal-capability experiments are the exception: they occupy
`0.9.7.1` through `0.9.7.7` as their own headings. They add content. 0.9.7
stays the last shipped itch build until one of them is cooked.

## v0.9.7.6 — the pot leaves the kitchen

A giant cauldron had been floating at counter height in the middle of the
kitchen, in every packaged build since 0.9.7.1. Nobody playing in the editor
could see it.

- **It only ever vanished in PIE.** The cauldron chose whether to show its
  pot by reading its own actor label and looking for "Kitchen". Actor labels
  are editor-only data — the cook throws them away — so in the shipped game
  the test was always false, and the branch it fell into does not merely fail
  to hide the pot: it loads one and switches it on. Map data survives
  cooking; editor labels do not.
- **The level owns the prop now.** No name test at all. The office map has no
  mesh on the cauldron (the stove furniture plays the shop) and the temple map
  has the pot saved on its instance, which is what the code always claimed to
  want.
- Shipped wrong in 0.9.7.3, 0.9.7.4 and 0.9.7.5. If you downloaded any of
  those and wondered about the levitating cauldron: it was not a secret.

## v0.9.7.5 — Kaia, properly lit

The opening cutscene, with the lighting fixed. Same words, same performance;
she now looks like a person instead of a light fixture.

- **The key light is in front of her.** It had been at a fixed world
  position written before anyone knew where the camera would end up — and
  the camera was later flown to the opposite side, leaving the key BEHIND
  her head. What looked like a hot rim on her hair was the key itself; her
  face was lit by spill. Lights are now placed on the camera-to-subject
  axis, so they follow the camera.
- **The intensities were roughly a thousand times too high.** 18000 on a
  rect light whose engine default is about 8. Auto-exposure had been quietly
  absorbing the error, which is why it never looked simply "too bright" —
  it came out as subsurface scatter: glowing nostrils, then a glowing chin,
  then a face you could barely find. Key is now 17.
- **Camera exposure is locked**, so it no longer meters off a frame that is
  99% black and re-brightens whatever you just turned down.
- `Tools/Scripts/relight_kaia_front.py` rebuilds the whole rig from the
  camera position — re-run it after any reframe.

## v0.9.7.4 — Kaia opens the game

The game no longer starts in a frumpy office. It starts on a face.

- **A talking cutscene, rendered live by the engine.** Kaia introduces
  herself by name, says what she is, and invites you upstairs — head and
  shoulders against pure black, lip-synced from her own ElevenLabs
  recording. Skippable on Space / Enter / Escape / gamepad A. When she
  finishes it travels to the office.
- **She uses your name.** *"Hello, Leonard."* Mrs. Hall never will — she
  refuses it for the whole game and calls you "Programmer". So the first
  words anyone speaks to you are an AI granting the identity your employer
  denies. That is the scene; the introduction is just its excuse.
- **Real lip sync, not a jaw flap.** MetaHuman Animator solved her face from
  the audio: `CTRL_expressions_jawOpen` moving across 766 keys, 79 of 162
  speech curves animated, tongue included. Recipe and the reason it works
  where the runtime attempt could not: `docs/CINEMATICS.md`.
- New `ASequenceCue` plays a Level Sequence and travels onward; `AVideoCue`
  ships alongside it for cutscenes that are not engine-rendered.
- ElectraPlayer enabled (the project had only legacy WmfMedia), and Movie
  Render Queue wired to ffmpeg for MP4 output.

## v0.9.7.3 — The agents speak

Every AI agent now says who she is, out loud, in her own voice — and the screen
finally gets out of the way of her face.

- **Five agents, five voices, each with her own name.** Kaia, Nyra, Isla,
  Aisling and Elise each say *"I am AI agent `<name>`. I have granted you a
  power. Use it wisely."* Five **Ivanna** voices from the ElevenLabs Voice
  Library — one actress, five reads, which is what an agent lineage should
  sound like. Soft-loaded per agent from `/Game/Audio/Dancers/dancer_power_<name>`,
  falling back to a deliberately **nameless** shared clip for any dancer without
  her own take (including the one the finale altar summons). Missing clip =
  silent close-up plus one warning naming the file, never a soft-lock.
- **The HUD goes dark for the shot.** The greeting subtitle is gone, and with it
  the crosshair through her eye, the objective across her forehead, the sauce
  counter, the Test-Drive hint — all of it. `ASibeliusHUD::HoldCinematic` blanks
  the canvas. It is a lease, not a flag: it expires on its own if the release
  never comes, because a HUD stuck blank is far worse than one that returns a
  beat late.
- **The portrait holds still.** `TalkDanceSpeed` is 0 — she stops dancing for
  the close-up — and the face is pinned to LOD0.
- **The camera actually follows her now.** `UpdateTalkShot` rode a component
  tick that never fired; a runtime-attached component's `TickComponent` simply
  never runs on these actors, and three separate fixes could not make it. The
  shot now runs on a 60 Hz timer (`TalkTick`), which is the real reason heads
  used to drift out of frame.
- The hold follows the recording — `GreetingSeconds`, or clip length plus a
  1.4 s tail if longer. Re-pressing E restarts the line rather than stacking a
  second copy of her voice.
- **No lip movement, and now we know exactly why.** The lip-blend-shape driver
  works — measured at 563 updates and a peak of 0.67 driven by the real audio
  envelope — but `PostAnimEvaluation` calls RigLogic's
  `UpdateCurvesPostEvaluation()` *after* our morph weights land, and RigLogic
  replaces the whole array rather than merging. Overwritten every frame.
  `bTalkMouthMotion` ships **false**. `docs/DANCER_VOICE.md` records the
  mechanism, the three dead ends, and the MetaHuman Animator route that would
  actually work.
- Voice recipe, cast list and the `-AllowCommandletAudio` headless-import trap:
  `docs/DANCER_VOICE.md`.

## v0.9.7.2 — Dancers talk with their faces (experiment 2 of 7)

- **[E] talk to a dancer** (the prompt after she has given her power, or if
  she has none) **zooms a close-up onto her MetaHuman face** while the HUD
  line is up. Movement and look are held for the shot. No NVIDIA ACE, no
  Echo. Echo is the temple Presence statue; this is Kaia / Nyra / Isla /
  Aisling / Elise.
- She **faces you** using the Body mesh forward (MetaHumans sit at yaw −90;
  actor +X turned her to the right, away from you).
- She does **not** play the victory-wave. The dance pauses, then resumes.
- First E while she still has a power still opens the slot trial. The trial
  keeps the camera; the talk close-up waits for the talk prompt.
- MetaHuman `ABP_Face_PostProcess` / RigLogic still owns the Face mesh.
  Leave it alone: C++ Control Rig and Flite TTS wrecked the portrait.
- **Fix: the close-up is a portrait, not a chin / ear / chest interior.**
  `nose − skull` on a MetaHuman points *down the face*, which put the
  camera under Kaia's chin, on Isla's ear, and inside Aisling's chest.
  The shot now sits at the eyes, at eye height, in front of where you were
  standing, and looks at eyes+mouth. She yaws to face that point.
- **Fix: when the close-up ends she faces the way she did before.** The talk
  yaw is restored so the dance does not resume with her back to you.
- Repeat E only refreshes the line.
- Flite TTS and a live Face Control Rig were tried and **reverted** — they
  wrecked the portrait (male robot voice, broken face). The MetaHuman Face
  stays as assembled. The subtitle is back.
- **Fix: Elise's hair no longer explodes after talk-E.** The yaw to face you
  was simulated as motion; grooms treated it as a spin. Talk now freezes
  hair sim, teleports the turn, then resets the grooms when the dance
  resumes.
- **Fix: winning her slot no longer leaves her offering the power.** The
  grant used to `Destroy()` without clearing her pointer, so the prompt
  still said refactor and a second E froze her. Claim now drops the bind;
  trial E no longer pauses the dance.

## v0.9.7.1 — Sauce is a fluid (experiment 1 of 7)

- **The stove simmers.** The kitchen cauldron — still the shop, still the invisible
  box over the pots — now runs a Niagara Fluids 3D gas. Steam rises sauce-green.
  The more sauce you hold, the harder it rolls. Open the shop, or blend a purchase,
  and it boils over for a few seconds. Walk far enough away and the sim sleeps so
  the box does not pay for a 3D grid in the attic.
- **The temple pours.** [E] on the sauce bowl used to reveal a green cylinder
  pretending to be a stream. It now fires a Niagara Fluids 2D FLIP hose into the
  pot for the pour, then a shallow-water ripple on the filled surface until you
  Compile it. (The 3D hose needs a Chaos plugin that logs Error in a commandlet;
  2D is still a real liquid sim and kinder to the box.) The cylinder stays as a
  fallback if the plugin did not load.
- **Escape hatch.** Console `sib.SauceFluids 0` turns the sims off. Meshes and the
  glow light remain. Hardware that already struggled with MetaHumans can keep the
  ceremony without the grid.
- **Saves from 0.9.7 load.** No new save fields.
- **Fix: the temple pot is not a swimming pool.** The first drop used Niagara's
  2D shallow-water *pool* template as the filled surface. That template is a
  room-sized blue water volume; it sat under the rim, bloomed the meniscus
  white, and overflowed the virtual shadow map. The pool no longer runs.
  Filled = green meniscus + a small 3D gas simmer. Pour = the green stream
  cylinder (always visible) plus steam, not a 2D water sheet. Kitchen E
  toasts THE SAUCE SIMMERS and boils the 3D gas. `sib.SauceFluids 0` still
  kills the sims.

## v0.9.7 — The machine shows you where it broke

- **The legacy system tells you where it failed.** The piece used to travel the whole
  row and get thrown out at the end no matter which stage was wrong, so a broken INTAKE
  and a broken GRADER looked identical. Now it stops dead at the part that rejected it,
  that part's lamp lights **REJECTED HERE**, and it goes to the bin from there. You still
  have to work out *why* — the plaque sounds perfectly reasonable until you hold **V**.
- **The housing keeps a run log**, and it is already full when you walk up: 03:41 through
  03:46, all failing at the same stage. Mrs. Hall says it threw overnight; now you can
  read the night.
- **You can stop the line and step it.** **E** on any crate halts it, then walks it one
  beat at a time — a leg of travel, the jam, the drop into the bin — and the prompt says
  what the next press will do. Press E once more between pieces to let it run again.
- **A second job.** Close the first ticket and the line runs clean for a while, then
  starts dropping pieces — not all of them, one in three. It is a different kind of bug
  and it needs a different habit: you cannot tell whether you fixed it by watching,
  because three good pieces is what luck looks like. Branch reality with **[6]**, press
  **E** to run twenty test pieces off the record, and **[7]** to keep the fix. The job
  will not close on a lucky cycle.
- **You can read the labels now.** Every plaque, true name, tally and log row sits on a
  dark plate, so text stops fighting the wood floor and the crates behind it. The two
  housing readouts were also rendering at twice the size of the plaques they support;
  they are sized against them now.
- **It looks like it is working.** The piece arcs between stations and turns a quarter
  each stage, squashes when a station presses on it, rattles under a blinking lamp when
  it jams, falls into the bin, and the winning bin's label swells as it lands.
- Saves from 0.9.6 load. If you already closed the first ticket, the second one is
  waiting for you in the living room.

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
