// go_engine_test.cpp — standalone tests for Source/SibeliusGame/GoEngine.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_go_engine_tests.ps1.
//
// Four kinds of claim. RULES: capture, suicide, the capture that only looks like suicide,
// ko, eyes, the count with its dead stones, first capture wins, take-backs, notation - each
// on a position built for it. BOOKKEEPING: the board keeps its groups and liberties up to
// date move by move instead of recounting them, so after every move of hundreds of random
// games, on four board sizes, it is compared with a board rebuilt from the stones alone;
// and every one of those games ends. PLAY: the ghost takes what is free, rescues what is in
// atari, kills a group by its vital point and lives by the same point, beats a beginner,
// and finishes its games. COACH: the hint's reason, word for word where it is known, and a
// review that is silent about the coach's own move and speaks up about a blunder.

#include "GoEngine.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int GPasses = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* Name, const std::string& Detail = std::string())
	{
		std::printf("%s  %s%s%s\n", bOk ? "PASS" : "FAIL", Name, Detail.empty() ? "" : "  -- ", Detail.c_str());
		if (bOk)
		{
			++GPasses;
		}
		else
		{
			++GFailures;
		}
	}

	template <typename... TArgs>
	std::string Fmt(const char* Format, TArgs... Args)
	{
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	struct FLcg
	{
		uint32_t State;
		uint32_t Next()
		{
			State = State * 1664525u + 1013904223u;
			return State >> 8;
		}
	};

	int At(int Size, int X, int Y) { return X + Y * Size; }

	bool StartsWith(const std::string& Text, const std::string& Prefix)
	{
		return Text.compare(0, Prefix.size(), Prefix) == 0;
	}

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	FGoStrength Seeded(FGoStrength S, uint32_t Seed)
	{
		S.Seed = Seed;
		return S;
	}

	// A white stone at E5 with one liberty left, at F5.
	FGoBoard CapturePosition()
	{
		return FGoBoard::FromDiagram({
			".........",
			".........",
			".........",
			"....X....",
			"...XO....",
			"....X....",
			".........",
			".........",
			".........",
		}, EGoColor::Black);
	}
}

// =====================================================================================
// RULES

static void TestEmptyBoard()
{
	const FGoBoard B(9);
	int Legal = 0;
	for (int P = 0; P < 81; ++P)
	{
		Legal += B.IsLegal(P) ? 1 : 0;
	}
	Check(Legal == 81 && B.IsLegal(GoPass), "an empty 9x9 board has 81 moves and a pass", Fmt("%d", Legal));
	Check(B.AreaDifference() == 0, "an empty board counts zero to zero");
	Check(B.ToMove() == EGoColor::Black, "Black moves first");
	const FGoBoard Big(25);
	const FGoBoard Small(2);
	Check(Big.Size() == GoMaxSize && Small.Size() == GoMinSize, "board sizes are kept between 5 and 19");
}

static void TestCapture()
{
	FGoBoard B = CapturePosition();
	const int F5 = At(9, 5, 4);
	const int E5 = At(9, 4, 4);
	Check(B.Liberties(E5) == 1, "the white stone has one liberty");
	Check(B.WouldCapture(F5) == 1, "F5 would capture one stone");
	const int Taken = B.Play(F5);
	Check(Taken == 1 && B.At(E5) == EGoColor::Empty, "F5 captures it", Fmt("took %d", Taken));
	Check(B.Prisoners(EGoColor::Black) == 1, "Black holds one prisoner");
	Check(B.ToMove() == EGoColor::White, "then it is White's move");
	FGoBoard Rebuilt = B;
	Rebuilt.Rebuild();
	Check(B.SameBookkeeping(Rebuilt), "the groups after a capture match a rebuilt board");
}

