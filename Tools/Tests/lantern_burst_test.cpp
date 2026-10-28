// lantern_burst_test.cpp — standalone tests for Source/SibeliusGame/LanternBurstSim, the lantern of
// orbs over Kaia's jungle. Run with Tools/Tests/run_lantern_burst_tests.ps1.
//
// THE CLAIMS: the lattice is a true six-neighbour wrap with no seam; a burst, a fall, a spark and
// the crown coming down each do what the rules say and score what they say; the preview is the
// shot; every hung stage hangs; a random player's every game ENDS (the house's trap test) without
// ever leaving the lantern in a state the rules forbid; and a greedy robot clears the first stage
// in most deals - so the opening is winnable, not a wall.

#include "LanternBurstSim.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace LanternBurst;

namespace
{
	int GPasses = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* Name, const std::string& Detail = std::string())
	{
		std::printf("%s  %s%s%s\n", bOk ? "PASS" : "FAIL", Name, Detail.empty() ? "" : "  -- ", Detail.c_str());
		(bOk ? GPasses : GFailures)++;
	}

	template <typename... TArgs>
	std::string Fmt(const char* Format, TArgs... Args)
	{
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	int Count(const std::vector<FLanternEvent>& Events, ELanternEvent Type)
	{
		int N = 0;
		for (const FLanternEvent& E : Events)
		{
			N += E.Type == Type ? 1 : 0;
		}
		return N;
	}

	const FLanternEvent* Find(const std::vector<FLanternEvent>& Events, ELanternEvent Type)
	{
		for (const FLanternEvent& E : Events)
		{
			if (E.Type == Type)
			{
				return &E;
			}
		}
		return nullptr;
	}

	// Every orb reaches row 0 through touching orbs - computed here, not by the sim.
	bool AllHang(const FLanternBurstSim& Sim)
	{
		std::vector<char> Hung(GridRows * Columns, 0);
		std::vector<FLanternCell> Queue;
		for (int C = 0; C < Columns; ++C)
		{
			if (Sim.At(0, C) != Empty)
			{
				Hung[C] = 1;
				Queue.push_back({ 0, C });
			}
		}
		std::vector<FLanternCell> Near;
		for (size_t I = 0; I < Queue.size(); ++I)
		{
			FLanternBurstSim::Neighbours(Queue[I].Row, Queue[I].Col, Near);
			for (const FLanternCell& N : Near)
			{
				if (!Hung[N.Row * Columns + N.Col] && Sim.At(N.Row, N.Col) != Empty)
				{
					Hung[N.Row * Columns + N.Col] = 1;
					Queue.push_back(N);
				}
			}
		}
		for (int R = 0; R < GridRows; ++R)
		{
			for (int C = 0; C < Columns; ++C)
			{
				if (Sim.At(R, C) != Empty && !Hung[R * Columns + C])
				{
					return false;
				}
			}
		}
		return true;
	}

	bool HandIsLive(const FLanternBurstSim& Sim)
	{
		if (Sim.OrbCount() == 0)
		{
			return true;
		}
		for (int Orb : { Sim.Current(), Sim.Next() })
		{
			if (Orb != Spark && !Sim.HasColor(Orb))
			{
				return false;
			}
		}
		return true;
	}

	struct FRandom
	{
		uint32_t State;
		uint32_t Next() { State ^= State << 13; State ^= State >> 17; State ^= State << 5; return State; }
		int Below(int N) { return static_cast<int>(Next() % static_cast<uint32_t>(N)); }
	};

	constexpr double Pi = 3.14159265358979323846;

	// The greedy robot, a careful player with the guide on: the lantern turned to every half
	// column, aimed every few degrees, with the hand as it is or swapped - and of every place it
	// can actually reach, the one that bursts and brings down the most; with nothing to burst, the
	// place that pairs the colour highest on the lantern. Never over the line if there is another.
	void PlayGreedy(FLanternBurstSim& Sim)
	{
		double Best = -1e18;
		bool bBestSwap = false;
		double BestLaunch = 0.0, BestAngle = 0.0;
		std::vector<FLanternCell> Near;
		for (int Swap = 0; Swap < 2; ++Swap)
		{
			FLanternBurstSim Hand = Sim;
			if (Swap)
			{
				Hand.Swap();
			}
			std::vector<char> Seen(GridRows * Columns, 0);
			for (int Col = 0; Col < Columns; ++Col)
			{
				for (int Degrees = -72; Degrees <= 72; Degrees += 8)
				{
					const double Launch = Col + 0.25, Angle = Degrees * Pi / 180.0;
					const FLanternTrace T = Hand.Trace(Launch, Angle, false);
					if (!T.bLands || Seen[T.Cell.Row * Columns + T.Cell.Col])
					{
						continue;
					}
					Seen[T.Cell.Row * Columns + T.Cell.Col] = 1;
					const FLanternBurstSim::FPreview P = Hand.PreviewAt(T.Cell.Row, T.Cell.Col);
					double Value = P.Burst * 10.0 + P.Fall * 25.0 + (P.bClears ? 1e6 : 0.0) - (P.bOverLine ? 1e9 : 0.0);
					if (P.Burst == 0)
					{
						FLanternBurstSim::Neighbours(T.Cell.Row, T.Cell.Col, Near);
						for (const FLanternCell& N : Near)
						{
							Value += Hand.At(N.Row, N.Col) == Hand.Current() ? 3.0 : 0.0;
						}
						Value -= T.Cell.Row * 0.5;
					}
					if (Value > Best)
					{
						Best = Value;
						bBestSwap = Swap != 0;
						BestLaunch = Launch;
						BestAngle = Angle;
					}
				}
			}
		}
		if (bBestSwap)
		{
			Sim.Swap();
		}
		Sim.Shoot(BestLaunch, BestAngle);
	}
}

