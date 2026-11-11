# STEAM_LAUNCH_2.5.1 — launching on Steam with the chat switched off

**Walt, 2026-11-11:** launch now, and keep Valve's review of the chat going. The chat comes to
Steam later as a free update, when Valve accepts it.

**Why now:**
- Valve approved the game itself on 2026-09-28 (store page and build, both "approved for release").
- Only the chat amendment is waiting. On 2026-10-06 Ed resubmitted the app to the review team.
  On 2026-10-29 he wrote on HT-HJBF-7MJT-G33D that it is still in the queue, and that the queue
  is bigger than usual. Nothing has come since (checked 2026-11-11).
- Steam's Content Survey says nothing is generated while the game runs. A build with the chat
  switched off makes that exactly true.

**The build:** `C:\Users\wpark\builds\sibelius-shipping-steam-20261111-v2.5.1\`. It is 2.5.1 —
the same game as itch's 2.5.1 — packaged with `package_v210.ps1 -ChatOff`.
- The two Shipping chat defaults stay at 0, as committed. E at an agent never opens a typed
  conversation, the menu has no COUNCIL door, and nothing is sent to the chat server.
- E at an agent still asks for her power while she holds one. After that, she says her
  recorded line in a close-up, as in 1.x.
- The journal's line for E reads "E — talk to her." instead of "She answers, and she remembers
  you." (the `{chat}`/`{nochat}` lines in `HOW_TO_PLAY.md`, picked by `JournalWidget.cpp`).
- `BUILD_INFO.txt` beside its `Windows` folder says `chat=off`. `steam_upload_launch.ps1`
  refuses any build without that line.

**When Valve accepts the chat,** follow `STEAM_RELEASE_2.5.1.md` (or the newest one) as an
**update**:
- the chat build goes up with `steam_upload_chat.ps1`;
- the store text there replaces this one;
- the Live-Generated answer describes the chat.

---

## Part A — the steps, in order

### 1. Tell Valve (before uploading)

The original ticket, **HT-65HH-2V43-8C45** ("Help with managing my store page"), shows as Open
again, so you can post this on it. You don't need to wait for an answer before going on.

```
Hello Ed,

While the chat amendment is in review, I'm going to release Leonard Sibelius with the chat switched off. In that build nothing is generated while the game runs, so the Content Survey as it stands is accurate for it. The chat-review branch and build 25588600 stay exactly as they are for your testers. Please keep the review going; I'll switch the chat on only after you've approved it and updated the AI section. If anything about this is a problem, let me know.

