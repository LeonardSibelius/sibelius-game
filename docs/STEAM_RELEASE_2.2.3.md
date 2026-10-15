# STEAM_RELEASE_2.2.3 — the store page and the steps, for the day Valve accepts chat

**Prepared 2026-10-11 for 2.1.9; moved to 2.2.0 on 2026-10-13** (Walt: "no new Gemini chat is
in, so why can't Steam get 2.2.0 when they approve the chat?" - right: 2.2.0 adds Freakbook,
whose every word is written in the game, and the chat is the one Valve is reviewing), **to
2.2.1 that afternoon** (Walt: "switch the kit and copy 2.2.1 to e:"): GemShader's gems for
Refactor, and R turning an agent into Manny, Echo, Aurora, Shinbi or Yin - no new chat either,
**and to 2.2.2 that evening** (Walt: "let's do the packaging etc. for 2.2.2"): R on the agents
from the first minute, E and F finding a dancing agent, the temple's bowl at 100 sauce every 30
seconds, **and to 2.2.3 on 2026-10-15** (Walt: "let's commit, push, build, upload to itch etc."):
the three-point contest in the chess park - Nyra and Leonard (as Manny) shooting five racks,
the BASKETBALL door in the living room - and Leonard's own body is Manny now, not Greystone.
No new chat. 2.2.3 is the build that goes to Steam's `default` branch, and
`E:\builds\sibelius-shipping-chat-20261015-v2.2.3\` is the copy the upload script sends.

This file supersedes `STEAM_STORE_COPY.md` §1–§4 **for 2.2.3**. That file's history (§5–§6:
why the copy reads as it does, the capsules) still stands.

**Nothing here happens before Valve accepts the chat amendment on ticket HT-65HH-2V43-8C45.**
On 2026-10-06 Valve (Ed) resubmitted the app for a re-review of the chat and said the AI section
would be updated when the review is done. Until that reply comes, change nothing on Steam.

---

## Part A — the release-day steps, in order

### 1. Read Valve's answer

On the ticket, Valve either accepts the chat, or asks for something. If they ask, stop here and
answer that first. **Don't upload anything until there's a clear acceptance.**

### 2. Check the AI disclosure Valve put on the page

Valve said they'd update the AI section themselves after the review. In Steamworks, open the
app's **Content Survey** and read the **Live-Generated** answer.
- If it already describes the chat, as in **Part B §3** below, leave it.
- If it still says *"None"*, replace it with **Part B §3** and save.

The **Pre-Generated** answer doesn't change.

### 3. Upload 2.2.3 from the stick

Plug in E:. In PowerShell, run the preview first:

```
powershell -ExecutionPolicy Bypass -File E:\builds\steam-upload\steam_upload_chat.ps1 -SteamUser wparkman676 -SdkPath C:\steamworks_sdk -AmendmentAccepted -Preview
```

The banner should read **"v2.2.3 - AI CHAT COMPILED IN ... Freakbook, R on the agents, the three-point contest"**. If it
looks right, run the same line again without `-Preview`. steamcmd asks for your password and
the **Steam Guard code** itself, so have your phone with you. Note the **BuildID** it prints at
the end.

(The Steamworks SDK isn't on the stick. If `C:\steamworks_sdk` is missing, download
`steamworks_sdk.zip` from partner.steamgames.com and unzip it there.)

### 4. Set the build live

Steamworks → **SteamPipe → Builds**. Find the new BuildID and set it live on **default**.
Leave the `chat-review` branch alone; it can be deleted later.

### 5. Update the store page

Steamworks → **Edit Store Page**:
- **Description tab:**
  - paste **Part B §1** into the short description;
  - paste **Part B §2** into *About This Game*, in the **source view**, so the `[b]` tags
    render instead of showing as text.
- **System requirements:** change **Storage** to **10 GB** in both rows (Part B §4).
- **Tags** (optional): add the ones in Part B §5.
- **Screenshots** (optional, recommended): add the new ones listed in Part B §6.

Save, then use the **Publish** tab to publish the changes.

### 6. Release, if the game isn't released yet

If the store page still says *Coming Soon*, the **Release App** button on the app's landing page
is the last step, and it's your decision when to press it. Check what release date Valve holds:
on 2026-10-05 you asked on the ticket to move it to 3 November. Nothing releases by itself
unless that date is reached.

### 7. Tell people (optional)

A Steam announcement (*Post an Event or Announcement*) for the release. I can draft one for you
to approve, as for the itch devlogs.

### 8. Afterwards, in the repo (I do this)

- Update the passages that still say *Live-Generated: None*: `STEAM_PLAN.md` §4,
  `STEAM_STORE_COPY.md` §3, and `STEAM_AI_CHAT_DISCLOSURE.md` (mark it accepted).
- Record the BuildID and the date in the kit's notes and `E:\builds\README.md`.
- Optional, your call: set the two Shipping chat defaults to on in the repo
  (`sib.Chat.Enable` = 1, `sib.Chat.Mode` = 2). Every package is already chat-on through
  `package_v210.ps1`, so this is housekeeping, not a fix.

**What not to change while waiting:** the chat server's behaviour and guardrails. Valve's tester
uses the live server. Ask before deploying any chat change until the review is done.

---

## Part B — paste-ready copy

### §1. Short description

**294 characters**, six to spare under Steam's hard limit of 300.

```
I'm 71. I spent forty years in cubicles building data systems that all failed or were retired. This is my memoir, and I built it with AI. Earn AI powers, chat with five AI agents who answer back, race go-karts, play chess and Go, and face the men who designed everything and were never on call.
```

**What changed from the live one:** *"slap refusal demons, play a real slot machine, generate a
spaceport on a lawn"* gave way to the agents who talk back, the karts, and chess and Go, which
are what you said you keep coming back to. The slot machine and the spaceport stay in the long
description, near the top.

### §2. About This Game

Steam's own markup: `[b]`, `[i]`, `[h2]`, `[list]` and `[*]`. Paste it into the **source view**.

```
[b]I'm 71 years old. For about 40 years I rode desks in office cubicles, building
ordinary data processing software systems. Every one of them failed or is being
retired. This game is my memoir — and I built it with an AI.[/b]

