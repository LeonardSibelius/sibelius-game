// billiards_sim_test.cpp — standalone tests for Source/SibeliusGame/BilliardsSim.
//
// No engine, no editor, no cook: this builds with the Visual Studio compiler alone and
// runs in a couple of seconds. Run it with Tools/Tests/run_billiards_sim_tests.ps1.
//
// Each test is a PHYSICAL CLAIM, not "it didn't crash": a centre-struck ball ends up
// rolling at five-sevenths of its speed, a stun shot stops dead, a draw shot comes back,
// a collision never adds energy, no ball ever leaves the cloth except down a pocket, and a
// whole game of clear-the-table always ends. If a tuning change breaks one of these, the
// table has stopped behaving like a pool table, whatever it looks like on screen.

#include "BilliardsSim.h"

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

	double Len(FBilliardsVec2 V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
	double Dot(FBilliardsVec2 A, FBilliardsVec2 B) { return A.X * B.X + A.Y * B.Y; }
	double AngleOf(FBilliardsVec2 V) { return std::atan2(V.Y, V.X); }

	struct FLcg
	{
		uint32_t State;
		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) * (1.0 / 16777216.0);
		}
	};

	int CountEvents(const std::vector<FBilliardsEvent>& Events, EBilliardsEvent Type, int Index = -2)
	{
		int Count = 0;
		for (const FBilliardsEvent& Event : Events)
		{
			if (Event.Type == Type && (Index == -2 || Event.Index == Index))
			{
				++Count;
			}
		}
		return Count;
	}

	void Collect(FBilliardsSim& Sim, std::vector<FBilliardsEvent>& Into)
	{
		const std::vector<FBilliardsEvent> Events = Sim.TakeEvents();
		Into.insert(Into.end(), Events.begin(), Events.end());
	}

	void RunSeconds(FBilliardsSim& Sim, double Seconds, std::vector<FBilliardsEvent>* Into = nullptr)
	{
		const int Steps = static_cast<int>(Seconds / FBilliardsSim::SubstepSeconds);
		for (int I = 0; I < Steps; ++I)
		{
			Sim.Substep();
			if (Into)
			{
				Collect(Sim, *Into);
			}
		}
	}

	// Runs a stroke to its end. Returns the simulated seconds it took, or -1 if it never
	// settled inside Limit.
	double RunToRest(FBilliardsSim& Sim, double Limit = 70.0, std::vector<FBilliardsEvent>* Into = nullptr)
	{
		double T = 0.0;
		while (Sim.Phase() == EBilliardsPhase::Rolling && T < Limit)
		{
			Sim.Substep();
			T += FBilliardsSim::SubstepSeconds;
			if (Into)
			{
				Collect(Sim, *Into);
			}
		}
		return Sim.Phase() == EBilliardsPhase::Rolling ? -1.0 : T;
	}

	double KineticEnergy(const FBilliardsSim& Sim, double R)
	{
		// Per unit mass: translation plus rotation of a solid sphere (I = 2/5 m R^2).
		double E = 0.0;
		for (int I = 0; I < FBilliardsSim::BallCount; ++I)
		{
			if (!Sim.IsOnTable(I))
			{
				continue;
			}
			const FBilliardsVec2 V = Sim.BallVelocity(I);
			const FBilliardsVec3 W = Sim.BallSpin(I);
			E += 0.5 * Dot(V, V) + 0.2 * R * R * (W.X * W.X + W.Y * W.Y + W.Z * W.Z);
		}
		return E;
	}

	bool AllOnCloth(const FBilliardsSim& Sim, std::string& Why)
	{
		const FBilliardsTableLayout& L = Sim.Layout();
		const double R = L.BallRadius;
		for (int I = 0; I < FBilliardsSim::BallCount; ++I)
		{
			if (!Sim.IsOnTable(I))
			{
				continue;
			}
			const FBilliardsVec2 P = Sim.BallPosition(I);
			if (std::fabs(P.X) > L.HalfLength - R + 0.05 || std::fabs(P.Y) > L.HalfWidth - R + 0.05)
			{
				Why = Fmt("ball %d at (%.2f, %.2f)", I, P.X, P.Y);
				return false;
			}
		}
		return true;
	}

	double WorstOverlap(const FBilliardsSim& Sim)
	{
		const double TwoR = 2.0 * Sim.Layout().BallRadius;
		double Worst = 0.0;
		for (int I = 0; I < FBilliardsSim::BallCount; ++I)
		{
			for (int J = I + 1; J < FBilliardsSim::BallCount; ++J)
			{
				if (Sim.IsOnTable(I) && Sim.IsOnTable(J))
				{
					const double O = TwoR - Len(Sim.BallPosition(J) - Sim.BallPosition(I));
					Worst = O > Worst ? O : Worst;
				}
			}
		}
		return Worst;
	}

	// A one-ball (or two-ball) test table: everything off, then place what the test needs.
	FBilliardsSim EmptyTable()
	{
		FBilliardsSim Sim;
		Sim.ClearTable();
		return Sim;
	}
}

