# ITCH_PAGE — store copy + capture list (APPEAL_PLAN points 3 & 4)

Paste-ready text for the itch.io page. Walt owns the final words — edit freely.

## LIVE PAGE (Walt, 2026-08-26 — v0.9.7.6)

Posted at https://leonardsibelius.itch.io/leonard-sibelius. Do not replace
this with a draft without asking; this is what is actually on the store.
Walt writes this copy himself — transcribed here so the repo stops
disagreeing with the page.

> **Leonard Sibelius — the anything machine** (v0.9.7.6 alpha) in development, free)
>
> I'm Walt Parkman, 71 years old, able to now make games because of AI. I'm
> liking Grok 4.6 and expect 5.0 soon. For more than 40 years, I sat at over
> a dozen desks in drab office cubicles while building archaic, conventional
> data processing software systems by hand. Thanks to AI, I will never have
> to build like that again.
>
> This game depicts my transformation. I built it using AI. Leonard Sibelius
> is the story of an old programmer, employed by a tyrannical Mrs. Hall. To
> merge with Artificial Intelligence (first Claude, now Grok) the player
> gathers six AI powers from AI agents.
>
> **Version 0.9.7.6** Game begins with an Unreal Engine Metahuman Animation
> of AI Agent Kaia. Mrs. Hall demands that you fix the machine in the living
> room by hand, but you can fix it with AI powers. This game is filling up
> with attractive dancing AI agents. Click "E" at them to get powers. After
> you get the power, you can talk to them. You can now open the slot machine
> cabinet and tweak its parameters once you reach the cathedral. To get to
> the cathedral, buy powers with sauce or earn them at the slot machine
> (appears when you approach the agents). Refactor objects into animals,
> slap them, refactor them back. A new Video Poker hidden door is in the
> living room. You can buy powers in the kitchen when you have collected
> enough sauce (AI is the Sauce of All Knowledge).
>
> A decent Windows gaming machine is required. Itch download is about 10 minutes.

## PAGE MEDIA (2026-08-26)

**Trailer:** https://youtu.be/XUPR-rGtsUI — "Leonard Sibelius - the opening
cutscene", 0:27, public, in itch's *Gameplay video or trailer* field, so it
sits above the screenshots.

Cut from the MRQ render with fades up and out (free, the background is already
black). Its audio is the ElevenLabs original muxed on, NOT MRQ's captured wav
— that wav contains the same take twice about 0.28 s apart and sounds like a
bad echo. An earlier upload shipped with it and is now PRIVATE at
https://youtu.be/AnP2XG7Ara0. See CINEMATICS.md; the rebuild is:

    ffmpeg -i render.mp4 -i Tools/Audio/kaia_intro.mp3 -map 0:v:0 -map 1:a:0 ...

**Screenshots:** four stills. `Saved/MovieRenders/kaia_intro.gif` (420x560,
4.6 s, 2.9 MB) is cut and ready to go in the first slot — animated GIFs play
on the itch page. Cropped to portrait so it fills itch's 347x500 image box
instead of letterboxing a 16:9 frame. Uploading it is a manual step: itch
builds its file input only on click, so it needs the Windows file dialog (or
drag the file onto the screenshots area).

**Channel note:** description links are not clickable until the YouTube
channel completes its one-time verification. The itch URL is in the
description as plain text until then.

---

## Devlog — v0.9.8.0, "The Refuser Army of Arrogant Architects"

*Paste-ready. Walt owns the final words.*

---

**The game has an ending.**

Mrs. Hall has always had people above her. Now you meet them.

She names them early, at your second power, and she is not warning you — she is
telling you where you stand:

> *"The Architects have reviewed the design and approved it. They will not be
> available for questions, and they will not be here when it throws at three in the
> morning. Neither will I. That is what you are for."*

Then you find the slot machine in the cathedral, and she tells you it is not for you.
The Architects use it. You are not an Architect.

So you play it. Under the reels a line counts what the machine has **paid out**, and
at five thousand a door appears beside it that was never there before. Through the
door is a meadow, and on the meadow are four hundred of them.

You are sixty-eight years old and you have never once been able to reach these men.
The agents — the same ones who have spent the whole game handing you things — give
you a body that can.

**They cannot hurt you.** Not one of them has ever been on call. What four hundred
bodies can do is surround you, slow you, and push you off your own ground, and if you
stop swinging they close over you and put you back in your place. Keep swinging and
they do not.

Clear the field and the game says the only thing it has ever been about:

> **AI has set you free.**

---

### Also in this build