static void TestSuicide()
{
	const FGoBoard B = FGoBoard::FromDiagram({
		".........",
		".........",
		".........",
		"....X....",
		"...X.X...",
		"....X....",
		".........",
		".........",
		".........",
	}, EGoColor::White);
	const int E5 = At(9, 4, 4);
	Check(!B.IsLegal(E5), "White may not play into Black's eye: suicide");
	const FGoBoard Black = FGoBoard::FromDiagram({
		".........",
		".........",
		".........",
		"....X....",
		"...X.X...",
		"....X....",
		".........",
		".........",
		".........",
	}, EGoColor::Black);
	Check(Black.IsLegal(E5), "Black may fill its own eye (the rules allow it)");
	Check(Black.IsEye(E5, EGoColor::Black) && !Black.IsEye(E5, EGoColor::White), "it is Black's eye, not White's");

	// Looks like suicide, but it captures first: White at A2 takes the black stone on A1.
	FGoBoard Corner = FGoBoard::FromDiagram({
		".........",
		".........",
		".........",
		".........",
		".........",
		".........",
		"X........",
		".X.......",
		"XO.......",
	}, EGoColor::White);
	const int A2 = At(9, 0, 1);
	Check(Corner.IsLegal(A2), "a move with no liberties is legal when it captures");
	Check(Corner.Play(A2) == 1 && Corner.At(At(9, 0, 0)) == EGoColor::Empty, "and it takes the stone");
	Check(Corner.Liberties(A2) == 1, "leaving the capturing stone the one liberty it freed");
}

static FGoBoard KoPosition()
{
	return FGoBoard::FromDiagram({
		".........",
		".........",
		".........",
		"..XO.....",
		".XO.O....",
		"..XO.....",
		".........",
		".........",
		".........",
	}, EGoColor::Black);
}

static void TestKo()
{
	const int D5 = At(9, 3, 4);
	const int C5 = At(9, 2, 4);
	FGoBoard B = KoPosition();
	Check(B.Play(D5) == 1, "Black takes the ko at D5");
	Check(B.KoPoint() == C5, "C5 is now the ko point", Fmt("%d", B.KoPoint()));
	Check(!B.IsLegal(C5), "White may not take straight back");

	FGoGame Game = FGoGame::FromBoard(KoPosition());
	Game.Play(D5);
	Check(!Game.IsLegal(C5), "the game agrees: no immediate retake");
	Check(Game.Play(At(9, 8, 8)) && Game.Play(At(9, 0, 0)), "a ko threat elsewhere, and an answer");
	Check(Game.IsLegal(C5), "now White may retake");
	Check(Game.Play(C5) && Game.Board().At(D5) == EGoColor::Empty, "and the retake captures");

	FGoBoard Passed = KoPosition();
	Passed.Play(D5);
	Passed.Play(GoPass);
	Check(Passed.KoPoint() == GoNoMove, "a pass clears the ko point");
}

static void TestFalseEye()
{
	// A2 is surrounded by Black, but White holds both diagonals that matter on the edge... one
	// diagonal on the edge is enough to make it false.
	const FGoBoard B = FGoBoard::FromDiagram({
		".....",
		".....",
		"XO...",
		".X...",
		"X....",
	}, EGoColor::Black);
	const int A2 = At(5, 0, 1);
	Check(!B.IsEye(A2, EGoColor::Black), "an edge point with an enemy diagonal is a false eye");
	const FGoBoard Real = FGoBoard::FromDiagram({
		".....",
		".....",
		"XX...",
		".X...",
		"X....",
	}, EGoColor::Black);
	Check(Real.IsEye(A2, EGoColor::Black), "the same point with friendly diagonals is a real eye");
	const FGoBoard Centre = FGoBoard::FromDiagram({
		".....",
		".OXO.",
		".X.X.",
		".OX..",
		".....",
	}, EGoColor::Black);
	Check(!Centre.IsEye(At(5, 2, 2), EGoColor::Black), "in the middle, two enemy diagonals make it false");
}

static void TestArea()
{
	const FGoBoard B = FGoBoard::FromDiagram({
		"..XO.",
		"..XO.",
		"..XO.",
		"..XO.",
		"..XO.",
	}, EGoColor::Black);
	Check(B.AreaDifference() == 5, "15 points to 10 is +5 for Black", Fmt("%d", B.AreaDifference()));
	const FGoBoard Shared = FGoBoard::FromDiagram({
		"..X..",
		"..X..",
		"..XO.",
		"..XO.",
		"..XO.",
	}, EGoColor::Black);
	Check(Shared.AreaDifference() == 15 - 3, "a region touching both colours is nobody's", Fmt("%d", Shared.AreaDifference()));
}

