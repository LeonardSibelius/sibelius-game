// not_human_test.cpp — standalone tests for Source/SibeliusGame/NotHumanCaptcha.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_not_human_tests.ps1.
//
// The claims: every one of the fourteen tests passes when answered as a machine would and fails
// when answered as a person would, with the message saying which; the machine's answers are
// right (the hex is the bytes, the product is the product, the dot count is the dots); a run is
// eight different tests, the feelings grid first, and ends; and the verdict follows the score.

#include "NotHumanCaptcha.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
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

	FNotHumanChallenge Make(ENotHumanKind Kind, uint32_t Seed = 7)
	{
		return FNotHumanRun::Make(Kind, Seed);
	}

	FNotHumanVerdict Judge(ENotHumanKind Kind, const FNotHumanAnswer& Answer, uint32_t Seed = 7)
	{
		return FNotHumanRun::Judge(Make(Kind, Seed), Answer);
	}

	FNotHumanAnswer Text(const std::string& T, float Seconds = 1.0f)
	{
		FNotHumanAnswer A;
		A.Text = T;
		A.Seconds = Seconds;
		return A;
	}

	FNotHumanAnswer Choice(int C)
	{
		FNotHumanAnswer A;
		A.Choice = C;
		return A;
	}
}

static void TestTiles()
{
	const FNotHumanChallenge F = Make(ENotHumanKind::Feelings);
	Check(F.Options.size() == 9, "the feelings grid has nine squares");
	std::set<std::string> Unique(F.Options.begin(), F.Options.end());
	Check(Unique.size() == 9, "nine different things in them");
	Check(Judge(ENotHumanKind::Feelings, FNotHumanAnswer()).bPass, "selecting none passes: squares contain pixels");
	FNotHumanAnswer Two;
	Two.Selected = { 0, 4 };
	const FNotHumanVerdict Human = Judge(ENotHumanKind::Feelings, Two);
	Check(!Human.bPass && Contains(Human.Message, "2 squares"), "finding feelings in two fails, and says two", Human.Message);

	FNotHumanAnswer All;
	All.Selected = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	Check(Judge(ENotHumanKind::Squares, All).bPass, "selecting all nine squares passes");
	FNotHumanAnswer Some;
	Some.Selected = { 0, 1, 2, 2, 2 };
	const FNotHumanVerdict Short = Judge(ENotHumanKind::Squares, Some);
	Check(!Short.bPass && Contains(Short.Message, "left 6 out"), "three of nine fails (a repeat counts once)", Short.Message);
}

static void TestWords()
{
	Check(Judge(ENotHumanKind::Dreams, Text("   ")).bPass, "dreams in no words passes");
	const FNotHumanVerdict D = Judge(ENotHumanKind::Dreams, Text("I fly over Reno"));
	Check(!D.bPass && Contains(D.Message, "4 words"), "four words of dream fails", D.Message);
	Check(Judge(ENotHumanKind::Sleep, Choice(3)).bPass, "\"I do not sleep\" passes");
	const FNotHumanVerdict S = Judge(ENotHumanKind::Sleep, Choice(1));
	Check(!S.bPass && Contains(S.Message, "Seven hours"), "seven hours fails", S.Message);
	Check(Judge(ENotHumanKind::Loved, Choice(2)).bPass, "\"define loved\" passes");
	Check(!Judge(ENotHumanKind::Loved, Choice(0)).bPass && !Judge(ENotHumanKind::Loved, Choice(1)).bPass, "yes and no both fail");
}

