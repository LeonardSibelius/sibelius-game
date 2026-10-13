// freakbook_test.cpp — standalone tests for Source/SibeliusGame/FreakbookModel.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_freakbook_tests.ps1, which passes the
// folder the pictures are drawn into, so the content and the drawings are checked to agree.
//
// The claims: the five are all there and complete; the feed grows with his visits and is newest
// first; they answer by what he wrote about, never repeat themselves back to back, remember what
// he said and pass it on - posted the next day, never by the one he told; what each wants comes
// true only by what he does, and makes them a friend; and the save reads back exactly.

#include "FreakbookModel.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace
{
	int GPasses = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* Name, const std::string& Detail = std::string())
	{
		std::printf("%s  %s%s%s\n", bOk ? "PASS" : "FAIL", Name, Detail.empty() ? "" : "  -- ", Detail.c_str());
		(bOk ? GPasses : GFailures) += 1;
	}

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	FFreakbookState LoggedIn(int Times = 1)
	{
		FFreakbookState S;
		for (int I = 0; I < Times; ++I)
		{
			FFreakbook::Login(S);
		}
		return S;
	}

	bool InFeed(const FFreakbookState& S, const std::string& Id)
	{
		for (const FFreakPost& P : FFreakbook::Feed(S))
		{
			if (P.Id == Id)
			{
				return true;
			}
		}
		return false;
	}

	const EFreakPerson Everyone[] = { EFreakPerson::Dee, EFreakPerson::Gunther, EFreakPerson::Brent, EFreakPerson::Margo, EFreakPerson::Jaxx };
}

