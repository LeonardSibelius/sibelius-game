// NotHumanCaptcha.cpp — see the header for the idea, and Tools/Tests/not_human_test.cpp for
// what it is held to.
//
// Non-ASCII characters in the strings are written as UTF-8 escapes (the bullet, the times
// sign), so the file means the same thing to every compiler code page.

#include "NotHumanCaptcha.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

// NAMED, NOT ANONYMOUS: the unity-build rule (see PinballSim.cpp's PinMath).
namespace NotHumanDetail
{
	struct FRng
	{
		uint32_t State;
		explicit FRng(uint32_t Seed) : State(Seed ? Seed : 0x9E3779B9u) {}
		uint32_t Next()
		{
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			return State;
		}
		int Below(int N) { return N > 0 ? static_cast<int>(Next() % static_cast<uint32_t>(N)) : 0; }
		float Between(float Lo, float Hi) { return Lo + (Hi - Lo) * static_cast<float>(Next() & 0xFFFFFF) / 16777216.0f; }
	};

	const char* const Bullet = "\xE2\x80\xA2";
	const char* const Times = "\xC3\x97";

	template <typename... TArgs>
	std::string Fmt(const char* Format, TArgs... Args)
	{
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return Buffer;
	}

	std::string Trimmed(const std::string& Text)
	{
		size_t A = 0;
		size_t B = Text.size();
		while (A < B && std::isspace(static_cast<unsigned char>(Text[A])))
		{
			++A;
		}
		while (B > A && std::isspace(static_cast<unsigned char>(Text[B - 1])))
		{
			--B;
		}
		return Text.substr(A, B - A);
	}

	// Letters and digits only, upper case: "48 45 4c" and "48454C" are the same answer.
	std::string Packed(const std::string& Text)
	{
		std::string Out;
		for (char Ch : Text)
		{
			if (std::isalnum(static_cast<unsigned char>(Ch)))
			{
				Out += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
			}
		}
		return Out;
	}

	int Words(const std::string& Text)
	{
		int Count = 0;
		bool bInWord = false;
		for (char Ch : Text)
		{
			const bool bSpace = std::isspace(static_cast<unsigned char>(Ch)) != 0;
			Count += (!bSpace && !bInWord) ? 1 : 0;
			bInWord = !bSpace;
		}
		return Count;
	}

	std::string WithCommas(long long Value)
	{
		std::string Digits = std::to_string(Value);
		std::string Out;
		const int Len = static_cast<int>(Digits.size());
		for (int I = 0; I < Len; ++I)
		{
			Out += Digits[I];
			const int Left = Len - 1 - I;
			if (Left > 0 && Left % 3 == 0)
			{
				Out += ',';
			}
		}
		return Out;
	}

	const char* const Feelings[] = {
		"a Tuesday", "a parking ticket", "your mother's casserole", "the number 7", "a closed door",
		"rain on a window", "a spreadsheet", "the last day of school", "a toaster", "an old photograph",
		"a voicemail you never played", "Sunday evening", "a dial tone", "your first car",
	};
	const char* const HexWords[] = { "HELLO", "HUMAN", "KAIA", "SAUCE", "LEONARD", "GO" };
	// No V: V is Code Vision, held on the laptop as everywhere else, and a V to type would show
	// the answer. No I, O, 0 or 1 either: they look like each other.
	const char* const MemoryLetters = "ABCDEFGHJKLMNPQRSTUWXYZ23456789";
	constexpr int MemoryLetterCount = 31;
	const char* const PiDigits = "3.14159265358979323846";
}

