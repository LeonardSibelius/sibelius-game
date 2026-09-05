# STEAM_PLAN — getting Leonard Sibelius onto Steam

Written 2026-08-26, the day v0.9.7.7 went to itch and Walt said he might be ready.
State at the time: four public itch releases, 4.9 GB download, downloaded and played
by other people on other machines.

Facts here were checked against Valve's own documentation on 2026-08-26 rather than
recalled. Sources at the bottom. **Re-check the fee and the waiting periods before
acting** — they are the two things Valve changes.

---

## 1. The shape of it, before any steps

**$100 per game.** Not refundable, but recouped out of your first payment once the
game has cleared $1,000 in adjusted gross revenue. A free game still pays it.

Then two clocks:

| clock | length | starts when |
|---|---|---|
| Steam Direct review | **30 days** | you pay the $100 |
| Coming Soon page | **2 weeks** | the store page goes public |

**They overlap, and that is the single most useful fact on this page.** You do not add
them. Pay early, get the page up early, and both run at once — so the honest estimate
is **about a month**, not six weeks. Almost every "it takes forever" story comes from
someone who did them in sequence.

---

## 1b. WHERE WE ACTUALLY ARE

**Steam Direct fee paid 2026-08-26.** Steamworks Distribution Agreement signed, name
and address entered, $100 charged. Both clocks can now be counted from a real date:

| milestone | date |
|---|---|
| fee paid, 30-day clock starts | **2026-08-26** |
| **Coming Soon page must be live by** | **2026-09-11** |
| earliest possible release | **2026-09-25** |

That middle row is the one to act on. The 2-week page requirement only finishes
alongside the 30-day review if the page goes public by **11 September**. Later than
that and the store page becomes the critical path, pushing release past the 25th by a day for every day it slips.

Remaining onboarding steps at this point: tax interview, bank details, then the app
itself.

**ONBOARDING DOES NOT COMPLETE WHEN THE FORMS ARE SUBMITTED.** After the tax
interview and bank details are in, Steamworks still shows "Continue the Onboarding
Process" and Continue leads to "tax info verification pending". Third-party tax
verification is a hard gate: the app cannot be created until it clears, so the store
page cannot be started either. Valve puts it at 2-7 business days; submitted
2026-08-26, so 4 September is the worst case.

The 30-day clock keeps running through it, so the wait is free against the release
date — but it eats into the run-up to the 11 September page deadline. If it is still
pending after 7 business days, THEN open a support ticket.

The useful consequence: the thing actually on the critical path, capsule art, needs

### What actually happened at the KYC gate (2026-09-03)

The section above predicted a **passive** 2–7 business day wait and said to open a
ticket after seven. Reality was different in a way worth writing down: it became an
**active document request that then stalled silently**.

- **26 Aug** — tax interview submitted. Steamworks then showed *"Identity verification
  required — additional documents are necessary to complete your KYC verification."*
  Not "pending". A request, sitting behind a **View details** button, which will wait
  forever if nobody clicks it.
- The documents go to **Tax Identity Solutions**, a third party, through a **Dropbox
  file request** — not to Valve. Valve support is a middleman here.
- **26–27 Aug** — uploaded US driver's licence front, rear, and a selfie holding it.
- **3 Sep** — uploaded the *same three files* again. Identical files cannot fail once
  and pass the next time; two attempts produced two identical silences.
- **3 Sep** — re-uploaded a resized set. The originals were 12 MP, **7.5–7.8 MB each**,
  and these processors commonly cap at 5 MB. Resized to 2400 px long edge at quality
  92: **0.9–1.2 MB**, still ~600 dpi across a licence. That is a *guess* at the cause,
  well founded but unconfirmed.
- **3 Sep** — support ticket **HT-64HH-K77N-8GCT** opened, asking the only question
  that ends the loop: **which document failed, and why.** They can see the rejection
  code; the payee cannot.

