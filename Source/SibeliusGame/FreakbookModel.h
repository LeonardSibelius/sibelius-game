// FreakbookModel.h
//
// FREAKBOOK — the fake social network on the living-room laptop, past PROVE YOU'RE NOT HUMAN
// (docs/FREAKBOOK.md; Walt, 2026-10-12: "let us call it Freakbook and the five people are all
// computer programmers, using Java, COBOL, Python, Javascript and C. They work on boring projects
// that they hate and they talk about their fantasies, which are comical" - and the fantasies are
// "meant to be pitiful"). Five people post, Leonard reads the feed, writes to them, and they
// answer, remember what he said, and gossip about it to each other.
//
// ===========================================================================
// THE HOUSE METHOD (FNotHumanRun, FGoGame): a pure model, standard library only, deterministic,
// tested standalone by Tools/Tests/run_freakbook_tests.ps1. UFreakbookSubsystem keeps one state
// and saves it; UFreakbookWidget only presents it.
//
// EVERY WORD IS WRITTEN HERE, IN THE GAME. Replies are picked from the lines below by what Leonard
// wrote about - his work, their fantasy, the thing they want - and rotate so nobody repeats
// themselves soon; every third reply or so remembers something he said earlier and quotes it back.
// Nothing goes to the chat server: Freakbook's live replies wait for Valve's answer on the chat
// review (docs/FREAKBOOK.md), and when they come they replace Send's body, not its shape.
//
// THE GAME DECIDES, NEVER A MODEL. What each of them wants - Dee's binder, Gunther's coconut,
// Brent's belt, one view of Margo's dashboard, one Accept on Jaxx's banner - comes true only by
// what Leonard does: finds the thing in the house and sends it, or clicks the button on the page.
// Getting it makes that person a friend instead of an acquaintance, and they post about it.
//
// UTF-8 throughout; the content is plain ASCII on purpose, so no font or encoding can mangle it.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class EFreakPerson : uint8_t
{
	Dee,       // COBOL, 74
	Gunther,   // C, 58
	Brent,     // Java, 44
	Margo,     // Python, 31
	Jaxx,      // JavaScript, 24
	Count
};

constexpr int FreakPeople = static_cast<int>(EFreakPerson::Count);

/** When a written post shows up in the feed. */
enum class EFreakGate : uint8_t
{
	Always,
	SecondVisit,   // from Leonard's second login
	ThirdVisit,    // from his third
	WantDone,      // once he has given them what they wanted
};

/** What a message from Leonard was about, as far as the person reading it can tell. */
enum class EFreakTopic : uint8_t
{
	Greeting,
	Work,
	Fantasy,
	Want,
	WantDone,   // he talks about the thing he already gave them
	Question,
	Other,
	Memory,     // not a topic of his: a reply that quotes something he said before
	Count
};

constexpr int FreakTopics = static_cast<int>(EFreakTopic::Count);

struct FFreakProfile
{
	const char* Id;          // "dee": save files and picture names use it
	const char* Name;        // Dolores "Dee" Pickering
	const char* Short;       // Dee
	int Age;
	const char* Language;    // COBOL
	const char* Job;
	const char* Hates;
	const char* Dreams;
	const char* Wants;
	const char* Thanks;      // the Wants line once Leonard has seen to it
	const char* Item;        // what he must find in the house first ("" when the laptop is enough)
	const char* Action;      // the profile's button: "Mail Dee her binder", "Open Margo's dashboard"
	const char* Avatar;      // picture name
};

struct FFreakComment
{
	EFreakPerson Who = EFreakPerson::Dee;
	std::string Text;
};

struct FFreakPost
{
	std::string Id;          // stable: likes are saved by it
	EFreakPerson Who = EFreakPerson::Dee;
	EFreakGate Gate = EFreakGate::Always;
	std::string When;        // "2 d", "Just now": what the feed prints
	std::string Text;
	std::string Picture;     // empty, or a picture name
	int Likes = 0;           // before Leonard's
	std::vector<FFreakComment> Comments;
	bool bGossip = false;    // written from something Leonard said, not from the content
};

