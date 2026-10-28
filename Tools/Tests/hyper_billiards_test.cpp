// hyper_billiards_test.cpp — standalone tests for Source/SibeliusGame/HyperBilliardsSim, the 4D
// pool table in Aisling's snowfield.
//
// No engine, no editor, no cook: this builds with the Visual Studio compiler alone and runs in a
// few seconds. Run it with Tools/Tests/run_hyper_billiards_tests.ps1.
//
// Each test is a PHYSICAL CLAIM, as at the living-room table (billiards_sim_test.cpp): a
// centre-struck 4D ball settles to rolling at three quarters of its speed, a stun shot stops
// dead, a collision never adds energy, no ball leaves the box but down a pocket - and the
// four-dimensional ones: a ball two radii off in W lets the cue ball through where it seems to
// stand, and a glancing touch in W knocks a ball into W.

#include "HyperBilliardsSim.h"

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

	double Dot(FHyperVec3 A, FHyperVec3 B) { return A.X * B.X + A.Y * B.Y + A.W * B.W; }
	double Len(FHyperVec3 V) { return std::sqrt(Dot(V, V)); }

	struct FLcg
	{
		uint32_t State;
		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) * (1.0 / 16777216.0);
		}
	};

	// Runs until the shot ends (or Seconds run out), returning whether it ended.
	bool RunShot(FHyperBilliardsSim& Sim, double Seconds = 70.0)
	{
		for (double T = 0.0; T < Seconds; T += FHyperBilliardsSim::SubstepSeconds)
		{
			Sim.Substep();
			if (Sim.Phase() != EHyperPhase::Rolling)
			{
				return true;
			}
		}
		return false;
	}

	bool HasEvent(const std::vector<FHyperEvent>& Events, EHyperEvent Type)
	{
		for (const FHyperEvent& E : Events)
		{
			if (E.Type == Type)
			{
				return true;
			}
		}
		return false;
	}

	// Aim straight at a ball's centre: the heading in X-Y and the tilt into W.
	void AimAt(FHyperVec3 From, FHyperVec3 To, double& Heading, double& Tilt)
	{
		const FHyperVec3 D = To - From;
		Heading = std::atan2(D.Y, D.X);
		Tilt = std::atan2(D.W, std::sqrt(D.X * D.X + D.Y * D.Y));
	}
}

static void TestRack()
{
	FHyperBilliardsSim Sim;
	Sim.StartGame();
	const FHyperTableLayout& L = Sim.Layout();
	const double R = L.BallRadius;

	bool bAllOn = true;
	double MinGap = 1e9;
	bool bInBox = true;
	double MaxAbsW = 0.0;
	for (int I = 0; I < FHyperBilliardsSim::BallCount; ++I)
	{
		bAllOn = bAllOn && Sim.IsOnTable(I);
		const FHyperVec3 P = Sim.BallPosition(I);
		bInBox = bInBox && std::fabs(P.X) <= L.HalfLength - R && std::fabs(P.Y) <= L.HalfWidth - R && std::fabs(P.W) <= L.HalfDepth - R;
		MaxAbsW = std::fmax(MaxAbsW, std::fabs(P.W));
		for (int J = I + 1; J < FHyperBilliardsSim::BallCount; ++J)
		{
			MinGap = std::fmin(MinGap, Len(P - Sim.BallPosition(J)) - 2.0 * R);
		}
	}
	Check(bAllOn && Sim.BallsLeft() == 10, "the rack: ten object balls and the cue ball, all on the table");
	Check(MinGap >= 0.0 && MinGap < 0.05, "no two racked balls overlap, and the closest all but touch", Fmt("closest gap %.4f cm", MinGap));
	Check(bInBox, "every racked ball sits inside the cushions");
	Check(MaxAbsW > R, "the rack really is four-dimensional: it spreads into W", Fmt("max |W| %.2f cm", MaxAbsW));

	// Every ball beyond the apex rests on three of the layer before (a tetrahedron's pockets),
	// except the apex layer's single ball, which rests on none.
	int Supported = 0;
	for (int I = 2; I <= 10; ++I)
	{
		int Touching = 0;
		for (int J = 1; J <= 10; ++J)
		{
			if (J != I && Sim.BallPosition(J).X < Sim.BallPosition(I).X - 1.0 && Len(Sim.BallPosition(I) - Sim.BallPosition(J)) < 2.0 * R + 0.1)
			{
				++Touching;
			}
		}
		Supported += Touching >= 1 ? 1 : 0;
	}
	Check(Supported == 9, "each ball behind the apex nests against the layer in front of it", Fmt("%d of 9", Supported));
}