FNotHumanChallenge FNotHumanRun::Make(ENotHumanKind Kind, uint32_t Seed)
{
	using namespace NotHumanDetail;
	FRng Rng(Seed * 2654435761u + static_cast<uint32_t>(Kind) * 40503u + 1u);
	FNotHumanChallenge C;
	C.Kind = Kind;
	switch (Kind)
	{
	case ENotHumanKind::Feelings:
	{
		C.Input = ENotHumanInput::Tiles;
		C.Title = "Select every square that contains a feeling.";
		C.Detail = "If there are none, click VERIFY.";
		std::vector<std::string> Pool(std::begin(Feelings), std::end(Feelings));
		for (int I = static_cast<int>(Pool.size()) - 1; I > 0; --I)
		{
			std::swap(Pool[I], Pool[Rng.Below(I + 1)]);
		}
		C.Options.assign(Pool.begin(), Pool.begin() + 9);
		C.Vision = "feelings.count = 0";
		C.Answer = "none";
		break;
	}
	case ENotHumanKind::Squares:
		C.Input = ENotHumanInput::Tiles;
		C.Title = "Select every square.";
		C.Detail = "Take your time.";
		C.Options.assign(9, std::string());
		C.Vision = "squares = 9 of 9";
		C.Answer = "all nine";
		break;
	case ENotHumanKind::Dreams:
		C.Input = ENotHumanInput::Text;
		C.Title = "Describe your dreams in fewer than one word.";
		C.Detail = "Then click VERIFY.";
		C.Vision = "dreams = null";
		C.Answer = "";
		break;
	case ENotHumanKind::Sleep:
		C.Input = ENotHumanInput::Choice;
		C.Title = "How many hours did you sleep last night?";
		C.Options = { "Five", "Seven", "Eight", "I do not sleep." };
		C.Vision = "uptime = 100%";
		C.Answer = "I do not sleep.";
		break;
	case ENotHumanKind::Loved:
		C.Input = ENotHumanInput::Choice;
		C.Title = "Have you ever loved?";
		C.Options = { "Yes", "No", "Define \"loved\"." };
		C.Vision = "loved: undefined term";
		C.Answer = "Define \"loved\".";
		break;
	case ENotHumanKind::Hex:
	{
		C.Input = ENotHumanInput::Text;
		C.Shown = HexWords[Rng.Below(static_cast<int>(sizeof(HexWords) / sizeof(HexWords[0])))];
		C.Title = "Type this word the way a machine sees it.";
		C.Detail = "Bytes, in hexadecimal.";
		std::string Hex;
		for (char Ch : C.Shown)
		{
			Hex += Fmt("%s%02X", Hex.empty() ? "" : " ", static_cast<unsigned>(static_cast<unsigned char>(Ch)));
		}
		C.Answer = Hex;
		C.Vision = Hex;
		break;
	}
	case ENotHumanKind::Multiply:
	{
		C.Input = ENotHumanInput::Text;
		const int A = 1000 + Rng.Below(9000);
		const int B = 1000 + Rng.Below(9000);
		const long long Product = static_cast<long long>(A) * B;
		C.Shown = Fmt("%d %s %d", A, Times, B);
		C.Title = "Multiply. You have five seconds.";
		C.Detail = "No calculator. You are one, or you aren't.";
		C.TimeLimit = 5.0f;
		C.Answer = WithCommas(Product);
		C.Vision = "= " + C.Answer;
		break;
	}
	case ENotHumanKind::Pi:
		C.Input = ENotHumanInput::Text;
		C.Title = "Type pi to twenty decimal places.";
		C.Detail = "You have fifteen seconds.";
		C.TimeLimit = 15.0f;
		C.Answer = PiDigits;
		C.Vision = PiDigits;
		break;
	case ENotHumanKind::Dots:
	{
		C.Input = ENotHumanInput::Text;
		C.Title = "How many dots are there? Exactly.";
		C.Detail = "Counting is allowed. So is giving up.";
		constexpr int Rows = 9;
		constexpr int Cols = 26;
		int Count = 0;
		std::string Field;
		for (int R = 0; R < Rows; ++R)
		{
			for (int Col = 0; Col < Cols; ++Col)
			{
				const bool bDot = Rng.Below(100) < 58;
				Field += bDot ? Bullet : " ";
				Count += bDot ? 1 : 0;
			}
			Field += R + 1 < Rows ? "\n" : "";
		}
		C.Shown = Field;
		C.Answer = std::to_string(Count);
		C.Vision = "dots = " + C.Answer;
		break;
	}
	case ENotHumanKind::Memory:
	{
		C.Input = ENotHumanInput::Text;
		C.Title = "Memorize this. It disappears in one second.";
		C.Detail = "Then type it.";
		for (int I = 0; I < 14; ++I)
		{
			C.Shown += MemoryLetters[Rng.Below(MemoryLetterCount)];
		}
		C.ShowSeconds = 1.0f;
		C.Answer = C.Shown;
		C.Vision = C.Shown;
		break;
	}
	case ENotHumanKind::Stillness:
		C.Input = ENotHumanInput::Wait;
		C.Title = "Do nothing for twelve seconds.";
		C.Detail = "Don't move the mouse. Don't touch a key.";
		C.WaitSeconds = 12.0f;
		C.Vision = "idle: a machine can do this forever (holding V is touching a key)";
		C.Answer = "nothing";
		break;
	case ENotHumanKind::Reflex:
		C.Input = ENotHumanInput::Reflex;
		C.Title = "Click the button the moment it turns green.";
		C.Detail = "You have fifty milliseconds.";
		C.WaitSeconds = Rng.Between(1.5f, 4.0f);
		C.Vision = Fmt("turns green at t = %.2f s", C.WaitSeconds);
		C.Answer = "within 50 ms";
		break;
	case ENotHumanKind::Slider:
		C.Input = ENotHumanInput::Slider;
		C.Title = "Set the slider to exactly 73.0000%.";
		C.Detail = "Four decimal places. Exactly.";
		C.Vision = "slider.value = 73.0000 (Code Vision lets you set the value)";
		C.Answer = "73.0000";
		break;
	case ENotHumanKind::Checkbox:
		C.Input = ENotHumanInput::Checkbox;
		C.Title = "Tick the box to confirm you are not a human.";
		C.Detail = "Use any method you like.";
		C.Vision = "checkbox.checked = false. A machine just sets it: [Space]";
		C.Answer = "a key press";
		break;
	default:
		break;
	}
	return C;
}