// ---------------------------------------------------------------------------

static void TestLayout()
{
	const FBilliardsTableLayout L = FBilliardsTableLayout::MakeDefault();
	Check(L.Pockets.size() == 6, "six pockets", Fmt("%zu", L.Pockets.size()));
	Check(L.Cushions.size() == 6, "six cushions: two per long rail, one per short", Fmt("%zu", L.Cushions.size()));

	bool bPocketsFit = true;
	for (const FBilliardsPocket& P : L.Pockets)
	{
		bPocketsFit = bPocketsFit && P.Radius > L.BallRadius * 1.5;
	}
	Check(bPocketsFit, "every pocket is wide enough to take a ball");

	// Every jaw lies on its pocket's circle: the cushion stops exactly where the hole begins.
	double WorstJaw = 0.0;
	for (const FBilliardsCushion& C : L.Cushions)
	{
		for (const FBilliardsVec2 End : { C.A, C.B })
		{
			double Nearest = 1e9;
			for (const FBilliardsPocket& P : L.Pockets)
			{
				const double Off = std::fabs(Len(End - P.Center) - P.Radius);
				Nearest = Off < Nearest ? Off : Nearest;
			}
			WorstJaw = Nearest > WorstJaw ? Nearest : WorstJaw;
		}
	}
	Check(WorstJaw < 1e-6, "every cushion ends on a pocket's edge", Fmt("worst %.2g cm", WorstJaw));
	Check(std::fabs(L.FootSpot.X - L.HalfLength * 0.5) < 1e-9 && std::fabs(L.HeadSpot.X + L.HalfLength * 0.5) < 1e-9,
		"the spots are a quarter length in from each end");
}

static void TestRack()
{
	FBilliardsSim Sim;
	Sim.StartGame();
	int OnTable = 0;
	for (int I = 0; I < FBilliardsSim::BallCount; ++I)
	{
		OnTable += Sim.IsOnTable(I) ? 1 : 0;
	}
	Check(OnTable == 16 && Sim.BallsLeft() == 15, "the rack: fifteen balls and the cue ball", Fmt("%d", OnTable));
	Check(WorstOverlap(Sim) <= 0.0, "no two racked balls overlap", Fmt("%.4f cm", WorstOverlap(Sim)));
	Check(Len(Sim.BallPosition(1) - Sim.Layout().FootSpot) < 1e-9, "the 1 ball sits on the foot spot");
	Check(Len(Sim.BallPosition(0) - Sim.Layout().HeadSpot) < 1e-9, "the cue ball sits on the head spot");
	Check(Sim.Phase() == EBilliardsPhase::Aiming, "a new game waits for the break");

	std::string Why;
	Check(AllOnCloth(Sim, Why), "the whole rack is on the cloth", Why);

	// Left alone, nothing moves: a rack a hair apart does not creep.
	const FBilliardsVec2 Before = Sim.BallPosition(8);
	RunSeconds(Sim, 2.0);
	Check(Len(Sim.BallPosition(8) - Before) < 1e-12 && !Sim.AnyBallMoving(), "an untouched rack stays still");
}

