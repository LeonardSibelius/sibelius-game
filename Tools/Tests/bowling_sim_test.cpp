// bowling_sim_test.cpp — standalone tests for Source/SibeliusGame/BowlingSim.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_bowling_sim_tests.ps1.
//
// Each test is a PHYSICAL or a RULES claim: a perfect game is 300; a straight ball stays
// straight; a hook stays nearly straight on the oil and breaks on the dry end; a gutter
// ball knocks nothing down; a ball into the pocket strikes far more often than one on the
// nose; nothing ever gains energy; every roll ends; every game ends in ten frames.

#include "BowlingSim.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	constexpr double Pi = 3.14159265358979323846;

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
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	struct FLcg
	{
		uint32_t State;
		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) * (1.0 / 16777216.0);
		}
	};

	int CountEvents(const std::vector<FBowlingEvent>& Events, EBowlingEvent Type)
	{
		int Count = 0;
		for (const FBowlingEvent& Event : Events)
		{
			Count += Event.Type == Type ? 1 : 0;
		}
		return Count;
	}

	// Runs one ball to the end of its roll. Returns the seconds it took, or -1.
	double RunRoll(FBowlingSim& Sim, std::vector<FBowlingEvent>* Into = nullptr, double Limit = 30.0)
	{
		double T = 0.0;
		while (Sim.Phase() == EBowlingPhase::Rolling && T < Limit)
		{
			Sim.Substep();
			T += FBowlingSim::SubstepSeconds;
			if (Into)
			{
				const std::vector<FBowlingEvent> E = Sim.TakeEvents();
				Into->insert(Into->end(), E.begin(), E.end());
			}
		}
		return Sim.Phase() == EBowlingPhase::Rolling ? -1.0 : T;
	}

	// Knocked down by one first ball from a fresh rack.
	int FirstBall(double StartY, double Angle, double Power, double Hook, std::vector<FBowlingEvent>* Into = nullptr)
	{
		FBowlingSim Sim;
		Sim.StartGame();
		Sim.TakeEvents();
		Sim.Roll(StartY, Angle, Power, Hook);
		RunRoll(Sim, Into);
		return Sim.Rolls().empty() ? -1 : Sim.Rolls()[0];
	}

	// Where a ball crosses X, rolled on an empty lane (a rack with no pins).
	double CrossingY(double StartY, double Angle, double Power, double Hook, double AtX)
	{
		FBowlingSim Sim;
		Sim.StartGame();
		const bool None[10] = {};
		Sim.SetPinsStanding(None);
		Sim.Roll(StartY, Angle, Power, Hook);
		double LastY = StartY;
		for (int I = 0; I < 200000 && Sim.IsBallOnLane(); ++I)
		{
			Sim.Substep();
			if (Sim.BallPosition().X >= AtX)
			{
				return Sim.BallPosition().Y;
			}
			LastY = Sim.BallPosition().Y;
		}
		return LastY;
	}

	// The angle that sends a ball from StartY to arrive at TargetY on the head pin's line,
	// found by bisection: a bowler's "target", not arithmetic, because a hook bends.
	double AimFor(double StartY, double TargetY, double Power, double Hook)
	{
		const double HeadX = FBowlingLaneLayout::MakeRegulation().HeadPinX;
		double Lo = -0.08;
		double Hi = 0.08;
		for (int I = 0; I < 40; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			if (CrossingY(StartY, Mid, Power, Hook, HeadX) < TargetY)
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}
		return 0.5 * (Lo + Hi);
	}
}