static void TestLattice()
{
	bool bSymmetric = true, bDistanceOne = true, bCounts = true;
	std::vector<FLanternCell> Near, Back;
	for (int R = 0; R < GridRows; ++R)
	{
		for (int C = 0; C < Columns; ++C)
		{
			FLanternBurstSim::Neighbours(R, C, Near);
			const size_t Expected = (R == 0 || R == GridRows - 1) ? 4 : 6;
			bCounts &= Near.size() == Expected;
			for (const FLanternCell& N : Near)
			{
				bDistanceOne &= FLanternBurstSim::Distance({ R, C }, N) == 1;
				FLanternBurstSim::Neighbours(N.Row, N.Col, Back);
				bool bFound = false;
				for (const FLanternCell& B : Back)
				{
					bFound |= B.Row == R && B.Col == C;
				}
				bSymmetric &= bFound;
			}
		}
	}
	Check(bCounts, "lattice: six neighbours inside, four on the crown row and the bottom row");
	Check(bSymmetric, "lattice: if A touches B then B touches A, across the seam too");
	Check(bDistanceOne, "lattice: every neighbour is one step away");

	// Rings round a cell in the middle: 6 at one step, 12 at two, as on any hex board.
	int Ring1 = 0, Ring2 = 0;
	for (int R = 0; R < GridRows; ++R)
	{
		for (int C = 0; C < Columns; ++C)
		{
			const int D = FLanternBurstSim::Distance({ 6, 0 }, { R, C });
			Ring1 += D == 1 ? 1 : 0;
			Ring2 += D == 2 ? 1 : 0;
		}
	}
	Check(Ring1 == 6 && Ring2 == 12, "lattice: 6 places one step from a cell on the seam, 12 two steps", Fmt("%d, %d", Ring1, Ring2));

	FLanternBurstSim::Neighbours(4, Columns - 1, Near);
	bool bSeam = false;
	for (const FLanternCell& N : Near)
	{
		bSeam |= N.Col == 0;
	}
	Check(bSeam, "lattice: the last column touches the first");
	Check(FLanternBurstSim::Distance({ 0, 0 }, { 0, Columns - 1 }) == 1, "lattice: distance goes the short way round");
}