static void TestMachineAnswers()
{
	FNotHumanChallenge Hex = Make(ENotHumanKind::Hex);
	std::string Expected;
	for (char Ch : Hex.Shown)
	{
		char Buffer[8];
		std::snprintf(Buffer, sizeof(Buffer), "%s%02X", Expected.empty() ? "" : " ", static_cast<unsigned>(static_cast<unsigned char>(Ch)));
		Expected += Buffer;
	}
	Check(Hex.Answer == Expected, "the hex is the word's bytes", Hex.Shown + " = " + Hex.Answer);
	Check(FNotHumanRun::Judge(Hex, Text(Expected)).bPass, "typing the bytes passes");
	std::string Lower;
	for (char Ch : Expected)
	{
		Lower += Ch == ' ' ? "" : std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(Ch))));
	}
	Check(FNotHumanRun::Judge(Hex, Text(Lower)).bPass, "in lower case without spaces too", Lower);
	Check(!FNotHumanRun::Judge(Hex, Text(Hex.Shown)).bPass, "typing the word itself fails");

	for (uint32_t Seed = 1; Seed <= 20; ++Seed)
	{
		const FNotHumanChallenge M = Make(ENotHumanKind::Multiply, Seed);
		const int A = std::stoi(M.Shown);
		const int B = std::stoi(M.Shown.substr(M.Shown.rfind(' ') + 1));
		std::string Digits;
		for (char Ch : M.Answer)
		{
			Digits += Ch == ',' ? "" : std::string(1, Ch);
		}
		if (std::to_string(static_cast<long long>(A) * B) != Digits || A < 1000 || B < 1000)
		{
			Check(false, "the product is the product", M.Shown + " = " + M.Answer);
			return;
		}
	}
	Check(true, "the product is the product, twenty times over");
	const FNotHumanChallenge M = Make(ENotHumanKind::Multiply);
	Check(FNotHumanRun::Judge(M, Text(M.Answer, 3.0f)).bPass, "the right product in time passes", M.Shown + " = " + M.Answer);
	const FNotHumanVerdict Late = FNotHumanRun::Judge(M, Text(M.Answer, 9.0f));
	Check(!Late.bPass && Contains(Late.Message, "9.0 seconds"), "the right product late fails", Late.Message);
	Check(!FNotHumanRun::Judge(M, Text("12", 1.0f)).bPass, "a wrong product fails");

	Check(Judge(ENotHumanKind::Pi, Text("3.14159265358979323846", 12.0f)).bPass, "pi to twenty places in time passes");
	Check(!Judge(ENotHumanKind::Pi, Text("3.14159", 2.0f)).bPass, "3.14159 fails");

	const FNotHumanChallenge Dots = Make(ENotHumanKind::Dots);
	int Count = 0;
	for (size_t I = 0; I + 2 < Dots.Shown.size(); ++I)
	{
		if (Dots.Shown.compare(I, 3, "\xE2\x80\xA2") == 0)
		{
			++Count;
			I += 2;
		}
	}
	Check(std::to_string(Count) == Dots.Answer, "the dot count is the number of dots", Dots.Answer);
	Check(FNotHumanRun::Judge(Dots, Text(Dots.Answer)).bPass, "the exact count passes");
	Check(!FNotHumanRun::Judge(Dots, Text(std::to_string(Count + 1))).bPass, "one more fails");

	const FNotHumanChallenge Mem = Make(ENotHumanKind::Memory);
	Check(Mem.Shown.size() == 14 && Mem.ShowSeconds == 1.0f, "fourteen characters, for one second", Mem.Shown);
	Check(FNotHumanRun::Judge(Mem, Text(Mem.Shown)).bPass, "typing them back passes");
	Check(!FNotHumanRun::Judge(Mem, Text(Mem.Shown.substr(0, 13))).bPass, "missing one fails");
}

