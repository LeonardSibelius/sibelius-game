// pinball_sim_test.cpp — standalone tests for Source/SibeliusGame/PinballSim.
//
// No engine, no editor, no cook: this builds with the Visual Studio compiler alone and
// runs in about a second. Run it with Tools/Tests/run_pinball_sim_tests.ps1.
//
// Each test is a PHYSICAL CLAIM, not "it didn't crash": a ball falls at the rate a rolling
// ball should, a wall is never crossed at the maximum speed, a bounce never adds energy, a
// flipper shot actually reaches the upper playfield. If a tuning change breaks one of these,
// the table has stopped behaving like a pinball table, whatever it looks like on screen.

#include "PinballSim.h"

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

	double Len(FPinVec2 V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
	double Dot(FPinVec2 A, FPinVec2 B) { return A.X * B.X + A.Y * B.Y; }

	// Deterministic, so a failure reproduces exactly.
	struct FLcg
	{
		uint32_t State;
		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) * (1.0 / 16777216.0);
		}
	};

	int CountEvents(const std::vector<FPinballEvent>& Events, EPinballEvent Type, int Index = -2)
	{
		int Count = 0;
		for (const FPinballEvent& Event : Events)
		{
			if (Event.Type == Type && (Index == -2 || Event.Index == Index))
			{
				++Count;
			}
		}
		return Count;
	}

	void Collect(FPinballSim& Sim, std::vector<FPinballEvent>& Into)
	{
		const std::vector<FPinballEvent> Events = Sim.TakeEvents();
		Into.insert(Into.end(), Events.begin(), Events.end());
	}

	void RunSeconds(FPinballSim& Sim, double Seconds, std::vector<FPinballEvent>* Into = nullptr)
	{
		const int Frames = static_cast<int>(Seconds * 60.0 + 0.5);
		for (int Frame = 0; Frame < Frames; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			if (Into)
			{
				Collect(Sim, *Into);
			}
		}
	}

	// A closed square box with nothing in it: the place to test pure physics away from
	// kickers, which add energy on purpose. Flippers are parked far outside.
	FPinballTableLayout MakeBox(double Size)
	{
		FPinballTableLayout L;
		L.Width = Size;
		L.Length = Size;
		L.Segments = {
			FPinballSegment{ FPinVec2(0.0, 0.0), FPinVec2(Size, 0.0) },
			FPinballSegment{ FPinVec2(Size, 0.0), FPinVec2(Size, Size) },
			FPinballSegment{ FPinVec2(Size, Size), FPinVec2(0.0, Size) },
			FPinballSegment{ FPinVec2(0.0, Size), FPinVec2(0.0, 0.0) },
		};
		L.Flippers[0].Pivot = FPinVec2(1e4, 1e4);
		L.Flippers[1].Pivot = FPinVec2(1e4 + 100.0, 1e4);
		L.PlungerRest = FPinVec2(Size * 0.5, Size * 0.5);
		L.ShooterLaneMinX = 1e9;
		L.DrainY = -1e9;
		return L;
	}
}