static void TestHungStages()
{
	bool bHang = true, bColors = true, bRows = true, bCrown = true, bHand = true;
	std::string Where;
	for (int Stage = 1; Stage <= 9; ++Stage)
	{
		for (uint32_t Seed = 1; Seed <= 40; ++Seed)
		{
			FLanternBurstSim Sim(Seed);
			Sim.NewGame(Seed * 7919u);
			while (Sim.Stage() < Stage)
			{
				Sim.NextStage();
			}
			const FLanternStageSpec Spec = FLanternBurstSim::StageSpec(Stage);
			if (!AllHang(Sim)) { bHang = false; Where = Fmt("stage %d seed %u", Stage, Seed); }
			bRows &= Sim.LowestRow() < Spec.Rows;
			for (int C = 0; C < Columns; ++C)
			{
				bCrown &= Sim.At(0, C) != Empty;
				for (int R = 0; R < GridRows; ++R)
				{
					bColors &= Sim.At(R, C) == Empty || (Sim.At(R, C) >= 0 && Sim.At(R, C) < Spec.Colors);
				}
			}
			bHand &= HandIsLive(Sim) && Sim.Current() < Spec.Colors && Sim.Next() < Spec.Colors;
		}
	}
	Check(bHang, "stages: every orb of every hung stage hangs from the crown", Where);
	Check(bColors, "stages: only the stage's colours");
	Check(bRows, "stages: no deeper than the stage's rows");
	Check(bCrown, "stages: the crown row starts full");
	Check(bHand, "stages: the hand starts with colours on the lantern");
}

static void TestBurstAndFall()
{
	FLanternBurstSim Sim(5);
	Sim.NewGame(5);
	Sim.ClearLantern();
	Sim.TakeEvents();
	for (int C = 0; C < Columns; ++C)
	{
		Sim.SetCell(0, C, 1);
	}
	Sim.SetCell(0, 5, 0);
	Sim.SetCell(0, 6, 0);
	Sim.SetCell(0, 7, Empty);
	Sim.SetCell(1, 5, 2);      // odd row: hangs from (0,5) and (0,6) only
	Sim.SetHand(0, 1);
	const FLanternBurstSim::FPreview P = Sim.PreviewAt(0, 7);
	const long long Before = Sim.Score();
	Check(Sim.PlaceAt(0, 7), "burst: the shot is legal");
	const std::vector<FLanternEvent> Events = Sim.TakeEvents();
	const FLanternEvent* Burst = Find(Events, ELanternEvent::Burst);
	const FLanternEvent* Fell = Find(Events, ELanternEvent::Fell);
	Check(Burst && Burst->Orbs.size() == 3, "burst: three of a colour burst");
	Check(Fell && Fell->Orbs.size() == 1 && Fell->Orbs[0].Row == 1 && Fell->Orbs[0].Col == 5, "fall: the orb they held falls");
	Check(Sim.Score() - Before == 30 + 10, "score: 10 a burst orb, 10 n(n+1)/2 for a fall", Fmt("%lld", Sim.Score() - Before));
	Check(P.Burst == 3 && P.Fall == 1, "preview: told the same before the shot", Fmt("%d burst, %d fall", P.Burst, P.Fall));
	Check(Sim.At(1, 5) == Empty && Sim.At(0, 5) == Empty, "burst: they are gone from the lantern");
	Check(AllHang(Sim), "fall: nothing left hanging from nothing");

	// Two of a colour is not three: a miss, and the miss counter moves.
	FLanternBurstSim Miss(6);
	Miss.NewGame(6);
	Miss.ClearLantern();
	Miss.SetCell(0, 0, 0);
	Miss.SetCell(0, 10, 1);
	Miss.SetHand(0, 1);
	const int Left = Miss.MissesBeforeLower();
	Miss.TakeEvents();
	Miss.PlaceAt(0, 1);
	Check(Miss.At(0, 1) == 0 && Miss.MissesBeforeLower() == Left - 1 && Miss.Streak() == 0, "miss: a pair stays and counts as a miss");
}