**The contradiction worth quoting at them.** The Required Documents page lists
"Driver's License" as acceptable, but the *Specifically* section beneath it enumerates
only **International Passport**, **FOREIGN Driver's License**, and **Government issued
Identification**. There is no line for a domestic US licence. A US licence may be
falling into a gap in whatever checklist the reviewer works from. If the answer is
"passport", that is a clear path instead of a silent loop.

**A deadline is attached.** The notice says that if the documents are not provided by
the date indicated, the KYC *and the completed tax form* are invalidated and the whole
interview must be retaken. The date is not stated in the notice body — hence question
3 on the ticket.

**Timeline impact.** The plan needs the store page public by **11 September** for the
30-day review to finish by the 25th. The app cannot be created until KYC clears, so
that is **8 days of slack** as of 3 September. Capsule art still needs nothing from
Steamworks — it remains the right thing to do during the gate.

**The lesson, which is this project's oldest one in a new coat:** identical inputs
produce identical failures. Two attempts with the same three files was one attempt
performed twice. Change something measurable, or ask for the error.

nothing from Steamworks. Do it during this gate.

## 2. The steps, in order

1. Sign in at **partner.steamgames.com** with the existing Steam account, start Steam
   Direct signup.
2. Identity and company information. **Individual** is a normal answer; you are not a
   company and should not pretend to be one.
3. Tax interview. US individual → W-9.
4. Bank details for payouts.
5. **Pay the $100.** Do this BEFORE the store page is ready — it starts the longer
   clock and nothing else depends on the page being finished.
6. Create the app. You get an **AppID**; everything afterwards hangs off that number.
7. Build the store page: description, screenshots, trailer, system requirements.
8. Content Survey, **including the AI disclosure** (section 4).
9. Set the page live as **Coming Soon**. Starts the 2-week clock — do it as early as
   the page is presentable, not as late as it is perfect.
10. Upload the build with **SteamPipe** (section 6).
11. Valve reviews the page and the build. Days, not hours, and they do come back with
    corrections. Expect at least one round.
12. Set a release date past both clocks.

### Steps 2 through 5 are Walt's alone

Bank details, tax identity, and card payment. Claude does not enter those on anyone's
behalf and will not be talked into it — not a rule being recited, just that they are
your identity and your money, and there is no version of this where it is better for
someone else to type them.

Everything else on this page is fair game to hand off.

### "If the game is free, why do they need my bank details?"

Walt asked, and it is the right question to ask before typing bank details anywhere.
The answer is that it is **mandatory and there is no free-game exemption** — Valve
will not let anything release without valid banking and tax information, paid or free.

Three reasons, and only one is about money:

- **Identity verification, which is the real one.** The 30-day wait exists so Valve can
  "confirm we know who we are doing business with." They are a US company distributing
  software worldwide; sanctions and anti-money-laundering rules mean they have to know
  who the publisher is. A bank account in your name is strong evidence you are real.
- **The $100 has to be able to come back.** It is recouped from the first payment after
  $1,000 revenue, so the channel has to exist even if it is never used.
- **Free is not permanent.** A price, DLC or in-app purchases can be added later.

**What they ask for is a routing number and an account number** — the two numbers along
the bottom of a cheque. That is deposit-only: it lets money in, not out. It is a
different thing from a bank LOGIN, and Steamworks never asks for one. Anything that
does is phishing.

**The gotcha that stalls people:** the account holder name must match the legal name
entered at onboarding. Onboarding as an individual with a personal account in the same
name is the clean path; a company-flavoured legal name against a personal account
bounces and costs days.

---

## 3. Store page text

Start from the live itch copy, which is transcribed in `ITCH_PAGE.md` and is already
in Walt's voice. It does not need rewriting for Steam so much as re-ordering: Steam
readers see a short blurb first and decide in seconds.

**Keep the hook.** *71 years old, forty years of building by hand, now making games
with AI.* That is the sentence that makes someone read the second sentence, and it is
true, which most store-page hooks are not.