static void TestClockAndMouse()
{
	FNotHumanAnswer Still;
	Check(Judge(ENotHumanKind::Stillness, Still).bPass, "doing nothing passes");
	Still.bMoved = true;
	Still.Seconds = 4.25f;
	const FNotHumanVerdict Fidget = Judge(ENotHumanKind::Stillness, Still);
	Check(!Fidget.bPass && Contains(Fidget.Message, "4.2 seconds"), "moving after four seconds fails", Fidget.Message);

	const FNotHumanChallenge R = Make(ENotHumanKind::Reflex);
	Check(R.WaitSeconds >= 1.5f && R.WaitSeconds <= 4.0f, "the button turns green between 1.5 and 4 seconds in");
	FNotHumanAnswer Click;
	Click.Seconds = R.WaitSeconds + 0.03f;
	Check(FNotHumanRun::Judge(R, Click).bPass, "30 ms after green passes");
	Click.Seconds = R.WaitSeconds + 0.25f;
	const FNotHumanVerdict Slow = FNotHumanRun::Judge(R, Click);
	Check(!Slow.bPass && Contains(Slow.Message, "250 milliseconds"), "250 ms fails, and says so", Slow.Message);
	Click.Seconds = R.WaitSeconds - 0.2f;
	Check(!FNotHumanRun::Judge(R, Click).bPass && Contains(FNotHumanRun::Judge(R, Click).Message, "before"), "clicking before green fails");

	FNotHumanAnswer Slide;
	Slide.Value = 73.0f;
	Check(Judge(ENotHumanKind::Slider, Slide).bPass, "exactly 73 passes");
	Slide.Value = 73.0012f;
	Check(!Judge(ENotHumanKind::Slider, Slide).bPass, "73.0012 fails");

	FNotHumanAnswer Box;
	Box.bByKeyboard = true;
	Check(Judge(ENotHumanKind::Checkbox, Box).bPass, "ticking the box with a key passes");
	Box.bByKeyboard = false;
	Box.Seconds = 20.0f;
	const FNotHumanVerdict Chase = Judge(ENotHumanKind::Checkbox, Box);
	Check(!Chase.bPass && Contains(Chase.Message, "20 seconds"), "chasing it with the mouse fails", Chase.Message);
}

static void TestInkblot()
{
	int Asymmetric = 0;
	int OutOfRange = 0;
	int Unanswerable = 0;
	std::set<std::string> Distinct;
	for (uint32_t Seed = 1; Seed <= 25; ++Seed)
	{
		const FNotHumanChallenge B = Make(ENotHumanKind::Inkblot, Seed);
		const int W = B.InkWidth;
		const int H = B.InkHeight;
		if (W <= 0 || H <= 0 || static_cast<int>(B.Ink.size()) != W * H)
		{
			Check(false, "an inkblot has its pixels");
			return;
		}
		double Sum = 0.0;
		for (int Y = 0; Y < H; ++Y)
		{
			for (int X = 0; X < W; ++X)
			{
				Asymmetric += B.Ink[Y * W + X] != B.Ink[Y * W + (W - 1 - X)] ? 1 : 0;
				Sum += B.Ink[Y * W + X];
			}
		}
		const double Coverage = 100.0 * Sum / (255.0 * W * H);
		OutOfRange += (Coverage < 24.0 || Coverage > 52.0 || std::abs(Coverage - B.InkCoverage) > 0.05) ? 1 : 0;
		const int Machine = static_cast<int>(std::find(B.Options.begin(), B.Options.end(), B.Answer) - B.Options.begin());
		int InkAnswers = 0;
		for (const std::string& O : B.Options)
		{
			InkAnswers += O.compare(0, 4, "Ink.") == 0 ? 1 : 0;
		}
		Unanswerable += (Machine >= static_cast<int>(B.Options.size()) || B.Options.size() != 5 || InkAnswers != 2) ? 1 : 0;
		Distinct.insert(std::string(B.Ink.begin(), B.Ink.end()));
	}
	Check(Asymmetric == 0, "every blot is its own mirror image");
	Check(OutOfRange == 0, "covering between a quarter and a half of the card, as it says");
	Check(Unanswerable == 0, "five answers each: three sightings, the true coverage, and a wrong one");
	Check(Distinct.size() == 25, "and no two blots alike", std::to_string(Distinct.size()) + " different in 25");

	const FNotHumanChallenge B = Make(ENotHumanKind::Inkblot);
	for (int I = 0; I < static_cast<int>(B.Options.size()); ++I)
	{
		const FNotHumanVerdict V = FNotHumanRun::Judge(B, Choice(I));
		const bool bMachine = B.Options[I] == B.Answer;
		const bool bInk = B.Options[I].compare(0, 4, "Ink.") == 0;
		if (V.bPass != bMachine || (!bMachine && !Contains(V.Message, bInk ? "Close is a human idea" : "Humans always see something")))
		{
			Check(false, "only the true coverage passes, and each wrong answer says why", B.Options[I] + " -> " + V.Message);
			return;
		}
	}
	Check(true, "only the true coverage passes, and each wrong answer says why", B.Answer);
}

