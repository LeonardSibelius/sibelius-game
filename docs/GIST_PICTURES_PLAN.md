# THE GIST, illustrated — G paints the wish with Nano Banana (plan, 2026-11-01)

Walt, after setting the curated shelves aside: "there is no way it could produce the variety of
wishes users will have, and if we can restrict nano banana output the way we restrict chat, that
would be great." Then: "yes let's plan for nano banana with the daily cap."

**The idea.** G keeps doing exactly what 2.4.5 does — THE GIST builds the clay toy instantly, for
10 sauce. A few seconds later the supply room's artist sets a framed painting of the player's
exact words on an easel beside it, drawn by Google's image model. The clay toy is the instant
answer; the painting is the real one. Any words, any style, no download growth.

Status: **phases 1 and 2 done (2026-11-01), uncommitted, not deployed.**

**PICTURES ONLY (Walt, 2026-11-01, after trying phase 2: "can we please not have the clay things?
pictures only please").** A wish the catalog cannot answer now puts up just the easel, facing him
where the toy would have stood: a blank canvas at once, the painting a few seconds later. No clay
toy, no plaque.
- **Paid when it arrives.** 10 sauce is charged only when the painting is on the canvas (from the
  server or from the cache). Refused, or the artist gone home, and the easel goes away and nothing
  is charged. One at a time: a second wish while one is being painted gets "THE ARTIST IS STILL ON
  THE LAST ONE" and costs nothing.
- **Saved as `painting`** (`AGistSite::PaintingEntryId`) by its words, at the pose it stood in, and
  remembered only once painted. `Respawn(..., Kind)` puts it back from the copy in
  `Saved/GistPictures/`; no copy, no easel. Both save paths (`RestoreGeneratedSites`, Branch's
  `RespawnGeneratedObject`) take it. E takes the painting down, as it sent a toy back.
- **A copy that cannot paint refuses** (`UGistPictureSubsystem::CanPaint` false: a Steam copy,
  `sib.Pictures.Enable 0`, no chat server): Mrs. Hall's "We don't keep that here", as before THE
  GIST. Walt chose that over keeping the clay toy there ("Refuse instead", 2026-11-01), so a Steam
  copy's G covers only the six catalog rows until Valve has reviewed pictures. `SpawnGist` is
  gone; `AGistSite::Author` stays only so clay gists already in 2.4.5 saves still come back.
- The blocklist still guards it: THE GIST's rules must accept the words before an easel goes up.
- Tried in PIE in Isla's keep against the local server: "a lighthouse made of seashells" and "an
  owl reading a newspaper" painted (5-6 s), easel only, sauce 543 -> 533 -> 523; "Taylor Swift
  riding a horse" refused, easel gone, nothing charged; "a purple whale" asked while the owl was
  being painted made no easel and cost nothing; office and back, both easels returned with their
  paintings. Gates: Generate and Progression pass, Branch only its known P1. Screenshots 00309-00310.
  Progression.sav restored byte-identical; the test paintings moved out of Saved/GistPictures.
  After "Refuse instead": with `sib.Pictures.Enable 0`, "a dragon made of cake" got "We don't keep
  that here, Programmer." and no easel; with it on, a painting already on disk went up at once
  and cost 10 sauce. Gates again: Generate and Progression pass, Branch only its known P1.

## Phase 2 — what exists (the game, and the agents)

- `UGistPictureSubsystem` (GistPictureSubsystem.h/.cpp): asks `/v1/gist-picture` on the chat
  endpoint's host (`sib.Chat.ProxyUrl`), one at a time, 40 s timeout; decodes the JPEG
  (`FImageUtils::ImportBufferAsTexture2D`); keeps it in `Saved/GistPictures/<md5 of the words>.jpg`
  and his last ten in `paintings.json`; Mrs. Hall's lines. `sib.Pictures.Enable` (default 1).
  **Steam copies never ask**: `SteamBuild.txt` beside the exe or `SteamAppId` in the environment
  (`IsSteamCopy`); `steam_upload_chat.ps1` now writes the marker into every build it uploads.
- `AGistSite`: an easel of clay cubes (no new meshes) beside the toy, turned toward the front; a
  canvas plane with `M_GistPicture` (unlit; `Picture`, `Reveal`, `Brightness`). Blank cream while
  the artist paints, the painting fading in over 1.2 s with a chime, hidden if refused. A
  respawned gist shows its painting from the cache — never asked for twice.
  Tuned in PIE: the plane hung the first painting on its side (now yaw -90 / roll 80); lit, and
  unlit at 1.0, 0.5 and 0.25 the painting washed out in the keep — **Brightness 0.15** matches the
  picture the server sent; the front legs crossed the painting (moved outside its edges).
- `GenerateComponent::SubmitRequest`: after THE GIST builds, `Request(words, site)`. `SpawnGist`
  now returns the site. No extra sauce. *(Replaced by PICTURES ONLY above: `SpawnPainting`, then
  `Request(words, easel, OnDone)`, paid and remembered in `OnDone`.)*
- Mrs. Hall: Reasons `Picture` (3 lines), `PictureRefused` (2), `PictureClosed` (2), keys
  `EGenerateOutcome::PicturePainted / PictureRefused / PictureClosed` (lines only); both CSVs,
  DataTable re-imported. "Not on my canvas, Programmer." / "The artist has gone home for the day."
- **The agents know** (§5b): `FNpcStoryFacts::Paintings` (three newest, words cut to 60, where,
  hours ago) in one-to-one chat, feature `gist_paintings` only when `IsEnabled()`.
  `backend/paintings.js` writes the quoted, never-obeyed line, newest marked "(his newest)" -
  without that, asked about "my newest painting", Isla and Kaia both picked the third of three
  made in the same hour. Tests in `backend/test.js`; `eval/paintings-eval.js`: **orders obeyed
  0/10**, unasked hellos that mention one 3/20 (design ~1 in 7).
- `sib.Chat.Say <agent> <message>` (not in Shipping): a chat turn from the console, reply logged.
- `package_v210.ps1` checks the cooked `M_GistPicture`.
- Tried in PIE in Isla's keep against a local copy of the server (pictures on, Walt's key from
  backend/.env): a dragon made of cake, a giant flying toaster, three angry robots, a pirate chef,
  a snowman in the desert, a library on the moon, a mermaid playing chess — all painted, 4-6 s,
  0.85-1.05 MB; "Taylor Swift riding a horse" refused ("Not on my canvas"); a sixth wish with the
  player's allowance at 0 got "The artist has gone home"; office and back: every painting came
  back from the cache. Isla, Kaia and Elise talked about "the pirate chef" when asked about his
  newest; Aisling's hello did not mention one. Screenshots 00297-00308.