static void TestSlice()
{
	const double R = 5.0;
	Check(std::fabs(FHyperBilliardsSim::SliceRadius(R, 0.0, 0.0) - R) < 1e-12, "a slice through a ball's centre shows the whole ball");
	Check(std::fabs(FHyperBilliardsSim::SliceRadius(R, 3.0, 0.0) - 4.0) < 1e-12, "a slice 3 off a 5 cm ball shows a 4 cm one (3-4-5)");
	Check(FHyperBilliardsSim::SliceRadius(R, 5.0, 0.0) == 0.0 && FHyperBilliardsSim::SliceRadius(R, -7.0, 0.0) == 0.0,
		"a slice a radius or more away shows nothing");
}

static void TestThreeQuarters()
{
	FHyperBilliardsSim Sim;
	Sim.StartGame();
	for (int I = 1; I <= 10; ++I)
	{
		Sim.RemoveBall(I);
	}
	Sim.Shoot(0.3, 0.2, 0.2, 0.0);   // a centre hit, partly into W, settling well short of a cushion
	const double V0 = Len(Sim.BallVelocity(0));
	bool bRolled = false;
	for (int Step = 0; Step < 20000 && !bRolled; ++Step)
	{
		Sim.Substep();
		bRolled = !Sim.IsSliding(0);
	}
	const double Ratio = Len(Sim.BallVelocity(0)) / V0;
	Check(bRolled && std::fabs(Ratio - 0.75) < 0.01, "a centre-struck 4D ball rolls on at three quarters of its speed (not 5/7)",
		Fmt("%.4f", Ratio));

	FHyperBilliardsSim Natural;
	Natural.StartGame();
	for (int I = 1; I <= 10; ++I)
	{
		Natural.RemoveBall(I);
	}
	Natural.Shoot(0.0, 0.0, 0.5, 2.0 / 3.0);
	Natural.Substep();
	Check(!Natural.IsSliding(0), "spin 2/3 is natural roll: the ball rolls from the first instant");
}

// The cue ball Gap short of touching an object ball on the same line, both in the slice at W = 0.
static FHyperBilliardsSim TwoBallLine(double Spin, double Power, FHyperVec3 ObjectOffset = FHyperVec3(), double Gap = 30.0)
{
	FHyperBilliardsSim Sim;
	Sim.ClearTable();
	Sim.PlaceBall(0, FHyperVec3(-(2.0 * Sim.Layout().BallRadius + Gap), 0.0, 0.0), FHyperVec3(), false);
	Sim.PlaceBall(1, FHyperVec3(0.0, 0.0, 0.0) + ObjectOffset, FHyperVec3(), false);
	Sim.Shoot(0.0, 0.0, Power, Spin);
	return Sim;
}

// Runs to the first ball-on-ball contact; returns false if there is none in Seconds.
static bool RunToContact(FHyperBilliardsSim& Sim, double Seconds = 3.0)
{
	for (double T = 0.0; T < Seconds; T += FHyperBilliardsSim::SubstepSeconds)
	{
		Sim.Substep();
		if (HasEvent(Sim.TakeEvents(), EHyperEvent::BallHitBall))
		{
			return true;
		}
	}
	return false;
}

