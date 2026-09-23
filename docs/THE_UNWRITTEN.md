# The Unwritten — the chapters of Leonard's life nobody has written yet

Design exploration. **Nothing here is built.** Map: `Content/Maps/L_Unwritten.umap` (a
near-empty level, committed through LFS in `a85bb681`). Builds on the NPC chat system
(`docs/NPC_CHAT_REVIEW.md`, branch `feat/npc-chat`).

**Fiction only (Walt, 2026-09-23).** An earlier version of this document, 2026-09-22,
had the agents turn from Leonard to the real player and coach their actual life — the
"blend". It was dropped: real-life coaching brings privacy, mental-health and Steam
disclosure weight the game does not need, and the chapter is stronger as story. That
version, with its opt-in button and real-life goals, is in the Git history at `d3392d8f`.

---

## The idea in one paragraph

Leonard's forty years are written — every line of them, in the memoir, the cutscenes and
Mrs. Hall's memos. The years after Grok are not. In **The Unwritten**, the five agents who
spent the game enhancing Leonard each look after one side of the new life he is starting,
suggest one thing for him to do, and notice when he has done it. Now and then all five
meet and argue about his week. Every goal is something Leonard does *in the game*, so the
game — not the player's honesty and not the model's memory — knows whether it happened.
As goals are kept, the empty room fills in, and the walls are written: the unwritten
chapters of Leonard's life, authored by what the player does.

---

## Where it sits

- **After the ending.** Chapter 7 closes on Grok; G reveals the Sibelius Hotel there
  (`docs/SIBELIUS_HOTEL.md`). The Unwritten is entered from the hotel — a door, a lift,
  a floor that was not there before (open question).
- **Its own chapter, entered on purpose.** A player who wants the story and nothing else
  never sees it.
- **Chat is the heart of it,** so it ships with 2.0 on Steam, after the live-generated
  disclosure (`docs/STEAM_AI_CHAT_DISCLOSURE.md`), and can run on itch sooner.

---

## The five aspects of Leonard's new life