static void TestScoring()
{
	int F[10];
	// Score first, then Check: the order a function's arguments are evaluated in is not
	// fixed, and the first version printed the PREVIOUS game's frame.
	const std::vector<int> Perfect(12, 10);
	const int PerfectScore = FBowlingSim::ScoreRolls(Perfect, F);
	Check(PerfectScore == 300 && F[0] == 30 && F[9] == 300, "a perfect game is 300", Fmt("%d", PerfectScore));

	const std::vector<int> Gutter(20, 0);
	const int GutterScore = FBowlingSim::ScoreRolls(Gutter, F);
	Check(GutterScore == 0 && F[9] == 0, "a gutter game is 0");

	const std::vector<int> AllSpares(21, 5);
	const int SparesScore = FBowlingSim::ScoreRolls(AllSpares, F);
	Check(SparesScore == 150, "all 5-spares is 150", Fmt("%d", SparesScore));

	std::vector<int> Nines;
	for (int I = 0; I < 10; ++I)
	{
		Nines.push_back(9);
		Nines.push_back(0);
	}
	Check(FBowlingSim::ScoreRolls(Nines, F) == 90, "all 9-0 frames is 90");

	// A worked example: X, 7/, 9-, X, -8, 8/, -6, X, X, X81 = 167.
	const std::vector<int> Game = { 10, 7, 3, 9, 0, 10, 0, 8, 8, 2, 0, 6, 10, 10, 10, 8, 1 };
	const int Total = FBowlingSim::ScoreRolls(Game, F);
	Check(Total == 167 && F[0] == 20 && F[1] == 39 && F[2] == 48, "a worked example scores 167, frame by frame",
		Fmt("%d (%d %d %d)", Total, F[0], F[1], F[2]));

	const std::vector<int> Pending = { 10, 10 };
	FBowlingSim::ScoreRolls(Pending, F);
	Check(F[0] == -1 && F[1] == -1, "a strike waits for its two bonus balls before it scores");

	// The running score on the board: pins so far plus the bonuses thrown so far.
	FBowlingSim Board;
	Board.StartGame();
	Board.Roll(-25.0, 0.0, 0.5, 0.8);
	RunRoll(Board);
	const int First = Board.Rolls()[0];
	Check(Board.RunningScore() == First, "the running score counts the first ball at once", Fmt("%d for %d pins", Board.RunningScore(), First));
}

static void TestRack()
{
	const FBowlingLaneLayout L = FBowlingLaneLayout::MakeRegulation();
	double Worst = 0.0;
	const int Pairs[][2] = { { 0, 1 }, { 0, 2 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 2, 4 }, { 2, 5 }, { 6, 7 }, { 8, 9 }, { 3, 6 }, { 5, 9 } };
	for (const auto& P : Pairs)
	{
		const FBowlingVec2 D = L.PinSpots[P[1]] - L.PinSpots[P[0]];
		Worst = std::fmax(Worst, std::fabs(std::sqrt(D.X * D.X + D.Y * D.Y) - L.PinSpacing));
	}
	Check(Worst < 1e-9, "neighbouring pins are exactly 12 in apart", Fmt("worst %.2g cm", Worst));
	Check(L.PinSpots[6].Y > 0.0 && L.PinSpots[9].Y < 0.0, "the 7 pin is on the left, the 10 on the right");
	Check(L.PinSpots[6].X + L.PinRadius < L.PitX, "the back row stands on the deck, short of the pit");
	Check(std::fabs(L.PinSpots[6].Y) + L.PinRadius < L.LaneHalfWidth, "the corner pins stand inside the lane edge");

	const FBowlingLaneLayout R = FBowlingLaneLayout::MakeRetroBowl();
	Check(R.PinSpots[6].X + R.PinRadius < R.PitX && std::fabs(R.PinSpots[9].Y) + R.PinRadius < R.LaneHalfWidth,
		"the alley's own lane: the whole rack fits on its deck", Fmt("back row ends %.1f cm short of the pit", R.PitX - R.PinSpots[6].X - R.PinRadius));

	// The alley plays like a lane too: a pocket hook carries on the pack's measured lane.
	FBowlingSim Alley(R);
	Alley.StartGame();
	int Strikes = 0;
	for (int I = 0; I < 6; ++I)
	{
		FBowlingSim Try(R);
		Try.StartGame();
		double Lo = -0.08, Hi = 0.08;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			FBowlingSim Probe(R);
			Probe.StartGame();
			const bool None[10] = {};
			Probe.SetPinsStanding(None);
			Probe.Roll(-25.0, Mid, 0.5, 0.8);
			double Y = 0.0;
			for (int J = 0; J < 400000 && Probe.IsBallOnLane(); ++J)
			{
				Probe.Substep();
				if (Probe.BallPosition().X >= R.HeadPinX) { Y = Probe.BallPosition().Y; break; }
			}
			if (Y < -6.3 + 0.4 * I - 1.0) { Lo = Mid; } else { Hi = Mid; }
		}
		Try.Roll(-25.0, 0.5 * (Lo + Hi), 0.5, 0.8);
		RunRoll(Try);
		Strikes += Try.Rolls()[0] == 10 ? 1 : 0;
	}
	Check(Strikes >= 3, "and a pocket hook strikes there too", Fmt("%d of 6", Strikes));
}