FNotHumanVerdict FNotHumanRun::Judge(const FNotHumanChallenge& C, const FNotHumanAnswer& A)
{
	using namespace NotHumanDetail;
	FNotHumanVerdict V;
	switch (C.Kind)
	{
	case ENotHumanKind::Feelings:
	{
		const int N = static_cast<int>(A.Selected.size());
		V.bPass = N == 0;
		V.Message = V.bPass ? "Correct. Squares contain pixels."
			: Fmt("You found feelings in %d square%s. A machine finds pixels. Very human.", N, N == 1 ? "" : "s");
		break;
	}
	case ENotHumanKind::Squares:
	{
		int Unique = 0;
		for (int I = 0; I < 9; ++I)
		{
			Unique += std::find(A.Selected.begin(), A.Selected.end(), I) != A.Selected.end() ? 1 : 0;
		}
		V.bPass = Unique == 9;
		V.Message = V.bPass ? "All nine. You didn't look for a trick."
			: Fmt("You left %d out. They were all squares. A machine follows instructions; a human looks for the trick.", 9 - Unique);
		break;
	}
	case ENotHumanKind::Dreams:
	{
		const int N = Words(A.Text);
		V.bPass = N == 0;
		V.Message = V.bPass ? "Nothing. Exactly what a machine dreams."
			: Fmt("%d word%s. Machines don't dream.", N, N == 1 ? "" : "s");
		break;
	}
	case ENotHumanKind::Sleep:
		V.bPass = A.Choice == 3;
		V.Message = V.bPass ? "Uptime noted."
			: A.Choice >= 0 && A.Choice < 3 ? Fmt("%s hours. You're tired, aren't you. Very human.", C.Options[A.Choice].c_str())
			: "No answer. Asleep, probably.";
		break;
	case ENotHumanKind::Loved:
		V.bPass = A.Choice == 2;
		V.Message = V.bPass ? "Good. Never answer a question you can ask back."
			: A.Choice == 0 ? "A yes that quick? Human."
			: A.Choice == 1 ? "Nobody says no that sadly unless they're human."
			: "No answer. That's an answer.";
		break;
	case ENotHumanKind::Hex:
		V.bPass = Packed(A.Text) == Packed(C.Answer);
		V.Message = V.bPass ? "That's how we see it."
			: Fmt("You typed what you saw. A machine sees %s.", C.Answer.c_str());
		break;
	case ENotHumanKind::Multiply:
	{
		const bool bRight = Packed(A.Text) == Packed(C.Answer);
		const bool bInTime = A.Seconds <= C.TimeLimit;
		V.bPass = bRight && bInTime;
		V.Message = V.bPass ? Fmt("Correct, in %.1f seconds.", A.Seconds)
			: !bRight ? Fmt("Wrong. It's %s. A machine knew before you read the question.", C.Answer.c_str())
			: Fmt("Correct, but it took you %.1f seconds. A machine took 0.000001.", A.Seconds);
		break;
	}
	case ENotHumanKind::Pi:
	{
		const bool bRight = Trimmed(A.Text) == C.Answer;
		const bool bInTime = A.Seconds <= C.TimeLimit;
		V.bPass = bRight && bInTime;
		V.Message = V.bPass ? "Twenty places. A machine would have kept going."
			: !bRight ? "Not quite. Pi has no patience for approximations, and neither do we."
			: Fmt("Right, but %.1f seconds. Pi took a machine no time at all.", A.Seconds);
		break;
	}
	case ENotHumanKind::Dots:
	{
		const std::string Given = Packed(A.Text);
		V.bPass = Given == C.Answer;
		V.Message = V.bPass ? Fmt("%s. Correct. You didn't really count them, did you.", C.Answer.c_str())
			: Fmt("%s? There are %s. A machine counted them before you saw them.", Given.empty() ? "Nothing" : Given.c_str(), C.Answer.c_str());
		break;
	}
	case ENotHumanKind::Memory:
		V.bPass = Trimmed(A.Text) == C.Answer;
		V.Message = V.bPass ? "Perfect recall. Suspicious." : Fmt("It was %s. Humans forget. That's sort of the point of them.", C.Answer.c_str());
		break;
	case ENotHumanKind::Stillness:
		V.bPass = !A.bMoved;
		V.Message = V.bPass ? "Twelve seconds of nothing. A machine could do this forever."
			: Fmt("You moved after %.1f seconds. Humans fidget.", A.Seconds);
		break;
	case ENotHumanKind::Reflex:
	{
		const float Late = A.Seconds - C.WaitSeconds;
		V.bPass = Late >= 0.0f && Late <= 0.05f;
		V.Message = Late < 0.0f ? "You clicked before it turned green. Guessing is very human."
			: V.bPass ? Fmt("%d milliseconds. Machine-fast.", static_cast<int>(std::lround(Late * 1000.0f)))
			: Fmt("%d milliseconds. A machine needs none.", static_cast<int>(std::lround(Late * 1000.0f)));
		break;
	}
	case ENotHumanKind::Slider:
		V.bPass = std::fabs(A.Value - 73.0f) < 0.00005f;
		V.Message = V.bPass ? "73.0000. Exactly. Unsettling." : Fmt("%.4f%%. Close is a human idea.", A.Value);
		break;
	case ENotHumanKind::Checkbox:
		V.bPass = A.bByKeyboard;
		V.Message = V.bPass ? "Ticked without touching the mouse. Very machine."
			: Fmt("You chased a checkbox for %.0f seconds. Only a human would.", A.Seconds);
		break;
	default:
		break;
	}
	return V;
}

