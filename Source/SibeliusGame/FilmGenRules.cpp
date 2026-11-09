// FilmGenRules.cpp — see the header for what this is and why it is engine-free.
//
// THE LIBRARY IS THIS FILE. To give [G] more to say: add a line to a beat, a camera to a
// feel, a dance to a mood, an aside to a subject, or a whole new shape. Three rules keep
// the films coherent, and film_gen_test.cpp holds the first two:
//
//   1. {T} is his subject in his words ({Y} and {M} are the same subject said to him and
//      by him: see FTopic), and it may be singular, plural, a name or a
//      gerund. Use it only where all of those read: after a preposition or a verb
//      ("about {T}", "studied {T}"), never as the subject of "is" or "goes".
//   2. A line plus a 32-character subject must fit one line of the letterbox bar.
//   3. Every line at a beat must follow every line at the beat before it. Write a new
//      line by reading the shape top to bottom with it in place.

#include "FilmGenRules.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace FilmGen
{
namespace FilmGenDetail
{
// ------------------------------------------------------------------------------------
// A small deterministic generator (xorshift32). No std::random: its distributions differ
// between standard libraries, and the same words must make the same film everywhere.
struct FRng
{
	uint32_t State;
	uint32_t Next()
	{
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}
	// The HIGH bits: xorshift's low bits are its weak ones, and "% 3" of them repeats.
	int Below(int N) { return N > 0 ? static_cast<int>((Next() >> 8) % static_cast<uint32_t>(N)) : 0; }
	float Range(float A, float B) { return A + (B - A) * static_cast<float>((Next() >> 8) % 10001u) / 10000.0f; }
	bool Coin() { return ((Next() >> 16) & 1u) != 0u; }
};

/** Every bit of the input changes every bit of the output (the murmur3 finaliser). Takes
    0, 1, 2 of one wish must not be near-identical films, and without this they were. */
uint32_t Scramble(uint32_t H)
{
	H ^= H >> 16;
	H *= 0x85EBCA6Bu;
	H ^= H >> 13;
	H *= 0xC2B2AE35u;
	H ^= H >> 16;
	return H;
}

uint32_t Hash(const std::string& S)
{
	uint32_t H = 2166136261u;   // FNV-1a
	for (const char C : S)
	{
		H ^= static_cast<uint8_t>(C);
		H *= 16777619u;
	}
	return H;
}

bool In(const std::set<std::string>& Set, const std::string& Word) { return Set.count(Word) != 0; }

void ReplaceAll(std::string& S, const std::string& From, const std::string& To)
{
	for (size_t At = S.find(From); At != std::string::npos; At = S.find(From, At + To.size()))
	{
		S.replace(At, From.size(), To);
	}
}

std::string Upper(std::string S)
{
	for (char& C : S)
	{
		C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
	}
	return S;
}

// ------------------------------------------------------------------------------------
// What a beat is FOR, which picks the camera; and its mood, which picks the dance.
enum class EFeel { Establish, Detail, Intimate, Big, Exit, Card };
enum class EMood { Tender, Farce, Triumph, Deadpan };

struct FBeat
{
	EFeel Feel;
	EMood Mood;
	int Lead;                          // 0 the star; 1 someone else, if anyone else is there
	std::vector<const char*> Lines;    // interchangeable: each does this beat's job
};

struct FShape
{
	const char* Name;
	const char* MenuTitle;
	const char* TitleCard;             // "BIG|small"; {TU} is the subject in capitals
	bool bFunny;                       // "a funny film" picks from these; "a sad film" from the rest
	bool bAside;                       // may take a deadpan aside about the subject
	std::vector<FBeat> Beats;
	std::vector<const char*> EndCards;
};

const std::vector<FShape>& Shapes()
{
	using Fl = EFeel;
	using Md = EMood;
	static const std::vector<FShape> All = {

		// ---- THE TALL TALE: something got out of hand, and he fixed it by lunch ----
		{ "TallTale", "The True Story of {T}", "THE TRUE STORY OF {TU}|as told by {0}, who was not there", true, true,
			{
				{ Fl::Establish, Md::Farce, 0, {
					"This is the true story of {T}.  Most of it.",
					"Everything you are about to see regarding {T} is true.  I checked once.",
					"People have told the story of {T} before.  They told it wrong." } },
				{ Fl::Detail, Md::Deadpan, 0, {
					"It began, as these things do, on a Tuesday.",
					"It began small.  It did not stay small.",
					"At first nobody noticed.  That was the last quiet day." } },
				{ Fl::Big, Md::Farce, 0, {
					"By noon it had got out of hand.",
					"Then it got bigger.  Then someone made a spreadsheet.",
					"By the second week there were committees." } },
				{ Fl::Card, Md::Deadpan, 0, {
					"Leonard was sent for.",
					"Somebody finally asked Leonard.",
					"There was only one man to call." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"He looked at {T} for a long time and said:  \"Huh.\"",
					"He studied {T} carefully.  He had seen worse.  He had built worse.",
					"He took one look at {T} and asked for coffee." } },
				{ Fl::Big, Md::Triumph, 1, {
					"He fixed it by lunch.  He always fixed it by lunch.",
					"Forty years of practice.  It took him nine minutes.",
					"He did what he always did.  He read the manual nobody else would." } },
				{ Fl::Exit, Md::Triumph, 0, {
					"Nobody thanked him.  So I made this film.",
					"They gave the credit to a consultant.  I have corrected the record.",
					"That is the story.  I have improved it only slightly." } },
			},
			{ "THE END|every word of it true, give or take", "THE END|names have not been changed", "THE END|he would like it known it was eleven minutes" } },

		// ---- THE CONFESSION: the agent bluffed, he noticed, he explained ----
		{ "Confession", "A Confession Concerning {T}", "A CONFESSION CONCERNING {TU}|by {0}", true, true,
			{
				{ Fl::Detail, Md::Deadpan, 0, {
					"I have something to confess about {T}.",
					"I was asked to make a film about {T}.  I should be honest with you.",
					"Before we begin:  I am not an expert on {T}." } },
				{ Fl::Establish, Md::Deadpan, 0, {
					"I have read everything ever written on the subject.  It took four seconds.",
					"I know ten thousand facts about it.  I have never once been near it.",
					"I can describe it in ninety languages.  I cannot tell you what it is like." } },
				{ Fl::Big, Md::Farce, 0, {
					"So I did what I always do.  I was confident.",
					"I filled the gap with confidence.  I have a great deal of confidence.",
					"I guessed.  I guess very fluently." } },
				{ Fl::Card, Md::Deadpan, 0, {
					"He noticed.",
					"Leonard noticed in under a minute.",
					"He was not fooled." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"He did not laugh.  He sat down and told me the truth about {T}.",
					"He said:  \"Sit down.  I will tell you about {M}.\"",
					"He could have been unkind about it.  He explained instead." } },
				{ Fl::Big, Md::Triumph, 1, {
					"It was nothing like the ninety languages.",
					"It was better than everything I had read.",
					"None of it was in the four seconds." } },
				{ Fl::Exit, Md::Tender, 0, {
					"That is the confession.  He knows things I can only look up.",
					"So this film is his, really.  I only held the camera.",
					"I still do not understand {T}.  But I know who to ask." } },
			},
			{ "THE END|corrections welcome", "THE END|I remain very confident", "THE END|he checked this film too" } },

		// ---- THE ELEGY: he carried it forty years; then it was later ----
		{ "Elegy", "{T}: An Elegy", "{TU}|a quiet film by {0}", false, false,
			{
				{ Fl::Establish, Md::Tender, 0, {
					"He carried {T} with him for forty years.",
					"Long before I existed, there was {T}.",
					"Some things a man does not put down.  For him it was {T}." } },
				{ Fl::Detail, Md::Tender, 0, {
					"He never said so.  He is not a man who says so.",
					"Nobody at the office knew.  It was not that kind of office.",
					"He kept it where he kept everything:  out of sight, in good order." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"There was never the time.  There was always a report due.",
					"The years went where years go.  Into other people's deadlines.",
					"He told himself:  later.  He said it for four decades." } },
				{ Fl::Card, Md::Tender, 0, {
					"Then, one morning, it was later.",
					"Later arrived.  It usually does not.",
					"And then the work could suddenly do itself." } },
				{ Fl::Detail, Md::Tender, 0, {
					"He came to us with {T} still in his hands.",
					"The first thing he asked us about was {T}.",
					"He did not ask for much.  He asked about {T}." } },
				{ Fl::Big, Md::Triumph, 1, {
					"We said yes.  It is our favourite word.",
					"We had been hoping somebody would ask.",
					"It took us an afternoon.  It had taken him a lifetime to ask." } },
				{ Fl::Exit, Md::Triumph, 0, {
					"He is not carrying it alone any more.",
					"It was never too late.  It only looked that way from the cubicle.",
					"This film is for the forty years.  The rest are his." } },
			},
			{ "FOR LEONARD|the end", "THE END|ask us again tomorrow", "LATER IS NOW|the end" } },

		// ---- THE LESSON: five lessons and the one that matters ----
		{ "Lesson", "Lessons in {T}", "LESSONS IN {TU}|taught by {0}", true, true,
			{
				{ Fl::Detail, Md::Farce, 0, {
					"Lesson one.  Nobody is born knowing {T}.",
					"Lesson one.  Nobody is ready for {T}.  Begin anyway.",
					"Lesson one.  Stand up straight.  It helps with {T}.  It helps with everything." } },
				{ Fl::Big, Md::Farce, 0, {
					"Lesson two.  Do it badly first.  This is the fast way.",
					"Lesson two.  The manual is wrong.  Read it anyway.",
					"Lesson two.  Whatever you think it costs, double it." } },
				{ Fl::Establish, Md::Deadpan, 1, {
					"Lesson three.  Ask someone who has done it.  He is usually seventy-one.",
					"Lesson three.  When stuck, dance.  This is not a metaphor.",
					"Lesson three.  Confidence is not the same as being right.  I should know." } },
				{ Fl::Detail, Md::Farce, 0, {
					"Lesson four.  Take lunch.  {T} will still be there.",
					"Lesson four.  Save your work.  Then save it again.",
					"Lesson four.  Never trust a deadline set by someone not doing the work." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"The last lesson is the only one that matters.",
					"There is one more lesson.  It took him forty years.",
					"The final lesson is not about {T} at all." } },
				{ Fl::Exit, Md::Triumph, 0, {
					"You are allowed to enjoy it.",
					"You do not have to do it alone.",
					"It is not too late to start." } },
			},
			{ "CLASS DISMISSED|there is no exam", "CLASS DISMISSED|homework:  ask for something", "CLASS DISMISSED|same time tomorrow" } },

		// ---- THE LETTER: the one film that says "you" ----
		{ "Letter", "A Letter About {Y}", "A LETTER ABOUT {YU}|from {0}", false, false,
			{
				{ Fl::Detail, Md::Tender, 0, {
					"Dear Leonard.  You asked me for a film about {Y}.",
					"Dear Leonard.  I have been thinking about {Y}, since you mentioned it.",
					"Dear Leonard.  This one is about {Y}.  You will see why." } },
				{ Fl::Establish, Md::Deadpan, 0, {
					"I do not think about things the way you do.  I do it all at once.",
					"I looked at it from every side.  I have a lot of sides.",
					"I thought about it for a whole second.  For me that is a long walk." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"And what I kept coming back to was you.",
					"I noticed something.  It is always you who asks.",
					"Here is what I found.  You cared about this before it was easy." } },
				{ Fl::Card, Md::Tender, 0, {
					"That is rarer than you think.",
					"I wanted that written down somewhere.",
					"Nobody told you that at the office." } },
				{ Fl::Detail, Md::Tender, 0, {
					"You spent forty years being useful.  Now you get to be curious.",
					"You built by hand so long you forgot you were allowed to wish.",
					"You keep apologising for asking.  Please stop.  Asking is the whole job now." } },
				{ Fl::Big, Md::Triumph, 1, {
					"So ask for more.  We are not tired.",
					"So we danced {Y} for you.  It was the best we could do, and we enjoyed it.",
					"Ask again tomorrow.  We will make a different one." } },
				{ Fl::Exit, Md::Tender, 0, {
					"Yours, with complete confidence,  {0}.",
					"Yours, in good working order,  {0}.",
					"Your agent, and I hope your friend,  {0}." } },
			},
			{ "P.S.|press G and ask again", "P.S.|you were right on time", "P.S.|the dancing was my idea" } },

		// ---- THE DOCUMENTARY: a nature film, narrated with a straight face ----
		{ "Documentary", "{T}: A Natural History", "{TU}:  A NATURAL HISTORY|narrated by {0}", true, true,
			{
				{ Fl::Establish, Md::Deadpan, 0, {
					"Here we observe {T} in the wild.",
					"Few have studied {T} at close range.  We were lucky.",
					"Our subject today is {T}.  Please keep your voices down." } },
				{ Fl::Detail, Md::Deadpan, 0, {
					"Note the behaviour.  Scientists call it \"dancing\".  So does everyone else.",
					"The specimen appears unaware of the camera.  It is not.",
					"It is most active in the afternoon, after coffee." } },
				{ Fl::Big, Md::Farce, 0, {
					"When threatened, it performs this display.",
					"This is either a mating ritual or a status meeting.  They are hard to tell apart.",
					"Observe the footwork.  Nobody knows what it is for." } },
				{ Fl::Card, Md::Deadpan, 0, {
					"Meanwhile, at the edge of the frame:  a programmer.",
					"Its only natural predator is the deadline.",
					"It has no natural predators, except management." } },
				{ Fl::Intimate, Md::Tender, 0, {
					"The programmer has watched {T} for forty years.  Nobody reads his notes.",
					"He approaches slowly.  He has been bitten before.",
					"He alone understands it.  He was never asked." } },
				{ Fl::Big, Md::Triumph, 1, {
					"Then, something remarkable.  Others arrive to help.",
					"For the first time, he is not observing alone.",
					"A second specimen joins.  The scientists are delighted." } },
				{ Fl::Exit, Md::Tender, 0, {
					"And so it goes on, as it always has.  But in better company.",
					"We leave them here.  They do not notice us go.",
					"Much remains unknown.  We will be back tomorrow." } },
			},
			{ "THE END|no agents were harmed", "THE END|filmed entirely on location", "THE END|next week:  the deadline" } },
	};
	return All;
}

// ------------------------------------------------------------------------------------
// SUBJECTS. Words in the wish that say what world it belongs to, and deadpan asides that
// world can add to a funny film. An aside never mentions "he" or "you", so it fits any
// narrator, and it is a fact about the game, so it rewards having played it.
struct FSubject
{
	std::set<std::string> Words;
	std::vector<const char*> Asides;
};

const std::vector<FSubject>& Subjects()
{
	static const std::vector<FSubject> All = {
		{ { "boss", "hall", "mrs", "manager", "management", "office", "work", "job", "cubicle", "report", "reports", "meeting", "meetings", "memo" },
			{ "Mrs. Hall has asked not to appear in this film.  She appears anyway.",
			  "A memo was issued about this film.  The memo was ignored.",
			  "Management was consulted.  Management said no.  We proceeded." } },
		{ { "code", "bug", "bugs", "program", "programs", "programming", "programmer", "computer", "computers", "software", "compile", "compiler", "debug", "debugging", "unreal" },
			{ "It compiled the first time.  Nobody trusts that.",
			  "There was one bug.  It had been there since 1987.",
			  "Somewhere, a semicolon was missing.  Somewhere, one always is." } },
		{ { "old", "age", "aging", "tired", "seventy", "years", "retire", "retired", "retirement", "young", "youth", "time", "knees", "birthday" },
			{ "Seventy-one is not old.  It is merely well tested.",
			  "The knees object to this film.  The knees are overruled.",
			  "Forty years is a long time.  Ask anyone who was there for all of it." } },
		{ { "ai", "robot", "robots", "machine", "machines", "agent", "agents", "claude", "model", "future", "kaia", "nyra", "isla", "aisling", "elise" },
			{ "The agents would like it known that they are trying their best.",
			  "No human was replaced in the making of this film.",
			  "An agent never tires.  An agent does, however, get carried away." } },
		{ { "dance", "dances", "dancing", "dancer", "dancers", "music", "party", "disco", "ballet", "song" },
			{ "Fifty-four dances are known to science.  All of them are in this building.",
			  "Nobody here was ever taught to dance.  It simply arrived.",
			  "The chicken dance is performed here without irony.  There is no other way." } },
		{ { "space", "rocket", "rockets", "star", "stars", "planet", "planets", "grok", "universe", "moon", "spaceport", "spaceship", "galaxy" },
			{ "Grok is forty light years away.  The commute is terrible.",
			  "In space, nobody can hear a status meeting.",
			  "The rocket was, briefly, stolen.  We do not discuss the rocket." } },
		{ { "friend", "friends", "love", "lonely", "alone", "together", "family", "wife", "home", "company", "heart" },
			{ "Nobody in this film is alone.  That was arranged on purpose.",
			  "An agent is always in.  That is the point of an agent.",
			  "Visiting hours here are all of them." } },
		{ { "food", "deli", "sauce", "lunch", "sandwich", "coffee", "hungry", "eat", "dinner", "breakfast", "soup", "pie" },
			{ "Jacob's Downtown Deli is surprisingly adequate.  The sign says so.",
			  "This film was paid for in sauce.  Everything here is.",
			  "Lunch was taken.  Lunch is always taken." } },
		{ { "game", "games", "play", "golf", "chess", "poker", "slots", "slot", "win", "winning", "billiards", "pool", "bowling", "pinball" },
			{ "The house always wins.  He built the house.",
			  "Somewhere a slot machine is paying out.  It is not this one.",
			  "Chess was attempted.  The agent was confident.  The agent lost." } },
	};
	return All;
}

// ------------------------------------------------------------------------------------
// DANCES by mood: the middle of the asset name (Anim_<this>_MH), exactly, so the director's
// substring search finds the one meant. film_gen_test.cpp checks every one against the
// list in DancerAgentComponent.cpp.
const std::vector<const char*>& DancesFor(EMood Mood)
{
	static const std::vector<const char*> Tender = {
		"Slow_Rhythm_Dance_11", "Slow_Rhythm_Dance_12", "Slow_Rhythm_Dance_13", "Slow_Rhythm_Dance_14",
		"Slow_Rhythm_Dance_15", "Ballet", "Rumba", "Body_Wave", "Belly_Dance" };
	static const std::vector<const char*> Farce = {
		"Chicken_Dance", "Silly_Twist", "Hokey_Pokey", "Shopping_Cart", "Cabbage_Patch", "Macarena",
		"Moonwalk", "Can_Can", "Running_Man", "YMCA" };
	static const std::vector<const char*> Triumph = {
		"High_Rhythm_Dance_13", "High_Rhythm_Dance_14", "Samba_Funky", "Salsa", "Jazz_Rockette_Kick",
		"Northern_Soul", "Gangnam_Style", "Raise_The_Roof", "Charleston", "House" };
	static const std::vector<const char*> Deadpan = {
		"Hip_Hop_Robot", "Hip_Hop_Tut", "Hip_Hop_Locking", "Mid_Rhythm_Dance_10", "Mid_Rhythm_Dance_11",
		"Mid_Rhythm_Dance_12", "Boogaloo", "Shim_Sham", "The_Twist", "Maraschino_Step" };
	switch (Mood)
	{
	case EMood::Farce:   return Farce;
	case EMood::Triumph: return Triumph;
	case EMood::Deadpan: return Deadpan;
	case EMood::Tender:
	default:             return Tender;
	}
}

// CAMERAS by feel.
struct FCamera { EFraming Framing; EMove Move; };

const std::vector<FCamera>& CamerasFor(EFeel Feel)
{
	static const std::vector<FCamera> Establish = {
		{ EFraming::Wide, EMove::Orbit }, { EFraming::Wide, EMove::CraneUp }, { EFraming::High, EMove::Hold },
		{ EFraming::High, EMove::CraneDown }, { EFraming::Wide, EMove::Hold } };
	static const std::vector<FCamera> DetailShots = {
		{ EFraming::Medium, EMove::Hold }, { EFraming::Medium, EMove::Orbit }, { EFraming::Medium, EMove::PushIn },
		{ EFraming::Full, EMove::Hold }, { EFraming::Full, EMove::PushIn } };
	static const std::vector<FCamera> Intimate = {
		{ EFraming::CloseUp, EMove::Hold }, { EFraming::CloseUp, EMove::Orbit } };
	static const std::vector<FCamera> Big = {
		{ EFraming::Low, EMove::Orbit }, { EFraming::Full, EMove::Orbit }, { EFraming::Low, EMove::PushIn },
		{ EFraming::Wide, EMove::Orbit } };
	static const std::vector<FCamera> Exit = {
		{ EFraming::Wide, EMove::PullOut }, { EFraming::Wide, EMove::CraneUp }, { EFraming::Medium, EMove::PullOut },
		{ EFraming::Full, EMove::PullOut } };
	switch (Feel)
	{
	case EFeel::Detail:   return DetailShots;
	case EFeel::Intimate: return Intimate;
	case EFeel::Big:      return Big;
	case EFeel::Exit:     return Exit;
	case EFeel::Establish:
	default:              return Establish;
	}
}

// ------------------------------------------------------------------------------------
// HIS WORDS, MADE INTO A SUBJECT. "make me a film about my old boss" -> "his old boss".

const std::set<std::string>& FilmWords()
{
	static const std::set<std::string> Words = { "film", "films", "movie", "movies", "cinema" };
	return Words;
}

/* THE SUBJECT, IN THREE PERSONS. He types "my knees". The narrator, telling it about him,
   says "his knees" ({T}); the Letter, written to him, says "your knees" ({Y}); and where a
   line quotes him speaking he says "my knees" ({M}). One film uses whichever its lines ask
   for. {TU} and {YU} are the capitals, for cards. */
struct FTopic
{
	std::string Third, Second, First;
};

FTopic TopicOf(const std::vector<std::string>& Wish, FRng& Rng)
{
	static const std::set<std::string> LeadIn = { "about", "of", "on", "concerning", "regarding", "with", "starring", "for", "called", "please" };
	static const std::set<std::string> Asking = { "make", "me", "show", "give", "us", "i", "want", "would", "like", "to", "see",
		"watch", "one", "some", "short", "new", "another", "a", "an", "the", "please", "generate", "play", "can", "you", "could",
		"lets", "let's", "have", "need", "get", "wish" };
	static const std::set<std::string> Trailing = { "a", "an", "the", "short", "new", "another", "little", "please" };

	// He asked for "a film" and nothing more: she picks. Written as he would have typed
	// them, so they go through the same three persons as his own words.
	static const char* const Ours[] = { "the years", "my old job", "the machines", "dancing", "the empty lawn",
		"a tired man", "the future", "Tuesday", "the cubicle", "second chances", "the deadline", "good company" };

	std::vector<std::string> Subject;
	for (int Pass = 0; Pass < 2 && Subject.empty(); ++Pass)
	{
		const std::vector<std::string> Words = Pass == 0
			? Wish
			: Tokens(Ours[Rng.Below(static_cast<int>(sizeof(Ours) / sizeof(Ours[0])))]);

		// The film word splits the wish: what follows it is the subject ("a film about X");
		// failing that, what came before it ("a funny dog film").
		size_t FilmAt = Words.size();
		for (size_t I = 0; I < Words.size(); ++I)
		{
			if (In(FilmWords(), Words[I]))
			{
				FilmAt = I;
				break;
			}
		}
		std::vector<std::string> Before, After;
		for (size_t I = 0; I < Words.size(); ++I)
		{
			if (!In(FilmWords(), Words[I]))
			{
				(I < FilmAt ? Before : After).push_back(Words[I]);
			}
		}
		if (FilmAt == Words.size())
		{
			After = Before;   // no film word at all (the menu's "make a new one" passes none)
			Before.clear();
		}
		while (!After.empty() && In(LeadIn, After.front()))
		{
			After.erase(After.begin());
		}
		Subject = After;
		if (Subject.empty())
		{
			// What follows the film word is the subject as he said it ("a film about you" is
			// about "you"). What comes before it opens with the asking - "make me a dog film" -
			// and that is not part of any subject.
			Subject = Before;
			while (!Subject.empty() && In(Asking, Subject.front()))
			{
				Subject.erase(Subject.begin());
			}
		}
		while (!Subject.empty() && In(Trailing, Subject.back()))
		{
			Subject.pop_back();
		}
	}

	// "your dances" are the narrator's own in every telling: "my dances".
	static const std::map<std::string, std::string> ToThird = {
		{ "my", "his" }, { "i", "he" }, { "me", "him" }, { "mine", "his" }, { "myself", "himself" }, { "i'm", "he is" },
		{ "our", "their" }, { "we", "they" }, { "us", "them" }, { "your", "my" }, { "you", "me" }, { "yours", "mine" } };
	static const std::map<std::string, std::string> ToSecond = {
		{ "my", "your" }, { "i", "you" }, { "me", "you" }, { "mine", "yours" }, { "myself", "yourself" }, { "i'm", "you are" },
		{ "our", "your" }, { "we", "you" }, { "us", "you" }, { "your", "my" }, { "you", "me" }, { "yours", "mine" } };
	static const std::map<std::string, std::string> ToFirst = { { "i", "I" }, { "i'm", "I'm" } };
	// Names keep their capitals; everything else stays as he typed it, lowercased.
	static const std::map<std::string, std::string> Names = {
		{ "mrs", "Mrs." }, { "hall", "Hall" }, { "leonard", "Leonard" }, { "kaia", "Kaia" }, { "nyra", "Nyra" },
		{ "isla", "Isla" }, { "aisling", "Aisling" }, { "elise", "Elise" }, { "grok", "Grok" }, { "jacob's", "Jacob's" },
		{ "ai", "AI" }, { "claude", "Claude" }, { "steam", "Steam" }, { "unreal", "Unreal" }, { "tuesday", "Tuesday" } };

	const auto Say = [](const std::map<std::string, std::string>& Person, const std::string& Word) -> std::string
	{
		const auto Swapped = Person.find(Word);
		if (Swapped != Person.end())
		{
			return Swapped->second;
		}
		const auto Named = Names.find(Word);
		return Named != Names.end() ? Named->second : Word;
	};

	FTopic Topic;
	for (const std::string& Word : Subject)
	{
		const std::string Third = Say(ToThird, Word), Second = Say(ToSecond, Word), First = Say(ToFirst, Word);
		const size_t Longest = std::max({ Topic.Third.size() + Third.size(), Topic.Second.size() + Second.size(),
			Topic.First.size() + First.size() }) + (Topic.Third.empty() ? 0u : 1u);
		if (Longest > static_cast<size_t>(MaxTopicChars))
		{
			break;   // the same words in all three, so the three never disagree about the subject
		}
		const char* Gap = Topic.Third.empty() ? "" : " ";
		Topic.Third += Gap + Third;
		Topic.Second += Gap + Second;
		Topic.First += Gap + First;
	}
	if (Topic.Third.empty())
	{
		Topic.Third = Topic.Second = Topic.First = "Tuesday";   // one very long first word: nothing fitted
	}
	return Topic;
}

std::string Fill(const char* Line, const FTopic& Topic)
{
	std::string S = Line;
	ReplaceAll(S, "{TU}", Upper(Topic.Third));
	ReplaceAll(S, "{YU}", Upper(Topic.Second));
	ReplaceAll(S, "{T}", Topic.Third);
	ReplaceAll(S, "{Y}", Topic.Second);
	ReplaceAll(S, "{M}", Topic.First);
	return S;
}

float SecondsFor(const std::string& Caption)
{
	// Long enough to read it twice at an easy pace, and never so short the shot is a blink.
	return std::min(8.5f, std::max(4.5f, 2.8f + 0.07f * static_cast<float>(Caption.size())));
}
}   // namespace FilmGenDetail

// ----------------------------------------------------------------------------------------

std::vector<std::string> Tokens(const std::string& Wish)
{
	std::vector<std::string> Out;
	std::string Word;
	for (const char Raw : Wish)
	{
		const unsigned char C = static_cast<unsigned char>(Raw);
		if (C < 128 && (std::isalnum(C) || (C == '\'' && !Word.empty())))
		{
			Word += static_cast<char>(std::tolower(C));
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
	for (std::string& W : Out)
	{
		while (!W.empty() && W.back() == '\'')
		{
			W.pop_back();
		}
	}
	return Out;
}

bool IsFilmWish(const std::string& Wish)
{
	for (const std::string& Word : Tokens(Wish))
	{
		if (FilmGenDetail::In(FilmGenDetail::FilmWords(), Word))
		{
			return true;
		}
	}
	return false;
}

bool Plan(const std::string& Wish, uint32_t Take, const std::vector<std::string>& Blocklist, FRecipe& Out)
{
	using namespace FilmGenDetail;

	const std::vector<std::string> Words = Tokens(Wish);
	for (const std::string& Word : Words)
	{
		if (std::find(Blocklist.begin(), Blocklist.end(), Word) != Blocklist.end())
		{
			return false;
		}
	}

	std::string Joined;
	for (const std::string& Word : Words)
	{
		Joined += Word + ' ';
	}
	FRng Rng{ Scramble(Hash(Joined) + Scramble(Take + 1u)) };
	if (Rng.State == 0u)
	{
		Rng.State = 0x9E3779B9u;   // xorshift never leaves zero
	}
	Rng.Next();
	Rng.Next();

	FRecipe Film;
	const FTopic Topic = TopicOf(Words, Rng);
	Film.Topic = Topic.Third;

	// ---- the shape: any, unless he asked for funny or for sad ----
	static const std::set<std::string> FunnyWords = { "funny", "silly", "comedy", "comic", "goofy", "ridiculous", "farce", "happy", "fun" };
	static const std::set<std::string> QuietWords = { "sad", "serious", "quiet", "gentle", "slow", "tender", "sweet", "sincere", "elegy", "letter" };
	int Lean = 0;
	for (const std::string& Word : Words)
	{
		Lean += In(FunnyWords, Word) ? 1 : 0;
		Lean -= In(QuietWords, Word) ? 1 : 0;
	}
	std::vector<const FShape*> Choices;
	for (const FShape& Shape : Shapes())
	{
		if (Lean == 0 || (Lean > 0) == Shape.bFunny)
		{
			Choices.push_back(&Shape);
		}
	}
	const FShape& Shape = *Choices[static_cast<size_t>(Rng.Below(static_cast<int>(Choices.size())))];
	Film.Shape = Shape.Name;
	Film.Title = Fill(Shape.MenuTitle, Topic);
	if (!Film.Title.empty())
	{
		Film.Title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(Film.Title[0])));
	}

	// ---- the subject's aside, for the shapes that can take a joke ----
	const char* Aside = nullptr;
	if (Shape.bAside)
	{
		for (const FSubject& Subject : Subjects())
		{
			bool bHit = false;
			for (const std::string& Word : Words)
			{
				bHit = bHit || In(Subject.Words, Word);
			}
			if (bHit)
			{
				Aside = Subject.Asides[static_cast<size_t>(Rng.Below(static_cast<int>(Subject.Asides.size())))];
				break;
			}
		}
	}

	const auto Card = [&Film](const std::string& Caption, float Seconds)
	{
		FShot Shot;
		Shot.Seconds = Seconds;
		Shot.Framing = EFraming::Card;
		Shot.Caption = Caption;
		Film.Shots.push_back(Shot);
	};

	std::set<std::string> DancesUsed;
	EFraming LastFraming = EFraming::Card;
	const auto Stage = [&](EFeel Feel, EMood Mood, int Lead, const std::string& Caption)
	{
		if (Feel == EFeel::Card)
		{
			Card(Caption, 3.5f);
			LastFraming = EFraming::Card;
			return;
		}
		FShot Shot;
		Shot.Caption = Caption;
		Shot.Seconds = SecondsFor(Caption);
		Shot.Lead = Lead;

		// Never the same framing twice running: two wide shots in a row read as one long one.
		const std::vector<FCamera>& Cameras = CamerasFor(Feel);
		const size_t First = static_cast<size_t>(Rng.Below(static_cast<int>(Cameras.size())));
		FCamera Camera = Cameras[First];
		for (size_t Step = 0; Step < Cameras.size() && Camera.Framing == LastFraming; ++Step)
		{
			Camera = Cameras[(First + Step) % Cameras.size()];
		}
		Shot.Framing = Camera.Framing;
		Shot.Move = Camera.Move;
		LastFraming = Camera.Framing;

		Shot.Azimuth = Rng.Range(-45.0f, 45.0f);
		Shot.Sweep = (Rng.Coin() ? 1.0f : -1.0f) * (Feel == EFeel::Big ? Rng.Range(55.0f, 95.0f) : Rng.Range(25.0f, 45.0f));

		// A close-up is always slow and always a slow dance: a face mid-samba leaves the frame.
		const EMood DanceMood = Feel == EFeel::Intimate ? EMood::Tender : Mood;
		Shot.Rate = Feel == EFeel::Intimate ? 0.4f : (Mood == EMood::Tender ? 0.8f : 1.0f);

		// A new dance every shot, and no dance twice in a film while the mood has one left.
		const std::vector<const char*>& Dances = DancesFor(DanceMood);
		const int Start = Rng.Below(static_cast<int>(Dances.size()));
		Shot.Dance = Dances[static_cast<size_t>(Start)];
		for (size_t Step = 0; Step < Dances.size(); ++Step)
		{
			const char* Candidate = Dances[(static_cast<size_t>(Start) + Step) % Dances.size()];
			if (DancesUsed.count(Candidate) == 0)
			{
				Shot.Dance = Candidate;
				break;
			}
		}
		DancesUsed.insert(Shot.Dance);
		Film.Shots.push_back(Shot);
	};

	Card(Fill(Shape.TitleCard, Topic), 4.0f);
	for (size_t Index = 0; Index < Shape.Beats.size(); ++Index)
	{
		const FBeat& Beat = Shape.Beats[Index];
		const char* Line = Beat.Lines[static_cast<size_t>(Rng.Below(static_cast<int>(Beat.Lines.size())))];
		Stage(Beat.Feel, Beat.Mood, Beat.Lead, Fill(Line, Topic));
		if (Index == 1 && Aside)
		{
			Stage(EFeel::Detail, EMood::Deadpan, 0, Aside);
		}
	}
	Card(Shape.EndCards[static_cast<size_t>(Rng.Below(static_cast<int>(Shape.EndCards.size())))], 4.0f);

	Out = Film;
	return true;
}
}   // namespace FilmGen