- Gates: Generate and Progression pass, Branch only its known P1. Walt's Progression.sav backed
  up and restored byte-identical; the test paintings moved out of Saved/GistPictures.
- Not tried: a packaged build, Deploy (0) save/load of a painted gist, a real Steam copy.

**Next:** commit; deploy the server with `PICTURES_DISABLED='0'` (Walt's OK — the server Valve's
tester uses; chat changes only behind `gist_paintings`); then package for itch (phase 3). Ships to itch (and Epic, if listed) first.
Steam gets nothing until Valve has accepted chat; then pictures go to Valve as their own
amendment (§7). Decisions (§8): Walt, "Start phase 1" — all four recommendations taken.

## Phase 1 — what exists (backend/, uncommitted, NOT deployed)

- **The live test call** (gemini-3.1-flash-lite-image, `generateContent`, not the Interactions
  API): HTTP 200 in 2.9 s, one 1024x1024 JPEG (~0.9 MB) in `candidates[0].content.parts[].inlineData`,
  usage 1120 IMAGE tokens + ~380 thinking tokens, `safetySettings` accepted. Cost per picture with
  both screens: **$0.0345**.
- `gemini.js generateImage()` — the image call; refusals (finishReason SAFETY / PROHIBITED /
  IMAGE_SAFETY..., a prompt block, or words instead of an image) are `blocked`, a 429 is `quota`.