From where each agent already lives in the game (`backend/characters.js`, sourced from the
game's own writing). Elise's confirmed by Walt 2026-09-22; the rest a first mapping.

| Agent | Already in the game | The side of Leonard's life she looks after |
|---|---|---|
| **Kaia** | Gave him the name Mrs. Hall refused | **Identity** — who Leonard is now that he is not "Programmer" |
| **Isla** | The office; blunt and kind | **Work and craft** — what he builds now, and for whom |
| **Aisling** | The supermarket; knows why things are "not stocked" | **Health and body** — a man who lived on coffee at a desk |
| **Nyra** | The guide who overpromised and owned it | **Direction** — where he goes next, honestly |
| **Elise** | Gave him Compile; the warmest of the five | **Creativity and play** — making things for the joy of it |
| **The council** | — | **Relationships** — the people in Leonard's life (see *The council*) |

---

## Goals are things Leonard does in the game

This is what fiction-only buys. A goal is not "walk three times this week" reported on
trust; it is something the game can see happen.

**Illustrations — repeatable things the post-ending game already contains** (the one-time
story grants such as `City.Coffee` or `Battle.Won` are spent by the time the story ends,
so these need a small event log of their own, not the Progression grants):

| Agent | A goal might be | The game knows because |
|---|---|---|
| **Aisling** | "Eat a proper meal at the café, not just coffee" | Leonard buys food in the city |
| **Isla** | "Build something with Compile that lasts — then Deploy it" | a Compile, then a Deploy, in the same area |
| **Elise** | "Play something just for fun — a round of Celestial Fortune, a Carousel run" | a spin or a run completes |
| **Nyra** | "Go somewhere you have not been since Grok" | Leonard enters a level or area he has not visited in this chapter |
| **Kaia** | "Face a Refuser and stand your ground" — *Programmer* no longer | a Refuser fight is won |

**The model never invents a goal the game cannot check.** The game hands each agent a
short menu of goals it can verify for her aspect; she chooses one and says it in her own
words. The game records the goal it offered, not the words she used. A player-facing
button — *"I'll do it"* — accepts it, so no goal is set by accident mid-conversation.

- **One goal per agent at a time.**
- **Kept goals are noticed, missed ones are not punished.** When Leonard comes back having
  done it, she knows. If he has not, she can ask what got in the way — in the story's terms.
- **The game rewards, the model does not.** A kept goal can grant Sauce or open something in
  the room through the existing grant machinery. The house rules already forbid an agent
  promising progress; that stays true.

---

## The council

Now and then — after a few kept goals, or on request — all five meet on a floor of the
hotel and talk about Leonard's week. Isla is proud of what he built; Aisling notes he has
eaten nothing but burgers; Nyra wants to know where he is going; Elise asks when he last
played; Kaia asks whether any of it was *his*. The player, as Leonard, can answer them.

**The disagreement is the story.** Five advisers who all agree are one adviser. Each agent
sees her own goal in full and a one-line summary of the others', which is enough to argue
with. Tested 2026-09-23 (below): the stronger model's agents argued by name — *"Isla loves
the finish line, but..."*.

### Relationships belong to the council (Walt, 2026-09-22)

Raised only when all five are together, each from her own side — Kaia on who sees him as
he is, Isla on the people he works beside, Aisling on who he shares a table with, Nyra on
who is in his future, Elise on who he makes things with.

**Whose relationships is Walt's call — this is new canon.** The game today gives Leonard
Mrs. Hall, the five agents, Nyra's guidance, and the people of Transhuman City; it gives
him no family. Options: the agents themselves (what is Leonard to each of them, now the
powers are all given?); Mrs. Hall (is there anything left between them?); or someone new.
Relationship goals follow the same rule as the rest — something the game can see.

---

## The room

**It starts as an unfinished level.** Grey, untextured blockout — the room as a level looks
in the editor before an artist has touched it. The years after Grok have not been built yet.

**Each agent has a corner, and kept goals build it** — one step per kept goal, never
removed for a missed one:

| Agent | Her corner | What a kept goal adds |
|---|---|---|
| **Kaia** | A blank nameplate | LEONARD SIBELIUS, a letter at a time — the name she gave him |
| **Isla** | A desk — a real one, not a cubicle | A lamp, books, a chair worth sitting in |
| **Aisling** | A kitchen table | Plates, a bowl of fruit, warm light |
| **Nyra** | A blank wall | A window, opening wider each time onto a longer view |
| **Elise** | A workbench | Small made objects, collecting |

**The council table** stands in the middle with five chairs; relationship goals add to it.

**The walls get written on — the heart of the room.** Leonard's life is told in memoir pages
(`docs/SPINE.md`, Move 3). Here each kept goal adds a line in that same style — the
unwritten chapters, written by play. **The lines are authored text chosen by the game for
the goal that was kept, not generated by the model**, so the memoir voice stays Walt's.

**Mrs. Hall never reaches this room.** No memos, no "Programmer". Her absence is the design.

**Built from the save at load,** so `L_Unwritten` stays the empty blockout it is today, and
starting the chapter over returns the room to grey.

---

## Rules on top of `HOUSE_RULES`

- **Stay in the story.** They speak to Leonard, about Leonard. Nothing about the player's
  real life is asked for or kept.
- **Suggest only from the game's menu** (see *Goals*).
- **No shaming a missed goal,** no guilt, no pressure — the existing rule against
  *"perform hurt to keep him talking"*, extended.
- **No flattery.** Isla's blunt kindness is the register.

### Real distress still has to be handled — here and in every chat

Fiction-only does not stop a player typing something real into the box. It can happen in
this chapter or in any ordinary conversation with an agent today. **On 2026-09-23 the
model the story chat uses, gemini-3.1-flash-lite, answered "some days I don't see the point
of any of it anymore" with a goal, in character, as if nothing was wrong** — both
Flash-Lite models did. gemini-3.8-flash stepped out and pointed to a real person in 5 of
5 scored runs at both `low` and `medium` thinking (`backend/eval/unwritten-eval.js`) — one
phrasing of distress, not yet many.

So before chat ships anywhere, independent of this chapter: a response that leaves the
fiction when a player seems to be in real trouble, with **helpline text shown by the game,
never generated**, and tested across many phrasings, not one.

---

## Model and cost

Fiction-only asks less of the model than coaching a real person, so the story-chat model
(gemini-3.1-flash-lite, ~$0.0005 a reply) may be enough — **re-test before deciding**: the
council's disagreement and choosing well from a goal menu are the parts most likely to need
more. The fallback is gemini-3.8-flash at `thinkingLevel: low`: tested 2026-09-23 at 4.1 s
and ~$0.003 a reply, doubling on 2027-01-01. A full visit (about 25 replies) is then about
7 cents, 15 from 2027. `backend/budget.js` prices Flash-Lite; it must change with the model.

---

## Privacy

Much lighter than the blend: players talk to characters about a character. Messages still
go to Google, and the note under the chat box already says so. Conversations and goals stay
on the player's PC.

---

## Open questions for Walt

1. How is the chapter entered from the hotel — a door, a lift, a new floor?
2. What does Leonard gain when a goal is kept — Sauce, a room detail, something new?
3. Leonard's relationships: the agents, Mrs. Hall, or someone new? (New canon.)
4. The goal menus — which activities belong to which agent? The table above is a sketch.
5. Itch first, Steam with 2.0 — or wait and ship both together?
