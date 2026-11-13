# AGENT_CHALLENGES_PLAN — the five agents challenge you, with Steam achievements

*Written 2026-11-13 by Claude (Opus 5.5) at Walt's request ("Yes, write the plan"), after he
asked what new direction the game could take "besides being an extreme dance demo with side
games". Same process as `docs/SPINE.md`: read it, mark each decision in §7 yes / no / change,
then build phase by phase.*

**RATIFIED 2026-11-13 — Walt: "Approve as written."** All eight decisions in §7 stand as
written: the agent-to-game mapping, three tiers, 50/100/150 sauce, phase 1 first with no Steam
features, about 30 achievements, global leaderboards, Claude drafts the agents' lines for his
approval, and the deadpan memoir voice for achievement names.

**PHASE 1 BUILT 2026-11-13 (uncommitted):** the 28 tiers (§2, "as built"), the new records in
every game, the CHALLENGES tab, the tier banner and its sauce, a self-test in ProgressionSmokeTest.
Next: Walt plays to set the targets (§6), then it ships as 2.6.0.

---

## 1. Why this, and why now

**What players told us in the first two days on Steam** (Steamworks, 2026-11-11 to 11-13):

- The one review praised *"Minigames, in buckets. Pool, chess, bowling, and many more"*, the
  racing and the humor. It never mentioned the dancing. The side games are what is working.
- A third of players leave inside ten minutes; the typical player stays 16 minutes; players who
  get past half an hour mostly stay over an hour. The middle of the game holds people. What is
  missing is **a reason to keep playing the games they like, and to come back tomorrow**.
- Fable 5.2's critique said the same thing in other words: nothing pulls the player along.

**What the game has today:** more than a dozen side games, each finished and fun, but each a
dead end. You can beat Nyra at the three-point contest and nothing remembers it except a
counter on the RECORDS tab. Several games keep their best score **for the session only**
(pool, 4D billiards, orbital golf). There are **no Steam achievements and no leaderboards** —
the game does not use Steam's features at all yet (`SibeliusGame.Build.cs` has the Steam lines
commented out).

**The idea in one line:** each of the five agents challenges you at *her* game — the one in her
home — at three levels, and the living-room classics get house records. Every level you clear
pays sauce, unlocks a Steam achievement, and (for scores) posts to a Steam leaderboard.

