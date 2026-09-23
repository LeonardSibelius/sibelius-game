# The Unwritten — the agents' council

Design exploration. **Nothing here is built.** Map: `Content/Maps/L_Unwritten.umap` (a
near-empty level, committed through LFS in `a85bb681`). Builds on the NPC chat system
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
| Council turns (five replies in order, each seeing the last) | Tested in `backend/eval/comic-eval.js`; **not in the server** |
| The fact list from the save | **Not built** — a small allowlisted `storyContext` addition |
| A chat panel that shows five speakers | **Not built** — the current panel shows one agent |
| The room | **Not built** — see below |

---

## The room — fixed, designed once (Walt, 2026-09-23)

`L_Unwritten` is **one room, built once, that does not change with progress:** the five
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

## Open questions for Walt

1. How is the chapter entered from the hotel — a door, a lift, a new floor?
2. Which facts should the council get to roast? The list above is everything already
   recorded; fewer may be funnier.
3. Leonard's relationships: the agents, Mrs. Hall, or someone new? (New canon.)
4. Does the council meet whenever the player sits down, or only now and then?
5. Itch first, Steam with 2.0 — or wait and ship both together?