// The cue ball's story after contact, with the object ball lifted off the table the moment it is
// struck (or a hard one comes back off the far cushion into it), at the living-room tests' gentle
// powers: the cloth's rolling resistance is low, and a hard shot rolls metres before it rests.
// The contact is at X = -2R.
static void TestStunDrawFollow()
{
	const double TwoR = 2.0 * FHyperTableLayout::MakeDefault().BallRadius;
	{
		// From close range the cue ball arrives still sliding, without roll, and stops dead.
		// Not quite: restitution 0.93 leaves it 3.5% of its speed, as real balls do.
		FHyperBilliardsSim Sim = TwoBallLine(0.0, 0.2, FHyperVec3(), 6.0);
		const bool bHit = RunToContact(Sim);
		Sim.RemoveBall(1);
		RunShot(Sim);
		const double Rest = Sim.BallPosition(0).X + TwoR;
		Check(bHit && std::fabs(Rest) < TwoR * 0.5, "a stun shot stops within half a ball of the contact",
			Fmt("cue ball rests %.2f cm from contact", Rest));
	}
	{
		FHyperBilliardsSim Sim = TwoBallLine(-1.0, 0.2, FHyperVec3(), 40.0);
		RunToContact(Sim);
		Sim.RemoveBall(1);
		RunShot(Sim);
		const double Rest = Sim.BallPosition(0).X + TwoR;
		Check(Rest < -10.0, "a draw shot comes back", Fmt("cue ball rests %.1f cm behind contact", -Rest));
	}
	{
		FHyperBilliardsSim Sim = TwoBallLine(1.0, 0.1, FHyperVec3(), 40.0);
		RunToContact(Sim);
		Sim.RemoveBall(1);
		RunShot(Sim);
		const double Rest = Sim.BallPosition(0).X + TwoR;
		Check(Rest > 10.0, "a follow shot follows the object ball", Fmt("cue ball rests %.1f cm past contact", Rest));
	}
}

static void TestEnergy()
{
	FLcg Rng{ 12345u };
	double WorstGain = 0.0;
	for (int Trial = 0; Trial < 200; ++Trial)
	{
		FHyperBilliardsSim Sim;
		Sim.ClearTable();
		const FHyperVec3 Va((Rng.Next01() - 0.5) * 600.0, (Rng.Next01() - 0.5) * 600.0, (Rng.Next01() - 0.5) * 600.0);
		const FHyperVec3 Off((Rng.Next01() - 0.5) * 6.0, (Rng.Next01() - 0.5) * 6.0, (Rng.Next01() - 0.5) * 6.0);
		// Ball 1 a hair outside contact along Va's line, so the collision comes at once.
		const FHyperVec3 Dir = Va * (1.0 / Len(Va));
		Sim.PlaceBall(0, FHyperVec3(), Va, true);
		Sim.PlaceBall(1, Dir * 10.2 + Off, FHyperVec3(), false);
		const double Before = Dot(Va, Va);
		for (int S = 0; S < 30; ++S)
		{
			Sim.Substep();
		}
		const FHyperVec3 A = Sim.BallVelocity(0);
		const FHyperVec3 B = Sim.BallVelocity(1);
		WorstGain = std::fmax(WorstGain, (Dot(A, A) + Dot(B, B)) - Before);
	}
	Check(WorstGain <= 1e-6, "a collision never adds energy, in any direction of the 4D cloth", Fmt("worst gain %.3g", WorstGain));
}

static void TestCushions()
{
	FHyperBilliardsSim Sim;
	Sim.ClearTable();
	const FHyperTableLayout& L = Sim.Layout();
	Sim.PlaceBall(0, FHyperVec3(0.0, 0.0, 0.0), FHyperVec3(0.0, 0.0, 300.0), true);   // straight into W
	double MaxW = 0.0;
	bool bBounced = false;
	for (int S = 0; S < 2400; ++S)
	{
		Sim.Substep();
		MaxW = std::fmax(MaxW, Sim.BallPosition(0).W);
		bBounced = bBounced || Sim.BallVelocity(0).W < 0.0;
	}
	Check(bBounced && MaxW <= L.HalfDepth - L.BallRadius + 1e-9, "a ball rolled into W bounces off the W cushion and never passes it",
		Fmt("deepest W %.3f of %.3f", MaxW, L.HalfDepth - L.BallRadius));
}