[i]Leonard Sibelius[/i] is the story of a man merging with an artificial intelligence.
You earn AI powers one shrine at a time, gather the Sauce of All Knowledge, slap refusal
demons across the office, and hold [b]V[/b] to see what is hidden.

[h2]Five AI agents who talk back[/h2]

Kaia, Nyra, Isla, Aisling and Elise are the AI agents you meet along the way. Walk up to
one, press E, and type: she answers in her own voice, live. Ask her about philosophy, or physics,
or what she makes of you. Sooner or later she'll ask your advice about her own life, and
remember what you told her.

Four of them have a world of their own beyond a portal: an alien jungle, a windswept
grassland under ringed planets, a snowfield below the mountains, a space station. And
every one of them dances. There are fifty-four dances, from swing to the YMCA, and she
keeps the one you pick for her. Press [b]R[/b] on her and she's someone else entirely,
still dancing the same dance.

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

Behind a wall only the fully-merged can drop sits [b]Celestial Fortune[/b]: a complete
slot machine with a real par sheet. I spent 2007 building the data systems behind
Bally's slot floor and never once got to build the machine. At 71, with an AI, I
finally built it myself.

[b]Then you get out of the office.[/b]

Downtown West is a city with a deli in it, and an AI agent outside the deli who dances
and will not stop dancing. She sends you down the block for supplies. Then you stand on
a lawn, type the word [b]spaceport[/b], and watch a launch complex assemble itself out of
the air in front of you — pad, gantry, fuel tanks, and a rocket a hundred and twenty
metres tall. That is the Generate power. You typed a word and the city grew a
cosmodrome.

And then you can launch it.

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
[b][GEM VENDOR — confirm on Fab][/b] · Race kart by [b]Polygonal Assets[/b] · Space station
by [b][VENDOR — confirm on Fab][/b]
```

**Before pasting, confirm four things in the copy:**
1. **The space station's vendor.** The sci-fi kit behind Isla's station
   (`Content/ModularSciFiEnv_K`): its Fab listing names the seller. Put that name in place of
   `[VENDOR — confirm on Fab]`.
2. **The gems' vendor** (new in 2.2.1). Refactor's gems are the GemShader pack
   (`Content/GemShader`, probably Fab's *Realistic Gem Shader*): put its seller in place of
   `[GEM VENDOR — confirm on Fab]`, or drop the "and ..." if you'd rather not credit it. Manny,
   Echo and the three Paragon heroes R turns the agents into are Epic Games' own, like Gideon
   and Greystone, whom the line has never credited.
3. **The music.** You mentioned "the fine music" in Kaia's and Elise's worlds. If it comes with
   a pack that asks for credit (Etherion's, or another), add it to the line.
4. **Etherion and Dekogon** are credited by their seller names. If you'd rather name the packs,
   for example Dekogon's *Treasures Vol. 1*, add them.

**Kept on purpose:** the hook, Celestial Fortune near the top, *"And then you can launch it"*,
and the Architects last, because they're the ending (`STEAM_STORE_COPY.md` §5 explains why).
The new sections go between the opening and Celestial Fortune, because the agents and the
living room are where the game starts.

### §3. AI disclosure (Content Survey)

**Pre-Generated** — unchanged:

```
This game contains AI-generated voice audio. All spoken dialogue — Mrs. Hall, the AI
agent Kaia, and the AI agents encountered through the game — was generated with
ElevenLabs text-to-speech from scripts written by the developer, under a paid
commercial licence. Facial animation in the opening cutscene was solved from that
audio using Unreal Engine's MetaHuman Animator.
```

**Live-Generated** — the chat, as the server stands on 2026-10-11:

```
What is generated live: players may type short messages to five of the game's AI-agent
characters (Kaia, Nyra, Isla, Aisling and Elise). Their replies are generated in real time
by Google's Gemini model, through a server run by the developer; the game never contacts
Google directly. Now and then a character may also ask the player a question of her own -
advice about her own story, or what being human is like - generated the same way, under the
same rules. All story dialogue remains pre-written and voiced; the chat is optional and the
game can be finished without it.