struct FFreakMessage
{
	bool bFromLeonard = false;
	std::string Text;
	std::string Picture;     // a reply may carry a photo
};

/** Something Leonard told one of them, passed on by another, posted at his next login. */
struct FFreakGossip
{
	int Serial = 0;          // its post id is "gossip_<Serial>": stable while older ones are dropped
	int Teller = 0;
	int About = 0;           // the one he told
	std::string Quote;
	int Login = 0;           // the login it was told in; it is posted from the next one
};

struct FFreakbookState
{
	int Logins = 0;
	bool bVerified = false;      // passed through notHUMAN once: the laptop then lets him straight in
	int MachinePercent = -1;     // what notHUMAN last said he was, for the welcome line
	bool Held[FreakPeople] = {};     // the thing they want, found and not yet sent
	bool Done[FreakPeople] = {};
	int DoneLogin[FreakPeople] = {};
	int Replies[FreakPeople] = {};
	int Cursor[FreakPeople][FreakTopics] = {};
	std::vector<std::string> Liked;
	std::vector<FFreakMessage> Threads[FreakPeople];
	std::vector<FFreakGossip> Gossip;
	int NextGossip = 1;
};

class FFreakbook
{
public:
	/** Messages kept per person. A bound, like the agents' MaxTurnsKept: the oldest go first. */
	static constexpr size_t MaxThread = 40;
	/** Gossip kept: the feed carries the newest few. */
	static constexpr size_t MaxGossip = 8;
	/** Longest message Leonard can send, in bytes of UTF-8. */
	static constexpr size_t MaxMessage = 280;

	static const FFreakProfile& Profile(EFreakPerson Who);
	static bool FromId(const std::string& Id, EFreakPerson& Out);

	/** He logged in: the counter that unlocks the second- and third-visit posts and the gossip. */
	static void Login(FFreakbookState& S);

	/** Everything he can see now, newest first. */
	static std::vector<FFreakPost> Feed(const FFreakbookState& S);

	/** One person's posts, newest first: their profile page. */
	static std::vector<FFreakPost> PostsBy(const FFreakbookState& S, EFreakPerson Who);

	static bool IsLiked(const FFreakbookState& S, const std::string& PostId);
	static void ToggleLike(FFreakbookState& S, const std::string& PostId);

	/**
	 * Leonard writes to someone. Records his message and their reply, queues the gossip, and
	 * returns the reply. An empty message (after trimming) is refused: the reply is empty and
	 * nothing is recorded.
	 */
	static FFreakMessage Send(FFreakbookState& S, EFreakPerson Who, const std::string& Message);

	/** What a message is about, for that reader. Exposed for the tests. */
	static EFreakTopic Classify(const FFreakbookState& S, EFreakPerson Who, const std::string& Message);

	/** Whether the profile's button can do its thing now (the item found, or none needed). */
	static bool CanGrant(const FFreakbookState& S, EFreakPerson Who);

	/** Do it: their want comes true. False if it cannot (yet) or already has. */
	static bool Grant(FFreakbookState& S, EFreakPerson Who);

	/** He picked up the thing they want, somewhere in the house. */
	static void TakeItem(FFreakbookState& S, EFreakPerson Who);

	/** Whether the thing is still lying where it was: neither taken nor already sent. */
	static bool ItemStillThere(const FFreakbookState& S, EFreakPerson Who);

	/** Friends, not acquaintances: the people he has done right by. */
	static int Friends(const FFreakbookState& S);

	/** A message, cleaned: one line, no control characters, at most MaxMessage bytes. */
	static std::string Clean(const std::string& Message);

	/** A short quote of a message, for gossip and memory: whole words, double quotes made single. */
	static std::string Quote(const std::string& Message);

	/** Every picture the content can show, for the tests and the import script to agree on. */
	static std::vector<std::string> AllPictures();

	/** The state as text, for the save slot; LoadText refuses anything it does not recognise. */
	static std::string SaveText(const FFreakbookState& S);
	static bool LoadText(const std::string& Text, FFreakbookState& Out);

	/** The people Leonard may know, in the right-hand column: not on Freakbook, and never will be. */
	static const std::vector<std::pair<std::string, std::string>>& PeopleYouMayKnow();
};