static void TestPockets()
{
	{
		// Down the box's diagonal toward the (+X, +Y, +W) corner: pocket 7 in MakeDefault's order.
		FHyperBilliardsSim Sim;
		Sim.ClearTable();
		const FHyperTableLayout& L = Sim.Layout();
		const FHyperVec3 Corner(L.HalfLength, L.HalfWidth, L.HalfDepth);
		const FHyperVec3 Start(L.HalfLength - 40.0, L.HalfWidth - 25.0, L.HalfDepth - 12.0);
		const FHyperVec3 D = (Corner - Start) * (1.0 / Len(Corner - Start));
		Sim.PlaceBall(1, Start, D * 250.0, true);
		RunShot(Sim);
		int Pocket = -1;
		for (const FHyperEvent& E : Sim.TakeEvents())
		{
			Pocket = E.Type == EHyperEvent::BallPocketed ? E.Other : Pocket;
		}
		Check(!Sim.IsOnTable(1) && Pocket == 7, "a ball rolled at a corner falls into that corner", Fmt("pocket %d", Pocket));
	}
	{
		// Along the long edge at the (+Y, +W) corner of the cross-section: past the middle pocket.
		FHyperBilliardsSim Sim;
		Sim.ClearTable();
		const FHyperTableLayout& L = Sim.Layout();
		const double R = L.BallRadius;
		Sim.PlaceBall(1, FHyperVec3(-60.0, L.HalfWidth - R, L.HalfDepth - R), FHyperVec3(200.0, 0.0, 0.0), true);
		RunShot(Sim);
		int Pocket = -1;
		for (const FHyperEvent& E : Sim.TakeEvents())
		{
			Pocket = E.Type == EHyperEvent::BallPocketed ? E.Other : Pocket;
		}
		Check(!Sim.IsOnTable(1) && Pocket == 11, "a ball run along a long edge drops into its middle pocket", Fmt("pocket %d", Pocket));
	}
	{
		// The middle of a long FACE is not a pocket: one dimension up, the side pockets are on
		// the edges, where the faces meet.
		FHyperBilliardsSim Sim;
		Sim.ClearTable();
		const FHyperTableLayout& L = Sim.Layout();
		Sim.PlaceBall(1, FHyperVec3(0.0, 0.0, 0.0), FHyperVec3(0.0, 200.0, 0.0), true);
		RunShot(Sim);
		Check(Sim.IsOnTable(1) && Sim.BallPosition(1).Y < L.HalfWidth, "a ball sent at the middle of a long face just bounces back");
	}
}

static void TestFourthDimension()
{
	{
		// The object ball 2.5 radii off in W: in the cue ball's slice it is not there at all,
		// and the cue ball runs straight through where its X and Y say it stands.
		FHyperBilliardsSim Sim = TwoBallLine(0.0, 0.4, FHyperVec3(0.0, 0.0, 12.5));
		const double Seen = FHyperBilliardsSim::SliceRadius(Sim.Layout().BallRadius, Sim.BallPosition(1).W, Sim.BallPosition(0).W);
		bool bTouched = false;
		double FurthestX = -1e9;
		for (double T = 0.0; T < 70.0 && Sim.Phase() == EHyperPhase::Rolling; T += FHyperBilliardsSim::SubstepSeconds)
		{
			Sim.Substep();
			bTouched = bTouched || HasEvent(Sim.TakeEvents(), EHyperEvent::BallHitBall);
			FurthestX = std::fmax(FurthestX, Sim.BallPosition(0).X);
		}
		Check(Seen == 0.0 && !bTouched && FurthestX > 20.0 && Len(Sim.BallPosition(1) - FHyperVec3(0.0, 0.0, 12.5)) < 1e-9,
			"a ball 2.5 radii away in W is invisible from the cue ball's slice, and the cue ball passes it untouched",
			Fmt("the cue ball reached X %.1f", FurthestX));
	}
	{
		// Half a radius off in W: a glancing hit, and the object ball goes partly into W.
		FHyperBilliardsSim Sim = TwoBallLine(0.0, 0.4, FHyperVec3(0.0, 0.0, 5.0));
		RunToContact(Sim);
		const FHyperVec3 V = Sim.BallVelocity(1);
		Check(V.X > 0.0 && V.W > 0.0 && V.W / Len(V) > 0.4, "a glancing touch in W knocks the object ball into W",
			Fmt("its velocity (%.1f, %.1f, %.1f)", V.X, V.Y, V.W));
	}
}