static void TestSlidingToRolling()
{
	// A centre hit: no spin, pure sliding. Theory: it rolls from then on at exactly 5/7 of
	// the speed it was struck at, having slid for 2V/(7*mu*g) seconds.
	FBilliardsSim Sim = EmptyTable();
	const double V0 = 300.0;
	Sim.PlaceBall(0, FBilliardsVec2(-100.0, 0.0), FBilliardsVec2(V0, 0.0), false);
	Check(Sim.IsSliding(0), "a centre-struck ball starts out sliding");

	const double Mu = FBilliardsSim::SlidingFriction;
	const double G = FBilliardsSim::Gravity;
	const double SlideSeconds = 2.0 * V0 / (7.0 * Mu * G);
	RunSeconds(Sim, SlideSeconds + 0.01);
	Check(!Sim.IsSliding(0), "then it rolls", Fmt("after %.3f s", SlideSeconds + 0.01));

	// Allow for the 0.01 s of rolling resistance since.
	const double Expected = V0 * 5.0 / 7.0 - FBilliardsSim::RollingFriction * G * 0.01;
	const double Got = Len(Sim.BallVelocity(0));
	Check(std::fabs(Got - Expected) < 0.01 * Expected, "at five-sevenths of its speed",
		Fmt("%.2f, expected %.2f", Got, Expected));
}

static void TestRollingDistance()
{
	// A rolling ball stops after v^2 / (2 * muR * g): 60 cm/s is 153 cm on this cloth.
	FBilliardsSim Sim = EmptyTable();
	const double V0 = 60.0;
	Sim.PlaceBall(0, FBilliardsVec2(-80.0, 0.0), FBilliardsVec2(V0, 0.0), true);
	Check(!Sim.IsSliding(0), "a ball placed rolling is rolling");
	const double T = RunToRest(Sim);
	const double Expected = V0 * V0 / (2.0 * FBilliardsSim::RollingFriction * FBilliardsSim::Gravity);
	const double Got = Sim.BallPosition(0).X + 80.0;
	Check(T > 0.0, "a rolling ball comes to rest", Fmt("%.2f s", T));
	Check(std::fabs(Got - Expected) < 0.02 * Expected, "and travels as far as rolling resistance allows",
		Fmt("%.1f cm, expected %.1f", Got, Expected));
	Check(std::fabs(Sim.BallPosition(0).Y) < 1e-9, "in a straight line");
}

static void TestShootSpin()
{
	// Through the real Shoot: spin 0.8 is natural roll, rolling from the first instant.
	FBilliardsSim Sim;
	Sim.StartGame();
	for (int I = 1; I < FBilliardsSim::BallCount; ++I)
	{
		Sim.RemoveBall(I);
	}
	Sim.PlaceBall(0, FBilliardsVec2(-60.0, 0.0), FBilliardsVec2(0.0, 0.0), false);
	Check(Sim.Shoot(0.0, 0.3, 0.8), "Shoot takes a stroke while aiming");
	Check(!Sim.IsSliding(0), "spin 0.8 is natural roll: no slip at all");
	Check(!Sim.Shoot(0.0, 0.3, 0.0), "and refuses another while the balls roll");
	Check(Sim.Shots() == 1, "one stroke counted");
}

