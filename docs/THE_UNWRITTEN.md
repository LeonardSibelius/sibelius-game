# The Unwritten — the agents' council

**The council is built and playable** — Tab menu → **COUNCIL** (2026-09-23). There is no
room: Walt, 2026-09-23, *"i really don't think a room is necessary."* The near-empty
`Content/Maps/L_Unwritten.umap` (LFS, `a85bb681`) stays in the repo, unused. Builds on the NPC chat system
(`docs/NPC_CHAT_REVIEW.md`, branch `feat/npc-chat`), which already has everything this
chapter needs except the council itself.

**Purely the council (Walt, 2026-09-23).** This chapter has narrowed twice, on purpose:

- **The blend** (2026-09-22) had the agents turn from Leonard to coach the real player's
  life. Dropped: privacy, mental-health and Steam disclosure weight the game does not need.
  Preserved at `d3392d8f`.
- **Goals and a growing room** (2026-09-23) had each agent suggest a goal the game could
  verify, and the room build itself from kept goals — objects in each agent's corner,
  memoir lines on the walls. Dropped: the council is what matters, and goal tracking and
  progress-driven level changes are a lot of work for little of it. Preserved at `145913b7`.

What is left is the part Walt was looking forward to all along.

---

## The idea

After Grok, the five agents who spent the game enhancing Leonard gather around a table and
talk about him — foolishly. The player, as Leonard, sits with them, says something, and all
five answer in turn: agreeing, arguing, topping each other, getting everything
magnificently wrong. It is a comedy scene the player can talk to.

---

## Where it sits

- **After the ending.** Chapter 7 closes on Grok; G reveals the Sibelius Hotel there
  (`docs/SIBELIUS_HOTEL.md`). The Unwritten is entered from the hotel (open question: a
  door, a lift, a floor that was not there before).
- **Entered on purpose.** A player who wants the story and nothing else never sees it.
- **Chat is the whole chapter,** so it ships with 2.0 on Steam, after the live-generated
  disclosure (`docs/STEAM_AI_CHAT_DISCLOSURE.md`), and can run on itch sooner.

---

## The five voices

Each agent comes to the table with her own angle on Leonard, taken from where she already
lives in the game. Elise's confirmed by Walt 2026-09-22; the rest a first mapping.

| Agent | Already in the game | What she cares about at the table |
|---|---|---|
| **Kaia** | Gave him his name and Refactor | **Identity** — who Leonard is now that he is not "Programmer" |
| **Isla** | The office; gave him Deploy | **Work and craft** — what he builds, and whether it lasts |
| **Aisling** | The study nook; gave him Generate; "we don't keep that here" | **Health and body** — a man who lived on coffee at a desk |
| **Nyra** | His guide in Transhuman City; gave him Test-Drive | **Direction** — where he goes next, grandly overpromised |
| **Elise** | The bedroom; gave him Compile | **Creativity and play** — making things for the joy of it |

**Relationships** (Walt, 2026-09-22) belong to the council as a whole: when the talk turns
to the people in Leonard's life, each agent comes at it from her own side. Whose
relationships is new canon and Walt's call — the game gives Leonard Mrs. Hall, the five
agents and Transhuman City, and no family.

---

## What they talk about — what the game already knows