static void TestPrediction()
{
	FHyperBilliardsSim Sim;
	Sim.ClearTable();
	Sim.PlaceBall(0, FHyperVec3(-50.0, 0.0, 0.0), FHyperVec3(), false);
	Sim.PlaceBall(1, FHyperVec3(10.0, 6.0, 4.0), FHyperVec3(), false);
	Sim.PlaceBall(2, FHyperVec3(40.0, -30.0, -10.0), FHyperVec3(), false);
	const double Heading = 0.04;
	const double Tilt = 0.03;
	const FHyperAimPrediction P = Sim.PredictShot(Heading, Tilt);
	Sim.Shoot(Heading, Tilt, 0.6, 0.0);
	FHyperVec3 FirstV;
	for (int S = 0; S < 2400 && Len(FirstV) == 0.0; ++S)
	{
		Sim.Substep();
		FirstV = Sim.BallVelocity(1);
	}
	const double Agree = Len(FirstV) > 0.0 ? Dot(FirstV * (1.0 / Len(FirstV)), P.ObjectDirection) : 0.0;
	Check(P.bHitsBall && P.Ball == 1 && Agree > 0.995, "the aim prediction names the ball the shot hits, and where it goes",
		Fmt("ball %d, agreement %.4f", P.Ball, Agree));
}

static void TestScratch()
{
	FHyperBilliardsSim Sim;
	Sim.ClearTable();
	const FHyperTableLayout& L = Sim.Layout();
	Sim.PlaceBall(5, FHyperVec3(0.0, 0.0, 0.0), FHyperVec3(), false);
	Sim.PlaceBall(0, FHyperVec3(L.HalfLength - 30.0, L.HalfWidth - 20.0, L.HalfDepth - 15.0), FHyperVec3(), false);
	double Heading = 0.0;
	double Tilt = 0.0;
	AimAt(Sim.BallPosition(0), FHyperVec3(L.HalfLength, L.HalfWidth, L.HalfDepth), Heading, Tilt);
	Sim.Shoot(Heading, Tilt, 0.3, 0.0);
	RunShot(Sim);
	const std::vector<FHyperEvent> Events = Sim.TakeEvents();
	Check(HasEvent(Events, EHyperEvent::CueBallScratched) && HasEvent(Events, EHyperEvent::CueBallRespotted) && Sim.Penalties() == 1
			&& Sim.IsOnTable(0) && Len(Sim.BallPosition(0) - L.HeadSpot) < 1e-9 && Sim.Strokes() == 2,
		"a scratch costs a penalty stroke, and the cue ball comes back to the head spot");
}