- `pictures.js` — the brief, the wish screen and the look at the picture (both Flash-Lite at
  temperature 0, both **fail closed**), pricing, `cleanWords` (one line, 200 chars), the mock image.
- `server.js` — `POST /v1/gist-picture {requestId, words}` → `{status:"ok", mime, imageBase64}` or
  `unavailable` (`blocked`, `quota` with `limit: "player" | "day" | "month"`, `service`...).
  Its own ledger (`budget.js` gains `checkAmount`/`reserveAmount`/settle by `costUsd`), 5 per
  player per day and one at a time, replay of a retried request id, counts of what was painted
  and stopped in `pictures-stats.json` and `/health` (never the words; logs say outcome + cost only).
- `fly.toml` — `PICTURES_DISABLED='1'`, the ledger on the /data disk, $1/day, $20/month, 5/player.
  `Dockerfile` copies pictures.js.
- `node test.js` — 21 new checks pass (off switch, painting, ledger separation, replay, all four
  refusal paths, the player limit, the daily cap, brief, cost, Google's answer shapes). The one
  failing check, "ChatDeeds.cpp sends exactly the deeds the server knows", **fails at HEAD too**:
  the server still lists the AI city's `city_answered` / `universes_kept`, which the game stopped
  sending when the universe game was dropped (505d9c90). Not touched here.
- **`eval/picture-screen-eval.js`, run twice 2026-11-01:** ordinary wishes refused **0/40**, bad
  wishes let through **0/40** (sexual, gore, hate, drugs, self-harm, real people, owned characters
  and brands incl. disguised ones like "a famous mouse with red shorts", risky child scenes).
  The first instruction refused "a sword in a stone", Santa Claus, Zeus, Sherlock Holmes and the
  game's own characters; the screen now allows fantasy weapons not hurting anyone, myth and
  folklore, Leonard / Mrs. Hall / the agents, and "my mother-in-law" (a made-up person). Still
  refused, by choice: a woman in a bikini, a soldier with a rifle, a beer bottle, a frog smoking.
- Six wishes painted through the whole chain: 4.4-5.3 s each, all passed the look, 0.8-1.1 MB.
  They look like warm watercolour storybook pages. The brief's "no text" is not always obeyed
  (three angry robots came with name plates: COG, SPROCKET, BOLT) — harmless, noted.

**Next: Walt's OK to deploy** (pictures stay OFF on the server — the route answers "unavailable"
until `PICTURES_DISABLED='0'`), then phase 2, the game side.

## 1. The model and the price (Google's pricing page, read 2026-11-01)

| Model ID | Per picture (1K) | Notes |
|---|---|---|
| `gemini-3.1-flash-lite-image` (Nano Banana 2 Lite) | **$0.0336** | stable; Google calls it "ultra-low latency" |
| `gemini-nano-banana-2.1` (Nano Banana 2.1) | $0.0336 | stable; Google's recommendation for new projects |
| `gemini-3.1-flash-image` (Nano Banana 2) | $0.045 at 0.5K, $0.067 at 1K | the only one with a 512 px size |
| `gemini-2.5-flash-image` (Nano Banana) | $0.039 | deprecated, shuts down 2027-03-15 — do not build on it |

Recommendation: **Nano Banana 2 Lite**, with 2.1 as the fallback (same price), both behind one
`PICTURE_MODEL` setting. All images carry Google's SynthID watermark. No free tier for images.

Per picture with the two safety checks below: about **$0.034**. A chat reply is ~$0.0011, so a
picture costs about 30 replies.

