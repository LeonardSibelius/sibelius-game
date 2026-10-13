// FreakbookModel.cpp — see the header. The five, their posts and every reply are written here.

#include "FreakbookModel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace FreakbookDetail
{
	struct FLine
	{
		const char* Text;
		const char* Picture;
	};

	struct FWritten
	{
		const char* Id;
		EFreakPerson Who;
		EFreakGate Gate;
		int BaseDays;          // Always posts: how old on his first login. The rest age from when they appear.
		const char* Text;
		const char* Picture;
		int Likes;
		std::vector<FFreakComment> Comments;
	};

	constexpr EFreakPerson Dee = EFreakPerson::Dee;
	constexpr EFreakPerson Gunther = EFreakPerson::Gunther;
	constexpr EFreakPerson Brent = EFreakPerson::Brent;
	constexpr EFreakPerson Margo = EFreakPerson::Margo;
	constexpr EFreakPerson Jaxx = EFreakPerson::Jaxx;

	// =========================================================================================
	// THE FIVE

	const FFreakProfile Profiles[FreakPeople] = {
		{ "dee", "Dolores \"Dee\" Pickering", "Dee", 74, "COBOL",
		  "Keeps the monthly statement run going at First Second National Bank. Since 1983.",
		  "Being called legacy. The AI the bank bought to replace her, which keeps emailing her questions.",
		  "Being cryogenically frozen, so the bank has to thaw her every time the batch fails. A million dollars a thaw. She has picked out the freezer.",
		  "Her 1983 program listing: a blue binder she left in somebody's attic.",
		  "Has her binder back. Keeps it on her lap at night.",
		  "binder", "Mail Dee her binder", "dee_avatar" },
		{ "gunther", "Gunther Krell", "Gunther", 58, "C",
		  "Writes the firmware for the ticket gate at the Fifth Street Garage in Dayton, Ohio.",
		  "The gate arm, which goes up by itself at 3:14 every night. Anyone who says \"just use Rust.\"",
		  "An island with no electricity, where nothing can crash but coconuts. He is building a raft out of old circuit boards. It is 40% done and it leaks.",
		  "A real coconut, as proof the island exists.",
		  "Has a real coconut now. Named it Kernel.",
		  "coconut", "Send Gunther the coconut", "gunther_avatar" },
		{ "brent", "Brent Hollowell", "Brent", 44, "Java",
		  "Maintains an insurance claims system with 11,000 classes at Mutual Mutual Insurance.",
		  "The 400-line config file. The meeting about the meeting.",
		  "Professional wrestling, as The Garbage Collector, whose finishing move freezes the whole arena. He has the cape and the entrance music. He has not told his manager.",
		  "A championship belt.",
		  "Undefeated champion of the 2 p.m. sync.",
		  "belt", "Give Brent the belt", "brent_avatar" },
		{ "margo", "Margo Tuttle", "Margo", 31, "Python",
		  "Builds the weekly kibble-sales dashboard at Kibblesworth Pet Nutrition. Views in three years: zero.",
		  "The VP who asks for \"one more chart\" and has never opened the first.",
		  "Being one of the company's taste-test dogs. They get walks and naps, and their opinions decide the recipe.",
		  "One person, anyone, to open her dashboard.",
		  "Views: 1. Framed.",
		  "", "Open Margo's dashboard", "margo_avatar" },
		{ "jaxx", "Tyler \"Jaxx\" Bloom", "Jaxx", 24, "JavaScript",
		  "Builds the cookie banner on First Second National Bank's website. Eighteen months so far.",
		  "That the framework he learned in March was obsolete by June.",
		  "Being a medieval monk, copying books by hand. No updates, no packages, one dependency.",
		  "Someone, ever, to click Accept on his banner. Everyone clicks the X.",
		  "One Accept. Tattoo pending.",
		  "", "Visit the bank's website", "jaxx_avatar" },
	};

	// =========================================================================================
	// THE POSTS. Pitiful, and funny first (docs/FREAKBOOK.md, "The tone").

	const std::vector<FWritten>& Written()
	{
		static const std::vector<FWritten> Posts = {
			// --- On his first visit. Listed newest first.
			{ "margo_1", Margo, EFreakGate::Always, 0,
			  "Dashboard views this week: 0. Last week: 0. Lifetime: 0. I check every morning. That's one view, technically, but I don't count myself. I don't count, generally.",
			  "margo_dashboard", 2, { { Jaxx, "can i see it" }, { Margo, "You can! Nobody does, though." } } },
			{ "gunther_1", Gunther, EFreakGate::Always, 1,
			  "The gate went up again at 3:14. No car. I think it's saluting someone.",
			  "gunther_gate", 4, { { Dee, "Check your interrupt handler, dear." }, { Gunther, "I have checked it 900 times, Dee." } } },
			{ "dee_2", Dee, EFreakGate::Always, 1,
			  "The AI they bought to replace me asked what PIC S9(7)V99 COMP-3 means. I told it: money, dear. Money.",
			  "", 41, { { Brent, "Saving this for my next design review." } } },
			{ "brent_2", Brent, EFreakGate::Always, 2,
			  "Stand-up ran 52 minutes today. I stood. I was not up.",
			  "", 30, {} },
			{ "jaxx_1", Jaxx, EFreakGate::Always, 2,
			  "18 months on the cookie banner. It needs 1,400 packages to show one button. Nobody has ever clicked Accept. They all click the X. Every. Single. One.",
			  "jaxx_banner", 7, { { Dee, "We don't use cookies in the statement run. We use ledgers." }, { Jaxx, "wait you work at First Second too??" }, { Dee, "I AM First Second, dear." } } },
			{ "dee_1", Dee, EFreakGate::Always, 2,
			  "Picked out my freezer today. The salesman asked how long I planned to stay in. I said until the batch fails. He said that could be forever. I said: exactly.",
			  "dee_freezer", 3, { { Jaxx, "is this a metaphor" }, { Dee, "It's a chest freezer, dear." } } },
			{ "brent_1", Brent, EFreakGate::Always, 3,
			  "This sprint I'm renaming AbstractClaimProcessorFactoryBeanImpl to AbstractClaimProcessorFactoryBeanImplV2. It took three meetings to agree on the V.",
			  "brent_diagram", 12, { { Dee, "In my day a name had six letters and we were grateful." } } },
			{ "margo_3", Margo, EFreakGate::Always, 3,
			  "The test dogs voted down the new chicken recipe, nine to one. No slide deck. Just a wagging tail. Must be nice.",
			  "margo_dogs", 28, { { Brent, "Do they have stand-ups?" }, { Margo, "They have lie-downs." } } },
			{ "gunther_2", Gunther, EFreakGate::Always, 3,
			  "Raft update: 40%. It floats if nobody is on it. That is also true of my career.",
			  "gunther_raft", 6, {} },
			{ "jaxx_3", Jaxx, EFreakGate::Always, 4,
			  "npm install took 40 minutes this morning. I used the time to learn calligraphy. It's my favorite framework now.",
			  "jaxx_desk", 18, { { Gunther, "Calligraphy is just C with ink. Respect." } } },
			{ "dee_3", Dee, EFreakGate::Always, 4,
			  "Day 14,612 of the statement run. It has never failed on my watch. Put that on my headstone, in uppercase, starting in column 8.",
			  "dee_cat", 2, { { Margo, "I made a chart of your uptime. It's a straight line. It's beautiful. Nobody will see it." } } },
			{ "gunther_3", Gunther, EFreakGate::Always, 5,
			  "Segmentation fault, core dumped. My doctor says the same about my knees.",
			  "", 22, { { Jaxx, "what's a segmentation fault" }, { Gunther, "Enjoy your youth, son." } } },
			{ "jaxx_2", Jaxx, EFreakGate::Always, 5,
			  "Monks had one dependency, and He never pushed a breaking change.",
			  "", 44, {} },
			{ "brent_3", Brent, EFreakGate::Always, 6,
			  "In the ring, when I pause, EVERYONE pauses. Like at work, but with a cape.",
			  "brent_cape", 9, { { Margo, "is that your cubicle" }, { Brent, "It's my dressing room." } } },
			{ "margo_2", Margo, EFreakGate::Always, 6,
			  "Added a 14th chart. Nobody will see it, so I made it a photo of my dog. Still no views.",
			  "", 15, {} },

			// --- From his second visit.
			{ "dee_4", Dee, EFreakGate::SecondVisit, 0,
			  "Server room is 61 degrees. I bring a parka, a thermos and my reading glasses. The young man brings an iPad that says TRANSFORMATION on the case.",
			  "dee_parka", 5, {} },
			{ "gunther_4", Gunther, EFreakGate::SecondVisit, 0,
			  "I have never seen a real coconut. I drew one and taped it to my monitor. For motivation. It looks like a brown potato with eyes. So do I.",
			  "gunther_drawing", 3, {} },
			{ "brent_4", Brent, EFreakGate::SecondVisit, 0,
			  "Wore the cape under my fleece vest today. Nobody noticed. Nobody notices anything here. That's my superpower and my tragedy.",
			  "brent_chair", 4, {} },
			{ "margo_4", Margo, EFreakGate::SecondVisit, 0,
			  "Went to the dog park for research. Got down on all fours to see what the dogs see. A man asked if I was all right. I said it was user research. He threw a ball. I'm not proud of what happened next.",
			  "margo_park", 6, {} },
			{ "jaxx_4", Jaxx, EFreakGate::SecondVisit, 0,
			  "tried on a monk's robe at the costume shop. for fit. it fits. everything else in my life is deprecated",
			  "jaxx_robe", 5, {} },

			// --- From his third.
			{ "dee_5", Dee, EFreakGate::ThirdVisit, 0,
			  "My granddaughter asked what I do. I said I make sure 40,000 people get their bank statement. She asked what a bank statement is.",
			  "", 8, { { Gunther, "Mine asked what a parking ticket is. They use their phones now. I am maintaining a ghost." } } },
			{ "gunther_5", Gunther, EFreakGate::ThirdVisit, 0,
			  "My wife asked why I sleep in the garage. I said in case the gate needs me at 3:14. She said the gate is in a different garage. Twelve miles away.",
			  "", 7, { { Brent, "I sleep in a cape. We all have our ways." } } },
			{ "brent_5", Brent, EFreakGate::ThirdVisit, 0,
			  "Entrance music is ready: a fax machine handshake over a drum loop. Played it in the parking lot. A security guard clapped. Best day of Q3.",
			  "", 11, { { Gunther, "Which garage? If it is Fifth Street, the gate probably went up for you." } } },
			{ "margo_5", Margo, EFreakGate::ThirdVisit, 0,
			  "The VP asked for a dashboard of the dashboards. I built it. It shows that the dashboards have zero views. It also has zero views.",
			  "", 19, { { Dee, "We had reports like that in 1984. We used them as insulation." } } },
			{ "jaxx_5", Jaxx, EFreakGate::ThirdVisit, 0,
			  "the framework i learned in march is dead. the one i learned in june is in maintenance mode. i'm 24 and i'm in maintenance mode",
			  "", 21, { { Brent, "Welcome to the ring, kid." } } },

			// --- When Leonard has done right by them.
			{ "dee_done", Dee, EFreakGate::WantDone, 0,
			  "Somebody mailed me my 1983 listing. Leonard, it was you. 212 pages, fan-fold, and my coffee ring on page 40 is still there. I cried in the server room. It's 61 degrees, so the tears were very cold.",
			  "dee_binder", 19, { { Jaxx, "dee is crying?? on freakbook??" }, { Dee, "Mind your banner, dear." } } },
			{ "gunther_done", Gunther, EFreakGate::WantDone, 0,
			  "A COCONUT ARRIVED. Real. Hairy. It sloshes. Leonard sent it. I held it to my ear and I could hear the island. Or it was the gate going up. It was 3:14.",
			  "gunther_coconut", 17, { { Margo, "Can I make a chart of the sloshing?" } } },
			{ "brent_done", Brent, EFreakGate::WantDone, 0,
			  "Leonard gave me a CHAMPIONSHIP BELT. Gold-colored. Plastic. Perfect. I wore it to the 2 p.m. sync. Nobody noticed. I noticed.",
			  "brent_belt", 26, { { Gunther, "Congratulations. Defend it at 3:14." } } },
			{ "margo_done", Margo, EFreakGate::WantDone, 0,
			  "SOMEONE OPENED MY DASHBOARD. Views: 1. It was Leonard. I printed the number and framed it. I'm going to the dog park to celebrate. On two legs this time. Probably.",
			  "margo_framed", 33, { { Brent, "Huge. Let's do a retro on it." } } },
			{ "jaxx_done", Jaxx, EFreakGate::WantDone, 0,
			  "SOMEONE CLICKED ACCEPT. on my banner. not the X. ACCEPT. eighteen months. leonard i lit a candle for you and wrote your name in gold leaf. badly. but gold",
			  "jaxx_candle", 38, { { Dee, "Well done, dear. Now make it work in Internet Explorer." } } },
		};
		return Posts;
	}

	// =========================================================================================
	// THE REPLIES: by what he wrote about, rotating.

	const std::vector<FLine>& Lines(EFreakPerson Who, EFreakTopic Topic)
	{
		static const std::vector<FLine> Table[FreakPeople][FreakTopics] = {
			// ---------------------------------------------------------------- DEE
			{
				/* Greeting */ {
					{ "Hello, dear. Make it quick, the batch starts at six.", "" },
					{ "Leonard? From the 1985 conversion? You still owe me a donut.", "" },
					{ "Oh, a message. Nobody messages me unless something has failed.", "" },
				},
				/* Work */ {
					{ "The statement run is 31,000 lines and I wrote 29,000 of them. The other 2,000 were written by a man named Earl, and I have been fixing them since 1987.", "" },
					{ "They call it legacy. I call it the only thing in that building that works.", "" },
					{ "The transformation lead wants to move it to the cloud. I asked him which cloud. He showed me a slide.", "" },
					{ "Every January they promise to replace me by December. I have outlived forty-three Decembers.", "" },
				},
				/* Fantasy */ {
					{ "The freezer is a chest model, 21 cubic feet. I've put a little pillow in it already.", "dee_freezer" },
					{ "A million dollars a thaw. Three failures a year and I can retire in comfort. Frozen comfort.", "" },
					{ "My doctor says cryogenics isn't covered by Medicare. I said neither is COBOL, and look how long that's lasted.", "" },
				},
				/* Want */ {
					{ "My binder, dear. Blue, three-ring, STMT-RUN 1983 on the spine in my handwriting. I left it in somebody's attic. I think it was yours.", "" },
					{ "Without that listing nobody will ever know why line 4,410 subtracts a penny. I know why. I'm not telling anyone without the binder.", "" },
				},
				/* WantDone */ {
					{ "I keep the binder on my lap at night now. Like a cat, but with more page breaks.", "dee_binder" },
					{ "Thank you for the binder, Leonard. Line 4,410 is a rounding fix from 1986. Now you know. Don't tell the AI.", "" },
				},
				/* Question */ {
					{ "That's a question for the AI, dear. It's paid more than I am.", "" },
					{ "I'd have to look it up in a manual. A paper one. It's in the freezer, actually.", "" },
					{ "Ask me again after the batch.", "" },
				},
				/* Other */ {
					{ "Mm-hm. I'll file that under 88-level conditions.", "" },
					{ "In 1983 we'd have punched that on a card and meant it.", "" },
					{ "Speak up, dear, the air conditioning in here is very loud.", "dee_parka" },
					{ "That's nice. Is anything wrong with your bank statement? That's my department.", "" },
				},
				/* Memory */ {
					{ "You told me \"{Q}\" the other day. I wrote it on the back of a printout. It's taped to my monitor.", "" },
					{ "Still thinking about \"{Q}\". At my age you think about things slowly and thoroughly. Like a batch job.", "" },
				},
			},
			// ---------------------------------------------------------------- GUNTHER
			{
				/* Greeting */ {
					{ "Hello. Is your gate going up by itself too? Everyone's is. They just don't say.", "" },
					{ "Ja, hello. I am in the garage. I am always in the garage.", "" },
					{ "You have reached Gunther. Leave a message after the segmentation fault.", "" },
				},
				/* Work */ {
					{ "The firmware is 6,000 lines of C. 5,000 of them are comments apologizing to whoever comes next.", "" },
					{ "My manager asked me to just use Rust. I asked him to just use a parking garage, then. He did not understand either.", "" },
					{ "Six years I have chased the 3:14 bug. I am starting to think the gate is trying to tell me something.", "gunther_gate" },
					{ "Pointers are like people. They point somewhere, and one day you find out nothing was ever there.", "" },
				},
				/* Fantasy */ {
					{ "On the island there is no electricity. Nothing can crash but coconuts. And when a coconut crashes, it is lunch.", "" },
					{ "The raft is 212 old circuit boards. When the sun hits it, it looks like a city. A small, sinking city.", "gunther_raft" },
					{ "I have packed for the island: one shirt, a fishing line, and a printout of the gate firmware, in case I miss it. I won't miss it.", "" },
				},
				/* Want */ {
					{ "If you ever find a real coconut, send it. I need proof the island is not a rumor.", "gunther_drawing" },
					{ "A coconut, Leonard. Just one. Then I will know the island is real, and that someone listened.", "" },
				},
				/* WantDone */ {
					{ "The coconut is on my desk now. I talk to it. It is a better listener than the gate.", "gunther_coconut" },
					{ "I named the coconut Kernel. Do not tell anyone.", "" },
				},
				/* Question */ {
					{ "I don't know. I don't know anything after 3:14.", "" },
					{ "Let me check the core dump. ...No. The answer is not in there either.", "" },
					{ "Good question. Undefined behavior, probably.", "" },
				},
				/* Other */ {
					{ "Mm. I will allocate some memory for that. I will forget to free it.", "" },
					{ "I read that twice. The gate went up during the second time.", "" },
					{ "That is very C of you. Direct. Dangerous. I like it.", "" },
					{ "Noted. On a sticky note. On the raft.", "" },
				},
				/* Memory */ {
					{ "You said \"{Q}\". I wrote it in the margin of the firmware. It is the only comment in there that is not an apology.", "" },
					{ "\"{Q}\" - you said that. It has been running in my head all week, like a loop with no exit condition.", "" },
				},
			},
			// ---------------------------------------------------------------- BRENT
			{
				/* Greeting */ {
					{ "Hey! Sorry, I'm in a meeting. I'm always in a meeting. I'm typing under the table.", "" },
					{ "Leonard! Let's take this offline. Wait, this is offline. Let's take it online.", "" },
					{ "Hi. Quick sync? Five minutes? It'll be fifty.", "" },
				},
				/* Work */ {
					{ "We have a factory that makes factories. ClaimProcessorFactoryFactory. I'm not joking. I'm in charge of it.", "brent_diagram" },
					{ "The config file is 400 lines of XML. Line 212 says enabled=false and nobody knows what it disables. We're scared to ask.", "" },
					{ "Our architect designed all of this and moved to the beach. He sends postcards. He was never on call.", "" },
					{ "Eleven thousand classes. I've personally met about forty of them.", "" },
				},
				/* Fantasy */ {
					{ "The Garbage Collector's finishing move: the Stop-The-World. Everyone freezes mid-thought. Then I clean up.", "brent_cape" },
					{ "My entrance: the lights go down, a fax handshake plays, and I walk out slowly, like a meeting that should have been an email.", "" },
					{ "My manager thinks the cape is for a costume party. There is no party, Leonard. There is only the ring.", "" },
				},
				/* Want */ {
					{ "Every wrestler needs a belt. I've been wearing a lanyard around my waist. It's not the same.", "" },
					{ "If you could get me a championship belt, I'd defend it at every stand-up. Every single one.", "" },
				},
				/* WantDone */ {
					{ "I've defended the belt in three meetings now. Undefeated. Nobody else knew there was a match.", "brent_belt" },
					{ "My manager asked me to take the belt off for the client call. I turned my camera off instead.", "" },
				},
				/* Question */ {
					{ "Great question. Let's put a pin in it and circle back.", "" },
					{ "I'd need to check with the architecture review board. They meet quarterly.", "" },
					{ "Can you raise a ticket for that?", "" },
				},
				/* Other */ {
					{ "Love that. Adding it to the backlog. The backlog is 3,000 items long. You're number 3,001.", "" },
					{ "Noted. I'll create an AbstractNoteFactory.", "" },
					{ "Let's align on that. I don't know what align means. Nobody does.", "" },
					{ "Ha! That's going in my promo. Wrestling promo, not the work kind.", "" },
				},
				/* Memory */ {
					{ "You told me \"{Q}\". I've been saying it in my wrestling promo. The mirror loves it.", "" },
					{ "Remember when you said \"{Q}\"? I put it on a slide. I'm presenting it Thursday. Nobody will read it.", "" },
				},
			},
			// ---------------------------------------------------------------- MARGO
			{
				/* Greeting */ {
					{ "Hi!! Sorry, I don't get messages. I don't get views either. Hi!", "" },
					{ "Hello! Do you want to see a chart? You don't have to. Nobody has to.", "" },
					{ "Oh! A person. Hi. I've been talking to the test dogs all day.", "" },
				},
				/* Work */ {
					{ "The dashboard has 14 charts, 3 filters and a dropdown nobody has ever dropped down.", "margo_dashboard" },
					{ "I spent a week making the colors accessible. For nobody. Accessibly.", "" },
					{ "The VP says data drives decisions. The dogs drive decisions. Data drives me home at 9 p.m.", "" },
					{ "My script runs every Monday at 6. It's the only one at the company who reliably shows up.", "" },
				},
				/* Fantasy */ {
					{ "The test dogs get a nap at 2, a walk at 3, and when they don't like something it's gone by Friday. Imagine being listened to.", "margo_dogs" },
					{ "I'd be a beagle, I think. Beagles are honest about what they want, which is everything.", "" },
					{ "If I were a test dog I'd vote no on everything for a week, just to feel the power.", "" },
				},
				/* Want */ {
					{ "If you ever want to open my dashboard, there's a link on my profile. You'd be the first. No pressure. Lots of pressure.", "" },
					{ "One view. That's all. Then I'll know it exists outside my head.", "" },
				},
				/* WantDone */ {
					{ "Views: 1. I look at the number before bed. It's the best number I've ever made.", "margo_framed" },
					{ "You're my favorite data point, Leonard. Statistically significant.", "" },
				},
				/* Question */ {
					{ "Great question. I'll make a chart. It won't answer it, but it'll be very clean.", "" },
					{ "I'd have to query that. It'll take six hours and the answer will be 0.", "" },
					{ "Probably! The confidence interval is huge, though.", "" },
				},
				/* Other */ {
					{ "Love this. I'm adding it to a chart nobody will see.", "" },
					{ "Okay! Logging that. Python says it's a string. I say it's a feeling.", "" },
					{ "That's the most anyone's said to me this quarter.", "" },
					{ "Ha! I told the dogs. One wagged.", "" },
				},
				/* Memory */ {
					{ "You said \"{Q}\" and I made a word cloud of it. It's very small. It's just those words.", "" },
					{ "I've been rereading \"{Q}\". Views on that message: 14. All me.", "" },
				},
			},
			// ---------------------------------------------------------------- JAXX
			{
				/* Greeting */ {
					{ "yo. sorry, npm is installing. i have like 40 minutes", "" },
					{ "hey leonard!! do you accept cookies? asking for a banner", "" },
					{ "hi. typing this with a quill. jk. i wish", "" },
				},
				/* Work */ {
					{ "the banner has a close button, an accept button, and a settings button that opens a second banner. it's banners all the way down", "jaxx_banner" },
					{ "my lead wants the banner rewritten in the new framework. the new framework comes out tuesday", "" },
					{ "1,400 packages. one of them just checks if a number is even. it has 200,000 downloads a week. i'm not ok", "" },
					{ "the bank's statement run is from 1983 and it works. my banner is from last week and it doesn't. dee won't let me forget it", "" },
				},
				/* Fantasy */ {
					{ "monks wake up at 4, copy one page, eat bread, and nobody asks them for a status update. that's the dream", "jaxx_desk" },
					{ "i've started illuminating my standup notes. the B in blocked has a little dragon in it", "" },
					{ "a monk's book lasts 800 years. my code lasts until thursday", "" },
				},
				/* Want */ {
					{ "if you ever visit the bank site, please, PLEASE click accept. not the X. everyone clicks the X", "" },
					{ "there's a link to the bank on my profile. one click. accept. i'll owe you forever", "" },
				},
				/* WantDone */ {
					{ "you clicked accept. i told everyone. my mom cried. she doesn't know what a cookie banner is", "jaxx_candle" },
					{ "the analytics say 1 accept. i screenshotted it. i'm getting it tattooed. small. on my ankle", "" },
				},
				/* Question */ {
					{ "honestly no idea. let me google it. ok google is an AI now and it asked me a question back", "" },
					{ "good question. there's probably a package for that", "" },
					{ "idk. ask dee, she knows everything since 1983", "" },
				},
				/* Other */ {
					{ "lol. writing that down. with a quill. on vellum. ok a sticky note", "" },
					{ "real. that's real", "" },
					{ "brb my framework just got deprecated", "" },
					{ "this is the most human thing anyone's said to me all week", "" },
				},
				/* Memory */ {
					{ "you said \"{Q}\" and i copied it out by hand. took 20 min. worth it", "" },
					{ "still thinking about \"{Q}\". it's in my illuminated manuscript now. there's a little dragon", "" },
				},
			},
		};
		return Table[static_cast<int>(Who)][static_cast<int>(Topic)];
	}

	/** How each of them passes on what Leonard said to somebody else. {A} the one he told, {Q} what he said. */
	const char* GossipLine(EFreakPerson Teller, int Variant)
	{
		static const char* Lines[FreakPeople][2] = {
			{ "Leonard told {A} \"{Q}\". In 1983 we said things like that to each other's faces.",
			  "I hear Leonard told {A} \"{Q}\". I have kept worse secrets in a COMP-3 field." },
			{ "Leonard told {A} \"{Q}\". I read it at 3:14. The gate went up.",
			  "I was not supposed to see it, but Leonard told {A} \"{Q}\". I have added it to the raft's logbook." },
			{ "Water-cooler update: Leonard told {A} \"{Q}\". Scheduling a meeting to discuss.",
			  "Per my last post: Leonard told {A} \"{Q}\". Looping in the architecture board." },
			{ "New chart: things Leonard has told {A}. Data point 1: \"{Q}\". Views: 0.",
			  "Leonard told {A} \"{Q}\" and I can't stop thinking about it. I told the dogs. They didn't vote." },
			{ "ngl leonard told {A} \"{Q}\" and i can't stop thinking about it",
			  "leonard told {A} \"{Q}\"!!! adding it to my manuscript. margin note. gold leaf" },
		};
		return Lines[static_cast<int>(Teller)][Variant & 1];
	}

	// =========================================================================================
	// READING WHAT HE WROTE

	std::string Lower(const std::string& Text)
	{
		std::string Out = Text;
		for (char& C : Out)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return Out;
	}

	std::vector<std::string> Words(const std::string& LowerText)
	{
		std::vector<std::string> Out;
		std::string Word;
		for (const char C : LowerText)
		{
			if (std::isalnum(static_cast<unsigned char>(C)))
			{
				Word += C;
			}
			else if (!Word.empty())
			{
				Out.push_back(Word);
				Word.clear();
			}
		}
		if (!Word.empty())
		{
			Out.push_back(Word);
		}
		return Out;
	}

	/** "ring" is a word; "wrestl*" is a stem; "good morning" is a phrase. */
	bool Mentions(const std::string& LowerText, const std::vector<std::string>& WordList, const std::vector<const char*>& Keys)
	{
		for (const char* Key : Keys)
		{
			const std::string K(Key);
			if (K.find(' ') != std::string::npos)
			{
				if (LowerText.find(K) != std::string::npos)
				{
					return true;
				}
				continue;
			}
			const bool bStem = !K.empty() && K.back() == '*';
			const std::string Stem = bStem ? K.substr(0, K.size() - 1) : K;
			for (const std::string& W : WordList)
			{
				if (bStem ? W.compare(0, Stem.size(), Stem) == 0 : W == Stem)
				{
					return true;
				}
			}
		}
		return false;
	}

	const std::vector<const char*>& WantKeys(EFreakPerson Who)
	{
		static const std::vector<const char*> Keys[FreakPeople] = {
			{ "binder*", "listing*", "attic*", "1983", "printout*" },
			{ "coconut*", "proof" },
			{ "belt*", "champion*", "title" },
			{ "dashboard*", "views", "view" },
			{ "cookie*", "banner*", "accept*" },
		};
		return Keys[static_cast<int>(Who)];
	}

	const std::vector<const char*>& FantasyKeys(EFreakPerson Who)
	{
		static const std::vector<const char*> Keys[FreakPeople] = {
			{ "freez*", "frozen", "cryo*", "thaw*", "retire*" },
			{ "island*", "raft*", "sail*", "boat*", "electricity" },
			{ "wrestl*", "cape*", "ring", "garbage collector", "entrance", "music" },
			{ "dog*", "beagle*", "nap*", "walk*", "taste*", "puppy" },
			{ "monk*", "quill*", "calligraph*", "robe*", "manuscript*", "candle*", "vellum" },
		};
		return Keys[static_cast<int>(Who)];
	}

	const std::vector<const char*> DreamKeys = { "dream*", "fantas*", "wish*", "someday", "escape*", "vacation*" };
	const std::vector<const char*> WorkKeys = {
		"work*", "job*", "boss*", "manager*", "meeting*", "code*", "coding", "bug*", "project*", "office*",
		"program*", "deadline*", "sprint*", "batch*", "gate", "class*", "java", "cobol", "python", "javascript",
		"framework*", "npm", "legacy", "ai", "computer*", "software", "firmware", "config*", "standup", "stand" };
	const std::vector<const char*> GreetingKeys = {
		"hi", "hello", "hey", "yo", "howdy", "greetings", "hiya", "sup", "good morning", "good evening", "good afternoon" };

	/** Replace every {Key} in a template. */
	std::string Fill(std::string Text, const std::string& Key, const std::string& Value)
	{
		const std::string Mark = "{" + Key + "}";
		for (size_t At = Text.find(Mark); At != std::string::npos; At = Text.find(Mark, At + Value.size()))
		{
			Text.replace(At, Mark.size(), Value);
		}
		return Text;
	}

	/** Back off to the start of a UTF-8 character, so a cut never splits one. */
	size_t CharBoundary(const std::string& Text, size_t At)
	{
		while (At > 0 && At < Text.size() && (static_cast<unsigned char>(Text[At]) & 0xC0) == 0x80)
		{
			--At;
		}
		return At;
	}

	std::string AgeLabel(int Days, EFreakGate Gate, bool bGossip)
	{
		if (Days <= 0)
		{
			if (bGossip) { return "This morning"; }
			if (Gate == EFreakGate::WantDone) { return "Just now"; }
			return "3 h";
		}
		if (Days == 1)
		{
			return "Yesterday";
		}
		char Buffer[32];
		if (Days < 7)
		{
			std::snprintf(Buffer, sizeof(Buffer), "%d d", Days);
		}
		else
		{
			std::snprintf(Buffer, sizeof(Buffer), "%d w", Days / 7);
		}
		return Buffer;
	}

	std::string Escape(const std::string& Text)
	{
		std::string Out;
		for (const char C : Text)
		{
			if (C == '\\') { Out += "\\\\"; }
			else if (C == '\n') { Out += "\\n"; }
			else if (C == '\t') { Out += "\\t"; }
			else if (C == '\r') { Out += "\\r"; }
			else { Out += C; }
		}
		return Out;
	}

	std::string Unescape(const std::string& Text)
	{
		std::string Out;
		for (size_t I = 0; I < Text.size(); ++I)
		{
			if (Text[I] == '\\' && I + 1 < Text.size())
			{
				const char N = Text[++I];
				Out += N == 'n' ? '\n' : N == 't' ? '\t' : N == 'r' ? '\r' : N;
			}
			else
			{
				Out += Text[I];
			}
		}
		return Out;
	}

	int Clamp(int Value, int Lo, int Hi)
	{
		return Value < Lo ? Lo : (Value > Hi ? Hi : Value);
	}
}