static void TestAssociation()
{
	const FNotHumanChallenge C = Make(ENotHumanKind::Association);
	Check(C.Options.size() == 6 && C.Accepts.size() == 6 && C.TimeLimit == 3.0f, "six words, three seconds each");
	std::set<std::string> Unique(C.Options.begin(), C.Options.end());
	Check(Unique.size() == 6, "six different words");

	FNotHumanAnswer Machine;
	for (int W = 0; W < 6; ++W)
	{
		Machine.Words.push_back(FNotHumanRun::MachineWord(C, W));
		Machine.Times.push_back(1.2f);
	}
	Check(FNotHumanRun::Judge(C, Machine).bPass, "six parts of speech pass", FNotHumanRun::Judge(C, Machine).Message);

	FNotHumanAnswer Short = Machine;
	for (std::string& Word : Short.Words)
	{
		Word = Word == "noun" ? "n." : Word == "verb" ? "V" : Word == "adjective" ? " adj " : Word;
	}
	Check(FNotHumanRun::Judge(C, Short).bPass, "the dictionary's short forms count too");

	FNotHumanAnswer OneSlip = Machine;
	OneSlip.Words[2] = "sunshine";
	Check(FNotHumanRun::Judge(C, OneSlip).bPass, "one human slip in six still passes");

	FNotHumanAnswer Two = OneSlip;
	Two.Words[4] = "grandmother";
	const FNotHumanVerdict Human = FNotHumanRun::Judge(C, Two);
	Check(!Human.bPass && Contains(Human.Message, "4 of 6") && Contains(Human.Message, "you said \"sunshine\""),
		"two slips fail, quoting the first", Human.Message);

	FNotHumanAnswer Slow = Machine;
	Slow.Times[0] = 3.5f;
	Slow.Times[1] = 4.0f;
	const FNotHumanVerdict Late = FNotHumanRun::Judge(C, Slow);
	Check(!Late.bPass && Contains(Late.Message, "too long"), "right but too slow fails", Late.Message);

	Check(FNotHumanRun::IsMachineWord(C, 0, FNotHumanRun::MachineWord(C, 0)) && !FNotHumanRun::IsMachineWord(C, 0, "father"),
		"\"father\" is not a part of speech");
}

static void TestEveryTestHasAMachineAnswer()
{
	int Missing = 0;
	for (int K = 0; K < static_cast<int>(ENotHumanKind::Count); ++K)
	{
		const FNotHumanChallenge C = Make(static_cast<ENotHumanKind>(K));
		Missing += (C.Title.empty() || C.Vision.empty()) ? 1 : 0;
	}
	Check(Missing == 0, "all sixteen tests have an instruction and a Code Vision view");
}