int main()
{
	std::printf("Pinball physics model - standalone tests\n\n");

	// --- the arithmetic behind the no-tunnelling guarantee ----------------------------
	{
		const FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		const double PerStep = FPinballSim::MaxBallSpeed * FPinballSim::SubstepSeconds;
		Check(PerStep < Sim.Layout().BallRadius, "a max-speed ball moves less than its radius per substep",
			Fmt("%.3f cm per substep vs %.3f cm radius", PerStep, Sim.Layout().BallRadius));
	}

	// --- gravity -----------------------------------------------------------------------
	{
		FPinballSim Sim(MakeBox(200.0));
		Check(std::fabs(Sim.EffectiveGravity() - 79.30) < 0.2, "effective gravity is 5/7 g sin(6.5 deg) ~ 79.3 cm/s^2",
			Fmt("%.2f", Sim.EffectiveGravity()));

		Sim.PlaceBall(FPinVec2(100.0, 150.0), FPinVec2());
		RunSeconds(Sim, 0.5);
		const double Expected = -Sim.EffectiveGravity() * 0.5;
		const double Got = Sim.BallVelocity().Y;
		Check(std::fabs(Got - Expected) < std::fabs(Expected) * 0.02, "a free ball falls at the rolling-ball rate",
			Fmt("vy after 0.5 s = %.2f, expected %.2f", Got, Expected));
	}

	// --- energy --------------------------------------------------------------------------
	{
		FPinballSim Sim(MakeBox(30.0));
		const double G = Sim.EffectiveGravity();
		auto Energy = [G](const FPinballSim& S) { const FPinVec2 V = S.BallVelocity(); return 0.5 * Dot(V, V) + G * S.BallPosition().Y; };

		Sim.PlaceBall(FPinVec2(15.0, 15.0), FPinVec2(400.0, 250.0));
		const double E0 = Energy(Sim);
		double MaxE = E0;
		for (int Step = 0; Step < 6000; ++Step)
		{
			Sim.Substep();
			const double E = Energy(Sim);
			MaxE = E > MaxE ? E : MaxE;
		}
		Check(MaxE <= E0 * 1.01, "bouncing off passive walls never adds energy",
			Fmt("start %.0f, highest seen %.0f", E0, MaxE));
	}

	// --- tunnelling --------------------------------------------------------------------
	{
		FPinballSim Sim(MakeBox(30.0));
		const double R = Sim.Layout().BallRadius;
		FLcg Rng{ 12345u };
		double Worst = 0.0;
		for (int Trial = 0; Trial < 300; ++Trial)
		{
			const double A = Rng.Next01() * 2.0 * Pi;
			Sim.PlaceBall(FPinVec2(15.0, 15.0), FPinVec2(std::cos(A), std::sin(A)) * FPinballSim::MaxBallSpeed);
			for (int Step = 0; Step < 1200; ++Step)
			{
				Sim.Substep();
				const FPinVec2 P = Sim.BallPosition();
				const double Past[4] = { R - P.X, P.X - (30.0 - R), R - P.Y, P.Y - (30.0 - R) };
				for (double Over : Past)
				{
					Worst = Over > Worst ? Over : Worst;
				}
			}
		}
		Check(Worst < 0.05, "300 max-speed shots: the ball never passes into a wall",
			Fmt("worst overlap %.4f cm", Worst));
	}

	// --- resting ----------------------------------------------------------------------
	{
		FPinballSim Sim(MakeBox(30.0));
		Sim.PlaceBall(FPinVec2(15.0, 20.0), FPinVec2(120.0, 0.0));
		RunSeconds(Sim, 30.0);
		const double Speed = Len(Sim.BallVelocity());
		const double Height = Sim.BallPosition().Y - Sim.Layout().BallRadius;
		Check(Speed < 1.0 && std::fabs(Height) < 0.05, "a ball comes to rest on the floor instead of buzzing",
			Fmt("speed %.3f cm/s, %.3f cm above the floor", Speed, Height));
	}

	// --- serving and the plunger --------------------------------------------------------
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		const FPinVec2 P = Sim.BallPosition();
		Check(Sim.Phase() == EPinballPhase::BallReady && Sim.BallNumber() == 1
			&& Len(P - Sim.Layout().PlungerRest) < 1e-9, "a new game serves ball 1 to the plunger");

		Check(Sim.Launch(1.0), "the plunger fires when a ball is waiting");
		Check(!Sim.Launch(1.0), "the plunger does nothing while the ball is in play");

		const double LaneX = Sim.Layout().ShooterLaneMinX;
		const double R = Sim.Layout().BallRadius;
		bool bReachedPlayfield = false;
		for (int Frame = 0; Frame < 180 && !bReachedPlayfield; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			const FPinVec2 Q = Sim.BallPosition();
			bReachedPlayfield = Q.X < LaneX - R && Q.Y > 30.0;
		}
		Check(bReachedPlayfield, "a full plunge clears the gate and the arc into the playfield");
	}
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		Sim.Launch(0.0);
		bool bBack = false;
		for (int Frame = 0; Frame < 360 && !bBack; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			bBack = Sim.Phase() == EPinballPhase::BallReady;
		}
		Check(bBack && Sim.BallNumber() == 1, "a soft plunge rolls back onto the plunger: same ball, another pull");
	}

	// --- flippers -------------------------------------------------------------------------
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		const FPinballFlipperDef& Def = Sim.Layout().Flippers[0];
		const double R = Sim.Layout().BallRadius;

		// Rest the ball on the resting left flipper, 70% of the way out, and flip.
		const double T = 0.7;
		const FPinVec2 Tip = Sim.FlipperTip(0);
		const FPinVec2 Along = Tip - Def.Pivot;
		const FPinVec2 Dir = Along * (1.0 / Len(Along));
		const FPinVec2 Up(-Dir.Y, Dir.X);
		const double FlipperRadius = Def.BaseRadius + (Def.TipRadius - Def.BaseRadius) * T;
		Sim.PlaceBall(Def.Pivot + Along * T + Up * (R + FlipperRadius + 0.01), FPinVec2());
		Sim.SetFlipper(0, true);

		RunSeconds(Sim, 0.1);
		const double VyAfterFlip = Sim.BallVelocity().Y;
		double HighestY = Sim.BallPosition().Y;
		for (int Frame = 0; Frame < 84; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			HighestY = Sim.BallPosition().Y > HighestY ? Sim.BallPosition().Y : HighestY;
		}
		Check(VyAfterFlip > 120.0, "a flipper stroke launches a resting ball upward",
			Fmt("vy 0.1 s after the flip = %.0f cm/s", VyAfterFlip));
		Check(HighestY > 40.0, "a flipper shot reaches the upper playfield",
			Fmt("highest point %.1f cm", HighestY));
	}
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		Sim.SetFlipper(0, true);
		RunSeconds(Sim, 0.1);
		Sim.PlaceBall(FPinVec2(16.5, 20.0), FPinVec2());
		RunSeconds(Sim, 4.0);
		const double Speed = Len(Sim.BallVelocity());
		Check(Sim.Phase() == EPinballPhase::InPlay && Speed < 5.0 && Sim.BallPosition().Y > 8.0,
			"a held flipper cradles the ball", Fmt("speed %.2f cm/s at (%.1f, %.1f)", Speed, Sim.BallPosition().X, Sim.BallPosition().Y));
	}

	// --- draining and the end of the game ---------------------------------------------------
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		const double CenterX = Sim.Layout().ShooterLaneMinX * 0.5;
		std::vector<FPinballEvent> Events;
		Sim.PlaceBall(FPinVec2(CenterX, 14.0), FPinVec2(0.0, -100.0));
		RunSeconds(Sim, 1.5, &Events);
		Check(CountEvents(Events, EPinballEvent::BallDrained, 1) == 1 && Sim.BallNumber() == 2
			&& Sim.Phase() == EPinballPhase::BallReady, "a ball between the flippers drains, and ball 2 is served");

		for (int Ball = 2; Ball <= 3; ++Ball)
		{
			Sim.PlaceBall(FPinVec2(CenterX, 14.0), FPinVec2(0.0, -100.0));
			RunSeconds(Sim, 1.5, &Events);
		}
		Check(Sim.Phase() == EPinballPhase::GameOver && CountEvents(Events, EPinballEvent::GameOver) == 1,
			"three drains end the game");
	}

	// --- scoring -------------------------------------------------------------------------
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		Sim.TakeEvents();
		const FPinballBumper& Bumper = Sim.Layout().Bumpers[0];
		const double R = Sim.Layout().BallRadius;
		std::vector<FPinballEvent> Events;
		Sim.PlaceBall(Bumper.Center + FPinVec2(0.0, Bumper.Radius + R + 2.0), FPinVec2(0.0, -150.0));
		RunSeconds(Sim, 0.1, &Events);

		// Judged by the bumper's own event, not the total score. The first version asserted
		// score == 100 and failed at 150: the kick sends the ball straight up the table from
		// x=15, and that line runs over the left rollover at (16, 95), which rightly lit.
		int BumperPointsPaid = 0;
		for (const FPinballEvent& Event : Events)
		{
			if (Event.Type == EPinballEvent::BumperHit)
			{
				BumperPointsPaid += Event.Points;
			}
		}
		Check(BumperPointsPaid == Sim.Layout().BumperPoints && CountEvents(Events, EPinballEvent::BumperHit, 0) == 1
			&& Sim.BallVelocity().Y > 150.0, "a pop bumper scores once and kicks the ball away",
			Fmt("bumper paid %d, vy %.0f", BumperPointsPaid, Sim.BallVelocity().Y));
	}
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		Sim.TakeEvents();
		const double R = Sim.Layout().BallRadius;
		const double CenterX = Sim.Layout().ShooterLaneMinX * 0.5;
		const FPinballSegment* Kicker = nullptr;
		for (const FPinballSegment& Segment : Sim.Layout().Segments)
		{
			if (Segment.Kind == EPinballSurface::Slingshot && Segment.Index == 0)
			{
				Kicker = &Segment;
			}
		}
		if (!Kicker)
		{
			Check(false, "the table has a left slingshot");
		}
		else
		{
			const FPinVec2 Mid = (Kicker->A + Kicker->B) * 0.5;
			const FPinVec2 AB = Kicker->B - Kicker->A;
			FPinVec2 Face = FPinVec2(-AB.Y, AB.X) * (1.0 / Len(AB));
			if (Dot(Face, FPinVec2(CenterX, 40.0) - Mid) < 0.0)
			{
				Face = Face * -1.0;
			}
			std::vector<FPinballEvent> Events;
			Sim.PlaceBall(Mid + Face * (R + 2.0), Face * -120.0);
			RunSeconds(Sim, 0.05, &Events);
			const double Outward = Dot(Sim.BallVelocity(), Face);
			Check(Sim.Score() == Sim.Layout().SlingshotPoints && CountEvents(Events, EPinballEvent::SlingshotHit, 0) == 1
				&& Outward > 150.0, "a slingshot scores and kicks the ball back toward the centre",
				Fmt("score %d, outward speed %.0f", Sim.Score(), Outward));
		}
	}
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		Sim.TakeEvents();
		const double CenterX = Sim.Layout().ShooterLaneMinX * 0.5;
		std::vector<FPinballEvent> Events;
		for (size_t I = 0; I < Sim.Layout().Rollovers.size(); ++I)
		{
			Sim.PlaceBall(Sim.Layout().Rollovers[I].Center, FPinVec2());
			Sim.Substep();
			Collect(Sim, Events);
			Sim.PlaceBall(FPinVec2(CenterX, 50.0), FPinVec2());
			Sim.Substep();
			Collect(Sim, Events);
		}
		const int Expected = 3 * Sim.Layout().Rollovers[0].Points + Sim.Layout().RolloverSetBonus;
		const bool bAllReset = !Sim.IsRolloverLit(0) && !Sim.IsRolloverLit(1) && !Sim.IsRolloverLit(2);
		Check(Sim.Score() == Expected && CountEvents(Events, EPinballEvent::RolloverLit) == 3
			&& CountEvents(Events, EPinballEvent::RolloversComplete) == 1 && bAllReset,
			"lighting all three rollovers pays the bonus and resets the lights",
			Fmt("score %d, expected %d", Sim.Score(), Expected));
	}

	// --- the lane gate --------------------------------------------------------------------
	{
		FPinballSim Sim(FPinballTableLayout::MakeTestTable());
		Sim.StartGame();
		const double LaneX = Sim.Layout().ShooterLaneMinX;
		Sim.PlaceBall(FPinVec2((LaneX + Sim.Layout().Width) * 0.5, 90.0), FPinVec2(0.0, -60.0));
		bool bEnteredLane = false;
		for (int Frame = 0; Frame < 90; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			const FPinVec2 P = Sim.BallPosition();
			bEnteredLane = bEnteredLane || (P.X > LaneX + 0.5 && P.Y < 79.0 && Sim.Phase() == EPinballPhase::InPlay);
		}
		Check(!bEnteredLane, "the lane gate is one-way: a ball dropping onto it from above stays out");
	}

	// --- the real table holds the ball, at any speed, from anywhere ---------------------------
	{
		const FPinVec2 Starts[] = {
			FPinVec2(10.0, 50.0), FPinVec2(23.25, 50.0), FPinVec2(36.0, 50.0), FPinVec2(23.25, 30.0),
			FPinVec2(10.0, 85.0), FPinVec2(36.0, 88.0), FPinVec2(23.25, 60.0), FPinVec2(20.0, 95.0),
			FPinVec2(30.0, 40.0), FPinVec2(16.0, 40.0),
		};
		FLcg Rng{ 777u };
		bool bEscaped = false;
		bool bIntoLane = false;
		for (int Trial = 0; Trial < 200; ++Trial)
		{
			FPinballSim Sim(FPinballTableLayout::MakeTestTable());
			Sim.StartGame();
			const double LaneX = Sim.Layout().ShooterLaneMinX;
			const double A = Rng.Next01() * 2.0 * Pi;
			Sim.PlaceBall(Starts[Trial % 10], FPinVec2(std::cos(A), std::sin(A)) * FPinballSim::MaxBallSpeed);
			for (int Step = 0; Step < 3600; ++Step)
			{
				Sim.Substep();
				if (Sim.Phase() != EPinballPhase::InPlay)
				{
					break;
				}
				const FPinVec2 P = Sim.BallPosition();
				bEscaped = bEscaped || P.X < -0.5 || P.X > Sim.Layout().Width + 0.5 || P.Y > Sim.Layout().Length + 0.5;
				bIntoLane = bIntoLane || (P.X > LaneX + 0.5 && P.Y < 79.0);
			}
		}
		Check(!bEscaped, "200 max-speed shots on the real table: the ball never leaves the cabinet");
		Check(!bIntoLane, "...and never gets into the shooter lane from the playfield");
	}

	// --- determinism --------------------------------------------------------------------------
	{
		auto Play = [](FPinballSim& S)
		{
			S.StartGame();
			S.Launch(0.8);
			for (int Frame = 0; Frame < 1200; ++Frame)
			{
				if (Frame % 40 == 0) { S.SetFlipper(0, (Frame / 40) % 2 == 0); }
				if (Frame % 55 == 0) { S.SetFlipper(1, (Frame / 55) % 2 == 0); }
				S.Advance(1.0 / 60.0);
				if (S.Phase() == EPinballPhase::BallReady) { S.Launch(0.6); }
			}
		};
		FPinballSim First(FPinballTableLayout::MakeTestTable());
		FPinballSim Second(FPinballTableLayout::MakeTestTable());
		Play(First);
		Play(Second);
		Check(First.BallPosition().X == Second.BallPosition().X && First.BallPosition().Y == Second.BallPosition().Y
			&& First.Score() == Second.Score() && First.BallNumber() == Second.BallNumber(),
			"same inputs, same game: the model is deterministic",
			Fmt("score %d, ball %d", First.Score(), First.BallNumber()));
	}

	// ===========================================================================
	// THE SINGALONG — the table actually standing in the living room (MakeDefault)
	// ===========================================================================
	std::printf("\n-- the Singalong --\n");
	{
		const FPinballSim Sim;
		const FPinballTableLayout& L = Sim.Layout();
		Check(L.Bumpers.size() == 4 && L.Posts.size() == 35 && L.Kickouts.size() == 8,
			"the default table is the Singalong: 4 bumpers, 35 posts, 8 holes",
			Fmt("%d bumpers, %d posts, %d holes", static_cast<int>(L.Bumpers.size()), static_cast<int>(L.Posts.size()), static_cast<int>(L.Kickouts.size())));

		const FPinVec2 LeftTip = Sim.FlipperTip(0);
		Check(Len(L.Flippers[0].Pivot - FPinVec2(15.28, 9.74)) < 0.01 && Len(LeftTip - FPinVec2(21.85, 4.70)) < 0.15,
			"the flippers sit where the cabinet's modelled flippers are",
			Fmt("left tip at rest (%.2f, %.2f), mesh says (21.85, 4.70)", LeftTip.X, LeftTip.Y));

		bool bAllInside = true;
		for (const FPinballPost& Post : L.Posts)
		{
			bAllInside = bAllInside && Post.Center.X > -1.0 && Post.Center.X < L.ShooterLaneMinX + 1.0 && Post.Center.Y > -1.0 && Post.Center.Y < L.Length;
		}
		Check(bAllInside, "every post is inside the playfield");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.Launch(1.0);
		const double LaneX = Sim.Layout().ShooterLaneMinX;
		const double R = Sim.Layout().BallRadius;
		bool bReachedPlayfield = false;
		for (int Frame = 0; Frame < 180 && !bReachedPlayfield; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			const FPinVec2 P = Sim.BallPosition();
			bReachedPlayfield = P.X < LaneX - R && P.Y < 85.0;
		}
		Check(bReachedPlayfield, "Singalong: a full plunge clears the gate and comes round the arch into play");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.Launch(0.0);
		bool bBack = false;
		for (int Frame = 0; Frame < 360 && !bBack; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			bBack = Sim.Phase() == EPinballPhase::BallReady;
		}
		Check(bBack && Sim.BallNumber() == 1, "Singalong: a soft plunge rolls back onto the plunger");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		const FPinballFlipperDef& Def = Sim.Layout().Flippers[0];
		const double R = Sim.Layout().BallRadius;
		const double T = 0.7;
		const FPinVec2 Along = Sim.FlipperTip(0) - Def.Pivot;
		const FPinVec2 Dir = Along * (1.0 / Len(Along));
		const FPinVec2 Up(-Dir.Y, Dir.X);
		const double FlipperRadius = Def.BaseRadius + (Def.TipRadius - Def.BaseRadius) * T;
		Sim.PlaceBall(Def.Pivot + Along * T + Up * (R + FlipperRadius + 0.01), FPinVec2());
		Sim.SetFlipper(0, true);
		double HighestY = 0.0;
		for (int Frame = 0; Frame < 90; ++Frame)
		{
			Sim.Advance(1.0 / 60.0);
			HighestY = Sim.BallPosition().Y > HighestY ? Sim.BallPosition().Y : HighestY;
		}
		Check(HighestY > 50.0, "Singalong: a flipper shot reaches the bumpers", Fmt("highest point %.1f cm", HighestY));
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.SetFlipper(0, true);
		RunSeconds(Sim, 0.1);
		Sim.PlaceBall(FPinVec2(19.0, 17.0), FPinVec2());
		RunSeconds(Sim, 4.0);
		const double Speed = Len(Sim.BallVelocity());
		Check(Sim.Phase() == EPinballPhase::InPlay && Speed < 5.0 && Sim.BallPosition().Y > 6.0,
			"Singalong: a held flipper cradles the ball", Fmt("speed %.2f at (%.1f, %.1f)", Speed, Sim.BallPosition().X, Sim.BallPosition().Y));
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		std::vector<FPinballEvent> Events;
		// Down the left of the two drain channels, between the tip post and the centre post.
		// (Dropped dead onto the rubber centre post, the ball rightly bounces back into play.)
		Sim.PlaceBall(FPinVec2(24.2, 14.0), FPinVec2(0.0, -100.0));
		RunSeconds(Sim, 2.0, &Events);
		Check(CountEvents(Events, EPinballEvent::BallDrained, 1) == 1 && Sim.BallNumber() == 2,
			"Singalong: a ball down a drain channel beside the centre post is lost");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.TakeEvents();
		const FPinballKickout& Hole = Sim.Layout().Kickouts[2];
		std::vector<FPinballEvent> Events;
		Sim.PlaceBall(Hole.Center + FPinVec2(0.0, -3.0), FPinVec2(0.0, 60.0));
		int HeldSteps = 0;
		bool bCaptured = false;
		for (int Step = 0; Step < 3600; ++Step)
		{
			Sim.Substep();
			Collect(Sim, Events);
			if (Sim.HeldInKickout() == 2)
			{
				bCaptured = true;
				++HeldSteps;
			}
			else if (bCaptured)
			{
				break;   // just ejected
			}
		}
		const FPinVec2 Out = Sim.BallVelocity();
		const double HeldFor = HeldSteps * FPinballSim::SubstepSeconds;
		Check(bCaptured && Sim.Score() == Hole.Points && CountEvents(Events, EPinballEvent::KickoutCaptured, 2) == 1,
			"a slow ball drops into a hole and scores it", Fmt("score %d, hole pays %d", Sim.Score(), Hole.Points));
		Check(std::fabs(HeldFor - FPinballSim::KickoutHoldSeconds) < 0.01 && Out.Y < -150.0,
			"...is held, then kicked back out", Fmt("held %.3f s, out at vy %.0f", HeldFor, Out.Y));
		RunSeconds(Sim, 0.3, &Events);
		Check(CountEvents(Events, EPinballEvent::KickoutCaptured, 2) == 1, "...and is not swallowed again on the way out");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.TakeEvents();
		const FPinballKickout& Hole = Sim.Layout().Kickouts[0];
		std::vector<FPinballEvent> Events;
		Sim.PlaceBall(Hole.Center + FPinVec2(0.0, -3.0), FPinVec2(0.0, 400.0));
		RunSeconds(Sim, 0.03, &Events);
		Check(CountEvents(Events, EPinballEvent::KickoutCaptured) == 0, "a fast ball rolls straight over a hole");
	}
	{
		FPinballSim Sim;
		Sim.StartGame();
		Sim.TakeEvents();
		const double R = Sim.Layout().BallRadius;
		const FPinballSegment* Kicker = nullptr;
		for (const FPinballSegment& Segment : Sim.Layout().Segments)
		{
			if (Segment.Kind == EPinballSurface::Slingshot && Segment.Index == 0)
			{
				Kicker = &Segment;
			}
		}
		if (!Kicker)
		{
			Check(false, "Singalong: the left slingshot exists");
		}
		else
		{
			const FPinVec2 Mid = (Kicker->A + Kicker->B) * 0.5;
			const FPinVec2 AB = Kicker->B - Kicker->A;
			FPinVec2 Face = FPinVec2(-AB.Y, AB.X) * (1.0 / Len(AB));
			if (Dot(Face, FPinVec2(27.0, 40.0) - Mid) < 0.0)
			{
				Face = Face * -1.0;
			}
			std::vector<FPinballEvent> Events;
			Sim.PlaceBall(Mid + Face * (R + 2.0), Face * -120.0);
			RunSeconds(Sim, 0.05, &Events);
			Check(CountEvents(Events, EPinballEvent::SlingshotHit, 0) == 1 && Dot(Sim.BallVelocity(), Face) > 150.0,
				"Singalong: the left slingshot kicks toward the centre");
		}
	}
	{
		const FPinVec2 Starts[] = {
			FPinVec2(27.0, 40.0), FPinVec2(10.0, 60.0), FPinVec2(45.0, 60.0), FPinVec2(27.0, 78.0),
			FPinVec2(12.0, 30.0), FPinVec2(42.0, 30.0), FPinVec2(27.0, 95.0), FPinVec2(20.0, 75.0),
			FPinVec2(33.0, 77.0), FPinVec2(27.0, 25.0),
		};
		FLcg Rng{ 4242u };
		bool bEscaped = false;
		bool bIntoLane = false;
		for (int Trial = 0; Trial < 200; ++Trial)
		{
			FPinballSim Sim;
			Sim.StartGame();
			const FPinballTableLayout& L = Sim.Layout();
			const double A = Rng.Next01() * 2.0 * Pi;
			Sim.PlaceBall(Starts[Trial % 10], FPinVec2(std::cos(A), std::sin(A)) * FPinballSim::MaxBallSpeed);
			for (int Step = 0; Step < 3600; ++Step)
			{
				Sim.Substep();
				if (Sim.Phase() != EPinballPhase::InPlay)
				{
					break;
				}
				const FPinVec2 P = Sim.BallPosition();
				bEscaped = bEscaped || P.X < -0.5 || P.X > L.Width + 0.5 || P.Y > L.Length + 0.5;
				bIntoLane = bIntoLane || (P.X > L.ShooterLaneMinX + 0.5 && P.Y < 87.0);
			}
		}
		Check(!bEscaped, "Singalong: 200 max-speed shots never leave the cabinet");
		Check(!bIntoLane, "...and never get into the shooter lane from the playfield");
	}
	{
		// THE TRAP DETECTOR. Play whole games with the flippers flapping on a timer. A ball
		// wedged anywhere on the table would keep a game alive for ever; every game must end.
		int GamesFinished = 0;
		double LongestGame = 0.0;
		for (int Game = 0; Game < 10; ++Game)
		{
			FPinballSim Sim;
			Sim.StartGame();
			double Seconds = 0.0;
			const int Period = 20 + Game * 3;   // a different rhythm each game
			for (int Frame = 0; Frame < 60 * 900 && Sim.Phase() != EPinballPhase::GameOver; ++Frame)
			{
				const bool bFlip = (Frame / Period) % 2 == 0;
				Sim.SetFlipper(0, bFlip);
				Sim.SetFlipper(1, !bFlip);
				if (Sim.Phase() == EPinballPhase::BallReady)
				{
					Sim.Launch(0.6 + 0.04 * Game);
				}
				Sim.Advance(1.0 / 60.0);
				Sim.TakeEvents();
				Seconds += 1.0 / 60.0;
			}
			if (Sim.Phase() == EPinballPhase::GameOver)
			{
				++GamesFinished;
			}
			LongestGame = Seconds > LongestGame ? Seconds : LongestGame;
		}
		Check(GamesFinished == 10, "Singalong: ten whole games all end - nothing on the table traps the ball",
			Fmt("%d of 10 finished, longest %.0f s", GamesFinished, LongestGame));
	}

	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