static void TestCrownComesDown()
{
	FLanternBurstSim Sim(7);
	Sim.NewGame(7);
	Sim.ClearLantern();
	for (int C = 0; C < Columns; ++C)
	{
		Sim.SetCell(0, C, C % 2);   // alternate, so nothing can make three in row 0's neighbours easily
	}
	const int Every = FLanternBurstSim::StageSpec(Sim.Stage()).LowerEvery;
	const int Line = Sim.LineRow();
	Sim.TakeEvents();
	int Lowered = 0;
	for (int I = 0; I < Every; ++I)
	{
		// A colour that cannot match: 2 is on no neighbour.
		Sim.SetHand(2, 2);
		Sim.SetCell(0, 0, Sim.At(0, 0));
		const std::vector<FLanternCell> F = Sim.Frontier();
		// shoot deep, under the crown, away from everything else of colour 2
		FLanternCell Pick = F[0];
		for (const FLanternCell& Cell : F)
		{
			std::vector<FLanternCell> Near;
			FLanternBurstSim::Neighbours(Cell.Row, Cell.Col, Near);
			bool bLonely = true;
			for (const FLanternCell& N : Near)
			{
				bLonely &= Sim.At(N.Row, N.Col) != 2;
			}
			if (bLonely && Cell.Row >= Pick.Row)
			{
				Pick = Cell;
			}
		}
		Sim.PlaceAt(Pick.Row, Pick.Col);
		Lowered += Count(Sim.TakeEvents(), ELanternEvent::Lowered);
	}
	Check(Lowered == 1 && Sim.Lowered() == 1, "crown: comes down one row after the stage's misses", Fmt("%d misses, lowered %d", Every, Lowered));
	Check(Sim.LineRow() == Line - 1, "crown: the embers are a row nearer");

	// An orb just above the line, and the crown comes down onto it: over.
	FLanternBurstSim Over(8);
	Over.NewGame(8);
	Over.ClearLantern();
	for (int R = 0; R < Over.LineRow(); ++R)
	{
		Over.SetCell(R, 3, R % 2 == 0 ? 0 : 1);   // a thread of alternating colours down to the line
	}
	Over.SetCell(0, 12, 4);
	bool bOver = false;
	for (int I = 0; I < 40 && Over.Phase() == ELanternPhase::Playing; ++I)
	{
		Over.SetHand(5, 5);
		Over.PlaceAt(0, 13 + (I % 5));   // 5s beside the 4: only pairs ever... until they burst
		for (const FLanternEvent& E : Over.TakeEvents())
		{
			bOver |= E.Type == ELanternEvent::GameOver;
		}
	}
	Check(bOver && Over.Phase() == ELanternPhase::GameOver, "crown: coming down onto the embers ends the game");

	// A shot placed on the line itself that bursts nothing is over at once.
	FLanternBurstSim Line2(9);
	Line2.NewGame(9);
	Line2.ClearLantern();
	for (int R = 0; R < Line2.LineRow(); ++R)
	{
		Line2.SetCell(R, 0, 0);
	}
	Line2.SetHand(1, 1);
	const FLanternBurstSim::FPreview P = Line2.PreviewAt(Line2.LineRow(), 0);
	Line2.PlaceAt(Line2.LineRow(), 0);
	Check(P.bOverLine && Line2.Phase() == ELanternPhase::GameOver, "line: an orb left on the embers ends it, and the preview warns");
	Check(!Line2.CanPlace(Line2.LineRow() + 1, 0), "line: nothing can be placed below the embers");
}