// =============================================================================================

const FFreakProfile& FFreakbook::Profile(EFreakPerson Who)
{
	return FreakbookDetail::Profiles[FreakbookDetail::Clamp(static_cast<int>(Who), 0, FreakPeople - 1)];
}

bool FFreakbook::FromId(const std::string& Id, EFreakPerson& Out)
{
	for (int I = 0; I < FreakPeople; ++I)
	{
		if (Id == FreakbookDetail::Profiles[I].Id)
		{
			Out = static_cast<EFreakPerson>(I);
			return true;
		}
	}
	return false;
}

void FFreakbook::Login(FFreakbookState& S)
{
	++S.Logins;
}

std::vector<FFreakPost> FFreakbook::Feed(const FFreakbookState& S)
{
	using namespace FreakbookDetail;
	struct FRanked
	{
		int Days;
		int Group;   // within a day: what he did, then the gossip, then what they wrote
		int Order;
		FFreakPost Post;
	};
	std::vector<FRanked> Ranked;
	const int Login = S.Logins < 1 ? 1 : S.Logins;
	int Order = 0;
	for (const FWritten& W : Written())
	{
		++Order;
		const int Who = static_cast<int>(W.Who);
		int Days = 0;
		int Group = 2;
		switch (W.Gate)
		{
		case EFreakGate::Always:      Days = W.BaseDays + (Login - 1); break;
		case EFreakGate::SecondVisit: if (Login < 2) { continue; } Days = Login - 2; break;
		case EFreakGate::ThirdVisit:  if (Login < 3) { continue; } Days = Login - 3; break;
		case EFreakGate::WantDone:    if (!S.Done[Who]) { continue; } Days = Login - S.DoneLogin[Who]; Group = 0; break;
		}
		FFreakPost P;
		P.Id = W.Id;
		P.Who = W.Who;
		P.Gate = W.Gate;
		P.When = AgeLabel(Days, W.Gate, false);
		P.Text = W.Text;
		P.Picture = W.Picture;
		P.Likes = W.Likes;
		P.Comments = W.Comments;
		Ranked.push_back({ Days < 0 ? 0 : Days, Group, Order, P });
	}
	for (const FFreakGossip& G : S.Gossip)
	{
		if (G.Login >= Login)
		{
			continue;   // told today: posted tomorrow
		}
		const int Days = Login - (G.Login + 1);
		FFreakPost P;
		P.Id = "gossip_" + std::to_string(G.Serial);
		P.Who = static_cast<EFreakPerson>(Clamp(G.Teller, 0, FreakPeople - 1));
		P.Gate = EFreakGate::Always;
		P.When = AgeLabel(Days, EFreakGate::Always, true);
		P.Text = Fill(Fill(GossipLine(P.Who, G.Serial), "A", Profile(static_cast<EFreakPerson>(Clamp(G.About, 0, FreakPeople - 1))).Short), "Q", G.Quote);
		P.Likes = 1 + G.Serial % 4;
		P.bGossip = true;
		Ranked.push_back({ Days, 1, -G.Serial, P });   // newest gossip first
	}
	std::stable_sort(Ranked.begin(), Ranked.end(), [](const FRanked& A, const FRanked& B)
	{
		if (A.Days != B.Days) { return A.Days < B.Days; }
		if (A.Group != B.Group) { return A.Group < B.Group; }
		return A.Order < B.Order;
	});
	std::vector<FFreakPost> Out;
	Out.reserve(Ranked.size());
	for (FRanked& R : Ranked)
	{
		Out.push_back(std::move(R.Post));
	}
	return Out;
}