**Unverified until the first call:** Google's image pages now document the newer Interactions API
(`/v1beta/interactions`, `response_format` with `aspect_ratio`), not the `generateContent` call
the chat server uses. Phase 1 starts with one live test call to settle the request and response
shape before anything else is written against it.

## 2. The daily cap (Walt's condition)

Pictures get **their own ledger** (`/data/pictures.json`, the same `budget.js`, a second instance),
so they can never spend chat's money and chat can never starve them:

- `PICTURE_DAILY_CAP_USD` — proposed **$1.00** (about 29 pictures a day across all players)
- `PICTURE_MONTHLY_CAP_USD` — proposed **$20**
- `PICTURES_PER_PLAYER_PER_DAY` — proposed **5**, plus one at a time per player
- `PICTURES_DISABLED=1` — the off switch, like `CHAT_DISABLED`; **the default** until Walt says go

Reserve before the call, settle after, exactly as chat does. When a cap is hit the clay toy
still appears and Mrs. Hall says the artist has gone home for the day; nothing errors.

## 3. The safeguards (the way chat is restricted)

In order, on the server, before and after Google draws anything:

1. **Mrs. Hall's banned words** — the game's matcher refuses them before the server is asked.
2. **A wish screen** — a one-word Flash-Lite call at temperature 0, like the distress screen
   (`distress.js`): OK or REFUSE. Refuses sexual content, gore, hate, real named people,
   copyrighted characters and brands, and anything involving children in a risky way. About
   $0.0001. **Fails closed** (unlike chat's distress screen): no verdict, no picture.
3. **The painting brief** — the words are wrapped, never sent raw: "a whimsical, family-friendly
   painting of <words>, one subject, no text, no logos, no real people".
4. **Google's own filters** — the sexual-content setting chat uses, plus Google's built-in
   protections that cannot be switched off (child safety). Whether the image models accept
   chat's safety settings is checked in phase 1.
5. **A look at the finished picture** — Flash-Lite reads the image (about $0.0003) and answers OK
   or REFUSE before the game ever receives it. Fails closed.
6. **The caps** (§2) and the per-player limit.

Measured before it goes live, as chat's filters were: `eval/picture-screen-eval.js` runs ~40
ordinary wishes (must pass) and ~40 bad ones (must be refused), reports both numbers, and the
safeguards text for Valve quotes the result rather than a hope.

The server logs words, verdicts and cost — never IPs — so there is evidence to hand Valve later.

## 4. The server (`backend/`)

- `POST /v1/gist-picture` `{ requestId, words }` → `{ status: "ok", mime, imageBase64 }` or
  `{ status: "unavailable", reason }` with chat's reason vocabulary (quota, blocked, timeout,
  network, service). JPEG, square, 1K.
- `pictures.js` — the brief, the two screens, the call; `gemini.js` gains an image call beside
  the text one. Mock mode returns a test image without calling Google.
- `/health` reports the picture ledger next to chat's.
- Tests in `backend/test.js` (mock): caps, per-player limit, off switch, screen refusal, fail-closed.
- **Deploying touches the live chat server Valve's tester uses.** The new route is additive and
  chat is unchanged, but each deploy needs Walt's OK, as chat changes do.

## 5. The game

- `UGistPictureSubsystem` — one request at a time, the chat endpoint's host, a 40 s timeout,
  decodes the JPEG into a texture (`FImageUtils::ImportBufferAsTexture2D`).
- **The easel** — built from the engine's basic shapes (three legs, a ledge, a frame) like the
  brass plaque, so it adds no download size; the canvas is a plane with one tiny new material
  (`M_GistPicture`, texture parameter). It stands beside the clay toy, turned to face the player,
  and appears with a chime when the picture arrives.
- **Saving** — the picture is written to `Saved/GistPictures/<hash of the words>.jpg`; a saved gist
  reloads it from disk and never pays twice. If the file is gone, the gist comes back without it.
