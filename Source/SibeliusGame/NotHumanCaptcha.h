// NotHumanCaptcha.h
//
// PROVE YOU'RE NOT HUMAN — the app on the living-room laptop (Walt, 2026-10-11: "an E trigger
// at the desk with the laptop", under the sign he moved there and renamed). A CAPTCHA turned
// inside out: every test is passed by answering the way a machine would, and failed by
// answering like a person. It is the game's story in one screen - a man becoming software -
// and Leonard's own power is what helps: under Code Vision ([V], once he has it) each test
// shows him the machine's view of itself (the hex, the exact count, the countdown).
//
// ===========================================================================
// THE HOUSE METHOD (FGoGame, FChessGame): a pure model, standard library only, deterministic
// for a seed, tested standalone by Tools/Tests/run_not_human_tests.ps1. UNotHumanWidget only
// presents it, and the parts that need a clock or a mouse (the stillness test, the reflex
// test, the fleeing checkbox) measure them and hand this model the numbers.
//
// A RUN is eight tests drawn from sixteen without repeats: the feelings grid first (the joke that
// explains all the others), and always the two that make it a psychological exam - THE INKBLOT
// and WORD ASSOCIATION (Walt, 2026-10-11: "can we do both"). The verdict is the share passed -
// "you are 63% machine" - with a rank and a line from one of the agents.
//
// THE INKBLOT is drawn here, a new one every time: blobs of ink on the left half, mirrored onto
// the right, edges roughened, then thresholded to a coverage between 28% and 48%. People see a
// butterfly; a machine sees ink, and how much of it. WORD ASSOCIATION flashes six words, three
// seconds each: people answer with a word the first one brings to mind; a machine files it, and
// answers with its part of speech.
//
// Every question is written here, in the game. Nothing goes to the chat server: the build
// under Steam's review is unchanged by this app.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class ENotHumanInput : uint8_t
{
	Tiles,      // a 3x3 grid to select from, then VERIFY
	Text,       // a text field, then VERIFY (Enter)
	Choice,     // one of a few buttons
	Wait,       // do nothing for a while: the widget reports whether he moved
	Reflex,     // click when the button turns green: the widget reports the timing
	Slider,     // set a value exactly
	Checkbox,   // tick a box that runs from the mouse: a key press ticks it
	Flash,      // words one at a time, a few seconds each: the widget reports each answer and its time
};

enum class ENotHumanKind : uint8_t
{
	Feelings,
	Dreams,
	Sleep,
	Loved,
	Hex,
	Multiply,
	Pi,
	Dots,
	Memory,
	Stillness,
	Reflex,
	Slider,
	Checkbox,
	Squares,
	Inkblot,
	Association,
	Count
};

struct FNotHumanChallenge
{
	ENotHumanKind Kind = ENotHumanKind::Feelings;
	ENotHumanInput Input = ENotHumanInput::Tiles;
	std::string Title;                  // the header line: the instruction
	std::string Detail;                 // smaller, under it
	std::vector<std::string> Options;   // tiles or choice buttons
	std::string Shown;                  // what the test shows: a word, a field of dots, a string to remember
	float TimeLimit = 0.0f;             // seconds to answer, 0 = none
	float ShowSeconds = 0.0f;           // Memory: how long Shown stays up
	float WaitSeconds = 0.0f;           // Stillness: how long to hold still; Reflex: when the button turns green
	std::string Vision;                 // what Code Vision shows: the machine's view
	std::string Answer;                 // the machine's answer, for the record and the tests

	// Inkblot: the blot, as ink per pixel (0 paper .. 255 ink), row by row, and how much is ink.
	std::vector<uint8_t> Ink;
	int InkWidth = 0;
	int InkHeight = 0;
	float InkCoverage = 0.0f;           // percent

	// Association: for each of Options (the words), the answers a machine gives, '|'-separated.
	std::vector<std::string> Accepts;
};

struct FNotHumanAnswer
{
	std::vector<int> Selected;          // Tiles: the indices he selected
	std::string Text;                   // Text
	int Choice = -1;                    // Choice
	float Seconds = 0.0f;               // how long he took; Reflex: when he clicked, after the start
	bool bMoved = false;                // Wait: he moved the mouse or touched a key
	float Value = 0.0f;                 // Slider
	bool bByKeyboard = false;           // Checkbox: ticked with a key (a machine's way)
	std::vector<std::string> Words;     // Flash: what he typed for each word
	std::vector<float> Times;           // Flash: how long each took
};

struct FNotHumanVerdict
{
	bool bPass = false;
	std::string Message;
};

class FNotHumanRun
{
public:
	static constexpr int DefaultCount = 8;

	void Start(uint32_t Seed, int Count = DefaultCount);
	// A run of one test, for looking at it (the laptop's development command).
	void StartSingle(ENotHumanKind Kind, uint32_t Seed);

	bool IsStarted() const { return !Challenges.empty(); }
	bool IsOver() const { return IsStarted() && Index >= static_cast<int>(Challenges.size()); }
	int CurrentIndex() const { return Index; }
	int Count() const { return static_cast<int>(Challenges.size()); }
	int Passed() const { return PassCount; }
	const FNotHumanChallenge& Current() const;
	const std::vector<FNotHumanChallenge>& All() const { return Challenges; }

	// Judges the answer to the current test and moves on to the next.
	FNotHumanVerdict Submit(const FNotHumanAnswer& Answer);

	int MachinePercent() const;
	std::string Rank() const;           // "Half machine. Leonard, is that you?"
	std::string AgentLine() const;      // one agent's word on it

	// Every test, built for a seed: the tests check each one's machine answer passes.
	static FNotHumanChallenge Make(ENotHumanKind Kind, uint32_t Seed);

	// The machine's answer to one association word ("noun"), and whether a reply is one of them.
	static std::string MachineWord(const FNotHumanChallenge& Challenge, int Word);
	static bool IsMachineWord(const FNotHumanChallenge& Challenge, int Word, const std::string& Reply);
	static FNotHumanVerdict Judge(const FNotHumanChallenge& Challenge, const FNotHumanAnswer& Answer);

private:
	std::vector<FNotHumanChallenge> Challenges;
	int Index = 0;
	int PassCount = 0;
	uint32_t RunSeed = 1;
};