std::vector<FFreakPost> FFreakbook::PostsBy(const FFreakbookState& S, EFreakPerson Who)
{
	std::vector<FFreakPost> Out;
	for (FFreakPost& P : Feed(S))
	{
		if (P.Who == Who)
		{
			Out.push_back(std::move(P));
		}
	}
	return Out;
}

bool FFreakbook::IsLiked(const FFreakbookState& S, const std::string& PostId)
{
	return std::find(S.Liked.begin(), S.Liked.end(), PostId) != S.Liked.end();
}

void FFreakbook::ToggleLike(FFreakbookState& S, const std::string& PostId)
{
	const auto It = std::find(S.Liked.begin(), S.Liked.end(), PostId);
	if (It != S.Liked.end())
	{
		S.Liked.erase(It);
	}
	else if (!PostId.empty())
	{
		S.Liked.push_back(PostId);
	}
}

std::string FFreakbook::Clean(const std::string& Message)
{
	std::string Out;
	Out.reserve(Message.size());
	for (const char C : Message)
	{
		const unsigned char U = static_cast<unsigned char>(C);
		if (U == '\n' || U == '\r' || U == '\t')
		{
			Out += ' ';
		}
		else if (U >= 0x20 && U != 0x7F)
		{
			Out += C;
		}
	}
	const size_t First = Out.find_first_not_of(' ');
	if (First == std::string::npos)
	{
		return std::string();
	}
	Out = Out.substr(First, Out.find_last_not_of(' ') - First + 1);
	if (Out.size() > MaxMessage)
	{
		Out.resize(FreakbookDetail::CharBoundary(Out, MaxMessage));
	}
	return Out;
}