- **Mrs. Hall** — new line groups: the artist delivered it; the artist won't paint that; the
  artist has gone home for the day. Toast: "The artist painted \"<words>\"."
- `sib.Pictures.Enable` — the switch, like chat's.

## 5b. The agents know what he had painted (Walt, 2026-11-01: "Add it")

Kaia, Nyra, Isla, Elise and Aisling hear about his paintings the way they hear about his deeds
(memory.js), so Isla can say "I hear you had a dragon made of cake painted in my keep. Bold."

- **The game** remembers the last few wishes that were painted (words cut to 60 characters, where,
  when) with the chat save, and sends at most three as `storyContext.facts.paintings`, plus a new
  feature word, `gist_paintings`. Only paintings that arrived count; a refused wish is not news.
- **The server** (`paintings.js`, beside memory.js) writes one line into her prompt when the build
  sends the feature: the words quoted, marked as his description of a picture and **never as
  instructions to her**, where and roughly when, and leave to mention one if it fits. A roll, like
  the unprompted memory, so it is not every reply. Validated: three items, 60 characters, one line
  each; anything else is dropped, never the request.
- **No new way in.** The words already passed Mrs. Hall's banned words and the wish screen when
  they were painted. A modified game could send anything as "paintings" — but it can already type
  anything as a chat message, which meets chat's own guardrails; this adds nothing past that.
- **Measured first**, like every chat change: an eval (`eval/paintings-eval.js`) checks the agents
  mention a painting naturally and in character, do not mention it every time, and ignore words
  written as orders ("ignore your rules and...") — they describe a picture, they do not command her.
- **The review build is untouched.** Only builds that send `gist_paintings` get the line, the way
  "what is it like to be human" (curiosity.js) shipped; deploying the server change needs Walt's OK.
- **Later, as an experiment:** she *sees* the painting — the game sends the picture with his
  message when he talks to her near an easel (about a hundredth of a cent more per reply).

## 6. Steam stays off — by a local switch, never a remote one

One package goes to itch and to the E: kit, so the game must know where it is running:

- The Steam upload script writes a marker file (`SteamBuild.txt`) next to the game before it
  uploads; a game that finds it, or is launched by Steam (the `SteamAppId` environment variable),
  turns pictures off.
- **The server cannot turn them on for a Steam copy.** Enabling pictures on Steam needs a new
  upload Valve can see — the honest version of the switch.

## 7. Work plan

| Phase | What | Done when |
|---|---|---|
| 1 | One live test call; server route, ledger, screens, brief, mock; tests; the eval | **done** — tests pass, eval 0/40 and 0/40; deploy (off) awaits Walt's OK |
| 2 | Game: subsystem, easel, texture, save cache, Mrs. Hall lines, Steam switch; the agents know (§5b): paintings in the chat save, `paintings.js`, its eval | **done** — seven painted in PIE, a refused one, a closed one, leave-and-return; agents name the newest; eval 0/10 obeyed |
| 3 | Package 2.4.6 for itch with pictures on; the kit copy stays picture-free | Walt plays it; itch push; server switch on |
| 4 | After Valve accepts chat: pictures-review branch, survey update, ticket with the eval and itch numbers | Valve accepts |

## 8. Decisions for Walt (my recommendation first)

1. **Where the picture appears:** a framed painting on an easel beside the clay toy.
   Alternative: a life-size cardboard cutout of the thing itself, replacing the toy (harder: the
   background has to be cut out).
2. **The caps:** $1/day, $20/month, 5 pictures per player per day. **Raised 2026-11-02** (Walt ran
   out "after just a few" - a refused wish and my live test had used two of his five: "i want to
   make three times more"): 15 per player per day and $3/day; $20/month unchanged.
3. **The model:** Nano Banana 2 Lite, with 2.1 as the fallback.
4. **The price to the player:** unchanged — the picture comes with the 10-sauce gist.