// Shoots the cue ball straight at an object ball D cm away, through Shoot, with Spin.
// Returns where the cue ball came to rest, relative to where the object ball was.
static double StraightShotCueRest(double Spin, double Distance, double Power, double* ObjectTravel = nullptr)
{
	FBilliardsSim Sim;
	Sim.StartGame();
	for (int I = 1; I < FBilliardsSim::BallCount; ++I)
	{
		Sim.RemoveBall(I);
	}
	const FBilliardsVec2 Obj(0.0, 0.0);
	Sim.PlaceBall(0, FBilliardsVec2(-Distance, 0.0), FBilliardsVec2(0.0, 0.0), false);
	Sim.PlaceBall(5, Obj, FBilliardsVec2(0.0, 0.0), false);
	Sim.Shoot(0.0, Power, Spin);
	RunToRest(Sim);
	if (ObjectTravel)
	{
		*ObjectTravel = Sim.IsOnTable(5) ? Sim.BallPosition(5).X - Obj.X : 999.0;
	}
	return Sim.BallPosition(0).X - Obj.X;
}

static void TestStunDrawFollow()
{
	const double TwoR = 2.0 * BilliardsDefaultBallRadius;

	// Stun: a centre hit from close range reaches the object ball still sliding, without
	// spin, and stops dead on the spot where it hit.
	// Not quite dead: restitution 0.93 leaves it 3.5% of its speed, as real balls do.
	const double Stun = StraightShotCueRest(0.0, 12.0, 0.2);
	Check(std::fabs(Stun + TwoR) < BilliardsDefaultBallRadius, "a stun shot stops within half a ball of the contact",
		Fmt("cue ball rests %.2f cm from contact", Stun + TwoR));

	// Draw: a low hit carries backspin through the collision and comes back toward you.
	const double Draw = StraightShotCueRest(-1.0, 40.0, 0.2);
	Check(Draw < -TwoR - 10.0, "a draw shot comes back", Fmt("cue ball rests %.1f cm behind contact", -(Draw + TwoR)));

	// Follow: a high hit carries topspin through and follows on.
	double ObjectTravel = 0.0;
	const double Follow = StraightShotCueRest(1.0, 40.0, 0.1, &ObjectTravel);
	Check(Follow > -TwoR + 10.0, "a follow shot follows the object ball", Fmt("cue ball rests %.1f cm past contact", Follow + TwoR));
}

static void TestCollisionEnergyAndAngle()
{
	// A cut: two sliding, spinless balls, the cue ball hitting half-ball. Just after the
	// hit the two leave at close to 90 degrees (exactly 90 only if perfectly elastic), and
	// no energy has been created.
	FBilliardsSim Sim = EmptyTable();
	const double R = Sim.Layout().BallRadius;
	Sim.PlaceBall(5, FBilliardsVec2(0.0, 0.0), FBilliardsVec2(0.0, 0.0), false);
	Sim.PlaceBall(0, FBilliardsVec2(-30.0, R), FBilliardsVec2(400.0, 0.0), false);

	const double E0 = KineticEnergy(Sim, R);
	double EMaxRise = 0.0;
	std::vector<FBilliardsEvent> Events;
	for (int I = 0; I < 1200 && CountEvents(Events, EBilliardsEvent::BallHitBall) == 0; ++I)
	{
		const double Before = KineticEnergy(Sim, R);
		Sim.Substep();
		Collect(Sim, Events);
		const double Rise = KineticEnergy(Sim, R) - Before;
		EMaxRise = Rise > EMaxRise ? Rise : EMaxRise;
	}
	Check(CountEvents(Events, EBilliardsEvent::BallHitBall) == 1, "the cut shot makes contact");
	Check(EMaxRise <= 1e-6 * E0, "no substep creates energy", Fmt("worst rise %.3g", EMaxRise));

	const double A0 = AngleOf(Sim.BallVelocity(0));
	const double A5 = AngleOf(Sim.BallVelocity(5));
	double Between = std::fabs(A0 - A5) * 180.0 / Pi;
	Between = Between > 180.0 ? 360.0 - Between : Between;
	Check(Between > 80.0 && Between < 95.0, "a cut sends the two balls off near 90 degrees apart", Fmt("%.1f deg", Between));
	Check(A5 < 0.0 && A5 > -Pi * 0.5, "the object ball goes off the far side of the line of centres", Fmt("%.1f deg", A5 * 180.0 / Pi));
}