// A whole game, aimed at a random ball each stroke: it always ends, nothing ever leaves the box
// but down a pocket, and the same strokes always give the same game.
static void TestWholeGames()
{
	for (uint32_t Seed : { 1u, 7u, 2026u })
	{
		FHyperBilliardsSim Sim;
		Sim.StartGame();
		FLcg Rng{ Seed };
		const FHyperTableLayout& L = Sim.Layout();
		const double R = L.BallRadius;
		bool bContained = true;
		bool bEveryShotEnded = true;
		int Guard = 0;
		while (Sim.Phase() != EHyperPhase::Cleared && Guard++ < 3000)
		{
			int Target = 1 + static_cast<int>(Rng.Next01() * 10.0) % 10;
			while (!Sim.IsOnTable(Target))
			{
				Target = 1 + Target % 10;
			}
			double Heading = 0.0;
			double Tilt = 0.0;
			AimAt(Sim.BallPosition(0), Sim.BallPosition(Target), Heading, Tilt);
			Heading += (Rng.Next01() - 0.5) * 0.08;
			Tilt += (Rng.Next01() - 0.5) * 0.08;
			Sim.Shoot(Heading, Tilt, 0.25 + Rng.Next01() * 0.75, (Rng.Next01() - 0.5) * 2.0);
			bool bEnded = false;
			for (double T = 0.0; T < 70.0 && !bEnded; T += FHyperBilliardsSim::SubstepSeconds)
			{
				Sim.Substep();
				bEnded = Sim.Phase() != EHyperPhase::Rolling;
				for (int I = 0; I < FHyperBilliardsSim::BallCount && bContained; ++I)
				{
					if (Sim.IsOnTable(I))
					{
						const FHyperVec3 P = Sim.BallPosition(I);
						bContained = std::fabs(P.X) <= L.HalfLength - R + 1e-6 && std::fabs(P.Y) <= L.HalfWidth - R + 1e-6
							&& std::fabs(P.W) <= L.HalfDepth - R + 1e-6;
					}
				}
			}
			bEveryShotEnded = bEveryShotEnded && bEnded;
			Sim.TakeEvents();
		}
		Check(Sim.Phase() == EHyperPhase::Cleared && bContained && bEveryShotEnded,
			"a whole game ends, every stroke stops, and no ball ever leaves the box but down a pocket",
			Fmt("seed %u: %d strokes (%d penalties)", Seed, Sim.Strokes(), Sim.Penalties()));
	}

	FHyperBilliardsSim A;
	FHyperBilliardsSim B;
	A.StartGame();
	B.StartGame();
	A.Shoot(0.01, -0.02, 1.0, 0.3);
	B.Shoot(0.01, -0.02, 1.0, 0.3);
	RunShot(A);
	RunShot(B);
	bool bSame = true;
	for (int I = 0; I < FHyperBilliardsSim::BallCount; ++I)
	{
		bSame = bSame && Len(A.BallPosition(I) - B.BallPosition(I)) == 0.0 && A.IsOnTable(I) == B.IsOnTable(I);
	}
	Check(bSame, "the same break always gives the same table");
}


static void TestPracticeShot()
{
    FHyperBilliardsSim Sim;
    Sim.StartGame();
    for (int I = 2; I < FHyperBilliardsSim::BallCount; ++I) Sim.RemoveBall(I);
    const auto& L = Sim.Layout();
    Sim.PlaceBall(0, FHyperVec3(0, -25, L.HalfDepth - L.BallRadius), FHyperVec3(), true);
    Sim.PlaceBall(1, FHyperVec3(0, L.HalfWidth - 30, L.HalfDepth - L.BallRadius), FHyperVec3(), true);
    Sim.Shoot(Pi / 2, 0, 0.25, 0);
    RunShot(Sim);
    Check(Sim.Phase() == EHyperPhase::Cleared, "the introductory single-ball shot clears at the suggested power");
}


static void TestLessons()
{
    for (int Lesson = 0; Lesson < 3; ++Lesson)
    {
        FHyperBilliardsSim Sim;
        Sim.StartLesson(Lesson);
        const auto D = Sim.BallPosition(1) - Sim.BallPosition(0);
        const double Heading = std::atan2(D.Y, D.X);
        const double Tilt = std::atan2(D.W, std::sqrt(D.X*D.X + D.Y*D.Y));
        Sim.TakeEvents();
        Sim.Shoot(Heading, Tilt, Lesson == 0 ? 0.25 : Lesson == 1 ? 0.35 : 0.7, 0);
        RunShot(Sim);
        bool Banked = false;
        for (const auto& E : Sim.TakeEvents()) Banked = Banked || (E.Type == EHyperEvent::BallHitCushion && E.Index == 1);
        Check(Sim.Phase() == EHyperPhase::Cleared, "lesson opening is completable", Fmt("lesson %d, ball end %.1f %.1f %.1f", Lesson + 1, Sim.BallPosition(1).X, Sim.BallPosition(1).Y, Sim.BallPosition(1).W));
        if (Lesson == 2) Check(Banked, "bank lesson object ball uses a cushion");
    }
}

int main()
{
	TestRack();
	TestSlice();
	TestThreeQuarters();
	TestStunDrawFollow();
	TestEnergy();
	TestCushions();
	TestPockets();
	TestFourthDimension();
	TestPrediction();
	TestScratch();
	TestWholeGames();
	TestPracticeShot();
	TestLessons();
	(void)Pi;
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