static void TestStraightAndHook()
{
	const double HeadX = FBowlingLaneLayout::MakeRegulation().HeadPinX;
	const double OilX = FBowlingLaneLayout::MakeRegulation().OilLength;

	const double Straight = CrossingY(0.0, 0.0, 0.5, 0.0, HeadX);
	Check(std::fabs(Straight) < 0.01, "a straight ball down the middle stays on the middle board", Fmt("%.4f cm off", Straight));

	const double HookOil = CrossingY(0.0, 0.0, 0.5, 1.0, OilX);
	const double HookEnd = CrossingY(0.0, 0.0, 0.5, 1.0, HeadX);
	Check(HookEnd > 15.0, "a full hook breaks well left by the pins", Fmt("%.1f cm", HookEnd));
	Check(HookOil < 0.45 * HookEnd, "most of it comes on the dry back end, not the oil",
		Fmt("%.1f of %.1f cm by the end of the oil", HookOil, HookEnd));
	const double Mirror = CrossingY(0.0, 0.0, 0.5, -1.0, HeadX);
	Check(std::fabs(Mirror + HookEnd) < 1e-6, "a left-hander's hook is the exact mirror", Fmt("%.3f vs %.3f", Mirror, HookEnd));

	const double Soft = CrossingY(0.0, 0.0, 0.0, 1.0, HeadX);
	const double Hard = CrossingY(0.0, 0.0, 1.0, 1.0, HeadX);
	Check(Soft > Hard, "a slower ball hooks more (more time for the lane to grab it)", Fmt("%.1f vs %.1f cm", Soft, Hard));

	FBowlingSim Sim;
	Sim.StartGame();
	const bool None[10] = {};
	Sim.SetPinsStanding(None);
	Sim.Roll(0.0, 0.0, 0.0, 1.0);
	double ArriveSpeed = 0.0;
	bool bRolledByPins = false;
	for (int I = 0; I < 200000 && Sim.IsBallOnLane(); ++I)
	{
		Sim.Substep();
		if (Sim.BallPosition().X >= HeadX && ArriveSpeed == 0.0)
		{
			const FBowlingVec2 V = Sim.BallVelocity();
			ArriveSpeed = std::sqrt(V.X * V.X + V.Y * V.Y);
			bRolledByPins = !Sim.IsBallSliding();
		}
	}
	Check(ArriveSpeed > 300.0, "the slowest ball still reaches the pins with pace", Fmt("%.0f cm/s", ArriveSpeed));
	Check(bRolledByPins, "and by then it has stopped skidding and rolls");
}

static void TestGutter()
{
	std::vector<FBowlingEvent> Events;
	const int Down = FirstBall(40.0, 0.03, 0.5, 0.0, &Events);
	Check(CountEvents(Events, EBowlingEvent::BallInGutter) == 1, "a ball thrown off the edge drops into the gutter");
	Check(Down == 0 && CountEvents(Events, EBowlingEvent::BallHitPin) == 0, "and a gutter ball knocks nothing down", Fmt("%d", Down));
}

