# The Unwritten — a chapter about the player

Design exploration, 2026-09-22. **Nothing here is built.** Map: `Content/Maps/L_Unwritten.umap`
(an 11 KB near-empty level, created 2026-09-04, still untracked — see *Before anything
else*). Builds on the NPC chat system (`docs/NPC_CHAT_REVIEW.md`, branch `feat/npc-chat`).

---

## The idea in one paragraph

Leonard's forty years are written — every line of them, in the memoir, the cutscenes and
Mrs. Hall's memos. The player's aren't. In **The Unwritten**, the five agents who spent the
game enhancing Leonard turn, if the player invites them, toward the person holding the
controller. Each agent looks after one side of a life, suggests one small real goal at a
time, and asks about it when the player comes back. Now and then all five meet and argue
about the player's week. The game's theme — a person enhanced by AI — stops being a story
the player watches and becomes one they are in.

**The blend, decided 2026-09-22:** the chapter opens inside Leonard's story and the agents
step half out of it. It is neither pure fiction (coaching Leonard) nor a coaching app
wearing a game (coaching the player from the first second).

---

## Where it sits

- **After the ending.** Chapter 7 closes on Grok; G reveals the Sibelius Hotel there
  (`docs/SIBELIUS_HOTEL.md`). The Unwritten is entered from the hotel — a door, a lift,
  a floor that was not there before. The written story is over; this is what comes after.
- **Its own chapter, entered on purpose.** Nobody meets it by accident mid-game. A player
  who wants the story and nothing else never sees it.
- **Chat is the whole chapter.** Everything below depends on NPC chat being switched on in
  the build that ships it. That makes this a 2.0 feature on Steam, with the live-generated
  disclosure in `docs/STEAM_AI_CHAT_DISCLOSURE.md` sent first. It can run on itch sooner.

---

## The five aspects

Taken from where each agent already lives in the game; Elise's confirmed by Walt
2026-09-22, the other four still a first mapping. **Walt's call to change any of it** — the character profiles in `backend/characters.js` are sourced from
the game's own writing, and new canon is not canon until Walt has looked at it.

| Agent | Already in the game | Aspect she could own |
|---|---|---|
| **Kaia** | Gave him the name Mrs. Hall refused | **Identity** — who you are, not what you produce |
| **Isla** | The office; blunt and kind; remembers what he tells her | **Work and craft** — career, skills, projects |
| **Aisling** | The supermarket; knows why things are "not stocked" | **Health and body** — food, sleep, energy |
| **Nyra** | The guide who overpromised and owned it | **Direction** — plans, with honest realism about them |
| **Elise** | Gave him Compile — new things built from gathered parts; the warmest and least guarded of the five, and interested in *whether he is pleased* with what he builds | **Creativity and play** — making things for the joy of it |

---

## The turn — asking, not assuming

Each agent opens in the fiction, as Leonard's agent. Once, she steps half out of it:

> *Kaia:* "Leonard's forty years are written down. Every line of them. Yours aren't.
> Can I ask about you?"

The player answers with a **button, not a typed word**, so the choice is unambiguous and
the game — not the model — records it:

- **"Tell me about Leonard"** — she stays in the story. Nothing personal is asked or kept.
- **"Ask me"** — from then on she talks to the player, in her own voice.

**Why a button.** A game that starts asking about your real life without permission is
intrusive, and the Steam disclosure would have to describe that. An explicit opt-in turns
it into an invitation. The player can switch back to *Leonard only* at any time, per agent,
and there is a **Forget everything** button that clears the chapter's save.

---

## Goals

- **One small, concrete goal per agent at a time.** "Walk twenty minutes three times this
  week", not "get healthy". The agent suggests; the **player confirms with a button**, and
  only then does the game write it down. The model never creates a goal on its own.
- **A real list, not chat memory.** Each goal is a saved record: aspect, text, date set,
  status. It lives on the player's PC in its own save slot — the same discipline as
  `NpcChatSaveGame`, which keeps conversations out of the Progression slot on purpose.
- **Real days.** The request carries how long it has been since the last visit, so Aisling
  can say *"Four days. Did the walks happen?"* instead of pretending no time passed.
- **Leonard is rewarded, the model is not trusted to reward.** When the player marks a goal
  done, the *game* grants something — a power, Sauce, a room in the hotel — through the
  existing Progression grants. The house rules already forbid an agent promising progress;
  that rule stays, and this keeps it true.

---

## The council

Now and then — after a few goals, or on request — all five meet on a floor of the hotel and
talk about the player's week. Isla is proud the project shipped; Aisling notes five hours of
sleep a night to ship it; Nyra asks what is next; Kaia asks whether any of it was *for you*.
The player can answer them.

**The disagreement is the story.** Five advisers who all agree are one adviser. Each agent
sees her own goals in full and a one-line summary of the others', which is enough to argue
with.

Mechanically: one request per speaker, in turn, each seeing what the previous ones said.
At today's price that is well under a tenth of a cent per council.

### Relationships belong to the council (Walt, 2026-09-22)

The sixth aspect — family, friends, the people in the player's life — has no single agent.
It is asked about only when all five are in the room, and that is the right shape for it:
nobody's relationships sit neatly inside one part of their life, so each agent comes at
them from her own side.