static void TestCushion()
{
	// A rolling ball into the long cushion at 45 degrees, well clear of any pocket: the
	// speed into the cushion comes back at the restitution, and the speed along it is kept
	// but for the rail's little friction.
	FBilliardsSim Sim = EmptyTable();
	const double V = 150.0;
	Sim.PlaceBall(0, FBilliardsVec2(-60.0, 40.0), FBilliardsVec2(V / std::sqrt(2.0), V / std::sqrt(2.0)), true);

	std::vector<FBilliardsEvent> Events;
	FBilliardsVec2 Into;
	for (int I = 0; I < 2400; ++I)
	{
		Into = Sim.BallVelocity(0);
		Sim.Substep();
		Collect(Sim, Events);
		if (CountEvents(Events, EBilliardsEvent::BallHitCushion) > 0)
		{
			break;
		}
	}
	const FBilliardsVec2 Out = Sim.BallVelocity(0);
	Check(CountEvents(Events, EBilliardsEvent::BallHitCushion) == 1, "the ball meets the cushion");
	Check(Out.Y < 0.0 && std::fabs(-Out.Y / Into.Y - FBilliardsSim::CushionRestitution) < 0.01,
		"and comes off it at the cushion's restitution", Fmt("%.3f", -Out.Y / Into.Y));
	Check(std::fabs(Out.X / Into.X - (1.0 - FBilliardsSim::CushionFriction)) < 0.01,
		"keeping its speed along the rail", Fmt("%.3f", Out.X / Into.X));
	Check(Sim.BallPosition(0).Y <= Sim.Layout().HalfWidth - Sim.Layout().BallRadius + 1e-9, "and never inside it");
}

static void TestPockets()
{
	const FBilliardsTableLayout L = FBilliardsTableLayout::MakeDefault();

	// Down the diagonal into the foot-left corner.
	{
		FBilliardsSim Sim = EmptyTable();
		const FBilliardsVec2 Target = L.Pockets[0].Center;
		const FBilliardsVec2 From(60.0, 30.0);
		const FBilliardsVec2 Dir = (Target - From) * (1.0 / Len(Target - From));
		Sim.PlaceBall(3, From, Dir * 120.0, true);
		std::vector<FBilliardsEvent> Events;
		RunToRest(Sim, 70.0, &Events);
		Check(!Sim.IsOnTable(3) && CountEvents(Events, EBilliardsEvent::BallPocketed, 3) == 1,
			"a ball rolled at a corner pocket drops");
		bool bRightHole = false;
		for (const FBilliardsEvent& E : Events)
		{
			bRightHole = bRightHole || (E.Type == EBilliardsEvent::BallPocketed && E.Other == 0);
		}
		Check(bRightHole, "into that corner");
	}

	// Straight across into a side pocket.
	{
		FBilliardsSim Sim = EmptyTable();
		Sim.PlaceBall(7, FBilliardsVec2(0.0, 0.0), FBilliardsVec2(0.0, -100.0), true);
		std::vector<FBilliardsEvent> Events;
		RunToRest(Sim, 70.0, &Events);
		Check(!Sim.IsOnTable(7) && CountEvents(Events, EBilliardsEvent::BallPocketed, 7) == 1,
			"a ball rolled straight at a side pocket drops");
	}

	// Straight into the middle of a short cushion: it bounces, it does not drop.
	{
		FBilliardsSim Sim = EmptyTable();
		Sim.PlaceBall(9, FBilliardsVec2(50.0, 0.0), FBilliardsVec2(120.0, 0.0), true);
		std::vector<FBilliardsEvent> Events;
		RunToRest(Sim, 70.0, &Events);
		Check(Sim.IsOnTable(9) && CountEvents(Events, EBilliardsEvent::BallHitCushion, 9) >= 1,
			"a ball into the middle of a cushion bounces back");
	}

	// Clipping a side-pocket jaw: aimed so the ball's edge catches the jaw point, it is
	// turned away rather than passing through the rail.
	{
		FBilliardsSim Sim = EmptyTable();
		const double R = L.BallRadius;
		const double JawX = L.Cushions[1].A.X;   // the +Y rail's side-pocket jaw, x = +6.19
		Sim.PlaceBall(4, FBilliardsVec2(JawX + R * 0.6 + 40.0, 20.0), FBilliardsVec2(-80.0, 80.0 * (L.HalfWidth - 20.0) / 40.0), true);
		std::vector<FBilliardsEvent> Events;
		RunToRest(Sim, 70.0, &Events);
		std::string Why;
		Check(AllOnCloth(Sim, Why), "a ball that clips a jaw stays on the cloth or drops, never escapes", Why);
	}
}