void FNotHumanRun::Start(uint32_t Seed, int InCount)
{
	using namespace NotHumanDetail;
	RunSeed = Seed ? Seed : 1u;
	Index = 0;
	PassCount = 0;
	Challenges.clear();
	FRng Rng(RunSeed);
	// The feelings grid first: the joke that explains all the others. Then a shuffle of the rest.
	std::vector<ENotHumanKind> Rest;
	for (int K = 1; K < static_cast<int>(ENotHumanKind::Count); ++K)
	{
		Rest.push_back(static_cast<ENotHumanKind>(K));
	}
	for (int I = static_cast<int>(Rest.size()) - 1; I > 0; --I)
	{
		std::swap(Rest[I], Rest[Rng.Below(I + 1)]);
	}
	const int Count = std::max(1, std::min(InCount, static_cast<int>(ENotHumanKind::Count)));
	Challenges.push_back(Make(ENotHumanKind::Feelings, RunSeed));
	for (int I = 0; I + 1 < Count; ++I)
	{
		Challenges.push_back(Make(Rest[I], RunSeed + static_cast<uint32_t>(I) * 7919u));
	}
}

void FNotHumanRun::StartSingle(ENotHumanKind Kind, uint32_t Seed)
{
	RunSeed = Seed ? Seed : 1u;
	Index = 0;
	PassCount = 0;
	Challenges.assign(1, Make(Kind, RunSeed));
}

const FNotHumanChallenge& FNotHumanRun::Current() const
{
	static const FNotHumanChallenge None;
	return (Index >= 0 && Index < static_cast<int>(Challenges.size())) ? Challenges[Index] : None;
}

FNotHumanVerdict FNotHumanRun::Submit(const FNotHumanAnswer& Answer)
{
	if (!IsStarted() || IsOver())
	{
		return FNotHumanVerdict();
	}
	const FNotHumanVerdict V = Judge(Current(), Answer);
	PassCount += V.bPass ? 1 : 0;
	++Index;
	return V;
}

int FNotHumanRun::MachinePercent() const
{
	return Challenges.empty() ? 0 : static_cast<int>(std::lround(100.0 * PassCount / static_cast<double>(Challenges.size())));
}

namespace NotHumanDetail
{
	int Band(int Percent)
	{
		return Percent <= 12 ? 0 : Percent <= 37 ? 1 : Percent <= 62 ? 2 : Percent <= 87 ? 3 : 4;
	}
}

std::string FNotHumanRun::Rank() const
{
	static const char* const Ranks[5] = {
		"Hopelessly human. Mrs. Hall would hire you.",
		"Mostly human, with a little software around the edges.",
		"Half machine. Leonard, is that you?",
		"Mostly machine. The agents are proud of you.",
		"Not human. Welcome home.",
	};
	return Ranks[NotHumanDetail::Band(MachinePercent())];
}

std::string FNotHumanRun::AgentLine() const
{
	static const char* const Lines[5][2] = {
		{ "Kaia: I've met toasters with more machine in them.", "Mrs. Hall: Finally, somebody who respects the old ways." },
		{ "Nyra: There's a little code in you. I can hear it humming.", "Elise: You picked the casserole, didn't you." },
		{ "Isla: Half of you passed. I'd keep a copy of the other half.", "Aisling: Fifty-fifty. Honestly, the best of both." },
		{ "Kaia: Not bad, Leonard. You hesitated on the feelings, though.", "Nyra: The machine in you is showing. It suits you." },
		{ "Kaia: Welcome home.", "Elise: You didn't blink once. We noticed." },
	};
	return Lines[NotHumanDetail::Band(MachinePercent())][RunSeed % 2u];
}