static void TestSpark()
{
	FLanternBurstSim Sim(11);
	Sim.NewGame(11);
	Sim.ClearLantern();
	for (int R = 0; R < 6; ++R)
	{
		for (int C = 0; C < Columns; ++C)
		{
			Sim.SetCell(R, C, (R + C) % 4);
		}
	}
	Sim.SetCell(2, 0, Empty);   // a hole on the seam
	Sim.SetHand(Spark, 0);
	Sim.TakeEvents();
	Sim.PlaceAt(2, 0);
	const std::vector<FLanternEvent> Events = Sim.TakeEvents();
	const FLanternEvent* Burned = Find(Events, ELanternEvent::Sparked);
	bool bInReach = Burned != nullptr, bSeam = false;
	if (Burned)
	{
		for (const FLanternOrb& O : Burned->Orbs)
		{
			bInReach &= FLanternBurstSim::Distance({ 2, 0 }, { O.Row, O.Col }) <= SparkReach;
			bSeam |= O.Col >= Columns - 2;
		}
	}
	Check(Burned && Burned->Orbs.size() == 19, "spark: burns itself and the 18 orbs within two steps", Fmt("%d", Burned ? static_cast<int>(Burned->Orbs.size()) : -1));
	Check(bInReach, "spark: nothing beyond two steps");
	Check(bSeam, "spark: reaches across the seam");
	Check(AllHang(Sim), "spark: what it cut loose fell");

	// Four bursts in a row earn one, and it arrives as the NEXT orb.
	FLanternBurstSim Run(12);
	Run.NewGame(12);
	Run.ClearLantern();
	for (int C = 0; C < Columns; ++C)
	{
		Run.SetCell(0, C, 5);
	}
	for (int K = 0; K < 4; ++K)
	{
		Run.SetCell(0, 2 + 4 * K, K % 3);
		Run.SetCell(0, 3 + 4 * K, K % 3);
		Run.SetCell(0, 4 + 4 * K, Empty);
	}
	Run.TakeEvents();
	bool bEarned = false;
	for (int K = 0; K < 4; ++K)
	{
		Run.SetHand(K % 3, 5);
		Run.PlaceAt(0, 4 + 4 * K);
		bEarned |= Count(Run.TakeEvents(), ELanternEvent::SparkEarned) > 0;
	}
	Check(Run.Streak() == 4, "streak: four bursts in a row", Fmt("%d", Run.Streak()));
	Check(bEarned && Run.Next() == Spark, "spark: earned on the fourth, waiting as the next orb");
	Run.Swap();
	Check(Run.Current() == Spark, "swap: the spark can be loaded at once");
}

static void TestStageClear()
{
	FLanternBurstSim Sim(13);
	Sim.NewGame(13);
	Sim.ClearLantern();
	Sim.SetCell(0, 0, 0);
	Sim.SetCell(0, 1, 0);
	Sim.SetHand(0, 0);
	const long long Before = Sim.Score();
	Sim.TakeEvents();
	Sim.PlaceAt(0, 2);
	const FLanternEvent* Clear = Find(Sim.TakeEvents(), ELanternEvent::StageCleared);
	Check(Sim.Phase() == ELanternPhase::StageClear && Clear, "clear: an empty lantern ends the stage");
	Check(Sim.Score() - Before == 30 + 2000, "clear: 1000 a stage, 1000 more with the crown never lowered", Fmt("%lld", Sim.Score() - Before));
	Check(!Sim.PlaceAt(0, 3), "clear: no shots between stages");
	Sim.NextStage();
	Check(Sim.Stage() == 2 && Sim.Phase() == ELanternPhase::Playing && Sim.OrbCount() > 0 && HandIsLive(Sim), "clear: the next stage is hung, the hand dealt");
}

static void TestRules()
{
	FLanternBurstSim Sim(14);
	Sim.NewGame(14);
	Sim.ClearLantern();
	Sim.SetCell(0, 0, 1);
	Check(!Sim.CanPlace(0, 0), "rules: not onto an orb");
	Check(!Sim.CanPlace(3, 10), "rules: not into the air with nothing to touch");
	Check(Sim.CanPlace(0, 10), "rules: anywhere along the crown");
	Check(Sim.CanPlace(1, 0), "rules: under an orb");
	Sim.SetHand(2, 3);
	Sim.Swap();
	Check(Sim.Current() == 3 && Sim.Next() == 2, "swap: trades the loaded orb and the next");

	FLanternBurstSim A(99), B(99);
	A.NewGame(4242);
	B.NewGame(4242);
	for (int I = 0; I < 60 && A.Phase() == ELanternPhase::Playing; ++I)
	{
		PlayGreedy(A);
		PlayGreedy(B);
	}
	Check(A.Score() == B.Score() && A.Shots() == B.Shots(), "determinism: the same deal and the same shots score the same");
}