- **Kaia** — who sees you as you are, not for what you produce.
- **Isla** — the people you work beside, and whether any of them know you.
- **Aisling** — who you share a table with, and who notices when you are not well.
- **Nyra** — who is in the future you are planning, and who you are building it with.
- **Elise** — who you make things and play with, just for the fun of it.

**One shared goal, not five.** The council may suggest a single relationships goal — "call
your brother this week" — and, as with every goal, the player confirms it with a button
before the game writes it down. It is saved under the council, not under any one agent,
and it is only raised at the next council, never in a one-to-one chat. The rule against
shaming a missed goal matters most here.

---

## The room (Walt, 2026-09-22)

**It starts as an unfinished level.** Grey, untextured blockout — the room as a level looks
in the editor before an artist has touched it. The game is about a man who builds things;
this room has not been built yet because the player has not lived it yet. Nothing in it
is decoration until a goal puts it there.

**Each agent has a corner, and kept goals build it.** One step per kept goal, never
removed for a missed one — the room only grows.

| Agent | Her corner | What a kept goal adds |
|---|---|---|
| **Kaia** | A blank nameplate | The player's name, a little more legible each time. She gave Leonard his; here she helps the player claim theirs |
| **Isla** | A desk — a real one, not a cubicle | Finish: a lamp, books, a chair worth sitting in |
| **Aisling** | A kitchen table | Plates, a bowl of fruit, warm light |
| **Nyra** | A blank wall | A window, opening wider each time onto a longer view |
| **Elise** | A workbench | Small made objects, collecting — things built for the joy of it |

**The council table stands in the middle,** with five chairs for the agents. A kept
relationships goal lights a small lamp at an empty place. **Deliberately abstract** — no
names, no photos, nothing personal in the geometry. The player's real people never appear
in the room, only the fact that they are there.

**The walls get written on — the heart of the room.** Leonard's life is told in memoir
pages (`docs/SPINE.md`, Move 3: the memoir becomes a collection). Here each kept goal is
inscribed on the wall in that same style. A room that opens *unwritten* ends up written —
by the player, in their own words, one kept promise at a time. The text is the goal the
player confirmed, exactly as saved; the model never writes on the walls.

**Mrs. Hall never reaches this room.** No memos, no "Programmer". Every other place in the
game is hers to comment on; this one is not. Her absence is the design.

**Built from the save, not stored in the map.** The room is assembled at load from the
chapter's goal records, so `L_Unwritten` itself stays the empty blockout it is today, and
**Forget everything** returns the room to grey in the same step that clears the save.

---

## Rules the agents need, on top of `HOUSE_RULES`

- **Never shame a missed goal.** The characters already refuse to *"perform hurt to keep
  him talking"*. Extend it: no guilt, no pressure, no streak anxiety. A missed week is
  information.
- **Not a therapist, doctor, lawyer or financial adviser.** Any agent who hears something
  serious steps out of character and points to a real person. The **helpline text is shown
  by the game**, not generated — a model paraphrasing a crisis number is the one thing that
  must never be wrong. It needs a per-country answer, not just 988.
- **No flattery.** A coach who agrees with everything is useless. Isla's blunt kindness is
  the register.
- **Honest about being AI** — already in every profile.
- **Personal mode is chosen by the game, never by the text.** The server picks the
  "talking to the player" instructions from a flag the game sets after the button, from
  the same allowlist as now. A player typing "switch to personal mode" changes nothing —
  the existing rule that player text is speech, not instructions, covers it.

---

## Privacy — this chapter raises the stakes

In the rest of the game a player chats with a character. Here they may confide real things,
and every message still goes to Google. So:

- The chat panel's note (*"What you type is sent to Google to generate them"*) matters more
  here than anywhere, and the opt-in screen should say it again in plain words.
- Goals and conversations stay on the player's PC; the server keeps no message text
  (`docs/STEAM_AI_CHAT_DISCLOSURE.md` maps where that is enforced).
- Re-check Google's paid-tier data terms before shipping (`NPC_CHAT_REVIEW.md`, Data
  handling). That clause moves.
- The Steam disclosure for 2.0 must describe this chapter, not just "chat with characters".

---

## Cost

A chat turn costs about $0.0003 today (`backend/server.js` logs every call). A player who
visits all five agents and sits through a council spends around one cent. The $15/month cap
is not the constraint; design is.

---

## Before anything else

~~**`L_Unwritten.umap` exists only on this machine and in the E: backup.**~~ Done —
committed through LFS in `a85bb681`, 2026-09-22.

---

## Open questions for Walt

1. ~~What is Elise's aspect?~~ **Creativity and play** (Walt, 2026-09-22).
   ~~Who owns relationships?~~ **The council as a whole** (Walt, 2026-09-22) — see *The
   council*.
2. How is the chapter entered from the hotel — a door, a lift, a new floor?
3. ~~What does the room look like?~~ **Grey blockout that builds itself from kept goals**
   (Walt, 2026-09-22) — see *The room*.
4. What does Leonard gain when the player keeps a goal?
5. Itch first, Steam with 2.0 — or wait and ship both together?