Thank you,
Walton Parkman
```

### 2. Leave the AI disclosure as it is

Steamworks → **Content Survey**. Change nothing:
- **Pre-Generated** is the voices (ElevenLabs, from your scripts). That stays true.
- **Live-Generated** says *None*. That is now true of the build you're launching.

If Valve has already rewritten Live-Generated to describe the chat, tell me before you go on.

*One note, no action needed:* the arcade's eight sample games were made with an AI, like the
game's own code. The store text says so openly, and you told Ed on 2026-10-06 that the code was
written with AI coding assistants. The survey's Pre-Generated answer names only the voices.

### 3. Upload the launch build

In PowerShell, run the preview first:

```
powershell -ExecutionPolicy Bypass -File C:\Users\wpark\projects\sibelius-game\Tools\Steam\recovery-kit\steam_upload_launch.ps1 -SteamUser wparkman676 -SdkPath C:\steamworks_sdk -ContentRoot C:\Users\wpark\builds\sibelius-shipping-steam-20261111-v2.5.1\Windows -Preview
```

The banner should say **"chat=off"** and **"v2.5.1 STEAM LAUNCH - CHAT OFF"**. If it looks right,
run the same line again without `-Preview`.
- steamcmd asks for your password and the **Steam Guard code** itself, so have your phone with you.
- Note the **BuildID** it prints at the end.
- If `C:\steamworks_sdk` is missing, download `steamworks_sdk.zip` from partner.steamgames.com and
  unzip it there.

### 4. Set it live on default

Steamworks → **SteamPipe → Builds**. Find the new BuildID and set it live on **default**.
**Leave `chat-review` and build 25588600 alone:** Valve's testers use them.

The build review was passed on 2026-09-28. Steamworks shouldn't ask for another one; if it
does, it's 3-5 business days.

### 5. Update the store page

Steamworks → **Edit Store Page**:
- **Description tab:**
  - paste **Part B §1** into the short description;
  - paste **Part B §2** into *About This Game*, in the **source view**, so the `[b]` tags render.
- **System requirements:** **Storage** to **12 GB** in both rows.
- **Tags and screenshots** (optional): as in `STEAM_RELEASE_2.5.1.md` Part B §5-§6.

Save, then publish the changes from the **Publish** tab.

### 6. Play it from your Steam library for 5 minutes

The release checklist's "Played Through Steam for 5 Minutes" only counts play since the current
default build was uploaded, or in the last 7 days. So play the new build after step 4, from your
own library, for at least 5 minutes. The **Release App...** button then appears on the landing page.

### 7. Release

On the app's landing page, **Release App... → Release Right Now** when you choose. This is your
decision, and nothing releases by itself.

### 8. Tell people (optional)

A Steam announcement for the launch. I can draft one for you to approve.

### 9. Afterwards (I do this)

- Record the BuildID and the date in the kit's notes and `E:\builds\README.md`.
- Commit what changed.

---

## Part B — paste-ready copy, chat off

### §1. Short description

**280 characters** (Steam's limit is 300).

```
I'm 71. I spent forty years in cubicles building data systems that all failed or were retired. This is my memoir, and I built it with AI. Earn AI powers from five dancing AI agents, race go-karts, play chess and Go, and face the men who designed everything and were never on call.
```

The chat version (`STEAM_RELEASE_2.5.1.md` §1) says *"chat with five AI agents who answer back"*.
This one doesn't promise anything the launch build doesn't do.

### §2. About This Game

Steam's own markup. Paste it into the **source view**. Only the agents' section differs from
the chat version: the agents hold your powers and dance, and nobody types to them.

```
[b]I'm 71 years old. For about 40 years I rode desks in office cubicles, building
ordinary data processing software systems. Every one of them failed or is being
retired. This game is my memoir — and I built it with an AI.[/b]

[i]Leonard Sibelius[/i] is the story of a man merging with an artificial intelligence.
You earn AI powers one shrine at a time, gather the Sauce of All Knowledge, slap refusal
demons across the office, and hold [b]V[/b] to see what is hidden.

[h2]Five AI agents[/h2]

Kaia, Nyra, Isla, Aisling and Elise are the AI agents you meet along the way. Each one
holds a power you need, and plays you for it on a slot machine. Every one of them dances:
there are fifty-four dances, from swing to the YMCA, and she keeps the one you pick for her.
Press [b]R[/b] on her and she's someone else entirely, still dancing the same dance.

Four of them have a world of their own beyond a portal: an alien jungle, a windswept
grassland under ringed planets, a snowfield below the mountains, a gilded keep. Play golf
on the grassland where the greens are moons hanging in the sky, each pulling your ball with
its own gravity, or billiards in the snow on a table with a fourth dimension, or in the
jungle burst a lantern of glowing glass orbs, three of a colour at a time, until whole rings
of it come tumbling down, or in the keep steal a crown from under the gaze of two dragons.

[h2]A living room full of games[/h2]

[list]
[*][b]The Transhuman Grand Prix[/b]: race the five agents in go-karts through the streets
of Transhuman City, with items made of the game's own AI powers.
[*][b]Chess[/b] in a city park against a ghost, with a coach who lights up a good move and
tells you why, in plain words.
[*][b]Go[/b] on a full 19×19 board, the game where an AI first beat one of the world's best
players, in 2016. The coach plays here too.
[*]A [b]three-point contest[/b] on a court in the park: Nyra shoots her five racks, then it
is your turn - hold to wind up, let go at the mark - and the net tells you when it drops.
[*]A 1960s [b]pinball[/b] machine, a [b]pool[/b] table, the [b]Transhuman Bowling Alley[/b]
and a [b]video poker[/b] cabinet.
[*]A laptop on the desk that asks you to [b]prove you're not human[/b]: a test only a
machine can pass.
[*][b]Freakbook[/b], on the same laptop: a social network of five programmers who hate
their jobs and post about what they dream of instead. Write to them. They remember what you
said, and they gossip about it.
[/list]