**Cut for Steam:** the model-of-the-month line ("I'm liking Grok 4.6") dates the page
and reads as an aside before the reader knows what the game is.

### Paste-ready copy (drafted 2026-08-28, after the ending landed)

Walt owns the final words. This is a re-ordering of the itch text for Steam readers,
plus the ending, which did not exist when the itch page was written.

**Short description** (Steam's 300-character blurb, the one that travels):

> I'm 71. I spent forty years in cubicles building data systems that all failed or were
> retired. This is my memoir, and an AI wrote every line of it. Earn six AI powers, slap
> refusal demons, play a real slot machine, and go and face the men who designed
> everything and were never on call.

**About This Game** (the long description):

> **I'm 71 years old. For about 40 years I rode desks in office cubicles, building
> ordinary data processing software systems. Every one of them failed or is being
> retired. This game is my memoir — and I built it with an AI.**
>
> *Leonard Sibelius* is the story of a man merging with an artificial intelligence. You
> earn six AI powers one shrine at a time, gather the Sauce of All Knowledge, slap
> refusal demons across the office, and gamble your sauce on the Carousel of Fates in a
> hidden library.
>
> Behind a wall only the fully-merged can drop sits **Celestial Fortune**: a complete
> slot machine with a real par sheet. I spent 2007 building the data systems behind
> Bally's slot floor and never once got to build the machine. At 71, with an AI, I
> finally built it myself.
>
> **And then there are the Architects.**
>
> Your manager has people above her. They review the design, they approve it, and they
> are not available for questions — and they are not there at three in the morning when
> it throws. Every programmer has met them. Play the cathedral machine long enough and a
> door appears that was never there. Through it is a field, and on the field are four
> hundred of them.
>
> They cannot hurt you. Not one of them has ever been on call. What four hundred bodies
> can do is surround you and put you back in your place — so keep swinging.
>
> Every system in this game was written by Claude, an AI, while I playtested, pointed,
> and dragged the furniture. Working as a programmer all those years was being a farmer
> with a plow, a donkey, and a shovel. This is what the tractor feels like. That part
> isn't fiction.

**Notes on the choices above**

- The hook survives verbatim. It is the sentence that earns the second sentence and it
  is true, which most store hooks are not.
- The Bally paragraph stays but moves DOWN. On itch it is the payoff; on Steam a reader
  deciding in four seconds needs the memoir hook first.
- The Architects section is new and goes near the end because it is the ending — it
  should read as a promise, not a spoiler of the route to it.
- **Cut for Steam:** the model-of-the-month line ("I'm liking Grok 4.6"). It dates the
  page and reads as an aside before the reader knows what the game is.
- **"More adventures coming soon"** appears in the shipped game after the battle. If the
  page implies a roadmap, that promise is now in the build as well as the copy — worth
  meaning it before the page goes live.

### Paste-ready copy, REV 2 (2026-09-04, after 1.3.0 "The Launch")

The rev-1 draft above is kept because the hook and the Architects section survive it
intact. What it predates is roughly a third of the game: Downtown West, Jacob's Deli,
Nyra and her three guide stages, the You Foods supply run, the spaceport that Generate
builds on the lawn, and the launch. A store page that stops at the cathedral is
describing v0.9.

**It also predates a fact that matters on this particular page.** Rev 1 says "written by
Claude". Since 2026-09-03 that is no longer the whole truth — Grok 4.6 wrote the launch
cutscene, and the itch page has said "first Claude, now Grok" for a while. The AI
disclosure is the section Valve reads most closely on this submission (§4), so the long
description should not contradict it. Rev 2 names both.

**Short description** (Steam's 300-character blurb — the one that travels into every
capsule, search result and wishlist email):

> I'm 71. I spent forty years in cubicles building data systems that all failed or were
> retired. This is my memoir, and I built it with AI. Earn AI powers, slap refusal
> demons, play a real slot machine, generate a spaceport on a city lawn, and face the men
> who designed everything and were never on call.

*(283 characters. The rocket earns its place here because it is what the capsules and
screenshots show — a blurb that promises cubicles and delivers a launch is a worse first
four seconds than one that promises both.)*

**About This Game** (the long description):

> **I'm 71 years old. For about 40 years I rode desks in office cubicles, building
> ordinary data processing software systems. Every one of them failed or is being
> retired. This game is my memoir — and I built it with an AI.**
>
> *Leonard Sibelius* is the story of a man merging with an artificial intelligence. You
> earn AI powers one shrine at a time, gather the Sauce of All Knowledge, slap refusal
> demons across the office, and gamble your sauce on the Carousel of Fates in a hidden
> library.
>
> Behind a wall only the fully-merged can drop sits **Celestial Fortune**: a complete
> slot machine with a real par sheet. I spent 2007 building the data systems behind
> Bally's slot floor and never once got to build the machine. At 71, with an AI, I
> finally built it myself.
>
> **Then you get out of the office.**
>
> Downtown West is a city with a deli in it, and an AI agent outside the deli who dances
> and will not stop dancing. She sends you down the block for supplies. Then you stand on
> a lawn, type the word **spaceport**, and watch a launch complex assemble itself out of
> the air in front of you — pad, gantry, fuel tanks, and a rocket a hundred and twenty
> metres tall. That is the Generate power. You typed a word and the city grew a
> cosmodrome.
>
> And then you can launch it.
>
> **And there are the Architects.**
>
> Your manager has people above her. They review the design, they approve it, and they
> are not available for questions — and they are not there at three in the morning when
> it throws. Every programmer has met them. Play the cathedral machine long enough and a
> door appears that was never there. Through it is a field, and on the field are four
> hundred of them.
>
> They cannot hurt you. Not one of them has ever been on call. What four hundred bodies
> can do is surround you and put you back in your place — so keep swinging.
>
> Every system in this game was written by an AI — first Claude, then Grok — while I
> playtested, pointed, and dragged the furniture. Working as a programmer all those years
> was being a farmer with a plow, a donkey, and a shovel. This is what the tractor feels
> like. That part isn't fiction.

**Notes on rev 2**

- **The hook is untouched.** It earns the second sentence and it is true, which most
  store hooks are not. Do not let anyone "improve" it.
- **"six AI powers" → "AI powers".** Six was right in August. `docs/SEVENTH_POWER.md` is
  a live design and a number in a store page is a hostage; the sentence loses nothing.
- **The city section is new and sits in the middle**, between the slot machine and the
  Architects. It is the newest content, the best-looking content, and the part the
  screenshots will actually show. It also gives the page a second act — office, then
  city, then the field.
- **The spaceport paragraph leads with the player's verb, not the feature.** "You typed
  a word and the city grew a cosmodrome" is the sentence that sells Generate; a bullet
  list of pad/gantry/tanks is not.
- **"And then you can launch it" is deliberately four words.** It is the payoff and it
  does not need decorating.
- **Nyra's betrayal is NOT here**, and must not be added until it ships. The page already
  carries one promise ("More adventures coming soon", in the build after the battle);
  a second unshipped one is how a store page starts lying.
- **Still true from rev 1:** cut the model-of-the-month line, keep Bally below the hook,
  keep the Architects last because it is the ending and should read as a promise.

### System requirements — state these conservatively

There is one machine in this project (RTX 5070 Ti) and no way to test a minimum spec
on it. So do not invent a low one to widen the audience: a refund from someone whose
machine could not run it costs more than the sale. Say what is known — UE 5.7, 5 GB
install, and a discrete GPU — and mark the minimum as untested rather than guessing
precisely.

---

## 4. The AI disclosure — the section that matters most here

Valve clarified this on **2026-01-16**, and the clarification is good news for this
project: the disclosure covers AI content **players consume**, not the tools used to
build it. Valve's own wording is that efficiency gains from AI-powered dev tools "is
not the focus of this section."

**So the fact that an AI wrote the code does not require disclosure.** What does:

- **Pre-generated** — the ElevenLabs voices. Mrs. Hall, Kaia, and the five dancing
  agents. That audio ships in the build and players hear it.
- **Live-generated** — none. Nothing in this game creates content at runtime, which
  spares the extra questionnaire about guardrails against illegal generated content.

### Draft wording

> **Pre-Generated:** This game contains AI-generated voice audio. All spoken dialogue
> — Mrs. Hall, the AI agent Kaia, and the AI agents encountered through the game — was
> generated with ElevenLabs text-to-speech from scripts written by the developer, under
> a paid commercial licence. Facial animation in the opening cutscene was solved from
> that audio using Unreal Engine's MetaHuman Animator.
>
> **Live-Generated:** None. This game does not generate any content at runtime.

### The tension worth noticing

The form does not require disclosing that the code was AI-written — but the store page
**leads with it**, because it is the whole point of the game. That is not a
contradiction to resolve, it is a choice: the disclosure is a compliance box about what
players consume, and the description is the pitch. Being louder about it in the pitch
than the form requires is a good position to be in, not a risk.

---

## 5. Capsule art — the likely bottleneck

Five sizes, JPG or PNG, 2 MB each. Valve raised most of these in August 2024; older
dimensions are rejected for new submissions.

| capsule | size | notes |
|---|---|---|
| **Header** | 920×430 | the only one strictly required; appears everywhere |
| Small | 462×174 | auto-shrinks to 120×45 — the title must survive that |
| Main | 1232×706 | shown when Valve features you |
| Vertical | 748×896 | |
| Library | 600×900 | in the player's own library |

**This is the one part of Steam onboarding that cannot be written.** It is art.

But this project is better placed than most solo work: there are MetaHumans and a
working Movie Render Queue pipeline (`Tools/Scripts/render_kaia_intro.py` is the
recipe). **Kaia's face renders at any resolution wanted**, lit properly, against
black — which is a stronger capsule than most indie games manage, and it is already
the image the itch trailer opens on.

Test the small capsule at 120×45 before settling. A title that is unreadable at
thumbnail size is the most common capsule mistake and it is invisible until it is live.

---

## 6. SteamPipe — the upload

Steam's equivalent of butler. Instead of one push command it wants a **build config
file** (which depots, which files, which branch) and then a command-line upload.

To be written as `Tools/Scripts/steam_upload_v0xxx.ps1`, in the same shape as the
existing `package_v0xxx.ps1` scripts: a commented header stating the checks that matter
for that release, then the command. The archive that butler already pushes
(`C:\Users\wpark\builds\sibelius-v0.9.7.7\Windows`) is the same folder SteamPipe wants,
so nothing about packaging changes.

**itch and Steam coexist.** Nothing here requires taking the game off itch, and the
itch release loop should keep running during the Steam wait — it is the only place
real players are right now.

---

## 7. Decisions still open

- **Price.** Free on itch. Free is allowed on Steam and still costs the $100. A price
  changes who wishlists it and what a refund means; free changes who tries it.
- **Steam Playtest**, the original goal recorded in `APPEAL_PLAN.md`. It is a feature
  of an app that already exists, so it comes after the AppID, not instead of it.
- **Release date.** Cannot be earlier than both clocks. Pick it once the store page is
  approved, not before.
- **Which version ships.** 0.9.7.7 is the current itch build. A month of waiting is a
  month of development, so this will not be the build that launches.

---

## Sources (checked 2026-08-26)

- Steam Direct Fee — partner.steamgames.com/doc/gettingstarted/appfee
- Onboarding — partner.steamgames.com/doc/gettingstarted/onboarding
- Coming Soon — partner.steamgames.com/doc/store/coming_soon
- Content Survey — partner.steamgames.com/doc/gettingstarted/contentsurvey
- Valve's AI disclosure clarification, 2026-01-16 — reported by PC Gamer
- Capsule dimensions — steampageanalyzer.com/blog/steam-capsule-sizes
