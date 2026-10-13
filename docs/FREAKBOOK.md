# FREAKBOOK — a fake social network of five miserable programmers

**Designed 2026-10-12** (Walt: "let us call it Freakbook and the five people are all computer
programmers, using Java, COBOL, Python, Javascript and C. They work on boring projects that
they hate and they talk about their fantasies, which are comical"). **Built 2026-10-13**
(Walt: "can you make Freakbook now?") - see *What is built* below.

**Nothing here goes to the chat server before Valve accepts the chat amendment on ticket
HT-65HH-2V43-8C45.** Freakbook is new chat behaviour; during the review the server stays as it is.
So the replies in the game today are written ones (below), and no request leaves the laptop.

---

## What is built (2026-10-13)

- **The model** - `Source/SibeliusGame/FreakbookModel.h/.cpp`, the house method: standard library
  only, deterministic, every word written in. The five, 30 posts (15 on the first visit, five
  more on the second and third, one each when a want comes true), comments, the replies, the
  gossip and the save. Tested standalone: `Tools/Tests/run_freakbook_tests.ps1` (64 tests; it
  also checks every picture the content names has been drawn).
- **Replies**: picked by what Leonard wrote about - a greeting, their work, their fantasy, the
  thing they want (or have, once it came true), a question, anything else - rotating so nobody
  says the same thing twice running. Every third reply or so quotes something he said before,
  word for word. The first thing anyone says back is hello.
- **Gossip**: anything he writes of three words or more, one of the other four posts the next
  day ("Leonard told Dee ..."), quoting him. Never the one he told.
- **The wants, decided by the game only**: Margo's dashboard counts its first view when he opens
  it from her profile; Jaxx's banner takes its first Accept on the bank's page (the X and the
  Settings banner are there to be clicked too); Dee's binder lies in the attic
  (`AFreakbookItem`, placed by `Tools/Scripts/place_freakbook_binder.py`) and her profile mails it
  once he has picked it up. Each one done makes that person a Friend instead of an Acquaintance.
  Gunther's coconut and Brent's belt are written and the item actor makes them, but **they are
  not placed anywhere yet** - their profiles say nobody has seen one around here.
- **The screen** - `UFreakbookWidget`: login (notHUMAN's verdict is the login - "Close enough."),
  feed, profiles, conversations with a moment of "typing...", the dashboard, the bank. Esc logs
  out. notHUMAN offers LOG IN TO FREAKBOOK on its verdict, and on its checkbox page once he has
  been judged before. `ANotHumanTerminal` swaps the two screens.
- **The state** - `UFreakbookSubsystem`, its own save slot `Freakbook` (`UFreakbookSaveGame`, one
  string written by `FFreakbook::SaveText`), saved on every change.
- **The pictures** - 27 cartoons drawn by `Tools/Scripts/make_freakbook_pictures.py` into
  `SourceArt/Freakbook/`, imported as `/Game/Freakbook/T_FB_*` by `make_freakbook_assets.py`,
  cooked by `DirectoriesToAlwaysCook` and checked by `package_v210.ps1`.

---

## What it is

A social network on the downstairs laptop, after **Prove You're Not Human**: you have to prove
you're not human to log in to a network of people. The name is free (a 2026-10-12 search found
only unrelated "Fakebook" projects). It has its own look: not Facebook's name, blue or layout.

- **A feed.** The five post about their jobs and their fantasies, with photos.
- **Messages.** Write to anyone. They answer in character and they remember what you told them.
- **Gossip.** They talk about you to each other. Tell Margo a secret and Brent posts about it the
  next day. The whole network remembers Leonard, not just the one he wrote to.
- **What each one wants.** Each has something they want, and the player can get it for them in
  the house. Getting it changes their posts and opens something up.
- **The sign.** The living-room sign over the laptop reads PROVE YOU'RE NOT HUMAN, with FREAKBOOK
  LOGIN under it (Walt, 2026-10-12).

## The tone: comical, and pitiful

The fantasies are **meant to be pitiful** (Walt, 2026-10-12). They are small, sad escapes from
jobs that ate these people's lives: a freezer, a raft that leaks, a cape nobody at work has seen,
a dog's life, a monk's cell. The player laughs first and feels sorry for them second. Nobody on
Freakbook is a winner, and none of them is cruel: the joke is never on them for being
programmers, it is on the work that was done to them. Write every post and reply to keep both.

## The five

### 1. Dolores "Dee" Pickering, 74 — COBOL

- **Project:** the monthly statement run at a regional bank, the same program since 1983. The
  bank has begged her back out of retirement twice.
- **Hates:** the 26-year-old "transformation lead" who calls her code legacy in meetings, and
  the AI the bank bought to replace her, which keeps emailing her questions.
- **Fantasy:** to be cryogenically frozen, so the bank has to thaw her every time the batch
  fails, at a million dollars a thaw. She has picked out the freezer.
- **Wants:** her 1983 program listing, a binder she left "in somebody's attic".
- **Lines:**
  - "The AI they bought to replace me asked what PIC S9(7)V99 COMP-3 means. I told it: money,
    dear. Money."
  - "Day 14,612 of the statement run. It has never failed on my watch. Put that on my headstone,
    in uppercase, starting in column 8."
- **Photos:** the freezer brochure; her cat asleep on a printout; Dee in a parka in the server
  room.

### 2. Gunther Krell, 58 — C

- **Project:** firmware for the ticket gate at a parking garage in Dayton, Ohio. Every night at
  3:14 a.m. the arm goes up by itself, and nobody knows why.
- **Hates:** the arm, and anyone who says "just use Rust."
- **Fantasy:** an island with no electricity, where nothing can crash but coconuts. He is
  building a raft out of old circuit boards in his garage. It is 40% done and it leaks.
- **Wants:** a real coconut, as proof the island exists.
- **Lines:**
  - "Segmentation fault, core dumped. My doctor says the same about my knees."
  - "The gate went up again at 3:14. No car. I think it's saluting someone."
- **Photos:** the raft; the gate at night, raised, no car; a coconut on his desk "for
  motivation."

### 3. Brent Hollowell, 44 — Java

- **Project:** an insurance claims system with 11,000 classes. This sprint he is renaming
  AbstractClaimProcessorFactoryBeanImpl to AbstractClaimProcessorFactoryBeanImplV2.
- **Hates:** the 400-line config file, and the meeting about the meeting.
- **Fantasy:** professional wrestling, as "The Garbage Collector," whose finishing move freezes
  the whole arena whenever he likes. He has the cape and the entrance music. He has not told his
  manager.
- **Wants:** a championship belt. Leonard can Compile one from parts he gathers.
- **Lines:**
  - "Stand-up ran 52 minutes today. I stood. I was not up."
  - "In the ring, when I pause, EVERYONE pauses. Like at work, but with a cape."
- **Photos:** Brent in the cape in his cubicle; a class diagram like a spider web; the cape
  drying on an office chair.

### 4. Margo Tuttle, 31 — Python

- **Project:** a weekly kibble-sales dashboard for a dog-food company. Views in three years:
  zero. She checks.
- **Hates:** the VP who asks for "one more chart" and has never opened the first.
- **Fantasy:** to be one of the company's taste-test dogs. They get walks and naps, and their
  opinions decide the recipe.
- **Wants:** one person, anyone, to open her dashboard. The player opens it on the laptop, the
  counter goes from 0 to 1, and all of Freakbook hears about it.
- **Lines:**
  - "Added a 14th chart. Nobody will see it, so I made it a photo of my dog. Still no views."
  - "The test dogs voted down the new chicken recipe, nine to one. No slide deck. Just a wagging
    tail. Must be nice."
- **Photos:** the dashboard, 0 views; the test dogs in a row like a board meeting; Margo on all
  fours at the dog park "for research."

### 5. Tyler "Jaxx" Bloom, 24 — JavaScript

- **Project:** the cookie banner on a bank's website. Eighteen months on it so far; it needs
  1,400 packages to show one button.
- **Hates:** that the framework he learned in March was obsolete by June.
- **Fantasy:** to be a medieval monk copying books by hand. No updates, no packages, one
  dependency.
- **Wants:** someone, ever, to click Accept on his banner. Everyone clicks the X.
- **Lines:**
  - "Monks had one dependency, and He never pushed a breaking change."
  - "npm install took 40 minutes this morning. I used the time to learn calligraphy. It's my
    favorite framework now."
- **Photos:** a candle and a quill between two monitors; a monk's robe from a costume shop,
  "trying it on for fit"; the cookie banner, enormous.

## How they treat each other

- Everyone is a little afraid of Dee. She has outlasted every technology they have heard of.
- Dee worked with Leonard on a project in the 1980s. She is the only one who knows his history,
  which ties Freakbook to the memoir.
- Gunther and Brent argue about whose job is more boring. Margo has a chart of it. Nobody opens
  it.
- Jaxx thinks COBOL is a kind of fish until Dee sets him straight.

---

## How it would work

### The model: Gemini 3.1 Flash-Lite, the agents' model

- **Text: yes.** At the server's pricing ($0.25 in, $1.50 out per million tokens) a reply costs
  about a tenth of a cent.
- **Memory: yes, the agents' way.** The game keeps each conversation locally and sends the recent
  turns with each message, plus a short note per person of what they know about Leonard. Gossip
  is the same note shared: what Leonard told one of them, the others may hear.
- **Outrageous needs help.** Flash-Lite plays it safe and repeats itself unless each person has a
  very specific obsession and example lines to copy. The lines above are those examples. Gemini's
  safety filters stay on.
- **Pictures: no.** Flash-Lite writes text only. Each person gets 5–10 pictures made ahead of
  time, and the model picks one by name from that person's list; the server refuses any name not
  on it. Live image generation would cost about 40 times a reply and would put live pictures
  in front of Valve's review.
- **The pictures are comical drawings, for now** (Walt, 2026-10-12): cartoons, not photos, so
  nobody mistakes the five for real people.

### The rules

1. **Only made-up people.** No celebrities, no real politicians, no real companies' products by
   name in a way that mocks them.
2. **The game decides gameplay, never the model.** The server's promise to Valve
   (`backend/server.js`, `docs/NPC_CHAT_REVIEW.md`) stands: nothing the model says changes the
   game. Whether someone gets what they want, and what that opens, comes from what the player
   finds or does (the binder, the coconut, the belt, opening the dashboard, clicking Accept),
   not from what the model says.
3. **After Valve.** It waits for the ticket. It needs a line in the Steam AI disclosure
   (Live-Generated: the five answer the player's messages; Pre-Generated, if the pictures are
   AI-made).
4. **Its own budget.** A social network gets far more messages than five agents. Freakbook gets
   its own daily and monthly brake so it can't spend the agents' money.

### Decided

- **Pictures:** comical drawings, for now.
- **Where it opens:** the downstairs laptop, after the not-human test. The sign says so.

### Open questions for Walt

- Where Gunther's coconut and Brent's belt go (the binder is in the attic). Placing one is
  `AFreakbookItem` with Person `gunther` or `brent`, anywhere in any level.
- The pictures are drawn by a script Claude wrote (shapes and Comic Sans, no image model), the
  way the GO and chess door signs were. Steam's Pre-Generated disclosure today names only the
  ElevenLabs voices (`STEAM_RELEASE_2.1.9.md`). Whether it should also say the code and the
  drawn art were written with AI is Walt's call, for the signs as much as for these.