static FGoBoard DeadStonesPosition()
{
	// Black's wall on E, White's on F; a white stone left in Black's area and a black one in
	// White's. Black: 36 + 9 = 45. White: 9 + 27 = 36, + 7.5 komi = 43.5. Black by 1.5.
	return FGoBoard::FromDiagram({
		"....XO...",
		"....XO...",
		"....XO...",
		"....XO...",
		".O..XO.X.",
		"....XO...",
		"....XO...",
		"....XO...",
		"....XO...",
	}, EGoColor::Black);
}

static void TestCount()
{
	FGoGame Game = FGoGame::FromBoard(DeadStonesPosition());
	Check(Game.Play(GoPass) && !Game.Result().IsOver(), "one pass does not end the game");
	Check(Game.Play(GoPass) && Game.Result().IsOver(), "two passes do");
	const FGoResult& R = Game.Result();
	Check(R.Outcome == EGoOutcome::Counted, "and the board is counted");
	Check(R.Count.Dead[At(9, 1, 4)], "the white stone in Black's area is dead");
	Check(R.Count.Dead[At(9, 7, 4)], "the black stone in White's area is dead");
	int DeadCount = 0;
	for (bool bDead : R.Count.Dead)
	{
		DeadCount += bDead ? 1 : 0;
	}
	Check(DeadCount == 2, "and the walls are alive", Fmt("%d dead", DeadCount));
	Check(R.Winner == EGoColor::Black && R.Margin == 1.5f, "Black wins by 1.5",
		Fmt("winner %d by %.1f (B %.1f, W %.1f)", static_cast<int>(R.Winner), R.Margin, R.Count.BlackArea, R.Count.WhiteArea));
	Check(R.Count.Owner[At(9, 1, 4)] == EGoColor::Black, "the dead stone's point counts for Black");
	Check(!Game.IsLegal(At(9, 0, 0)) && !Game.Play(GoPass), "nothing is played after the end");
	Check(Game.Undo() && !Game.Result().IsOver(), "a take-back reopens the game");
}

static void TestUnclosedBorder()
{
	// A beginner's pass: White's wall has a gap at F5. The gap is White's by the look of it,
	// and the quick games agree; a strict count would give that region to nobody.
	FGoGame Game = FGoGame::FromBoard(FGoBoard::FromDiagram({
		"....XO...",
		"....XO...",
		"....XO...",
		"....XO...",
		"....X....",
		"....XO...",
		"....XO...",
		"....XO...",
		"....XO...",
	}, EGoColor::Black), EGoMode::Full, 0.5f);
	Game.Play(GoPass);
	Game.Play(GoPass);
	const FGoResult& R = Game.Result();
	Check(R.Count.Owner[At(9, 7, 4)] == EGoColor::White, "a gap in a border goes to the side that surrounds it",
		Fmt("B %.1f W %.1f", R.Count.BlackArea, R.Count.WhiteArea));
}

static void TestFirstCapture()
{
	FGoGame Game = FGoGame::FromBoard(CapturePosition(), EGoMode::FirstCapture);
	Check(Game.Play(At(9, 5, 4)), "Black captures");
	const FGoResult& R = Game.Result();
	Check(R.Outcome == EGoOutcome::FirstCapture && R.Winner == EGoColor::Black, "the first capture wins");
	Check(!Game.Play(At(9, 0, 0)), "and the game is over");
	Check(Game.Undo() && !Game.Result().IsOver() && Game.ToMove() == EGoColor::Black, "a take-back reopens it");

	FGoGame Full = FGoGame::FromBoard(CapturePosition(), EGoMode::Full);
	Full.Play(At(9, 5, 4));
	Check(!Full.Result().IsOver(), "in a full game a capture is just a capture");
}