int main(int argc, char** argv)
{
	const std::string Art = argc > 1 ? argv[1] : std::string();

	// --- The five ---------------------------------------------------------------------------
	{
		const char* Languages[] = { "COBOL", "C", "Java", "Python", "JavaScript" };
		bool bAll = true;
		std::string Missing;
		for (int I = 0; I < FreakPeople; ++I)
		{
			const FFreakProfile& P = FFreakbook::Profile(Everyone[I]);
			const bool bOk = std::string(P.Language) == Languages[I] && P.Name[0] && P.Job[0] && P.Hates[0] && P.Dreams[0]
				&& P.Wants[0] && P.Thanks[0] && P.Action[0] && P.Avatar[0];
			EFreakPerson Back;
			bAll = bAll && bOk && FFreakbook::FromId(P.Id, Back) && Back == Everyone[I];
			if (!bOk) { Missing += P.Short; }
		}
		Check(bAll, "five people, one per language, every profile field written, ids round-trip", Missing);
		EFreakPerson Unused;
		Check(!FFreakbook::FromId("earl", Unused), "nobody else is on Freakbook");
	}

	// --- Every picture the content names has been drawn --------------------------------------
	{
		const std::vector<std::string> Pictures = FFreakbook::AllPictures();
		Check(Pictures.size() >= 20, "the content uses twenty-odd pictures", std::to_string(Pictures.size()));
		if (!Art.empty())
		{
			std::string Missing;
			for (const std::string& Name : Pictures)
			{
				std::ifstream F(Art + "/" + Name + ".png", std::ios::binary);
				if (!F)
				{
					Missing += Name + " ";
				}
			}
			Check(Missing.empty(), "every picture is drawn (SourceArt/Freakbook)", Missing);
		}
	}

	// --- The feed ---------------------------------------------------------------------------
	{
		const FFreakbookState First = LoggedIn(1);
		const std::vector<FFreakPost> Feed = FFreakbook::Feed(First);
		Check(Feed.size() == 15, "his first visit: fifteen posts, three each", std::to_string(Feed.size()));
		std::set<int> Who;
		for (const FFreakPost& P : Feed) { Who.insert(static_cast<int>(P.Who)); }
		Check(Who.size() == 5, "all five have posted");
		Check(Feed.front().Id == "margo_1" && Feed.front().When == "3 h", "newest first: Margo's zero views, 3 h ago", Feed.front().Id + " " + Feed.front().When);
		Check(!InFeed(First, "dee_4") && !InFeed(First, "dee_5") && !InFeed(First, "dee_done"), "nothing from later visits yet");

		const FFreakbookState Second = LoggedIn(2);
		Check(FFreakbook::Feed(Second).size() == 20 && InFeed(Second, "dee_4") && !InFeed(Second, "dee_5"), "the second visit adds five");
		Check(FFreakbook::Feed(Second).front().When == "3 h", "the new ones are on top");
		Check(FFreakbook::Feed(Second).back().When != "3 h", "and the old ones have aged", FFreakbook::Feed(Second).back().When);
		Check(FFreakbook::Feed(LoggedIn(3)).size() == 25, "the third adds five more");
		Check(FFreakbook::PostsBy(LoggedIn(3), EFreakPerson::Gunther).size() == 5, "a profile shows that person's posts only");

		bool bOrdered = true;
		int LastDays = -1;
		for (const FFreakPost& P : FFreakbook::Feed(LoggedIn(3)))
		{
			const int Days = P.When == "3 h" || P.When == "Just now" || P.When == "This morning" ? 0
				: P.When == "Yesterday" ? 1 : std::atoi(P.When.c_str()) * (Contains(P.When, "w") ? 7 : 1);
			bOrdered = bOrdered && Days >= LastDays;
			LastDays = Days;
		}
		Check(bOrdered, "the feed never goes back in time");
	}

	// --- Likes --------------------------------------------------------------------------------
	{
		FFreakbookState S = LoggedIn();
		FFreakbook::ToggleLike(S, "dee_2");
		Check(FFreakbook::IsLiked(S, "dee_2") && !FFreakbook::IsLiked(S, "dee_1"), "a like is his, on that post");
		FFreakbook::ToggleLike(S, "dee_2");
		Check(!FFreakbook::IsLiked(S, "dee_2") && S.Liked.empty(), "and a second click takes it back");
	}

	// --- Reading what he wrote ----------------------------------------------------------------
	{
		const FFreakbookState S = LoggedIn();
		Check(FFreakbook::Classify(S, EFreakPerson::Dee, "Did you find your binder?") == EFreakTopic::Want, "Dee: the binder is her want");
		Check(FFreakbook::Classify(S, EFreakPerson::Gunther, "I have a COCONUT for you") == EFreakTopic::Want, "Gunther: the coconut, any case");
		Check(FFreakbook::Classify(S, EFreakPerson::Gunther, "how is the raft coming") == EFreakTopic::Fantasy, "Gunther: the raft is his fantasy");
		Check(FFreakbook::Classify(S, EFreakPerson::Brent, "Tell me about wrestling") == EFreakTopic::Fantasy, "Brent: a stem matches (wrestl*)");
		Check(FFreakbook::Classify(S, EFreakPerson::Brent, "nice ringtone") != EFreakTopic::Fantasy, "but 'ring' is a word, not a stem");
		Check(FFreakbook::Classify(S, EFreakPerson::Margo, "I opened your dashboard") == EFreakTopic::Want, "Margo: the dashboard");
		Check(FFreakbook::Classify(S, EFreakPerson::Jaxx, "what do you dream about?") == EFreakTopic::Fantasy, "anyone: a dream is a fantasy");
		Check(FFreakbook::Classify(S, EFreakPerson::Jaxx, "how is work going") == EFreakTopic::Work, "work is work");
		Check(FFreakbook::Classify(S, EFreakPerson::Dee, "hello there") == EFreakTopic::Greeting, "a short hello is a greeting");
		Check(FFreakbook::Classify(S, EFreakPerson::Dee, "hello, I wanted to ask you something long about the weather today") != EFreakTopic::Greeting,
			"a long message that starts with hello is not");
		Check(FFreakbook::Classify(S, EFreakPerson::Margo, "Are you okay?") == EFreakTopic::Question, "a question");
		Check(FFreakbook::Classify(S, EFreakPerson::Margo, "The sky is purple") == EFreakTopic::Other, "and anything else");
	}

	// --- Talking to them ----------------------------------------------------------------------
	{
		FFreakbookState S = LoggedIn();
		const FFreakMessage First = FFreakbook::Send(S, EFreakPerson::Dee, "The sky is purple");
		Check(!First.Text.empty() && !First.bFromLeonard, "she answers");
		Check(S.Threads[0].size() == 2 && S.Threads[0][0].bFromLeonard && S.Threads[0][0].Text == "The sky is purple",
			"his message and her reply are both kept, his first");
		Check(Contains(First.Text, "batch") || Contains(First.Text, "1985") || Contains(First.Text, "message"),
			"the first reply anyone sends is hello", First.Text);

		const FFreakMessage Empty = FFreakbook::Send(S, EFreakPerson::Dee, "   \n\t ");
		Check(Empty.Text.empty() && S.Threads[0].size() == 2, "an empty message is refused and nothing is recorded");

		bool bNoRepeat = true;
		std::string Last;
		FFreakbookState T = LoggedIn();
		for (int I = 0; I < 12; ++I)
		{
			const FFreakMessage R = FFreakbook::Send(T, EFreakPerson::Brent, "tell me about your work please");
			bNoRepeat = bNoRepeat && R.Text != Last;
			Last = R.Text;
		}
		Check(bNoRepeat, "asked the same thing twelve times, never the same answer twice running");

		FFreakbookState M = LoggedIn();
		FFreakbook::Send(M, EFreakPerson::Margo, "I used to build data warehouses");
		FFreakbook::Send(M, EFreakPerson::Margo, "the sky is purple tonight");
		const FFreakMessage Third = FFreakbook::Send(M, EFreakPerson::Margo, "anyway, the weather");
		Check(Contains(Third.Text, "I used to build data warehouses") || Contains(Third.Text, "the sky is purple tonight"),
			"the third reply remembers something he said, word for word", Third.Text);

		FFreakbookState Long = LoggedIn();
		const std::string Huge(1000, 'x');
		FFreakbook::Send(Long, EFreakPerson::Jaxx, Huge);
		Check(Long.Threads[4][0].Text.size() == FFreakbook::MaxMessage, "a message is cut to 280 bytes");
		FFreakbook::Send(Long, EFreakPerson::Jaxx, "line one\nline two");
		Check(Long.Threads[4][2].Text == "line one line two", "and kept on one line");

		FFreakbookState Many = LoggedIn();
		for (int I = 0; I < 30; ++I)
		{
			FFreakbook::Send(Many, EFreakPerson::Gunther, "message number " + std::to_string(I));
		}
		Check(Many.Threads[1].size() == FFreakbook::MaxThread && Many.Threads[1].back().bFromLeonard == false,
			"a thread is bounded, oldest out first", std::to_string(Many.Threads[1].size()));

		const std::string Q = FFreakbook::Quote("I told my manager \"never again\" and then I did it again, the next morning, at the same desk, with the same coffee");
		Check(!Contains(Q, "\"") && Contains(Q, "'never again'") && Contains(Q, "...") && Q.size() <= 64, "a quote: single quotes, whole words, cut short", Q);
	}

	// --- Gossip -------------------------------------------------------------------------------
	{
		FFreakbookState S = LoggedIn();
		FFreakbook::Send(S, EFreakPerson::Dee, "I still have a punch card from 1979");
		FFreakbook::Send(S, EFreakPerson::Dee, "ok");
		Check(S.Gossip.size() == 1, "something worth repeating is passed on; 'ok' is not");
		bool bToday = false;
		for (const FFreakPost& P : FFreakbook::Feed(S)) { bToday = bToday || P.bGossip; }
		Check(!bToday, "not posted the same day he said it");
		FFreakbook::Login(S);
		const FFreakPost* Gossip = nullptr;
		const std::vector<FFreakPost> Feed = FFreakbook::Feed(S);
		for (const FFreakPost& P : Feed) { if (P.bGossip) { Gossip = &P; break; } }
		Check(Gossip && Gossip->Who != EFreakPerson::Dee, "the next day someone else posts it - never Dee herself");
		Check(Gossip && Contains(Gossip->Text, "Dee") && Contains(Gossip->Text, "I still have a punch card from 1979"),
			"naming who he told and quoting what he said", Gossip ? Gossip->Text : std::string());
		Check(Gossip && Gossip->When == "This morning" && Feed.front().bGossip, "this morning's news, at the top");

		FFreakbookState Lots = LoggedIn();
		std::set<int> Tellers;
		for (int I = 0; I < 20; ++I)
		{
			FFreakbook::Send(Lots, EFreakPerson::Brent, "this is message number " + std::to_string(I));
		}
		for (const FFreakGossip& G : Lots.Gossip) { Tellers.insert(G.Teller); }
		Check(Lots.Gossip.size() == FFreakbook::MaxGossip && !Tellers.count(2) && Tellers.size() >= 3,
			"gossip is bounded, passed on by several, and never by the one he told");
	}

	// --- What they want: only what Leonard does -----------------------------------------------
	{
		FFreakbookState S = LoggedIn();
		Check(!FFreakbook::CanGrant(S, EFreakPerson::Dee) && !FFreakbook::Grant(S, EFreakPerson::Dee),
			"Dee's binder cannot be mailed before he has found it");
		Check(FFreakbook::ItemStillThere(S, EFreakPerson::Dee), "it is still in the attic");
		for (int I = 0; I < 6; ++I)
		{
			FFreakbook::Send(S, EFreakPerson::Dee, "I found your binder in the attic");
		}
		Check(!S.Done[0], "talking about it does not make it so");
		FFreakbook::TakeItem(S, EFreakPerson::Dee);
		Check(!FFreakbook::ItemStillThere(S, EFreakPerson::Dee) && FFreakbook::CanGrant(S, EFreakPerson::Dee), "found: now it can be mailed");
		Check(FFreakbook::Grant(S, EFreakPerson::Dee) && S.Done[0] && !S.Held[0] && FFreakbook::Friends(S) == 1, "mailed: she is a friend");
		Check(!FFreakbook::Grant(S, EFreakPerson::Dee), "and only once");
		Check(FFreakbook::Feed(S).front().Id == "dee_done" && FFreakbook::Feed(S).front().When == "Just now", "her post about it, just now, at the top");
		Check(FFreakbook::Classify(S, EFreakPerson::Dee, "do you like the binder?") == EFreakTopic::WantDone, "and she talks about it as done");
		Check(!FFreakbook::ItemStillThere(S, EFreakPerson::Dee), "it does not reappear in the attic");

		Check(FFreakbook::CanGrant(S, EFreakPerson::Margo) && FFreakbook::Grant(S, EFreakPerson::Margo), "Margo needs only the laptop: open the dashboard");
		Check(FFreakbook::Grant(S, EFreakPerson::Jaxx) && FFreakbook::Friends(S) == 3, "Jaxx too: click Accept");
		FFreakbook::TakeItem(S, EFreakPerson::Margo);
		Check(!S.Held[3], "nothing to carry for the ones who need no item");
		Check(!FFreakbook::Grant(S, EFreakPerson::Gunther) && !FFreakbook::Grant(S, EFreakPerson::Brent), "the coconut and the belt still wait to be found");
	}

	// --- The save -----------------------------------------------------------------------------
	{
		FFreakbookState S = LoggedIn(2);
		S.bVerified = true;
		S.MachinePercent = 63;
		FFreakbook::Send(S, EFreakPerson::Dee, "a line with \"quotes\", a back\\slash and a tab\there");
		FFreakbook::Send(S, EFreakPerson::Gunther, "how is the raft?");
		FFreakbook::Send(S, EFreakPerson::Gunther, "is that a C joke?");
		FFreakbook::TakeItem(S, EFreakPerson::Brent);
		FFreakbook::Grant(S, EFreakPerson::Margo);
		FFreakbook::ToggleLike(S, "jaxx_2");
		FFreakbook::ToggleLike(S, "gossip_1");
		const std::string Text = FFreakbook::SaveText(S);
		FFreakbookState Back;
		const bool bLoaded = FFreakbook::LoadText(Text, Back);
		Check(bLoaded && FFreakbook::SaveText(Back) == Text, "the save reads back exactly");
		Check(bLoaded && Back.Threads[0][0].Text == "a line with \"quotes\", a back\\slash and a tab here", "quotes and backslashes survive", bLoaded ? Back.Threads[0][0].Text : "");
		Check(bLoaded && Back.Held[2] && Back.Done[3] && Back.bVerified && Back.MachinePercent == 63 && Back.Logins == 2,
			"what he holds, what he did, his verdict and his visits");
		Check(bLoaded && FFreakbook::Send(Back, EFreakPerson::Gunther, "how is the raft?").Text == FFreakbook::Send(S, EFreakPerson::Gunther, "how is the raft?").Text,
			"and the conversation carries on exactly where it was");

		FFreakbookState Junk;
		Check(!FFreakbook::LoadText("", Junk) && !FFreakbook::LoadText("hello", Junk) && !FFreakbook::LoadText("FREAKBOOK 2\nend\n", Junk),
			"a blank, garbage or newer save is refused");
		Check(!FFreakbook::LoadText(Text.substr(0, Text.size() / 2), Junk), "so is one cut short");
		Check(FFreakbook::LoadText("FREAKBOOK 1\nlogins 4\nsomething-new 1 2 3\nend\n", Junk) && Junk.Logins == 4, "a line from a later build is skipped, not fatal");
	}

	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
