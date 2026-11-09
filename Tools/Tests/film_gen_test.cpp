// [G]'s film generator, held to the three rules at the top of FilmGenRules.cpp. Run by
// run_film_gen_tests.ps1, which passes the repo root so the dances are checked against the
// real list in DancerAgentComponent.cpp.
//
//   film_gen_test.exe <repo>            the checks
//   film_gen_test.exe <repo> print      the checks, and every film written out to be read
#include "FilmGenRules.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

using namespace FilmGen;

static int Failures = 0;
static void Expect(bool bOk, const std::string& What)
{
	if (!bOk) { ++Failures; if (Failures <= 40) std::cout << "FAIL: " << What << "\n"; }
}

static std::string ReadFile(const std::string& Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In) { std::cout << "cannot read " << Path << "\n"; std::exit(2); }
	std::stringstream S;
	S << In.rdbuf();
	return S.str();
}

static std::string Serialize(const FRecipe& R)
{
	std::ostringstream S;
	S << R.Shape << '|' << R.Topic << '|' << R.Title << '\n';
	for (const FShot& Shot : R.Shots)
		S << Shot.Seconds << ' ' << int(Shot.Framing) << ' ' << int(Shot.Move) << ' ' << Shot.Lead << ' ' << Shot.Azimuth << ' '
		  << Shot.Dance << ' ' << Shot.Rate << ' ' << Shot.Sweep << ' ' << Shot.Caption << '\n';
	return S.str();
}

static void Print(const std::string& Wish, uint32_t Take, const FRecipe& R)
{
	float Total = 0.0f;
	for (const FShot& Shot : R.Shots) Total += Shot.Seconds;
	std::cout << "\n=== \"" << Wish << "\" take " << Take << "  [" << R.Shape << ", " << int(Total) << " s]  " << R.Title << "\n";
	for (const FShot& Shot : R.Shots)
		std::cout << (Shot.Framing == EFraming::Card ? "   [" : "    ") << Shot.Caption << (Shot.Framing == EFraming::Card ? "]" : "") << "\n";
}