static void TestUndoAndResign()
{
	FGoGame Game(9);
	const uint64_t Start = Game.Board().Hash();
	Game.Play(At(9, 4, 4));
	Game.Play(At(9, 2, 2));
	Game.Play(GoPass);
	Check(Game.MovesPlayed() == 3 && Game.LastMove() == GoPass, "three moves, the last a pass");
	Game.Undo();
	Game.Undo();
	Game.Undo();
	Check(Game.MovesPlayed() == 0 && Game.Board().Hash() == Start && Game.ToMove() == EGoColor::Black, "three take-backs restore the start");
	Check(!Game.Undo(), "and there is nothing more to take back");
	Game.Resign(EGoColor::Black);
	Check(Game.Result().Outcome == EGoOutcome::Resignation && Game.Result().Winner == EGoColor::White, "Black resigns, White wins");
	Check(Game.Undo() && !Game.Result().IsOver(), "a resignation can be taken back");
}

static void TestNotation()
{
	const FGoGame Game(9);
	Check(Game.PointName(0) == "A1" && Game.PointName(40) == "E5" && Game.PointName(80) == "J9", "A1, E5 (the centre), J9");
	Check(GoColumnLetter(7) == 'H' && GoColumnLetter(8) == 'J', "the columns skip I");
	Check(Game.PointName(GoPass) == "pass", "a pass is called a pass");
	FGoGame Played(9);
	Played.Play(40);
	Played.Play(20);
	Played.Play(GoPass);
	Check(Played.MoveListText() == "1. B E5  2. W C3  3. B pass", "the move list", Played.MoveListText());
	Check(Played.MoveListText(1) == "3. B pass", "its last move only", Played.MoveListText(1));
}

// =====================================================================================
// BOOKKEEPING

static void TestRandomGames()
{
	const int Sizes[5] = { 5, 7, 9, 13, 19 };
	int Games = 0;
	int Ended = 0;
	int Bad = 0;
	int Moves = 0;
	int Captures = 0;
	int LongestGame = 0;
	FLcg Rng{ 12345 };
	for (int Size : Sizes)
	{
		const int Count = Size == 19 ? 12 : Size == 13 ? 40 : 120;
		for (int G = 0; G < Count; ++G)
		{
			FGoGame Game(Size);
			++Games;
			int Played = 0;
			for (; Played < 4 * Size * Size && !Game.Result().IsOver(); ++Played)
			{
				// A random legal move that does not fill its own eye; now and then a pass. The board's
				// own check first: the game's superko check copies the board, so only the pick pays it.
				const FGoBoard& B = Game.Board();
				std::vector<int> Options;
				for (int P = 0; P < Size * Size; ++P)
				{
					if (B.IsLegal(P) && !B.IsEye(P, B.ToMove()))
					{
						Options.push_back(P);
					}
				}
				const int Before = B.Prisoners(EGoColor::Black) + B.Prisoners(EGoColor::White);
				bool bPlayed = false;
				while (!bPlayed && !Options.empty() && Rng.Next() % 50 != 0)
				{
					const size_t Pick = Rng.Next() % Options.size();
					bPlayed = Game.Play(Options[Pick]);
					Options[Pick] = Options.back();
					Options.pop_back();
				}
				if (!bPlayed && !Game.Play(GoPass))
				{
					++Bad;
					break;
				}
				const FGoBoard& After = Game.Board();
				Captures += After.Prisoners(EGoColor::Black) + After.Prisoners(EGoColor::White) - Before;
				FGoBoard Rebuilt = After;
				Rebuilt.Rebuild();
				if (!After.SameBookkeeping(Rebuilt))
				{
					++Bad;
					break;
				}
				// Every group on the board has a liberty.
				for (int P = 0; P < Size * Size; ++P)
				{
					if (After.At(P) != EGoColor::Empty && After.Liberties(P, 1) == 0)
					{
						++Bad;
					}
				}
			}
			Moves += Played;
			LongestGame = Played > LongestGame ? Played : LongestGame;
			Ended += Game.Result().IsOver() ? 1 : 0;
		}
	}
	Check(Bad == 0, "after every move of every random game, the groups match a rebuilt board", Fmt("%d bad", Bad));
	Check(Ended == Games, "every random game ends", Fmt("%d of %d, longest %d moves", Ended, Games, LongestGame));
	Check(Captures > 1000, "and they are full of captures", Fmt("%d games, %d moves, %d stones taken", Games, Moves, Captures));
}