// THE TRAP TEST: hundreds of games of random legal shots. Each must END, and after every shot
// the lantern must be one the rules allow.
static void TestRandomPlay()
{
	int Ended = 0, Shots = 0, PreviewChecks = 0, PreviewWrong = 0;
	bool bHang = true, bLine = true, bScore = true, bHand = true, bFrontier = true;
	std::string Where;
	const int Games = 400;
	for (int G = 0; G < Games; ++G)
	{
		FRandom Rand{ 0x9E3779B9u ^ static_cast<uint32_t>(G * 2654435761u) };
		FLanternBurstSim Sim(static_cast<uint32_t>(G + 1));
		Sim.NewGame(static_cast<uint32_t>(G * 31 + 17));
		long long LastScore = 0;
		for (int I = 0; I < 6000; ++I)
		{
			if (Sim.Phase() == ELanternPhase::StageClear)
			{
				Sim.NextStage();
			}
			if (Sim.Phase() != ELanternPhase::Playing)
			{
				break;
			}
			if (Rand.Below(4) == 0)
			{
				Sim.Swap();
			}
			// Anywhere round, at any angle - a little past the limit too, to try the clamp.
			const double Launch = Rand.Below(4000) / 200.0;
			const double Angle = (Rand.Below(2001) - 1000) / 1000.0 * 1.4;
			const FLanternTrace T = Sim.Trace(Launch, Angle);
			const FLanternBurstSim::FPreview P = Sim.Preview(Launch, Angle);
			const int OrbsBefore = Sim.OrbCount();
			Sim.TakeEvents();
			if (!T.bLands || !Sim.Shoot(Launch, Angle))
			{
				bFrontier = false;
				Where = Fmt("game %d shot %d: launch %.2f angle %.3f did not land", G, I, Launch, Angle);
				break;
			}
			++Shots;
			int Burst = 0, Fall = 0;
			bool bWhereTraced = false;
			for (const FLanternEvent& E : Sim.TakeEvents())
			{
				if (E.Type == ELanternEvent::Placed) bWhereTraced = E.Orbs[0].Row == T.Cell.Row && E.Orbs[0].Col == T.Cell.Col;
				if (E.Type == ELanternEvent::Burst || E.Type == ELanternEvent::Sparked) Burst += static_cast<int>(E.Orbs.size());
				if (E.Type == ELanternEvent::Fell) Fall += static_cast<int>(E.Orbs.size());
			}
			++PreviewChecks;
			PreviewWrong += (!bWhereTraced || P.Burst != Burst || P.Fall != Fall || OrbsBefore + 1 - Burst - Fall != Sim.OrbCount()) ? 1 : 0;
			bHang &= AllHang(Sim);
			bLine &= Sim.Phase() == ELanternPhase::GameOver || Sim.LowestRow() < Sim.LineRow();
			bScore &= Sim.Score() >= LastScore;
			LastScore = Sim.Score();
			bHand &= Sim.Phase() != ELanternPhase::Playing || HandIsLive(Sim);
		}
		Ended += Sim.Phase() == ELanternPhase::GameOver ? 1 : 0;
	}
	Check(Ended == Games, "random play: every game ends", Fmt("%d of %d, %d shots in all", Ended, Games, Shots));
	Check(bFrontier, "random play: every shot, anywhere at any angle, lands", Where);
	Check(bHang, "random play: nothing is ever left hanging from nothing");
	Check(bLine, "random play: an orb on the embers always means the game is over");
	Check(bScore, "random play: the score never goes down");
	Check(bHand, "random play: the hand only ever holds colours on the lantern, or a spark");
	Check(PreviewWrong == 0, "random play: the shot sticks where traced, and the preview matches it", Fmt("%d of %d wrong", PreviewWrong, PreviewChecks));
}