static void TestScratch()
{
	FBilliardsSim Sim;
	Sim.StartGame();
	// Clear the head end and fire the cue ball into the head-left corner.
	const FBilliardsVec2 Target = Sim.Layout().Pockets[2].Center;
	const FBilliardsVec2 From = Sim.BallPosition(0);
	Sim.TakeEvents();
	Check(Sim.Shoot(AngleOf(Target - From), 0.35, 0.8), "the scratch shot is taken");
	std::vector<FBilliardsEvent> Events;
	RunToRest(Sim, 70.0, &Events);
	Check(CountEvents(Events, EBilliardsEvent::CueBallScratched) == 1, "the cue ball goes down: a scratch");
	Check(Sim.Penalties() == 1 && Sim.Strokes() == 2, "a scratch costs a penalty stroke",
		Fmt("shots %d penalties %d", Sim.Shots(), Sim.Penalties()));
	Check(Sim.IsOnTable(0) && CountEvents(Events, EBilliardsEvent::CueBallRespotted) == 1, "and the cue ball comes back");
	Check(Len(Sim.BallPosition(0) - Sim.Layout().HeadSpot) < 1e-9, "to the head spot");
	Check(Sim.Phase() == EBilliardsPhase::Aiming, "ready for the next stroke");
}

static void TestBreak()
{
	FBilliardsSim Sim;
	Sim.StartGame();
	Sim.TakeEvents();
	const FBilliardsVec2 Apex = Sim.BallPosition(1);
	Sim.Shoot(AngleOf(Apex - Sim.BallPosition(0)) + 0.002, 1.0, 0.0);

	double MaxSpeed = 0.0;
	double EMaxRise = 0.0;
	const double R = Sim.Layout().BallRadius;
	std::vector<FBilliardsEvent> Events;
	double T = 0.0;
	while (Sim.Phase() == EBilliardsPhase::Rolling && T < 70.0)
	{
		const double Before = KineticEnergy(Sim, R);
		Sim.Substep();
		T += FBilliardsSim::SubstepSeconds;
		Collect(Sim, Events);
		const double Rise = KineticEnergy(Sim, R) - Before;
		EMaxRise = Rise > EMaxRise ? Rise : EMaxRise;
		for (int I = 0; I < FBilliardsSim::BallCount; ++I)
		{
			const double S = Len(Sim.BallVelocity(I));
			MaxSpeed = S > MaxSpeed ? S : MaxSpeed;
		}
	}
	Check(Sim.Phase() != EBilliardsPhase::Rolling, "a full-power break comes to rest", Fmt("%.1f s", T));
	Check(T < 30.0, "within half a minute", Fmt("%.1f s", T));
	Check(EMaxRise < 1e-6 * 0.5 * FBilliardsSim::MaxCueSpeed * FBilliardsSim::MaxCueSpeed, "the break creates no energy",
		Fmt("worst rise %.3g", EMaxRise));
	Check(MaxSpeed <= FBilliardsSim::MaxBallSpeed + 1e-6, "no ball exceeds the tunnelling cap", Fmt("%.0f cm/s", MaxSpeed));

	// Spread: how far the balls left on the table are from the rack's centre.
	double Spread = 0.0;
	int Left = 0;
	for (int I = 1; I < FBilliardsSim::BallCount; ++I)
	{
		if (Sim.IsOnTable(I))
		{
			Spread += Len(Sim.BallPosition(I) - Apex);
			++Left;
		}
	}
	Spread = Left > 0 ? Spread / Left : 999.0;
	Check(Spread > 40.0, "the break spreads the rack", Fmt("mean %.0f cm from the apex, %d potted", Spread, 15 - Left));

	std::string Why;
	Check(AllOnCloth(Sim, Why), "every ball still on the table is on the cloth", Why);
	Check(WorstOverlap(Sim) < 0.01, "and no two overlap at rest", Fmt("%.4f cm", WorstOverlap(Sim)));
}