// =====================================================================================
// PLAY

static void TestTakesWhatIsFree()
{
	const FGoGame Game = FGoGame::FromBoard(CapturePosition());
	const FGoThought T = Game.Think(Seeded(FGoStrength::Master(), 7));
	Check(T.Move == At(9, 5, 4), "the master takes a stone in atari", Game.PointName(T.Move));

	const FGoGame Atari = FGoGame::FromBoard(CapturePosition(), EGoMode::FirstCapture);
	const FGoThought A = Atari.Think(Seeded(FGoStrength::Beginner(), 3));
	Check(A.Move == At(9, 5, 4) && A.WinRate > 0.9f, "with first capture wins, even the beginner takes it",
		Fmt("%s %.2f", Atari.PointName(A.Move).c_str(), A.WinRate));
}

static FGoBoard RescuePosition()
{
	// Black's two stones on D5-E5 have one liberty, at F5; White threatens to take them.
	return FGoBoard::FromDiagram({
		".........",
		".........",
		".........",
		"...OO....",
		"..OXXO...",
		"...O.....",
		".........",
		".........",
		".........",
	}, EGoColor::Black);
}

static void TestRescues()
{
	const FGoBoard B = RescuePosition();
	Check(B.Liberties(At(9, 3, 4)) == 1, "Black's two stones are in atari");
	const FGoGame Game = FGoGame::FromBoard(B, EGoMode::FirstCapture);
	const FGoThought T = Game.Think(Seeded(FGoStrength::Hustler(), 11));
	Check(T.Move == At(9, 4, 3), "with first capture wins, it runs out of atari at E4", Game.PointName(T.Move));
}

static FGoBoard StraightThree(EGoColor ToMove)
{
	// White's corner group has a straight three for eyes: A1-B1-C1. Whoever plays B1 first
	// decides it - two eyes for White, or none.
	return FGoBoard::FromDiagram({
		".....",
		".....",
		"XXXXX",
		"OOOOX",
		"...OX",
	}, ToMove);
}

static void TestVitalPoint()
{
	// Komi 9.5 makes the corner the whole game: alive, White has 8 + 9.5 to Black's 17.
	const FGoGame Kill = FGoGame::FromBoard(StraightThree(EGoColor::Black), EGoMode::Full, 9.5f);
	const FGoThought K = Kill.Think(Seeded(FGoStrength::Coach(), 5));
	Check(K.Move == At(5, 1, 0), "Black kills the straight three at its vital point, B1",
		Fmt("%s %.2f", Kill.PointName(K.Move).c_str(), K.WinRate));
	const FGoGame Live = FGoGame::FromBoard(StraightThree(EGoColor::White), EGoMode::Full, 9.5f);
	const FGoThought L = Live.Think(Seeded(FGoStrength::Coach(), 5));
	Check(L.Move == At(5, 1, 0), "White lives at the same point", Fmt("%s %.2f", Live.PointName(L.Move).c_str(), L.WinRate));
}

static void TestDeterminism()
{
	const FGoGame Game(9);
	const FGoThought A = Game.Think(Seeded(FGoStrength::Beginner(), 99));
	const FGoThought B = Game.Think(Seeded(FGoStrength::Beginner(), 99));
	Check(A.Move == B.Move && A.WinRate == B.WinRate, "the same seed thinks the same thought");
	const FGoThought Opening = Game.Think(Seeded(FGoStrength::Hustler(), 1));
	const int X = Opening.Move % 9;
	const int Y = Opening.Move / 9;
	Check(Opening.Move != GoPass && X >= 2 && X <= 6 && Y >= 2 && Y <= 6, "its first move is not on the edge",
		Game.PointName(Opening.Move));
	Check(!Opening.Line.empty() && Opening.Line[0] == Opening.Move, "the line it expects starts with its move");
	Check(Opening.Ownership.size() == 81, "and it says who it thinks owns each point");
}