**What it does not do** (Walt's standing rules): no new levels, no new story, no new AI or
transhumanism points. Every challenge lives inside a game that already exists, in a level that
already exists. It also needs nothing from the live AI chat, so it ships on the chat-off Steam
build.

---

## 2. The challenges

Each agent's challenge is the game in her home. Three tiers each: **Bronze** (a newcomer can do
it in one evening), **Silver** (practice), **Gold** (Walt's own best, or near it). The numbers
are placeholders — §6 says how to set them from real play.

| Agent | Her game (where) | Bronze | Silver | Gold | Already recorded? |
|---|---|---|---|---|---|
| **Kaia** | Lantern Burst (her jungle) | clear one lantern | score ≥ *A* | score ≥ *B* | best saved (`LanternBurst.sav`) |
| **Elise** | Orbital golf (her grassland) | finish the 3 holes | ≤ *N* strokes | ≤ *M* strokes | session best only (`OrbitGolfCourse::BestRound`) |
| **Aisling** | 4D billiards (her snowfield) | clear the table | ≤ *N* strokes | ≤ *M* strokes | session best only (`HyperBilliardsTable::BestStrokes`) |
| **Isla** | The Dragon's Wager (her keep) | steal the crown | score ≥ *A* | crown, never seen | best saved (`DragonsWagerSaveGame`) |
| **Nyra** | Three-point contest (chess park) | beat Nyra | score ≥ *A* | score ≥ *B* | wins saved (`ThreePointWins`) |
| **All five** | The Grand Prix (Transhuman City) | finish a race | win a race | win by *X* s | races/wins saved (`GrandPrixRaces`, `GrandPrixWins`) |

**House records** — the living-room games Leonard grew up with (Walt's rule: downstairs is what
he grew up with). One achievement each, plus a leaderboard where there is a score:

| Game | Achievement | Already recorded? |
|---|---|---|
| Chess | beat the Beginner / Hustler / Master ghost (3) | `ChessWins` (count only — the level is not saved) |
| Go | beat the Beginner / Hustler / Master ghost (3) | `GoWins` (count only) |
| Pinball | score ≥ *A* | `PinballBest` |
| Bowling | 150 / a perfect strike game | `BowlingBest` |
| Pool | clear the table in ≤ *N* strokes | `PoolTablesCleared` (count); best is session only |
| Video poker | a royal flush | not recorded |

That is **about 30 achievements** and **6 leaderboards** (Grand Prix best lap, three-point
score, pinball, bowling, Lantern Burst, Dragon's Wager). A number that size reads as a real game
on the store page without padding it.

**AS BUILT IN PHASE 1 (2026-11-13) — 28 tiers, `Source/SibeliusGame/ChallengeTypes.cpp`.** Four
changes from the tables above, each because the game records it more plainly:

| Tier | Planned | Built | Why |
|---|---|---|---|
| Kaia | clear a lantern / score A / score B | clear stage 1 / 3 / 5 | stages are a number a player understands; the score goes to the leaderboard |
| Isla Gold | crown, never seen | bank 600 treasure (Bronze any, Silver 300) | "never seen" is not recorded anywhere yet |
| Grand Prix Gold | win by X seconds | win with the agents set to Fierce | the pace setting already exists; a margin needs new timing |
| Poker | a royal flush (1 in 40,000 hands) | four of a kind | a royal is kept for phase 2 as a rare bonus achievement |

First-guess targets, to be set from Walt's play (§6): golf 11 / 8 strokes (par is 8), 4D
billiards 20 / 14 strokes (10 balls), three-point 12 / 18 (30 is perfect), pinball 2,000, bowling
150, pool 25 strokes, Dragon's Wager 300 / 600.

**The sauce.** Each Bronze pays sauce (suggest 50), Silver 100, Gold 150 — one-time grants, the
way chapters already pay. This matters for the opening: a new player who loses an agent's slot
trial can earn the power's 150 sauce at the living-room games, which are open from minute one.
The challenges become a second road to powers, next to the AI Temple's bowl.

---

## 3. What the player sees

1. **A CHALLENGES tab in the M menu** (beside STATUS / RECORDS / CONTROLS / GRAPHICS). One row per
   challenge: the agent's name, her game, the three tiers with ✓ or the target ("Silver: ≤ 9
   strokes — your best is 11"), and how to get there ("Elise's grassland: the portal at her
   bedroom window"). Unreached homes say where the portal is and what opens it — the tab doubles
   as a map of the game's best content, which is exactly what the opening is missing.
2. **A banner when a tier falls**: *"KAIA'S CHALLENGE — SILVER  +100 SAUCE"*, the way a power's
   ceremony banner works today, and the Steam achievement pops at the same moment.
3. **The agent notices.** With chat off on Steam, this is an authored line on the HUD, not a voice:
   the next time he greets her, her prompt line mentions it (*"You beat my lantern. Try the Gold."*).
   A short table of lines per agent per tier — Walt writes them, or approves drafts.
4. **Her F menu gets a CHALLENGE row** that travels to her home when it is unlocked (the same
   travel the portals use), so the challenge is one press from the agent who issues it.

---

## 4. The Steam part

**Technology** — small and already on disk:

- Unreal 5.7 ships Steam's SDK (**v1.61**) through the **SteamShared** plugin
  (`Engine/Plugins/Runtime/Steam/SteamShared`). Enable it in `SibeliusGame.uproject`; add
  `SteamShared` / `Steamworks` to `SibeliusGame.Build.cs`.
- One new `USteamStatsSubsystem` (GameInstanceSubsystem) owns everything Steam: initialise when the
  game is launched by Steam, request the player's stats, `SetAchievement` + `StoreStats` when a
  challenge tier falls, `FindLeaderboard` + `UploadLeaderboardScore` for scores. **It does nothing
  when Steam is not running** (the editor, a copy started from the builds folder), so no other code
  has to care.
- **The game's own save stays the authority.** Achievements are unlocked *from* the save, so a
  player who earned something while Steam was offline gets it the next time Steam is there (a
  re-sync on startup). Nothing in the game depends on Steam answering.
- The package script gains a check that `steam_api64.dll` is staged (check 103).

**Walt's part in Steamworks** (I can walk you through it, or do it in Chrome with you watching):

1. *Stats & Achievements → Achievements*: one row per achievement — an API name (I supply the
   list), a display name, a description, and two 64×64 icons (achieved, and a grey unachieved
   one). I make the icons from game screenshots, the way the Steam event covers were made.
2. *Stats & Achievements → Leaderboards*: the six leaderboards (or the game creates them on first
   use — your choice).
3. *Publish* the changes. (I do not expect a Valve review for achievements, but the Publish tab
   will say.)

---

## 5. Phases — each one ships as a Steam update

| Phase | What | Ships as | Size |
|---|---|---|---|
| **1** | Save the bests that are session-only today (pool, 4D billiards, orbital golf) and the chess/Go ghost level beaten; the `FChallengeTier` table and its smoke test; the **CHALLENGES tab**; the tier banner and its sauce. No Steam yet. | 2.6.0 | a day |
| **2** | `USteamStatsSubsystem`, the achievements in Steamworks, icons, the re-sync. | 2.6.1 | a day, plus your Steamworks steps |
| **3** | Leaderboards: upload on a new best; the CHALLENGES tab shows your global rank. | 2.6.2 | half a day |
| **4** | The agents notice: her authored line on the next greeting, the CHALLENGE row in her F menu. | 2.6.3 | half a day, plus your lines |

Phase 1 is useful on its own: even with no Steam features, the tab gives every player a list of
goals and a map of where the good games are.

---

## 6. Setting the numbers

Tiers that are too hard are the same wall as the slot trial; too easy and they mean nothing.

1. Phase 1 logs every finished game's result (score, strokes, time) to the save.
2. Walt plays each agent's game a few times as a newcomer would; I read the results.
3. **Bronze** = a newcomer's first good game. **Gold** = Walt's best, or a little better. Silver
   between.
4. After a week on Steam, the leaderboards show where real players land; the tiers can move in a
   patch (an unlocked achievement stays unlocked).

---

## 7. Decisions for Walt

Mark each one — the build waits on 1 to 4.

1. **The mapping** — each agent's challenge is the game in her home (Kaia: Lantern Burst, Elise:
   golf, Aisling: 4D billiards, Isla: Dragon's Wager, Nyra: three-point, all five: Grand Prix)?
2. **Three tiers** (Bronze / Silver / Gold), or one challenge per agent?
3. **Sauce** — 50 / 100 / 150 per tier, so challenges become a second road to the powers?
4. **Phase 1 first**, with no Steam features, shipped on its own?
5. Achievements: about 30, as in §2 — or a smaller first set?
6. Leaderboards: global (everyone on Steam), or friends-only?
7. The agents' lines in phase 4: you write them, or I draft and you approve?
8. Achievement names and descriptions: the deadpan voice of the memoir (*"Hustled the Hustler —
   beat the chess ghost at its own game"*), or plain (*"Chess: beat the Hustler"*)?

---

## 8. What this is not

- Not new levels, not new story, not new AI — every challenge is a game that already exists.
- Not the dance studio or the slot floor. Both stay good later directions; each could add its own
  achievement set once this framework exists (a routine shared by code; a machine tuned to a
  target RTP).
- Not dependent on Valve approving the chat. When the chat arrives, the agents can talk about the
  challenges in their own words — `ChatDeeds.h` already feeds them the records.