static void TestNoEscape()
{
	// Hard shots in every direction from all over the table, into a full rack. Nothing may
	// ever end up off the cloth except down a pocket, and every stroke must settle.
	FLcg Rng{ 20260927u };
	int Unsettled = 0;
	int Escaped = 0;
	std::string FirstEscape;
	for (int Game = 0; Game < 20; ++Game)
	{
		FBilliardsSim Sim;
		Sim.StartGame();
		for (int Stroke = 0; Stroke < 8 && Sim.Phase() == EBilliardsPhase::Aiming; ++Stroke)
		{
			Sim.Shoot(Rng.Next01() * 2.0 * Pi, 0.5 + 0.5 * Rng.Next01(), Rng.Next01() * 2.0 - 1.0);
			if (RunToRest(Sim) < 0.0)
			{
				++Unsettled;
				break;
			}
			std::string Why;
			if (!AllOnCloth(Sim, Why))
			{
				if (Escaped++ == 0)
				{
					FirstEscape = Fmt("game %d stroke %d: ", Game, Stroke) + Why;
				}
			}
		}
	}
	Check(Unsettled == 0, "every hard random stroke settles", Fmt("%d did not", Unsettled));
	Check(Escaped == 0, "no ball ever leaves the cloth except by a pocket", FirstEscape);
}

// A plain player: for every ball and pocket, the ghost-ball aim point; take the straightest
// cut that the cue ball can reach without hitting another ball first, with a little
// wobble, so no two games are the same. It is not good. It does not need to be.
static bool PlayOneGame(uint32_t Seed, int& StrokesOut, std::string& Why)
{
	FLcg Rng{ Seed };
	FBilliardsSim Sim;
	Sim.StartGame();
	const FBilliardsTableLayout& L = Sim.Layout();
	const double R = L.BallRadius;

	// The break.
	Sim.Shoot(AngleOf(Sim.BallPosition(1) - Sim.BallPosition(0)) + (Rng.Next01() - 0.5) * 0.02, 1.0, 0.0);
	if (RunToRest(Sim) < 0.0)
	{
		Why = "the break never settled";
		return false;
	}

	for (int Stroke = 0; Stroke < 400; ++Stroke)
	{
		if (Sim.Phase() == EBilliardsPhase::Cleared)
		{
			StrokesOut = Sim.Strokes();
			return true;
		}
		const FBilliardsVec2 Cue = Sim.BallPosition(0);
		double BestCut = 1e9;
		double BestAngle = Rng.Next01() * 2.0 * Pi;
		for (int B = 1; B < FBilliardsSim::BallCount; ++B)
		{
			if (!Sim.IsOnTable(B))
			{
				continue;
			}
			for (const FBilliardsPocket& P : L.Pockets)
			{
				const FBilliardsVec2 ToPocket = P.Center - Sim.BallPosition(B);
				const FBilliardsVec2 Ghost = Sim.BallPosition(B) - ToPocket * (2.0 * R / Len(ToPocket));
				const FBilliardsVec2 Aim = Ghost - Cue;
				const double Cut = std::acos(std::fmax(-1.0, std::fmin(1.0, Dot(Aim, ToPocket) / (Len(Aim) * Len(ToPocket)))));
				if (Cut > 1.3)
				{
					continue;
				}
				const FBilliardsAimPrediction Pred = Sim.PredictShot(AngleOf(Aim));
				if (!Pred.bHitsBall || Pred.Ball != B)
				{
					continue;
				}
				if (Cut < BestCut)
				{
					BestCut = Cut;
					BestAngle = AngleOf(Aim);
				}
			}
		}
		Sim.Shoot(BestAngle + (Rng.Next01() - 0.5) * 0.01, 0.35 + 0.2 * Rng.Next01(), 0.0);
		if (RunToRest(Sim) < 0.0)
		{
			Why = Fmt("stroke %d never settled", Stroke);
			return false;
		}
		std::string Off;
		if (!AllOnCloth(Sim, Off))
		{
			Why = Fmt("stroke %d: ", Stroke) + Off;
			return false;
		}
	}
	Why = Fmt("not cleared in 400 strokes, %d balls left", Sim.BallsLeft());
	return false;
}