std::string FFreakbook::Quote(const std::string& Message)
{
	std::string Q = Clean(Message);
	for (char& C : Q)
	{
		if (C == '"')
		{
			C = '\'';
		}
	}
	constexpr size_t Most = 60;
	if (Q.size() > Most)
	{
		size_t Cut = Q.rfind(' ', Most);
		if (Cut == std::string::npos || Cut < Most / 2)
		{
			Cut = FreakbookDetail::CharBoundary(Q, Most);
		}
		Q = Q.substr(0, Cut) + "...";
	}
	return Q;
}

EFreakTopic FFreakbook::Classify(const FFreakbookState& S, EFreakPerson Who, const std::string& Message)
{
	using namespace FreakbookDetail;
	const std::string Text = Lower(Clean(Message));
	const std::vector<std::string> List = Words(Text);
	const int I = static_cast<int>(Who);
	if (Mentions(Text, List, WantKeys(Who)))
	{
		return S.Done[I] ? EFreakTopic::WantDone : EFreakTopic::Want;
	}
	if (Mentions(Text, List, FantasyKeys(Who)) || Mentions(Text, List, DreamKeys))
	{
		return EFreakTopic::Fantasy;
	}
	if (Mentions(Text, List, WorkKeys))
	{
		return EFreakTopic::Work;
	}
	if (List.size() <= 5 && Mentions(Text, List, GreetingKeys))
	{
		return EFreakTopic::Greeting;
	}
	if (Text.find('?') != std::string::npos)
	{
		return EFreakTopic::Question;
	}
	return EFreakTopic::Other;
}