static void TestPocketVersusNose()
{
	// A right-hander's hook into the 1-3 pocket, about 6.3 cm right of the head pin's
	// centre line (the classic entry), missed by up to 3 cm either way the way a real
	// bowler misses, against dead-straight balls on the nose of the head pin. Measured
	// when this was written: dead in the pocket carries every time, 4-8 cm off it about
	// two times in three, the nose never.
	FLcg Rng{ 20260927u };
	int PocketStrikes = 0;
	int NoseStrikes = 0;
	int PocketTotal = 0;
	const int Shots = 30;
	for (int I = 0; I < Shots; ++I)
	{
		const double Power = 0.45 + 0.1 * Rng.Next01();
		const double Target = -6.3 + (Rng.Next01() - 0.5) * 6.0;
		const double Angle = AimFor(-25.0, Target, Power, 0.8);
		const int Down = FirstBall(-25.0, Angle, Power, 0.8);
		PocketStrikes += Down == 10 ? 1 : 0;
		PocketTotal += Down;

		const double NoseAngle = AimFor(0.0, (Rng.Next01() - 0.5) * 1.0, Power, 0.0);
		NoseStrikes += FirstBall(0.0, NoseAngle, Power, 0.0) == 10 ? 1 : 0;
	}
	Check(PocketStrikes * 100 >= Shots * 50 && PocketStrikes < Shots, "a hook near the 1-3 pocket strikes most of the time, not always",
		Fmt("%d of %d, %.1f pins a ball", PocketStrikes, Shots, double(PocketTotal) / Shots));
	Check(NoseStrikes < PocketStrikes, "a straight ball on the nose strikes less often than the pocket",
		Fmt("nose %d vs pocket %d", NoseStrikes, PocketStrikes));

	// A thin hit on the 3 pin leaves wood.
	const double Thin = AimFor(-25.0, -24.0, 0.5, 0.3);
	const int ThinDown = FirstBall(-25.0, Thin, 0.5, 0.3);
	Check(ThinDown > 0 && ThinDown < 10, "a thin hit takes some pins and leaves some", Fmt("%d down", ThinDown));
}

static void TestEnergyAndEnding()
{
	FLcg Rng{ 77u };
	int Unended = 0;
	int Escaped = 0;
	double WorstSeconds = 0.0;
	for (int I = 0; I < 150; ++I)
	{
		FBowlingSim Sim;
		Sim.StartGame();
		Sim.TakeEvents();
		Sim.Roll((Rng.Next01() - 0.5) * 90.0, (Rng.Next01() - 0.5) * 0.06, Rng.Next01(), Rng.Next01() * 2.0 - 1.0);
		const double T = RunRoll(Sim);
		if (T < 0.0)
		{
			++Unended;
			continue;
		}
		WorstSeconds = std::fmax(WorstSeconds, T);
		for (int P = 0; P < FBowlingSim::PinCount; ++P)
		{
			if (Sim.PinState(P) == EPinState::Standing)
			{
				const FBowlingVec2 D = Sim.PinPosition(P) - Sim.Layout().PinSpots[P];
				Escaped += (D.X * D.X + D.Y * D.Y) > 1e-12 ? 1 : 0;
			}
		}
	}
	Check(Unended == 0, "every one of 150 random balls finishes its roll", Fmt("worst %.1f s", WorstSeconds));
	Check(WorstSeconds < 12.0, "and the pinsetter is never kept waiting long", Fmt("%.1f s", WorstSeconds));
	Check(Escaped == 0, "a pin left standing is exactly on its spot");

	// Collisions never create energy (ball plus pins, translation only; the ball's spin is
	// untouched by a hit, and friction only takes).
	FBowlingSim Sim;
	Sim.StartGame();
	Sim.Roll(-25.0, AimFor(-25.0, -6.3, 0.5, 0.8), 0.5, 0.8);
	double Worst = 0.0;
	auto Energy = [&Sim]()
	{
		double E = 0.0;
		if (Sim.IsBallOnLane())
		{
			const FBowlingVec2 V = Sim.BallVelocity();
			E += 0.5 * FBowlingSim::BallMass * (V.X * V.X + V.Y * V.Y);
		}
		for (int P = 0; P < FBowlingSim::PinCount; ++P)
		{
			const FBowlingVec2 V = Sim.PinVelocity(P);
			E += 0.5 * FBowlingSim::PinMass * (V.X * V.X + V.Y * V.Y);
		}
		return E;
	};
	for (int I = 0; I < 40000 && Sim.Phase() == EBowlingPhase::Rolling; ++I)
	{
		const double Before = Energy();
		Sim.Substep();
		Worst = std::fmax(Worst, Energy() - Before);
	}
	// The ball's spin feeds its speed on the dry end (that is the hook), so allow the
	// rolling ball's own gain; a collision's gain would be far larger than this.
	Check(Worst < 0.5 * FBowlingSim::BallMass * 2.0 * 2.0 * 1000.0, "no hit creates energy", Fmt("worst rise %.3g", Worst));
}