int main(int Argc, char** Argv)
{
	if (Argc < 2) { std::cout << "usage: film_gen_test <repo> [print]\n"; return 2; }
	const std::string Repo = Argv[1];
	const bool bPrint = Argc > 2 && std::string(Argv[2]) == "print";
	const std::string DanceSource = ReadFile(Repo + "/Source/SibeliusGame/DancerAgentComponent.cpp");

	const char* const Wishes[] = {
		"a film about my old boss", "make me a movie about dancing", "film", "a funny film about the bug I fixed",
		"a sad film about getting old", "movie about Mrs. Hall", "a film about the moon", "I want a film about my knees",
		"a short film about lunch at the deli", "funny dog film", "make a film about you", "a film about chess",
		"a movie about a tired old man who made a video game", "film about AI agents", "a quiet movie about home",
		"a film about nothing", "", "a film about the supercalifragilisticexpialidocious committee meeting of doom" };

	std::set<std::string> Shapes;
	for (const char* Wish : Wishes)
	{
		for (uint32_t Take = 0; Take < 6; ++Take)
		{
			FRecipe A, B;
			Expect(Plan(Wish, Take, {}, A), std::string("plans: ") + Wish);
			Plan(Wish, Take, {}, B);
			Expect(Serialize(A) == Serialize(B), std::string("same words and take, same film: ") + Wish);
			Shapes.insert(A.Shape);
			if (bPrint) Print(Wish, Take, A);

			Expect(static_cast<int>(A.Topic.size()) <= MaxTopicChars && !A.Topic.empty(), "topic fits: " + A.Topic);
			Expect(A.Shots.size() >= 7 && A.Shots.size() <= 11, "7 to 11 shots");
			Expect(A.Shots.front().Framing == EFraming::Card && A.Shots.back().Framing == EFraming::Card, "opens and closes on a card");

			float Total = 0.0f;
			std::set<std::string> Dances;
			EFraming Last = EFraming::Card;
			for (const FShot& Shot : A.Shots)
			{
				Total += Shot.Seconds;
				Expect(Shot.Caption.find("{T") == std::string::npos && Shot.Caption.find("{Y") == std::string::npos
					&& Shot.Caption.find("{M") == std::string::npos, "no unfilled subject: " + Shot.Caption);
				Expect(Shot.Caption.find('|') == std::string::npos || Shot.Framing == EFraming::Card, "only a card splits on |");
				if (Shot.Framing == EFraming::Card) { Last = Shot.Framing; continue; }

				Expect(static_cast<int>(Shot.Caption.size()) <= MaxCaptionChars, "caption fits the bar: " + Shot.Caption);
				Expect(!Shot.Caption.empty(), "a shot says something");
				Expect(Shot.Framing != Last, "no framing twice running");
				Last = Shot.Framing;
				Expect(DanceSource.find("Anim_" + Shot.Dance + "_MH") != std::string::npos, "a real dance: " + Shot.Dance);
				Expect(Dances.insert(Shot.Dance).second, "no dance twice in a film: " + Shot.Dance);
				Expect(Shot.Framing != EFraming::CloseUp || Shot.Rate < 0.6f, "a close-up is slow");
			}
			Expect(Total >= 40.0f && Total <= 85.0f, "40 to 85 seconds");
		}
	}
	Expect(Shapes.size() == 6, "all six shapes turn up");

	// Different takes of one wish are different films.
	{
		std::set<std::string> Seen;
		for (uint32_t Take = 0; Take < 20; ++Take)
		{
			FRecipe R;
			Plan("a film about my old boss", Take, {}, R);
			Seen.insert(Serialize(R));
		}
		Expect(Seen.size() >= 18, "twenty takes are (nearly) twenty films");
	}

	// The subject is found, and said the way she would say it.
	const auto TopicOf = [](const char* Wish) { FRecipe R; Plan(Wish, 0, {}, R); return R.Topic; };
	Expect(TopicOf("a film about my old boss") == "his old boss", "my -> his");
	Expect(TopicOf("make me a movie about the moon") == "the moon", "keeps the subject's article");
	Expect(TopicOf("funny dog film") == "funny dog", "subject before the film word");
	Expect(TopicOf("a film about Mrs. Hall") == "Mrs. Hall", "names keep capitals");
	Expect(TopicOf("a film about you") == "me", "you -> me");
	Expect(TopicOf("a film about {0}|x") == "0 x", "braces and bars cannot reach a caption");
	{
		// A letter is written to him: "my knees" is "your knees" there, and "his" everywhere else.
		bool bLetterSeen = false;
		for (uint32_t Take = 0; Take < 40; ++Take)
		{
			FRecipe R;
			Plan("a film about my knees", Take, {}, R);
			for (const FShot& Shot : R.Shots)
			{
				if (R.Shape == "Letter")
				{
					bLetterSeen = true;
					Expect(Shot.Caption.find("his knees") == std::string::npos, "a letter says your, not his: " + Shot.Caption);
				}
				Expect(R.Shape == "Letter" || Shot.Caption.find("your knees") == std::string::npos, "only a letter says your: " + Shot.Caption);
			}
		}
		Expect(bLetterSeen, "forty takes include a letter");
	}

	// "funny" and "sad" lean the shape.
	for (uint32_t Take = 0; Take < 30; ++Take)
	{
		FRecipe Funny, Sad;
		Plan("a funny film about rain", Take, {}, Funny);
		Plan("a sad film about rain", Take, {}, Sad);
		Expect(Funny.Shape != "Elegy" && Funny.Shape != "Letter", "funny is not an elegy");
		Expect(Sad.Shape == "Elegy" || Sad.Shape == "Letter", "sad is an elegy or a letter");
	}

	// What asks for a film, and what Mrs. Hall will not have.
	Expect(IsFilmWish("a FILM about dogs") && IsFilmWish("movie") && !IsFilmWish("a ladder") && !IsFilmWish("filmy curtains"),
		"film wishes are recognised");
	{
		FRecipe R;
		R.Shape = "untouched";
		Expect(!Plan("a film about badword things", 0, { "badword" }, R) && R.Shape == "untouched", "a blocked word is refused");
	}

	std::cout << (Failures == 0 ? "\nfilm generator: all checks passed\n" : "\nfilm generator: FAILED\n");
	return Failures == 0 ? 0 : 1;
}