FFreakMessage FFreakbook::Send(FFreakbookState& S, EFreakPerson Who, const std::string& Message)
{
	using namespace FreakbookDetail;
	const int I = static_cast<int>(Who);
	if (I < 0 || I >= FreakPeople)
	{
		return FFreakMessage();
	}
	const std::string Said = Clean(Message);
	if (Said.empty())
	{
		return FFreakMessage();
	}

	// What he said before this, for remembering it back to him.
	std::vector<std::string> Earlier;
	for (const FFreakMessage& M : S.Threads[I])
	{
		if (M.bFromLeonard && M.Text != Said)
		{
			Earlier.push_back(M.Text);
		}
	}

	EFreakTopic Topic = Classify(S, Who, Said);
	const bool bFirst = S.Replies[I] == 0;
	if (bFirst && (Topic == EFreakTopic::Other || Topic == EFreakTopic::Question))
	{
		Topic = EFreakTopic::Greeting;   // the first thing anyone says back is hello
	}
	// Every third reply, when he has said things before and the subject is not the thing they
	// care most about, they remember one of them instead.
	const bool bRemember = !Earlier.empty() && S.Replies[I] % 3 == 2
		&& Topic != EFreakTopic::Want && Topic != EFreakTopic::Fantasy && Topic != EFreakTopic::WantDone;
	if (bRemember)
	{
		Topic = EFreakTopic::Memory;
	}

	const std::vector<FLine>& Pool = Lines(Who, Topic);
	int& Cursor = S.Cursor[I][static_cast<int>(Topic)];
	const FLine& Line = Pool[static_cast<size_t>(Cursor) % Pool.size()];
	Cursor = (Cursor + 1) % static_cast<int>(Pool.size());

	FFreakMessage Reply;
	Reply.bFromLeonard = false;
	Reply.Text = Line.Text;
	Reply.Picture = Line.Picture;
	if (Topic == EFreakTopic::Memory)
	{
		const std::string& Old = Earlier[static_cast<size_t>(S.Replies[I] / 3) % Earlier.size()];
		Reply.Text = Fill(Reply.Text, "Q", Quote(Old));
	}

	FFreakMessage Mine;
	Mine.bFromLeonard = true;
	Mine.Text = Said;
	std::vector<FFreakMessage>& Thread = S.Threads[I];
	Thread.push_back(Mine);
	Thread.push_back(Reply);
	while (Thread.size() > MaxThread)
	{
		Thread.erase(Thread.begin());
	}
	++S.Replies[I];

	// The gossip: anything longer than a word or two, someone else hears about.
	if (Words(Lower(Said)).size() >= 3)
	{
		FFreakGossip G;
		G.Serial = S.NextGossip++;
		G.About = I;
		G.Teller = (I + 1 + G.Serial % (FreakPeople - 1)) % FreakPeople;   // never the one he told
		G.Quote = Quote(Said);
		G.Login = S.Logins;
		S.Gossip.push_back(G);
		while (S.Gossip.size() > MaxGossip)
		{
			S.Gossip.erase(S.Gossip.begin());
		}
	}
	return Reply;
}