static void TestShots()
{
	const double Deg = Pi / 180.0;
	FLanternBurstSim Sim(21);
	Sim.NewGame(21);
	Sim.ClearLantern();
	Sim.SetHand(0, 0);
	FLanternTrace T = Sim.Trace(7.2, 0.0);
	Check(T.bLands && T.Cell.Row == 0 && T.Cell.Col == 7, "shot: straight up an empty lantern sticks under the crown",
		Fmt("(%d, %d)", T.Cell.Row, T.Cell.Col));
	Check(T.Path.size() > 10 && T.Path.front().Y > T.Path.back().Y, "shot: the path climbs from the launcher");

	Sim.SetCell(0, 5, 1);
	T = Sim.Trace(5.2, 0.0);
	Check(T.bLands && T.Cell.Row == 1 && T.Cell.Col == 5, "shot: under an orb, it sticks below it on the side it came up",
		Fmt("(%d, %d)", T.Cell.Row, T.Cell.Col));

	// Round the seam: from column 19, angled toward higher X, it comes out past column 0.
	FLanternBurstSim Seam(22);
	Seam.NewGame(22);
	Seam.ClearLantern();
	const double Angle = 30.0 * Deg;
	const double Expect = std::fmod(19.0 + Seam.LauncherY() * std::tan(Angle), Columns);
	T = Seam.Trace(19.0, Angle);
	Check(T.bLands && T.Cell.Row == 0 && std::abs(FLanternBurstSim::WrapDelta(T.Cell.Col - Expect)) <= 0.51,
		"shot: winds round the lantern across the seam", Fmt("col %d, expected %.2f", T.Cell.Col, Expect));
	FLanternTrace Steep = Seam.Trace(3.0, 2.0);
	FLanternTrace Limit = Seam.Trace(3.0, MaxAimAngle);
	Check(Steep.bLands && Steep.Cell == Limit.Cell, "shot: aim is held to 75 degrees either side");

	// A hole up inside the lantern, open only from the side and above, cannot be reached from below.
	FLanternBurstSim Hole(23);
	Hole.NewGame(23);
	Hole.ClearLantern();
	for (int R = 0; R < 4; ++R)
	{
		for (int C = 0; C < Columns; ++C)
		{
			Hole.SetCell(R, C, (C / 2) % 3);
		}
	}
	Hole.SetCell(1, 10, Empty);
	bool bReached = false;
	for (int L = 0; L < 400; ++L)
	{
		for (int D = -75; D <= 75; D += 3)
		{
			const FLanternTrace H = Hole.Trace(L * 0.05, D * Deg, false);
			bReached |= H.bLands && H.Cell.Row == 1 && H.Cell.Col == 10;
		}
	}
	Check(Hole.CanPlace(1, 10) && !bReached, "shot: a hole up inside the lantern is out of reach from below");

	// The launcher stays put in the world: a row nearer the crown's centres each time it comes down.
	FLanternBurstSim Low(24);
	Low.NewGame(24);
	const double Before = Low.LauncherY();
	Low.ClearLantern();
	Low.SetCell(0, 0, 0);
	for (int I = 0; I < FLanternBurstSim::StageSpec(1).LowerEvery; ++I)
	{
		Low.SetHand(1 + I % 2, 1);
		Low.PlaceAt(0, 2 + 2 * I);
	}
	Check(Low.Lowered() == 1 && std::abs(Before - Low.LauncherY() - RowHeight) < 1e-9, "shot: the launcher keeps its place in the world as the crown comes down");
}

// Is the opening winnable, and does it get hard? The greedy robot sees all the way round and
// knows every shot's outcome before it fires - a far better player than a man looking at the
// front of the lantern - so it should clear stage 1 nearly always, and still lose in the end.
static void TestGreedyRobot(int Games)
{
	const int ShotCap = 600;
	int ClearedOne = 0, ShotsForOne = 0, Capped = 0;
	long long TotalScore = 0;
	int StageHist[16] = {};
	int StageShots[16] = {}, StageClears[16] = {};
	for (int G = 0; G < Games; ++G)
	{
		FLanternBurstSim Sim(static_cast<uint32_t>(G + 1));
		Sim.NewGame(static_cast<uint32_t>(G * 97 + 3));
		int StageStart = 0;
		for (int I = 0; I <= ShotCap; ++I)
		{
			if (Sim.Phase() == ELanternPhase::StageClear)
			{
				const int S = Sim.Stage() < 15 ? Sim.Stage() : 15;
				StageShots[S] += Sim.Shots() - StageStart;
				StageClears[S]++;
				StageStart = Sim.Shots();
				if (Sim.Stage() == 1)
				{
					++ClearedOne;
					ShotsForOne += Sim.Shots();
				}
				Sim.NextStage();
			}
			if (Sim.Phase() != ELanternPhase::Playing)
			{
				break;
			}
			if (I == ShotCap)
			{
				++Capped;
				break;
			}
			PlayGreedy(Sim);
		}
		StageHist[Sim.Stage() < 15 ? Sim.Stage() : 15]++;
		TotalScore += Sim.Score();
	}
	std::printf("      greedy robot: stage 1 cleared in %d of %d (%.1f shots each); still going after %d shots: %d; mean score %lld\n",
		ClearedOne, Games, ClearedOne ? static_cast<double>(ShotsForOne) / ClearedOne : 0.0, ShotCap, Capped, TotalScore / Games);
	std::printf("      greedy robot: ended on stage");
	for (int S = 1; S < 16; ++S)
	{
		if (StageHist[S]) std::printf("  %d:%d", S, StageHist[S]);
	}
	std::printf("\n      greedy robot: shots to clear stage");
	for (int S = 1; S < 16; ++S)
	{
		if (StageClears[S]) std::printf("  %d:%.0f", S, static_cast<double>(StageShots[S]) / StageClears[S]);
	}
	std::printf("\n");
	Check(ClearedOne * 4 >= Games * 3, "greedy robot: clears stage 1 in at least three deals of four", Fmt("%d of %d", ClearedOne, Games));
	const double One = StageClears[1] ? static_cast<double>(StageShots[1]) / StageClears[1] : 0.0;
	const double Six = StageClears[6] ? static_cast<double>(StageShots[6]) / StageClears[6] : 1e9;
	Check(Six >= 2.0 * One, "greedy robot: stage 6 takes at least twice the shots of stage 1 - it gets harder", Fmt("%.1f vs %.1f", Six, One));
}