static void TestFrames()
{
	// Scripted pins: a strike resets the rack; an open first ball leaves the leave in place.
	FBowlingSim Sim;
	Sim.StartGame();
	const double Pocket = AimFor(-25.0, -6.3, 0.5, 0.8);
	Sim.Roll(-25.0, Pocket, 0.5, 0.8);
	std::vector<FBowlingEvent> Events;
	RunRoll(Sim, &Events);
	const int First = Sim.Rolls()[0];
	if (First == 10)
	{
		Check(Sim.Frame() == 1 && Sim.PinsStanding() == 10 && CountEvents(Events, EBowlingEvent::Strike) == 1,
			"a strike ends the frame and brings a fresh rack");
	}
	else
	{
		Check(Sim.Frame() == 0 && Sim.BallInFrame() == 1 && Sim.PinsStanding() == 10 - First,
			"an open first ball leaves its leave standing for the second", Fmt("%d down", First));
	}

	// Ten frames always end the game, with 11 to 21 balls.
	FLcg Rng{ 5u };
	int Ended = 0;
	int MinBalls = 99;
	int MaxBalls = 0;
	int Best = 0;
	for (int G = 0; G < 15; ++G)
	{
		FBowlingSim Game;
		Game.StartGame();
		for (int Ball = 0; Ball < 30 && Game.Phase() == EBowlingPhase::Ready; ++Ball)
		{
			Game.Roll(-25.0 + (Rng.Next01() - 0.5) * 10.0, Pocket + (Rng.Next01() - 0.5) * 0.01, 0.5, 0.8);
			if (RunRoll(Game) < 0.0)
			{
				break;
			}
		}
		if (Game.Phase() == EBowlingPhase::GameOver)
		{
			++Ended;
			const int Balls = static_cast<int>(Game.Rolls().size());
			MinBalls = Balls < MinBalls ? Balls : MinBalls;
			MaxBalls = Balls > MaxBalls ? Balls : MaxBalls;
			Best = Game.Score() > Best ? Game.Score() : Best;
		}
	}
	Check(Ended == 15 && MinBalls >= 11 && MaxBalls <= 21, "fifteen whole games all end in ten frames",
		Fmt("%d ended, %d-%d balls, best %d", Ended, MinBalls, MaxBalls, Best));
}

static void TestDeterminism()
{
	auto Play = []()
	{
		FBowlingSim Sim;
		Sim.StartGame();
		Sim.Roll(-20.0, 0.004, 0.6, 0.7);
		RunRoll(Sim);
		double Sum = 0.0;
		for (int P = 0; P < 10; ++P)
		{
			Sum += Sim.PinPosition(P).X * (P + 1) + Sim.PinPosition(P).Y * (P + 11);
		}
		return Sum + Sim.Rolls()[0];
	};
	Check(Play() == Play(), "the same ball gives the same pinfall, to the last bit");
}

int main()
{
	std::printf("Bowling physics - Source/SibeliusGame/BowlingSim\n\n");
	TestScoring();
	TestRack();
	TestStraightAndHook();
	TestGutter();
	TestPocketVersusNose();
	TestEnergyAndEnding();
	TestFrames();
	TestDeterminism();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