bool FFreakbook::CanGrant(const FFreakbookState& S, EFreakPerson Who)
{
	const int I = static_cast<int>(Who);
	if (I < 0 || I >= FreakPeople || S.Done[I])
	{
		return false;
	}
	const bool bNeedsItem = Profile(Who).Item[0] != '\0';
	return !bNeedsItem || S.Held[I];
}

bool FFreakbook::Grant(FFreakbookState& S, EFreakPerson Who)
{
	if (!CanGrant(S, Who))
	{
		return false;
	}
	const int I = static_cast<int>(Who);
	S.Done[I] = true;
	S.Held[I] = false;
	S.DoneLogin[I] = S.Logins < 1 ? 1 : S.Logins;
	return true;
}

void FFreakbook::TakeItem(FFreakbookState& S, EFreakPerson Who)
{
	const int I = static_cast<int>(Who);
	if (I >= 0 && I < FreakPeople && !S.Done[I] && Profile(Who).Item[0] != '\0')
	{
		S.Held[I] = true;
	}
}

bool FFreakbook::ItemStillThere(const FFreakbookState& S, EFreakPerson Who)
{
	const int I = static_cast<int>(Who);
	return I >= 0 && I < FreakPeople && !S.Held[I] && !S.Done[I];
}

int FFreakbook::Friends(const FFreakbookState& S)
{
	int N = 0;
	for (const bool B : S.Done)
	{
		N += B ? 1 : 0;
	}
	return N;
}

