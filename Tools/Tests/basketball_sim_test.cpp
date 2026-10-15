// basketball_sim_test.cpp - standalone tests for Source/SibeliusGame/BasketballSim.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_basketball_sim_tests.ps1.
//
// Each test is a PHYSICAL or a RULES claim: the racks stand on the three-point arc; a solved
// arc really comes down through its target; a ball dead on the middle swishes and one a metre
// off misses; the rattles go both ways; a perfect shooter scores 30; the money ball is worth
// two; nothing counts after the buzzer; a better shooter scores more over many rounds; early
// is short and late is long; the same seed plays the same round.

#include "BasketballSim.h"

#include <cmath>
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
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	double Planar(const FHoopVec3& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }

	// One whole agent round, stepped like a game at 60 frames a second.
	FBasketballContest PlayAgent(const FShooterStyle& Style, uint64_t Seed)
	{
		FBasketballContest Contest;
		Contest.Begin(Style, /*bPlayer=*/false, Seed);
		std::vector<FShot> Shots;
		for (int Frame = 0; Frame < 60 * 90 && !Contest.IsFinished(); ++Frame)
		{
			Contest.Advance(1.0 / 60.0, Shots);
		}
		return Contest;
	}

	// A player round that releases every ball the moment it is ready, at one timing.
	FBasketballContest PlayPlayer(double Timing, uint64_t Seed)
	{
		FShooterStyle Leonard;
		Leonard.Name = "Leonard";
		FBasketballContest Contest;
		Contest.Begin(Leonard, /*bPlayer=*/true, Seed);
		std::vector<FShot> Ignored;
		for (int Frame = 0; Frame < 60 * 90 && !Contest.IsFinished(); ++Frame)
		{
			FShot Shot;
			Contest.ShootPlayer(Timing, Shot);
			Contest.Advance(1.0 / 60.0, Ignored);
		}
		return Contest;
	}

	void TestRacksOnTheArc()
	{
		bool bOk = true;
		std::string Detail;
		for (int Rack = 0; Rack < FBasketballContest::Racks; ++Rack)
		{
			const FHoopVec3 Spot = FBasketballContest::RackSpot(Rack);
			const bool bCorner = Rack == 0 || Rack == FBasketballContest::Racks - 1;
			const double Line = bCorner ? BasketballCourt::CornerDistance : BasketballCourt::ArcDistance;
			const double Behind = Planar(Spot) - Line;
			if (Behind < 0.0 || Behind > 40.0 || Spot.X < 0.0)
			{
				bOk = false;
				Detail += Fmt("rack %d is %.1f cm behind its line at x %.0f; ", Rack, Behind, Spot.X);
			}
		}
		const FHoopVec3 Right = FBasketballContest::RackSpot(0);
		const FHoopVec3 Left = FBasketballContest::RackSpot(4);
		bOk = bOk && Right.Y < 0.0 && Left.Y > 0.0 && std::fabs(Right.Y + Left.Y) < 1e-6;
		Check(bOk, "the five racks stand just behind the three-point line, right corner to left", Detail);

		const FHoopVec3 Corner = FBasketballContest::RackSpot(0);
		Check(Corner.X < BasketballCourt::RimToBaseline + 100.0 && Corner.X > -BasketballCourt::RimToBaseline,
			"the corner racks are in the corners, in front of the baseline", Fmt("x %.0f", Corner.X));
	}

	void TestLaunchHitsTarget()
	{
		bool bOk = true;
		std::string Detail;
		for (int Rack = 0; Rack < FBasketballContest::Racks; ++Rack)
		{
			for (double Arc : { 45.0, 50.0, 55.0 })
			{
				const FHoopVec3 Spot = FBasketballContest::RackSpot(Rack);
				const FHoopVec3 Release(Spot.X, Spot.Y, 235.0);
				const FHoopVec3 Target(3.0, -5.0, BasketballCourt::RimHeight);
				FHoopVec3 Velocity;
				double Flight = 0.0;
				if (!SolveLaunch(Release, Target, Arc, Velocity, Flight))
				{
					bOk = false;
					Detail += Fmt("no arc from rack %d at %.0f deg; ", Rack, Arc);
					continue;
				}
				const FHoopVec3 At = BallAt(Release, Velocity, Flight);
				const double Miss = std::sqrt((At.X - Target.X) * (At.X - Target.X) + (At.Y - Target.Y) * (At.Y - Target.Y)
					+ (At.Z - Target.Z) * (At.Z - Target.Z));
				// Coming DOWN through the target, not up through it.
				const double VzAtTarget = Velocity.Z - BasketballCourt::Gravity * Flight;
				if (Miss > 0.01 || VzAtTarget >= 0.0 || Flight < 0.6 || Flight > 2.0)
				{
					bOk = false;
					Detail += Fmt("rack %d %.0f deg: off %.3f cm, vz %.0f, %.2f s; ", Rack, Arc, Miss, VzAtTarget, Flight);
				}
			}
		}
		Check(bOk, "a solved arc comes down through its target from every rack, in a believable time", Detail);

		FHoopVec3 Velocity;
		double Flight = 0.0;
		const bool bImpossible = SolveLaunch(FHoopVec3(700.0, 0.0, 235.0), FHoopVec3(0.0, 0.0, 2000.0), 30.0, Velocity, Flight);
		Check(!bImpossible, "a target above the arc's reach has no solution");
	}

	void TestResolve()
	{
		Check(ResolveShot(0.0, 0.0, 0.99) == EShotResult::Swish, "dead centre is a swish, whatever the roll");
		Check(ResolveShot(BasketballCourt::CleanRadius - 0.1, 0.0, 0.99) == EShotResult::Swish,
			"just inside the clean window still swishes");
		Check(ResolveShot(-100.0, 5.0, 0.0) == EShotResult::Short, "a metre short misses short");
		Check(ResolveShot(100.0, 5.0, 0.0) == EShotResult::Long, "a metre long misses long");
		Check(ResolveShot(5.0, 80.0, 0.0) == EShotResult::Wide, "80 cm to the side misses wide");

		const double Band = 0.5 * (BasketballCourt::CleanRadius + BasketballCourt::ContactRadius);
		Check(ResolveShot(Band, 0.0, 0.01) == EShotResult::RimIn && ResolveShot(Band, 0.0, 0.99) == EShotResult::RimOut,
			"on the iron the roll decides: in one way, out the other");
		Check(ResolveShot(BasketballCourt::ContactRadius - 0.01, 0.0, 0.01) == EShotResult::RimOut,
			"at the far edge of the iron it all but never drops");
		Check(IsMade(EShotResult::Swish) && IsMade(EShotResult::RimIn) && !IsMade(EShotResult::RimOut)
			&& !IsMade(EShotResult::Short), "only swishes and rim-ins count");
	}

	void TestPerfectShooter()
	{
		FShooterStyle Machine;
		Machine.Name = "machine";
		Machine.Spread = 0.0;
		Machine.ShotSeconds = 1.0;
		Machine.MoveSeconds = 1.0;
		const FBasketballContest Round = PlayAgent(Machine, 7);
		Check(Round.GetScore() == 30 && Round.GetBallsTaken() == 25 && Round.IsFinished(),
			"a shooter who never misses and is quick enough scores 30 from 25 balls",
			Fmt("%d from %d", Round.GetScore(), Round.GetBallsTaken()));

		int MoneyBalls = 0;
		int MoneyPoints = 0;
		for (const FShot& Shot : Round.GetShots())
		{
			if (Shot.bMoney)
			{
				++MoneyBalls;
				MoneyPoints += Shot.Points;
			}
		}
		Check(MoneyBalls == 5 && MoneyPoints == 10, "the fifth ball of each rack is the money ball, worth two",
			Fmt("%d money balls worth %d", MoneyBalls, MoneyPoints));
	}

	void TestBuzzer()
	{
		FShooterStyle Slow;
		Slow.Name = "slow";
		Slow.Spread = 0.0;
		Slow.ShotSeconds = 3.0;
		Slow.MoveSeconds = 3.0;
		const FBasketballContest Round = PlayAgent(Slow, 3);
		bool bInTime = true;
		for (const FShot& Shot : Round.GetShots())
		{
			bInTime = bInTime && Shot.Clock <= FBasketballContest::TimeLimit;
		}
		Check(Round.IsFinished() && Round.GetBallsTaken() < 25 && bInTime,
			"a slow shooter runs out of time, and no ball after the buzzer counts",
			Fmt("%d balls by %.1f s", Round.GetBallsTaken(), Round.GetClock()));
		// Balls at 3..15 s, a move, 21..33, 39..51, then 57 and 60 - the one on the buzzer counts.
		Check(Round.GetBallsTaken() == 17, "three seconds a ball and three to move gets exactly 17 balls away",
			Fmt("%d", Round.GetBallsTaken()));
	}

	void TestAgentsAreBelievable()
	{
		const std::vector<FShooterStyle> Styles = FBasketballContest::AgentStyles();
		Check(Styles.size() == 5, "five agents shoot");
		bool bOk = true;
		std::string Detail;
		std::vector<double> Average;
		for (size_t Index = 0; Index < Styles.size(); ++Index)
		{
			int Made = 0;
			int Taken = 0;
			int Points = 0;
			constexpr int Rounds = 400;
			for (int Round = 0; Round < Rounds; ++Round)
			{
				const FBasketballContest Contest = PlayAgent(Styles[Index], 1000 + Round * 7 + Index);
				Taken += Contest.GetBallsTaken();
				Points += Contest.GetScore();
				for (const FShot& Shot : Contest.GetShots())
				{
					Made += IsMade(Shot.Result) ? 1 : 0;
				}
			}
			const double Rate = static_cast<double>(Made) / Taken;
			const double Balls = static_cast<double>(Taken) / Rounds;
			Average.push_back(static_cast<double>(Points) / Rounds);
			Detail += Fmt("%s %.0f%% of %.1f balls, %.1f pts; ", Styles[Index].Name.c_str(), Rate * 100.0, Balls, Average.back());
			bOk = bOk && Rate > 0.25 && Rate < 0.70 && Balls > 17.0;
		}
		Check(bOk, "each agent makes a believable share of her threes and gets most balls away", Detail);
		// Isla (10 cm) is the steadiest hand and Aisling (14.5 cm) the wildest.
		Check(Average[2] > Average[3], "the steadier hand scores more on average", Fmt("Isla %.1f, Aisling %.1f", Average[2], Average[3]));
	}

	void TestPlayerTiming()
	{
		int PerfectPoints = 0;
		int PoorPoints = 0;
		int EarlyShort = 0;
		int EarlyLong = 0;
		int LateLong = 0;
		int LateShort = 0;
		for (int Round = 0; Round < 200; ++Round)
		{
			PerfectPoints += PlayPlayer(0.0, 50 + Round).GetScore();
			PoorPoints += PlayPlayer(0.6, 50 + Round).GetScore();
			// Kept in locals: a range-for over a temporary round's GetShots() would read a
			// round that is already gone.
			const FBasketballContest Early = PlayPlayer(-0.8, 900 + Round);
			const FBasketballContest Late = PlayPlayer(0.8, 900 + Round);
			for (const FShot& Shot : Early.GetShots())
			{
				EarlyShort += Shot.Result == EShotResult::Short ? 1 : 0;
				EarlyLong += Shot.Result == EShotResult::Long ? 1 : 0;
			}
			for (const FShot& Shot : Late.GetShots())
			{
				LateLong += Shot.Result == EShotResult::Long ? 1 : 0;
				LateShort += Shot.Result == EShotResult::Short ? 1 : 0;
			}
		}
		Check(PerfectPoints > 2 * PoorPoints, "releasing on the sweet spot scores far more than releasing late",
			Fmt("perfect %d, poor %d over 200 rounds", PerfectPoints, PoorPoints));
		Check(EarlyShort > 10 * (EarlyLong + 1), "released early, the ball falls short", Fmt("%d short, %d long", EarlyShort, EarlyLong));
		Check(LateLong > 10 * (LateShort + 1), "released late, the ball flies long", Fmt("%d long, %d short", LateLong, LateShort));

		const double PerfectAverage = PerfectPoints / 200.0;
		Check(PerfectAverage > 12.0 && PerfectAverage < 30.0,
			"perfect timing is a good round, not a sure 30", Fmt("%.1f points on average", PerfectAverage));
	}

	void TestPlayerWaitsForTheBall()
	{
		FShooterStyle Leonard;
		Leonard.Name = "Leonard";
		FBasketballContest Contest;
		Contest.Begin(Leonard, /*bPlayer=*/true, 1);
		FShot Shot;
		Check(!Contest.ShootPlayer(0.0, Shot), "no shot before the first ball is picked up");
		std::vector<FShot> Ignored;
		Contest.Advance(Leonard.ShotSeconds + 0.01, Ignored);
		Check(Contest.ShootPlayer(0.0, Shot) && Shot.Rack == 0 && Shot.Ball == 0, "then the first ball goes from the first rack");
		Check(!Contest.ShootPlayer(0.0, Shot), "and the next one is not ready at once");
		Check(Ignored.empty(), "the clock never shoots for the player");
		Contest.Advance(FBasketballContest::TimeLimit, Ignored);
		Check(Contest.IsFinished() && !Contest.ShootPlayer(0.0, Shot), "after the buzzer nothing more is thrown");
	}

	void TestDeterminism()
	{
		const FShooterStyle Kaia = FBasketballContest::AgentStyles()[0];
		const FBasketballContest A = PlayAgent(Kaia, 42);
		const FBasketballContest B = PlayAgent(Kaia, 42);
		bool bSame = A.GetShots().size() == B.GetShots().size() && A.GetScore() == B.GetScore();
		for (size_t Index = 0; bSame && Index < A.GetShots().size(); ++Index)
		{
			bSame = A.GetShots()[Index].ErrorAlong == B.GetShots()[Index].ErrorAlong
				&& A.GetShots()[Index].Result == B.GetShots()[Index].Result;
		}
		Check(bSame, "the same seed plays the same round, shot for shot");
		const FBasketballContest C = PlayAgent(Kaia, 43);
		Check(C.GetShots().size() != A.GetShots().size() || C.GetShots()[0].ErrorAlong != A.GetShots()[0].ErrorAlong,
			"another seed plays another round");
	}

	void TestRanking()
	{
		const std::vector<int> Order = RankScores({ 14, 19, 11, 19, 7, 22 });
		Check(Order == std::vector<int>({ 5, 1, 3, 0, 2, 4 }), "places run best first, ties in shooting order");
	}
}

int main()
{
	TestRacksOnTheArc();
	TestLaunchHitsTarget();
	TestResolve();
	TestPerfectShooter();
	TestBuzzer();
	TestAgentsAreBelievable();
	TestPlayerTiming();
	TestPlayerWaitsForTheBall();
	TestDeterminism();
	TestRanking();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