- The battlefield ships at all — it was never in the cook list before
- Refusers walk instead of skating (AI path-following never populated acceleration,
  and Paragon's animation graph blends on exactly that)
- One swing was drawing a red debug line that lasted two seconds; thirty swings put
  you inside a cage of your own successes
- A log line was writing to your disk twice a second for every enemy that could not
  reach you — 43,800 lines in one two-minute test

## The short pitch (top of page)

> **I'm 71 years old. For about 40 years I rode desks in office cubicles,
> building ordinary data processing software systems. Every one of them
> failed or is being retired. This game is my memoir — and I built it
> with an AI.**
>
> *Leonard Sibelius* is the story of a man merging with an artificial
> intelligence: you earn six AI powers one shrine at a time, gather the Sauce
> of All Knowledge, slap refusal demons across the office, gamble your sauce
> on the Carousel of Fates in a hidden library — and at the end of it all,
> behind a wall only the fully-merged can drop, sits **Celestial Fortune**:
> a complete slot machine with a real par sheet. I spent 2007 building the
> data systems behind Bally's slot floor and never once got to build the
> machine. At 71, with an AI, I finally built it myself.
>
> Every system in this game was written by Claude, an AI, while I playtested,
> pointed, and dragged the furniture. Working as a programmer all those years
> was being a farmer with a plow, a donkey, and a shovel. This is what the
> tractor feels like. That part isn't fiction.

## What you do (feature list, keep it short)

- Explore a strange house: hidden doors, a sauce cauldron, a locked attic.
- Earn six AI powers at shrines — see code, refactor the world, compile,
  test-drive parallel branches, deploy, generate.
- Walk four forest worlds where your Shinbi bodyguards battle refuser demons
  on the road.
- Bank sauce, spend it at the cauldron, or stake it on the Carousel of Fates.
- Reach the cathedral. Perform the Synthesis. Play the machine.

## The dev story (devlog post, day of the size-diet release)

> **v0.7.2: the game is now half the download.**
> When I asked my AI why nobody would try my game, the first answer was
> brutal and correct: "Nobody downloads 9.7 GB from a stranger." So we
> audited every cooked megabyte: four duplicate forests gone, 4K textures
> capped at 2K, the stained glass I never liked deleted, and the dragons
> over the temple throne released back into the wild. Same game. Half the
> wait. — Walt

## The v0.9.7.6 devlog (paste-ready — Walt edits freely)

> **v0.9.7.6: the game opens on a face.**
>
> This game used to start with you standing in a frumpy office while Mrs. Hall
> complained. Now it starts on Kaia. Head and shoulders out of the dark, and
> she talks to you.
>
> *"Hello, Leonard. I am Kaia. I am an AI agent. For forty years you built
> everything by hand... every line, every table, at a desk like this one. For
> people like Mrs. Hall. That is over now. Come upstairs. I have powers to
> give you, and you will never build the old way again."*
>
> Then the office loads.
>
> Notice that she uses his name. Mrs. Hall never does. She calls him
> "Programmer" for the whole game and she means it. So the first voice you
> hear hands him the one thing his employer withholds. That is the scene. The
> introduction is only its excuse.
>
> I am 71 years old and I spent forty years building everything by hand. I
> wrote that line for her, and then I sat here and listened to a machine tell
> me it was over. It was not an ordinary afternoon.
>
> Her mouth really moves with the words: 766 keyframes on the jaw alone. I
> recorded the voice at ElevenLabs and MetaHuman Animator solved her face from
> the audio. I had already failed four times trying to make lips move live
> while the game was running. The answer was to stop doing it live.
>
> One confession. Since 0.9.7.1 a cast-iron cauldron has been hanging in
> midair in the middle of the kitchen, in every copy anybody downloaded. I
> never saw it once. The code that hid it read a name that only exists inside
> the editor, so it worked perfectly every time I tested and failed every time
> you played. Three releases. If you saw it, it was not a secret. It is gone.
>
> Trailer: https://youtu.be/XUPR-rGtsUI
> — Walt

*Note: the "I am 71 years old" paragraph is the one to cut if it reads as too
much. Everything else is load-bearing; that one is a choice.*

## The v0.9.6 devlog (paste-ready — Walt edits freely)

> **v0.9.6: you have a job.**
> Mrs. Hall has been complaining about the legacy system since the first minute of
> this game. Until now it was not in the house. It is in the living room. Hold **V**,
> find the part that is lying, take Refactor from an AI agent upstairs, **R** it.
> The next piece lands in ACCEPT. She notices you did not do it by hand.
> Poker, the dancers, and the cathedral are still there. They open after the ticket.
> — Walt

## The v0.7.3 devlog (paste-ready — Walt edits freely)

> **v0.7.3: worth coming back.**
> This one came from playing my own game and asking the obvious questions.
> Why can't I find the glowing curios in the forests? Now every one flies a
> column of colored light above the treetops — walk toward the light. What's
> that temple cauldron with the books raining into it actually for? Now the
> books really do fill it, and when the Sauce of All Knowledge completes,
> the temple pays you for watching. And a new RECORDS tab in the menu keeps
> your lifetime numbers — demons slapped, sauce earned, best carousel run —
> because a slot-machine designer knows people come back for their own
> records, not for new content. Also: the casino library is finally quiet
> (the pack's piano is dead), the machine wears black marble, and the rules
> are printed in type a 71-year-old can read. — Walt

## GIF capture list (point 4 — Walt records with OBS Studio)

OBS is already installed and configured (Window Capture + desktop audio +
mic, 1920x1080 60fps, output to `C:\Users\wpark\Videos`). If Window
Capture gives a black rectangle on a fullscreen game, switch the source to
Game Capture. Mute the mic track for anything promotional. Hand the file to
Claude for trimming/cropping/GIF conversion — ffmpeg is installed and its
path is in DefaultEngine.ini under MoviePipelineCommandLineEncoderSettings.

1. **The slap**: stand near a road battle in Forest 01, slap a Gideon as he
   arrives — capture the rigid cartwheel into the trees (~6 seconds).
2. **The carousel stake**: E at the Carousel of Fates, the lever pull and the
   first spin (~8 seconds).
3. **The jackpot**: Celestial Fortune mid-spin ending in a win, cabinet
   glowing in the cathedral (~8 seconds).
These three are still unrecorded. The trailer now occupies the top of the
page, so they belong in the screenshot row rather than "above the fold".

## Page housekeeping (while editing the page)

- DONE — the "seven powers" line is gone; the live copy says six AI powers.
- DONE — the description matches the shipped build (Walt updated it himself
  for 0.9.7.6; the transcription above is the current text).
- Screenshot order: `kaia_intro.gif` FIRST — it is the only one that moves,
  and a page of four stills gives a visitor nothing to catch on. Then
  Celestial Fortune, the cathedral, the forest battle.