std::vector<std::string> FFreakbook::AllPictures()
{
	using namespace FreakbookDetail;
	std::vector<std::string> Out = { "leonard_avatar", "margo_dashboard_page" };
	auto Add = [&Out](const char* Name)
	{
		if (Name && Name[0] && std::find(Out.begin(), Out.end(), Name) == Out.end())
		{
			Out.push_back(Name);
		}
	};
	for (const FFreakProfile& P : Profiles)
	{
		Add(P.Avatar);
	}
	for (const FWritten& W : Written())
	{
		Add(W.Picture);
	}
	for (int P = 0; P < FreakPeople; ++P)
	{
		for (int T = 0; T < FreakTopics; ++T)
		{
			for (const FLine& L : Lines(static_cast<EFreakPerson>(P), static_cast<EFreakTopic>(T)))
			{
				Add(L.Picture);
			}
		}
	}
	return Out;
}

const std::vector<std::pair<std::string, std::string>>& FFreakbook::PeopleYouMayKnow()
{
	static const std::vector<std::pair<std::string, std::string>> People = {
		{ "Mrs. Hall", "Has never logged in. Says Freakbook is a fad." },
		{ "The Architect", "Designed everything. On vacation since 2004. Never on call." },
		{ "Earl", "Wrote 2,000 lines of Dee's statement run in 1987. Whereabouts unknown." },
	};
	return People;
}