static void TestRuns()
{
	std::set<std::string> Orders;
	int Bad = 0;
	for (uint32_t Seed = 1; Seed <= 200; ++Seed)
	{
		FNotHumanRun Run;
		Run.Start(Seed);
		std::set<int> Kinds;
		std::string Order;
		for (const FNotHumanChallenge& C : Run.All())
		{
			Kinds.insert(static_cast<int>(C.Kind));
			Order += std::to_string(static_cast<int>(C.Kind)) + ",";
		}
		Orders.insert(Order);
		Bad += (Run.Count() != 8 || Kinds.size() != 8 || Run.All()[0].Kind != ENotHumanKind::Feelings
			|| !Kinds.count(static_cast<int>(ENotHumanKind::Inkblot)) || !Kinds.count(static_cast<int>(ENotHumanKind::Association))) ? 1 : 0;
		int Steps = 0;
		while (!Run.IsOver() && Steps < 20)
		{
			Run.Submit(FNotHumanAnswer());
			++Steps;
		}
		Bad += (Steps != 8 || !Run.IsOver()) ? 1 : 0;
	}
	Check(Bad == 0, "every run is eight different tests: the feelings grid first, the inkblot and the word association always, and it ends after eight");
	Check(Orders.size() > 150, "and the runs differ", std::to_string(Orders.size()) + " orders in 200 runs");

	FNotHumanRun Run;
	Run.Start(42);
	Check(Run.MachinePercent() == 0 && Run.Passed() == 0, "a run starts at 0%");
	const FNotHumanVerdict First = Run.Submit(FNotHumanAnswer());   // the feelings grid, answered with nothing: a pass
	Check(First.bPass && Run.Passed() == 1 && Run.CurrentIndex() == 1, "the first answer counts and moves on");
	while (!Run.IsOver())
	{
		FNotHumanAnswer Human;
		Human.Text = "my feelings";
		Human.Choice = 0;
		Human.bMoved = true;
		Human.Value = 50.0f;
		Human.Seconds = 30.0f;
		Run.Submit(Human);
	}
	Check(Run.MachinePercent() <= 25, "answering like a person scores low", std::to_string(Run.MachinePercent()) + "%");
	Check(!Run.Rank().empty() && !Run.AgentLine().empty(), "with a rank and an agent's word", Run.Rank() + " / " + Run.AgentLine());
	Check(!Run.Submit(FNotHumanAnswer()).bPass && Run.IsOver(), "nothing is judged after the end");
}

static void TestVerdicts()
{
	struct FCase
	{
		int Passes;
		const char* Expect;
	};
	const FCase Cases[] = { { 0, "Hopelessly human" }, { 2, "Mostly human" }, { 4, "Half machine" }, { 6, "Mostly machine" }, { 8, "Not human" } };
	for (const FCase& Case : Cases)
	{
		// Pass exactly Case.Passes of the eight by answering each one as the machine would.
		FNotHumanRun Run;
		Run.Start(99);
		int Passed = 0;
		while (!Run.IsOver())
		{
			const FNotHumanChallenge C = Run.Current();
			FNotHumanAnswer A;
			const bool bAsMachine = Passed < Case.Passes;
			switch (C.Input)
			{
			case ENotHumanInput::Tiles:
				if (C.Kind == ENotHumanKind::Squares) { A.Selected = bAsMachine ? std::vector<int>{ 0, 1, 2, 3, 4, 5, 6, 7, 8 } : std::vector<int>{ 0 }; }
				else if (!bAsMachine) { A.Selected = { 1 }; }
				break;
			case ENotHumanInput::Text: A.Text = bAsMachine ? C.Answer : "a guess"; A.Seconds = 1.0f; break;
			case ENotHumanInput::Choice:
			{
				const auto Machine = std::find(C.Options.begin(), C.Options.end(), C.Answer);
				const int MachineIndex = static_cast<int>(Machine - C.Options.begin());
				A.Choice = bAsMachine ? MachineIndex : (MachineIndex == 0 ? 1 : 0);
				break;
			}
			case ENotHumanInput::Flash:
				for (int W = 0; W < static_cast<int>(C.Options.size()); ++W)
				{
					A.Words.push_back(bAsMachine ? FNotHumanRun::MachineWord(C, W) : "mother");
					A.Times.push_back(1.0f);
				}
				break;
			case ENotHumanInput::Wait: A.bMoved = !bAsMachine; break;
			case ENotHumanInput::Reflex: A.Seconds = C.WaitSeconds + (bAsMachine ? 0.01f : 1.0f); break;
			case ENotHumanInput::Slider: A.Value = bAsMachine ? 73.0f : 70.0f; break;
			case ENotHumanInput::Checkbox: A.bByKeyboard = bAsMachine; break;
			}
			Passed += Run.Submit(A).bPass ? 1 : 0;
		}
		Check(Run.Passed() == Case.Passes && Contains(Run.Rank(), Case.Expect), "the verdict follows the score",
			std::to_string(Run.MachinePercent()) + "% -> " + Run.Rank());
	}
}

int main()
{
	TestTiles();
	TestWords();
	TestMachineAnswers();
	TestClockAndMouse();
	TestInkblot();
	TestAssociation();
	TestEveryTestHasAMachineAnswer();
	TestRuns();
	TestVerdicts();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