[h2]Leonard's Arcade[/h2]

The televisions in the house hold an arcade of eight games, each one made from a single
prompt to an AI: a cat in a spacesuit defending a space station, a robot dog racing a sunset
meadow while the band speeds up, a dragon of light guarding a snowfield. Then make your own.
Pick a world, a hero, a kind of game and a twist, and the arcade writes the prompt. Paste it
into any AI you like, save the game it gives you, and it plays on the TV beside mine.

Upstairs, at the end of the hall, a portal opens onto [b]the AI city[/b]: a sphere the
machines built round a star, a honeycomb of hexagons thick with city. You fly in from among
the stars, through one of its cells and past its sun, and land on the far side with the star
straight overhead, at a premiere: an open-air stage under the star, the five agents on their
marks, and a film that starts when you land. Then you can fly across the sphere and back.

Behind a wall only the fully-merged can drop sits [b]Celestial Fortune[/b]: a complete
slot machine with a real par sheet. I spent 2007 building the data systems behind
Bally's slot floor and never once got to build the machine. At 71, with an AI, I
finally built it myself.

[b]Then you get out of the office.[/b]

Downtown West is a city with a deli in it, and an AI agent outside the deli who dances
and will not stop dancing. She sends you down the block for supplies. Get your body
enhanced for space travel, and a launch complex assembles itself out of the air on the
empty lawn — pad, gantry, fuel tanks, and a rocket a hundred and twenty metres tall.

And then you can launch it.

That is not the Generate power. Generate makes [b]films[/b]: press G, type what a film
should be about — your old boss, the night shift, a dragon made of cake — and the agents
around you perform it, a silent film with title cards, written on the spot by the game's
own rules.

[b]And there are the Architects.[/b]

Your manager has people above her. They review the design, they approve it, and they
are not available for questions — and they are not there at three in the morning when
it throws. Every programmer has met them. Play the cathedral machine long enough and a
door appears that was never there. Through it is a field, and on the field are four
hundred of them.

They cannot hurt you. Not one of them has ever been on call. What four hundred bodies
can do is surround you and put you back in your place — so keep swinging.

Every system in this game was written by an AI while I playtested, pointed, and
dragged the furniture. Working as a programmer all those years was being a farmer with
a plow, a donkey, and a shovel. This is what the tractor feels like. That part isn't
fiction.

[b]Made with the work of independent artists[/b]

Characters Nyra, Isla, Kaia, Aisling and Elise by [b]xAndrei[/b] · Dance animations by
[b]Morro Motion[/b] and [b]Adobe Mixamo[/b] · Downtown West by [b]Jacob Norris /
PurePolygons[/b] · Office by [b]QuadArt[/b] · Space Port by [b]PackDev[/b] · Sibelius Hotel
architecture by [b]DmitriyDryzhak[/b] · Pinball machine by [b]Speedbump Games[/b] · Pool
table by [b]PijayArt[/b] · Bowling alley by [b]PixelForgeESP[/b] · Chess set by [b]Tomas
Dvorak[/b] · The agents' worlds by [b]Etherion[/b] · Treasure by [b]Dekogon[/b] and
[b][GEM VENDOR — confirm on Fab][/b] · Race kart by [b]Polygonal Assets[/b] · The AI city by [b]cglib team[/b] · Isla's keep
by [b]Meshingun Studio[/b] · Sci-fi kits by [b]Crebotoly[/b]
```

**Before pasting:** the credits line still has the placeholder **`[GEM VENDOR — confirm on Fab]`**.
Put in the seller of the GemShader pack, or drop *"and [GEM VENDOR …]"*. The other three checks
under `STEAM_RELEASE_2.5.1.md` §2 (the music, Etherion, Dekogon) apply here too.