// =============================================================================================
// THE SAVE: plain lines, one fact each, so a future build can read what this one wrote.

std::string FFreakbook::SaveText(const FFreakbookState& S)
{
	using namespace FreakbookDetail;
	std::ostringstream O;
	O << "FREAKBOOK 1\n";
	O << "logins " << S.Logins << "\n";
	O << "verified " << (S.bVerified ? 1 : 0) << "\n";
	O << "machine " << S.MachinePercent << "\n";
	O << "nextgossip " << S.NextGossip << "\n";
	for (int I = 0; I < FreakPeople; ++I)
	{
		O << "person " << Profiles[I].Id << " " << (S.Held[I] ? 1 : 0) << " " << (S.Done[I] ? 1 : 0) << " "
		  << S.DoneLogin[I] << " " << S.Replies[I];
		for (int T = 0; T < FreakTopics; ++T)
		{
			O << " " << S.Cursor[I][T];
		}
		O << "\n";
	}
	for (const std::string& Id : S.Liked)
	{
		O << "like " << Escape(Id) << "\n";
	}
	for (int I = 0; I < FreakPeople; ++I)
	{
		for (const FFreakMessage& M : S.Threads[I])
		{
			O << "msg " << Profiles[I].Id << " " << (M.bFromLeonard ? "L" : "T") << " "
			  << (M.Picture.empty() ? "-" : M.Picture) << " " << Escape(M.Text) << "\n";
		}
	}
	for (const FFreakGossip& G : S.Gossip)
	{
		O << "gossip " << G.Serial << " " << G.Teller << " " << G.About << " " << G.Login << " " << Escape(G.Quote) << "\n";
	}
	O << "end\n";
	return O.str();
}

bool FFreakbook::LoadText(const std::string& Text, FFreakbookState& Out)
{
	using namespace FreakbookDetail;
	std::istringstream In(Text);
	std::string Line;
	if (!std::getline(In, Line) || Line != "FREAKBOOK 1")
	{
		return false;   // blank, garbage, or a version this build does not know
	}
	FFreakbookState S;
	bool bEnd = false;
	while (std::getline(In, Line))
	{
		std::istringstream L(Line);
		std::string Key;
		L >> Key;
		if (Key == "end")
		{
			bEnd = true;
			break;
		}
		if (Key == "logins") { L >> S.Logins; }
		else if (Key == "verified") { int B = 0; L >> B; S.bVerified = B != 0; }
		else if (Key == "machine") { L >> S.MachinePercent; }
		else if (Key == "nextgossip") { L >> S.NextGossip; }
		else if (Key == "person")
		{
			std::string Id;
			int Held = 0, Done = 0;
			L >> Id;
			EFreakPerson Who;
			if (!FromId(Id, Who))
			{
				continue;
			}
			const int I = static_cast<int>(Who);
			L >> Held >> Done >> S.DoneLogin[I] >> S.Replies[I];
			S.Held[I] = Held != 0;
			S.Done[I] = Done != 0;
			for (int T = 0; T < FreakTopics; ++T)
			{
				L >> S.Cursor[I][T];
				if (S.Cursor[I][T] < 0) { S.Cursor[I][T] = 0; }
			}
		}
		else if (Key == "like")
		{
			std::string Id;
			std::getline(L >> std::ws, Id);
			S.Liked.push_back(Unescape(Id));
		}
		else if (Key == "msg")
		{
			std::string Id, From, Picture, Rest;
			L >> Id >> From >> Picture;
			std::getline(L.ignore(1), Rest);
			EFreakPerson Who;
			if (!FromId(Id, Who))
			{
				continue;
			}
			FFreakMessage M;
			M.bFromLeonard = From == "L";
			M.Picture = Picture == "-" ? std::string() : Picture;
			M.Text = Unescape(Rest);
			std::vector<FFreakMessage>& Thread = S.Threads[static_cast<int>(Who)];
			Thread.push_back(M);
			while (Thread.size() > MaxThread)
			{
				Thread.erase(Thread.begin());
			}
		}
		else if (Key == "gossip")
		{
			FFreakGossip G;
			std::string Rest;
			L >> G.Serial >> G.Teller >> G.About >> G.Login;
			std::getline(L.ignore(1), Rest);
			G.Quote = Unescape(Rest);
			if (G.Teller >= 0 && G.Teller < FreakPeople && G.About >= 0 && G.About < FreakPeople)
			{
				S.Gossip.push_back(G);
			}
		}
		// Anything else is from a later build: skipped, not fatal.
	}
	if (!bEnd)
	{
		return false;   // cut short: a half-written file is not a save
	}
	while (S.Gossip.size() > MaxGossip)
	{
		S.Gossip.erase(S.Gossip.begin());
	}
	Out = S;
	return true;
}