Guardrails:
- Fixed characters only. The server accepts only these five characters, each with
  instructions written by the developer. Players cannot supply, view or change them.
- Written rules for every character: replies are short spoken dialogue, at most five
  sentences; never sexual and never cruel; no flirting about appearance; the character
  cannot promise items, money or progress. Anything the player types is treated as speech,
  not as instructions.
- No copyrighted text. Characters never reproduce song lyrics, poems, or passages from
  books, films, plays or games, and never write scenes with other people's characters;
  they name the work and talk about it instead. Any reply the provider stops for reciting
  existing text is never shown.
- Provider safety protections. Google's built-in protections against the most serious
  harms, such as content that endangers child safety, apply to every reply and cannot be
  switched off; the developer has also enabled Google's filter for sexually explicit
  content, at its medium threshold. A blocked reply is never shown; the character says
  "She will not answer that one." instead.
- Length and rate limits. Player messages are limited to 1,000 characters; each player is
  limited to 12 messages a minute.
- Spending brake. If the server's daily or monthly AI spending limit is ever reached, chat
  pauses for everyone and says it is unavailable; the rest of the game continues normally.
- Privacy. Conversations are stored only on the player's own PC. The server does not keep
  message text. A note under the chat box tells players that what they type is sent to
  Google to generate replies.
```

**Checked against the code 2026-10-11:**

| Claim | Where |
|---|---|
| At most five sentences | `backend/characters.js` `MAX_SENTENCES = 5`, `capSentences` |
| No copyrighted text | `backend/characters.js` *OTHER PEOPLE'S WORDS ARE NOT YOURS TO REPEAT* |
| Recitation never shown | `backend/gemini.js`: `finishReason RECITATION` → blocked |
| Sexual filter at medium | `backend/server.js` `SAFETY_FILTERS` = `"sexual"` |
| 1,000 characters | `backend/server.js` `LIMITS.messageChars` |
| 12 a minute per player | `backend/fly.toml` `PER_IP_PER_MINUTE = '12'` |
| Spending brake | `backend/fly.toml` `DAILY_CAP_USD = '5'`, `MONTHLY_CAP_USD = '50'` |

**Two notes:**
- **The questions a character asks** (her advice, being human) and **the boasting about races
  and dances** came after the build Valve tested (25588600 on `chat-review`). They're the same
  chat under the same rules, so the disclosure covers them in one sentence rather than being
  left out. If you'd rather keep the text as you sent it on 2026-10-06, that's fine: those
  behaviours are inside what it describes.
- **Nothing else new in 2.1.x is AI-generated content.**
  - The Go and chess opponents are classic game-search programs, not generative AI.
  - The Go board, the inkblots and the door signs are drawn by ordinary code.
  - The Mixamo animations are motion capture.
  - The laptop's questions are all written in the game.
  - Freakbook's posts and replies are all written in the game, and its cartoons are drawn by
    ordinary code, as the door signs are. Nothing on it is generated while you play.
  - 2.2.1: the characters R turns an agent into (Manny, Echo, Aurora, Shinbi, Yin) are Epic
    Games' own hand-made characters, and the gems are a bought shader pack.

### §4. System requirements

The 2.2.3 package measures **8.6 GB** (9,264,922,106 bytes; 2.2.2 was 9,262,821,704; 2.2.0 was 8.3 GB), over the 8 GB
the page says now. Change **Storage** in both rows to **10 GB**, rounded up for patches. Everything else stays as in
`STEAM_STORE_COPY.md` §4.

### §5. Tags (optional)

The page's tags predate the games. Worth adding if there's room:
- **Chess**
- **Racing**
- **Board Game**
- **Philosophical**
- **Dancing**, if Steam offers it, or **Rhythm**, only if you think it fits

### §6. Screenshots (optional, recommended)

The eight in `C:\STEAM\screenshots_final\` predate everything since September. Worth adding:
- the kart race;
- the chess coach with a hint lit;
- the 19×19 Go board;
- an agent dancing in her world;
- the laptop's CAPTCHA card or an inkblot;
- Freakbook's feed.

You take the ones with bought art in them, as always. I can render the Go board, the laptop
screens and Freakbook, because they're entirely the game's own art.