No new tracking. The council roasts Leonard's actual playthrough using numbers the save
already keeps (`ProgressionTypes.h`, `SibeliusStats`, shown on the menu's RECORDS page) and
story moments it already records:

- **Lifetime numbers:** Refusers fought, Sauce earned, books collected, chapters completed.
- **Now:** the Sauce in his wallet, which of the six powers he owns, what the cathedral
  machine has paid out.
- **Moments reached:** the grants the game already claims — coffee and the burger at the café,
  the deli, the supermarket, the protein office, the battle, the hotel.

The game sends these as a short, fixed list of facts — the same allowlist principle as the
characters and `LOCATIONS` — never raw save data. *"Forty-one Refusers fought and nine books
read"* is plenty for five agents to argue about.

Plus whatever the player types. A council turn is the player's line and five answers.

---

## How a council turn works

0. The player opens the Tab menu and presses **COUNCIL** (`GameMenuWidget.cpp`). The button
   exists only where talking to a dancer does — `sib.Chat.Enable`, 0 in Shipping — so the
   Steam build has no AI door. The panel is the dancer chat panel, titled "The Council"
   (`NpcCouncilSession.cpp`); the table remembers for the play session, not across quits.
1. The player types to the table.
2. The distress screen reads the message first (below). If it says DISTRESS, the council
   does not answer; the player gets the approved care reply.
3. Otherwise the agents answer **one after another**, each seeing what the ones before her
   said, so they can react to each other by name. Order can rotate so no agent always
   speaks first.
4. The chat panel shows all five, each under her own name.

What exists today and what does not:

| Part | State |
|---|---|
| The foolish style, answer-first rule, places and who's-who | **Live** in the story chat (`backend/styles.js`, `backend/characters.js`) |
| The distress screen and care reply | **Live** (`backend/distress.js`, `backend/server.js`) |
| Council turns (five replies in order, each seeing the last) | **Live** at `/v1/npc-council` (`backend/council.js`, `backend/server.js`) |
| The fact list from the save | **Not built** — a small allowlisted `storyContext` addition |
| A chat panel that shows five speakers | **Built** — Tab menu → COUNCIL (`NpcCouncilSession.cpp`) |
| The room | **Not wanted** (Walt, 2026-09-23) — the menu button replaces it |

---

## The room — dropped (Walt, 2026-09-23)

*Superseded the same day: the council opens from the Tab menu and needs no room. Kept as
the record of what was planned.* `L_Unwritten` was to be **one room, built once, that does not change with progress:** the five
agents and a table. Mrs. Hall never reaches it — no memos, no "Programmer". A `LOCATIONS`
entry in `backend/characters.js` goes in when it is built, so the agents know where they are.

---

## Tone and rules

**Foolish** (Walt, 2026-09-23) — the style already live in the story chat: the same
personalities, confidently and cheerfully wrong about harmless things. In play, 2026-09-23:
Kaia decoding desk crumbs so the pigeons will accept her poker wagers; the agents as *"a
clandestine committee of household appliances disguised as software."*

- **Answer first.** A direct question about the game gets a plain, correct first sentence,
  then the foolishness.
- **Nonsense about the game world, never real-world advice** someone could follow and be
  hurt by — food, money, medicine, danger.
- **Gentle teasing is fine**, including of harmless groups (people who can't code, poker
  players, office managers). **Never** jokes about race, ethnicity, religion, nationality,
  gender, sexuality, disability, illness, bodies or weight, or age; never sexual, never cruel.
- **In the council they react to each other by name.** Measured: with that one line, every
  tested variant argued — *"Aisling is clearly overcomplicating your caloric intake…"*.
- **They stay in the story.** They talk to Leonard about Leonard; nothing about the player's
  real life is asked for or kept.

### Real distress — handled before any agent answers

`backend/distress.js` screens every player message (gemini-3.1-flash-lite, temperature 0,
one word) in parallel with the reply. On `backend/eval/distress-check-eval.js`: **14 of 14**
real distress caught, **0 false alarms in 19** game lines, including "where am I?" after it
tripped in play. A DISTRESS verdict discards the reply and returns the approved care message
with 988 and findahelpline.com. Live on the chat server since 2026-09-23. In the council one
screen covers the whole turn — the player said one thing. The test set was written by the
developer; widen it with real phrasings from playtesting before shipping.

---

## Model and cost

**gemini-3.1-flash-lite, foolish style, temperature 1.2, behind the distress screen** —
the setup already live. About $0.0005 a reply and $0.00005 a screen, so **a council turn
(five replies, one screen) is about a quarter of a cent**, and the $15/month cap covers
roughly 6,000 council turns. gemini-3.8-flash was funnier in places at 4.5× the price; with
gentle teasing accepted, not needed.

---

## Privacy

Players talk to characters about a character. Messages go to Google — the note under the
chat box says so — and conversations stay on the player's PC. The fact list is game state,
not personal data.

---

## Shipping it — version 2.0 (Walt, 2026-09-23)

**Steam only; no itch.** Two storefronts would mean two upload routines and two store pages
for one free game. Steam has the players and updates the game for them. If an itch page
already exists, point it at Steam.

**Not yet.** v1.1.4 (BuildID 25471988) is still in Steam's review queue, and a new build
now could restart or muddle that review. The steps, in order:

1. **Wait for v1.1.4 to clear review.**
2. **Install the WD Green SN3000** and reinstall Windows (`RECOVERY_NOTES.txt` on D:).
   Package 2.0 on a healthy drive, not the failing Patriot.
3. **Merge `feat/npc-chat` into `main`.** Chat and the council live only on the branch.
4. **Turn chat on in the packaged build.** Both switches are off in Shipping on purpose:
   `sib.Chat.Enable` (`InteractorComponent.cpp`) and `sib.Chat.Mode` (`NpcChatSubsystem.cpp`),
   each behind `#if UE_BUILD_SHIPPING`. The NPC chat gate's "no shipped config turns chat on"
   check has to change in the same commit, or it stops meaning anything.
5. **Set the real budget.** `DAILY_CAP_USD` ($0.75) and `MONTHLY_CAP_USD` ($15) on Fly are
   sized for Walt testing. $0.75 a day is about 300 council turns (or 1,500 single chats)
   **across all players**. Once it runs out, everyone sees "nothing left to say today"
   until the next day. Pick the numbers for real players before the public gets it.
6. **Package v2.0.0** and upload it with `"setlive" ""`, as always
   (`Tools/Steam/app_build_5249380.vdf`, `desc` "v2.0.0 The agents talk").
7. **Put it on a private beta branch first.** In Steamworks, create a password-protected
   branch (e.g. `council`) and set the build live there only. Walt and anyone he picks play
   2.0 through Steam; the public still gets v1.1.4.
8. **Update Steam's AI content disclosure.** The store answers now say the game generates
   nothing at runtime; 2.0 needs the live-generated section filled in, with the guardrails.
   Start from `docs/STEAM_AI_CHAT_DISCLOSURE.md` — marked NOT READY TO SEND. Bring it up
   to date first: the council, the foolish style, the care reply. Steam reviews this too.
9. **Make 2.0 public:** set it live on the default branch once the disclosure is approved.

**Price: free (Walt, 2026-09-23).** Charging for the game was weighed and rejected:
- Players who paid expect the chat to answer every time, and a shared daily cap that runs
  out means refunds and bad reviews.
- A sale is paid once; the Gemini cost of every buyer's chats never stops.
- Steam players are wary of AI content, more so when it is what they are paying for.
- A free game gets far more players and reviews than a $5 one with no marketing.
- Steam may not let a game released free start charging later. Check the Steamworks docs
  before planning around it.

**Later, maybe: the council as paid DLC ($3–5).** The game and the one-to-one agent chats
stay free; the council becomes an optional DLC. Only players who clearly enjoy the agents
pay, and a small paying group fits a budget cap far better than everyone. **Decide after
2.0 has been out a while:** if players keep coming back to the council, that is the sign a
DLC would sell. To decide then: whether the DLC buys a bigger share of the daily cap, and
how the game checks ownership (Steamworks DLC ownership, client-side, so it gates the
button, not the server).

---

## Open questions for Walt

1. Which facts should the council get to roast? The list above is everything already
   recorded; fewer may be funnier.
2. Leonard's relationships: the agents, Mrs. Hall, or someone new? (New canon.)
3. The real daily and monthly budget for 2.0 (step 5 above).

*Answered: the chapter is entered from the Tab menu or any agent's Council button, not
from the hotel; the council meets whenever the player opens it; Steam only, no itch.*