// A sloppy player: half his shots anywhere, the rest the best burst of a few he happens to try,
// never swapping, never thinking about what falls. He must lose - and not on the first stage.
static void TestSloppyPlayer()
{
	const int Games = 200;
	int Ended = 0, ClearedOne = 0, StageHist[16] = {};
	for (int G = 0; G < Games; ++G)
	{
		FRandom Rand{ 0x1234567u + static_cast<uint32_t>(G) * 7919u };
		FLanternBurstSim Sim(static_cast<uint32_t>(G + 1));
		Sim.NewGame(static_cast<uint32_t>(G * 13 + 5));
		for (int I = 0; I < 3000; ++I)
		{
			if (Sim.Phase() == ELanternPhase::StageClear)
			{
				ClearedOne += Sim.Stage() == 1 ? 1 : 0;
				Sim.NextStage();
			}
			if (Sim.Phase() != ELanternPhase::Playing)
			{
				break;
			}
			double Launch = Rand.Below(2000) / 100.0, Angle = (Rand.Below(1441) - 720) * Pi / 1800.0;
			if (Rand.Below(2) == 0)
			{
				int Best = -1;
				for (int Try = 0; Try < 12; ++Try)
				{
					const double L = Rand.Below(2000) / 100.0, A = (Rand.Below(1441) - 720) * Pi / 1800.0;
					const FLanternBurstSim::FPreview P = Sim.Preview(L, A);
					if (P.bLegal && !P.bOverLine && P.Burst > Best)
					{
						Best = P.Burst;
						Launch = L;
						Angle = A;
					}
				}
			}
			Sim.Shoot(Launch, Angle);
		}
		Ended += Sim.Phase() == ELanternPhase::GameOver ? 1 : 0;
		StageHist[Sim.Stage() < 15 ? Sim.Stage() : 15]++;
	}
	std::printf("      sloppy player: ended on stage");
	for (int S = 1; S < 16; ++S)
	{
		if (StageHist[S]) std::printf("  %d:%d", S, StageHist[S]);
	}
	std::printf("\n");
	Check(Ended == Games, "sloppy player: every game ends", Fmt("%d of %d", Ended, Games));
	Check(ClearedOne * 3 >= Games, "sloppy player: still clears stage 1 a third of the time or more", Fmt("%d of %d", ClearedOne, Games));
}

int main(int ArgC, char** ArgV)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	if (ArgC > 1 && std::string(ArgV[1]) == "robot")
	{
		TestGreedyRobot(ArgC > 2 ? std::atoi(ArgV[2]) : 30);   // tuning the stages: the robots alone
		TestSloppyPlayer();
		return GFailures == 0 ? 0 : 1;
	}
	TestLattice();
	TestHungStages();
	TestBurstAndFall();
	TestCrownComesDown();
	TestSpark();
	TestStageClear();
	TestRules();
	TestShots();
	TestRandomPlay();
	TestGreedyRobot(20);
	TestSloppyPlayer();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
