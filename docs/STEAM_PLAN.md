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