static void PlayGame(FGoGame& Game, const FGoStrength& BlackS, const FGoStrength& WhiteS, uint32_t Seed, int MaxMoves)
{
	for (int M = 0; M < MaxMoves && !Game.Result().IsOver(); ++M)
	{
		FGoStrength S = Game.ToMove() == EGoColor::Black ? BlackS : WhiteS;
		S.Seed = Seed + static_cast<uint32_t>(M) * 2654435761u;
		const FGoThought T = Game.Think(S);
		if (!Game.Play(T.Move))
		{
			Game.Play(GoPass);
		}
	}
}

static void TestStrength()
{
	FGoStrength Strong = FGoStrength::Hustler();
	Strong.Temperature = 0.0f;
	const FGoStrength Weak = FGoStrength::Beginner();
	int StrongWins = 0;
	int Ended = 0;
	const int Games = 4;
	for (int G = 0; G < Games; ++G)
	{
		FGoGame Game(9);
		const bool bStrongBlack = G % 2 == 0;
		PlayGame(Game, bStrongBlack ? Strong : Weak, bStrongBlack ? Weak : Strong, 1000u + static_cast<uint32_t>(G), 250);
		const FGoResult& R = Game.Result();
		Ended += R.IsOver() ? 1 : 0;
		const EGoColor StrongColor = bStrongBlack ? EGoColor::Black : EGoColor::White;
		StrongWins += R.Winner == StrongColor ? 1 : 0;
		std::printf("      game %d: %s (strong) - %s, %d moves, %s by %.1f\n", G + 1, bStrongBlack ? "Black" : "White",
			bStrongBlack ? "White" : "Black", Game.MovesPlayed(), R.Winner == EGoColor::Black ? "Black" : "White", R.Margin);
	}
	Check(Ended == Games, "games between ghosts end in a count", Fmt("%d of %d", Ended, Games));
	Check(StrongWins >= Games - 1, "the hustler beats the beginner", Fmt("%d of %d", StrongWins, Games));

	FGoGame Atari(9, EGoMode::FirstCapture);
	PlayGame(Atari, Weak, Weak, 77u, 200);
	Check(Atari.Result().Outcome == EGoOutcome::FirstCapture, "a first-capture game between beginners ends with a capture",
		Fmt("%d moves", Atari.MovesPlayed()));
}

static void TestSpeed()
{
	const FGoGame Game(9);
	const auto Start = std::chrono::steady_clock::now();
	const FGoThought T = Game.Think(Seeded(FGoStrength::Master(), 4));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const double Rate = T.Playouts / (Seconds > 0.0 ? Seconds : 1e-9);
	Check(Seconds < 4.0, "the master thinks for under four seconds on an empty board",
		Fmt("%d quick games in %.2f s, %.0f a second, plays %s", T.Playouts, Seconds, Rate, Game.PointName(T.Move).c_str()));
}

// The full-size board (Walt, 2026-10-10: "I feel like the game should be regulation sized").
static void TestBigBoard()
{
	FGoGame Game(19);
	Check(Game.Size() == 19 && Game.PointName(0) == "A1" && Game.PointName(18) == "T1" && Game.PointName(360) == "T19",
		"a 19x19 board runs A to T (no I) and 1 to 19", Game.PointName(360));

	const auto Start = std::chrono::steady_clock::now();
	const FGoThought T = Game.Think(Seeded(FGoStrength::Master(), 4));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const int X = T.Move % 19;
	const int Y = T.Move / 19;
	const int Line = std::min(std::min(X, Y), std::min(18 - X, 18 - Y)) + 1;
	Check(Seconds < 4.0, "the master thinks for under four seconds on an empty 19x19 board",
		Fmt("%d quick games in %.2f s", T.Playouts, Seconds));
	Check(T.Move != GoPass && (Line == 3 || Line == 4), "and opens on the third or fourth line",
		Fmt("%s, line %d", Game.PointName(T.Move).c_str(), Line));

	const FGoAdvice A = Game.Advise(Seeded(FGoStrength::Coach(), 6), "the ghost");
	Check(Contains(A.Text, "corner") || Contains(A.Text, "third or fourth line"), "the coach explains a big-board opening", A.Text);

	// A white stone in atari in the middle of a big board: still taken.
	FGoBoard B(19);
	B.Play(At(19, 9, 10));   // B K11
	B.Play(At(19, 9, 9));    // W K10
	B.Play(At(19, 8, 9));    // B J10
	B.Play(At(19, 15, 15));  // W elsewhere
	B.Play(At(19, 10, 9));   // B L10
	B.Play(At(19, 3, 3));    // W elsewhere
	const FGoGame Atari = FGoGame::FromBoard(B);
	Check(Atari.Board().Liberties(At(19, 9, 9)) == 1, "a white stone in atari on 19x19");
	const FGoThought Take = Atari.Think(Seeded(FGoStrength::Coach(), 2));
	Check(Take.Move == At(19, 9, 8), "and the coach takes it", Atari.PointName(Take.Move));
}