static void TestWholeGamesEnd()
{
	int Cleared = 0;
	int Total = 0;
	int Worst = 0;
	std::string FirstWhy;
	for (uint32_t Seed = 1; Seed <= 10; ++Seed)
	{
		int Strokes = 0;
		std::string Why;
		if (PlayOneGame(Seed * 7919u, Strokes, Why))
		{
			++Cleared;
			Total += Strokes;
			Worst = Strokes > Worst ? Strokes : Worst;
		}
		else if (FirstWhy.empty())
		{
			FirstWhy = Fmt("seed %u: ", Seed) + Why;
		}
	}
	Check(Cleared == 10, "ten whole games of clear-the-table all end",
		Cleared > 0 ? Fmt("%d cleared, mean %.0f strokes, worst %d%s%s", Cleared, double(Total) / Cleared, Worst,
			FirstWhy.empty() ? "" : "; ", FirstWhy.c_str()) : FirstWhy);
}

static void TestDeterminism()
{
	auto Play = []()
	{
		FBilliardsSim Sim;
		Sim.StartGame();
		Sim.Shoot(0.001, 1.0, -0.3);
		RunToRest(Sim);
		Sim.Shoot(1.0, 0.6, 0.5);
		RunToRest(Sim);
		double Sum = 0.0;
		for (int I = 0; I < FBilliardsSim::BallCount; ++I)
		{
			Sum += Sim.BallPosition(I).X * (I + 1) + Sim.BallPosition(I).Y * (I + 17);
		}
		return Sum;
	};
	Check(Play() == Play(), "the same strokes give the same table, to the last bit");
}

static void TestAimPrediction()
{
	FBilliardsSim Sim;
	Sim.StartGame();
	const FBilliardsAimPrediction Straight = Sim.PredictShot(0.0);
	Check(Straight.bHitsBall && Straight.Ball == 1, "the aim line down the table meets the 1 ball", Fmt("ball %d", Straight.Ball));
	Check(std::fabs(Straight.CueAtContact.X - (Sim.BallPosition(1).X - 2.0 * Sim.Layout().BallRadius)) < 1e-6,
		"touching it exactly", Fmt("x %.3f", Straight.CueAtContact.X));
	Check(Len(Straight.CueDeflection) == 0.0, "a dead-straight hit has no deflection line");

	const FBilliardsAimPrediction Away = Sim.PredictShot(Pi);
	Check(!Away.bHitsBall, "aimed at the head rail it meets no ball");
	Check(std::fabs(Away.CueAtContact.X - (-Sim.Layout().HalfLength + Sim.Layout().BallRadius)) < 1e-6, "and stops at the cushion");
}

int main()
{
	std::printf("Billiards physics - Source/SibeliusGame/BilliardsSim\n\n");
	TestLayout();
	TestRack();
	TestSlidingToRolling();
	TestRollingDistance();
	TestShootSpin();
	TestStunDrawFollow();
	TestCollisionEnergyAndAngle();
	TestCushion();
	TestPockets();
	TestScratch();
	TestBreak();
	TestNoEscape();
	TestWholeGamesEnd();
	TestDeterminism();
	TestAimPrediction();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