// =====================================================================================
// COACH

static void TestCoach()
{
	const FGoGame Game = FGoGame::FromBoard(CapturePosition());
	const FGoAdvice A = Game.Advise(Seeded(FGoStrength::Coach(), 2), "the ghost");
	Check(A.Move == At(9, 5, 4) && StartsWith(A.Text, "F5 captures 1 stone."), "the hint names the capture", A.Text);
	Check(Contains(A.Text, "% of the coach's games"), "and says how often it wins from there");

	const FGoGame Atari = FGoGame::FromBoard(CapturePosition(), EGoMode::FirstCapture);
	const FGoAdvice W = Atari.Advise(Seeded(FGoStrength::Coach(), 2), "the ghost");
	Check(StartsWith(W.Text, "F5 captures - and the first capture wins the game!"), "with first capture wins, it says so", W.Text);

	const FGoGame Rescue = FGoGame::FromBoard(RescuePosition(), EGoMode::FirstCapture);
	const FGoAdvice R = Rescue.Advise(Seeded(FGoStrength::Coach(), 2), "the ghost");
	Check(StartsWith(R.Text, "E4 rescues your 2 stones"), "the rescue, named", R.Text);

	const FGoGame Opening(9);
	const FGoAdvice O = Opening.Advise(Seeded(FGoStrength::Coach(), 2), "the ghost");
	Check(!O.Text.empty() && O.Move != GoPass, "an opening hint", O.Text);
}

static void TestReview()
{
	const FGoStrength Coach = Seeded(FGoStrength::Coach(), 8);
	// He runs out of atari, as the coach would: nothing to say.
	FGoGame Good = FGoGame::FromBoard(RescuePosition(), EGoMode::FirstCapture);
	const FGoAdvice Before = Good.Advise(Coach, "the ghost");
	Good.Play(Before.Move);
	const FGoReview Silent = Good.ReviewLastMove(Coach, "the ghost", &Before);
	Check(!Silent.bMistake, "the coach is silent about its own move");

	// He ignores the atari and plays in the corner: White takes two stones and wins.
	FGoGame Bad = FGoGame::FromBoard(RescuePosition(), EGoMode::FirstCapture);
	Bad.Play(At(9, 0, 0));
	const FGoReview Blunder = Bad.ReviewLastMove(Coach, "the ghost", nullptr);
	Check(Blunder.bMistake && Contains(Blunder.Text, "The coach liked E4."), "a blunder, and the move that saves the stones",
		Blunder.Text);
	Check(StartsWith(Blunder.Text, "That leaves your 2 stones in atari: the ghost takes them at E4, and wins."),
		"and what happens next", Blunder.Text);

}

int main()
{
	const auto Start = std::chrono::steady_clock::now();
	TestEmptyBoard();
	TestCapture();
	TestSuicide();
	TestKo();
	TestFalseEye();
	TestArea();
	TestCount();
	TestUnclosedBorder();
	TestFirstCapture();
	TestUndoAndResign();
	TestNotation();
	TestRandomGames();
	TestTakesWhatIsFree();
	TestRescues();
	TestVitalPoint();
	TestDeterminism();
	TestStrength();
	TestSpeed();
	TestBigBoard();
	TestCoach();
	TestReview();

	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	std::printf("\n%d passed, %d failed  (%.1f s)\n", GPasses, GFailures, Seconds);
	return GFailures == 0 ? 0 : 1;
}
