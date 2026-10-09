// kart_sim_test.cpp — standalone tests for Source/SibeliusGame/KartSim.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_kart_sim_tests.ps1.
//
// Four kinds of claim. TRACK: the corridor is the shape it was asked for - its length, its
// corners' radius, and a point found on it is found where it was put. HANDLING: the kart
// reaches its top speed and no more, stops in the distance physics says, turns, reverses,
// slides off a wall instead of through it, and a drift held through a corner pays out a
// boost sized to how long it was held. RACING: laps count only the right way round, five
// agents finish three laps of every test track without ever leaving the road, and the items
// do what they say. WHOLE RACES MUST END: hundreds of races with a player mashing the
// controls at random all finish by the rules, and the same seed runs the same race twice.

#include "KartSim.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
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

	double Seconds(std::chrono::steady_clock::time_point Since)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Since).count();
	}

	struct FLcg
	{
		uint32_t State;
		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) / 16777216.0;
		}
	};

	constexpr double Half = 600.0;      // a city street and its kerbs: 12 m of road
	constexpr double Radius = 900.0;
	constexpr double Pi = 3.14159265358979323846;

	FKartTrack MakeTrack(const std::vector<std::pair<double, double>>& Corners, double HalfWidth = Half, double R = Radius)
	{
		std::vector<FKartTrackPoint> Points;
		for (const auto& C : Corners)
		{
			FKartTrackPoint P;
			P.X = C.first;
			P.Y = C.second;
			Points.push_back(P);
		}
		FKartTrack Track;
		std::string Error;
		if (!Track.Build(Points, HalfWidth, R, 150.0, &Error))
		{
			std::printf("BAD TEST TRACK: %s\n", Error.c_str());
		}
		return Track;
	}

	// The four test circuits: an anticlockwise square, a clockwise city block, an L with a
	// right-hand corner among the lefts, and a straight with a chicane in it.
	FKartTrack Square() { return MakeTrack({ { 0, 0 }, { 10000, 0 }, { 10000, 10000 }, { 0, 10000 } }); }
	FKartTrack Block() { return MakeTrack({ { 0, 0 }, { 0, 8000 }, { 20000, 8000 }, { 20000, 0 } }); }
	FKartTrack Ell() { return MakeTrack({ { 0, 0 }, { 16000, 0 }, { 16000, 6000 }, { 8000, 6000 }, { 8000, 14000 }, { 0, 14000 } }); }
	FKartTrack Chicane() { return MakeTrack({ { 0, 0 }, { 12000, 0 }, { 14000, 3000 }, { 16000, 0 }, { 26000, 0 }, { 26000, 9000 }, { 0, 9000 } }); }
	FKartTrack LongStraight() { return MakeTrack({ { 0, 0 }, { 40000, 0 }, { 40000, 8000 }, { 0, 8000 } }); }

	// The city block again, with a bottleneck on its far side: the road tapers from 6 m
	// either side of the line to 4.3 m over 20 m, stays narrow for 40 m, and opens again -
	// the pedestrian crossing in the middle of Transhuman City's westernmost street.
	FKartTrack Bottleneck()
	{
		std::vector<FKartTrackPoint> Points;
		const double Corners[8][3] = {
			{ 0, 0, 0 }, { 0, 8000, 0 }, { 20000, 8000, 0 }, { 20000, 0, 0 },
			{ 14000, 0, 0 }, { 12000, 0, 430 }, { 8000, 0, 430 }, { 6000, 0, 0 },
		};
		for (const auto& C : Corners)
		{
			FKartTrackPoint P;
			P.X = C[0];
			P.Y = C[1];
			P.HalfWidth = C[2];
			Points.push_back(P);
		}
		FKartTrack Track;
		std::string Error;
		if (!Track.Build(Points, Half, Radius, 150.0, &Error))
		{
			std::printf("BAD TEST TRACK: %s\n", Error.c_str());
		}
		return Track;
	}

	FKartRaceSetup Solo(int Laps = 3)
	{
		FKartRaceSetup S;
		S.Karts = 1;
		S.Player = 0;
		S.Laps = Laps;
		S.CountdownSeconds = 0.0;
		return S;
	}

	void Run(FKartRace& Race, double Time, const FKartInput& In)
	{
		const double Dt = Race.Params.StepSeconds;
		for (double T = 0.0; T < Time; T += Dt)
		{
			Race.SetInput(0, In);
			Race.Advance(Dt);
		}
	}

	bool HasEvent(const std::vector<FKartEventRecord>& Events, EKartEvent Type, int Kart = -2)
	{
		for (const FKartEventRecord& E : Events)
		{
			if (E.Type == Type && (Kart == -2 || E.Kart == Kart))
			{
				return true;
			}
		}
		return false;
	}

	// -------------------------------------------------------------------------------------
	// TRACK

	void TestTrack()
	{
		const FKartTrack Sq = Square();
		const double Expected = 4.0 * (10000.0 - 2.0 * Radius) + 2.0 * Pi * Radius;
		Check(Sq.IsBuilt() && std::abs(Sq.Length() - Expected) / Expected < 0.002, "the square is as long as its straights and arcs",
			Fmt("%.1f cm, expected %.1f", Sq.Length(), Expected));
		Check(Sq.MinRadius() == Radius, "its corners have the radius asked for");

		double MaxCurve = 0.0;
		double MinCurve = 0.0;
		for (double S = 0.0; S < Sq.Length(); S += 50.0)
		{
			MaxCurve = std::max(MaxCurve, Sq.CurvatureAt(S));
			MinCurve = std::min(MinCurve, Sq.CurvatureAt(S));
		}
		Check(std::abs(MaxCurve - 1.0 / Radius) < 1e-9 && MinCurve == 0.0, "an anticlockwise square turns only left, at one over its radius");
		const FKartTrack Bl = Block();
		double BlockMin = 0.0;
		for (double S = 0.0; S < Bl.Length(); S += 50.0)
		{
			BlockMin = std::min(BlockMin, Bl.CurvatureAt(S));
		}
		Check(std::abs(BlockMin + 1.0 / Radius) < 1e-9, "a clockwise block turns right");

		// Found where it was put: points across the whole width, all the way round - anywhere
		// at all with no hint, and walking round the loop with each answer the next one's hint,
		// the way a kart is tracked.
		FLcg Rng{ 11u };
		double WorstS = 0.0;
		double WorstD = 0.0;
		for (int I = 0; I < 400; ++I)
		{
			const double S = Sq.Length() * Rng.Next01();
			const double D = (Rng.Next01() * 2.0 - 1.0) * (Half - 100.0);
			const FKartVec P = Sq.PointAt(S) + Sq.TangentAt(S).Left() * D;
			const FKartTrack::FFrame F = Sq.Locate(P);
			WorstS = std::max(WorstS, std::abs(Sq.Gap(S, F.S)));
			WorstD = std::max(WorstD, std::abs(F.D - D));
		}
		int Hint = -1;
		double WalkS = 0.0;
		double WalkD = 0.0;
		for (double S = 0.0; S < Sq.Length() * 2.0; S += 37.0)
		{
			const double D = std::sin(S * 0.003) * (Half - 100.0);
			const FKartVec P = Sq.PointAt(S) + Sq.TangentAt(S).Left() * D;
			const FKartTrack::FFrame F = Sq.Locate(P, Hint);
			Hint = F.Segment;
			WalkS = std::max(WalkS, std::abs(Sq.Gap(S, F.S)));
			WalkD = std::max(WalkD, std::abs(F.D - D));
		}
		// Off the centre line on an arc, a point projects a little along from where its offset
		// started (the arc's own geometry): a few tens of centimetres, never another stretch.
		Check(WorstS < 450.0 && WorstD < 3.0 && WalkS < 450.0 && WalkD < 3.0, "a point is located where it was put, on straights and arcs",
			Fmt("anywhere: worst %.1f cm along, %.2f across; walking: %.1f along, %.2f across", WorstS, WorstD, WalkS, WalkD));

		Check(Sq.Gap(Sq.Length() - 100.0, 100.0) == 200.0 && Sq.Gap(100.0, Sq.Length() - 100.0) == -200.0,
			"distances along the loop go the short way round, across the line");

		FKartTrack Bad;
		std::string Why;
		std::vector<FKartTrackPoint> Corners(4);
		Corners[1].X = 10000.0;
		Corners[2].X = 10000.0;
		Corners[2].Y = 10000.0;
		Corners[3].Y = 10000.0;
		Check(!Bad.Build(Corners, 600.0, 500.0, 150.0, &Why), "refuses corners tighter than the road is wide", Why);
		Check(!Bad.Build(Corners, 600.0, 6000.0, 150.0, &Why), "refuses corners too round for their straights", Why);
		Corners.resize(2);
		Check(!Bad.Build(Corners, 600.0, 900.0, 150.0, &Why), "refuses fewer than three corners", Why);

		// Widths: tapering along a straight, and a corner's own width held to its radius.
		const FKartTrack Neck = Bottleneck();
		double Narrowest = 1e9;
		double Widest = 0.0;
		for (double S = 0.0; S < Neck.Length(); S += 25.0)
		{
			Narrowest = std::min(Narrowest, Neck.HalfWidthAt(S));
			Widest = std::max(Widest, Neck.HalfWidthAt(S));
		}
		const FKartTrack::FFrame Middle = Neck.Locate(FKartVec(10000.0, 0.0));
		const FKartTrack::FFrame Taper = Neck.Locate(FKartVec(13000.0, 0.0));
		Check(Neck.IsBuilt() && std::abs(Narrowest - 430.0) < 0.5 && std::abs(Widest - Half) < 0.5
			&& std::abs(Neck.HalfWidthAt(Middle.S) - 430.0) < 0.5 && std::abs(Neck.HalfWidthAt(Taper.S) - 515.0) < 2.0,
			"a road narrows where its points say, tapering in and out",
			Fmt("narrowest %.1f, widest %.1f, in the middle %.1f, half way down the taper %.1f",
				Narrowest, Widest, Neck.HalfWidthAt(Middle.S), Neck.HalfWidthAt(Taper.S)));
		std::vector<FKartTrackPoint> Wide(4);
		Wide[1].X = 10000.0;
		Wide[2].X = 10000.0;
		Wide[2].Y = 10000.0;
		Wide[3].Y = 10000.0;
		Wide[2].HalfWidth = 950.0;
		Check(!Bad.Build(Wide, 600.0, 900.0, 150.0, &Why), "refuses a corner wider than its radius", Why);
	}

	// -------------------------------------------------------------------------------------
	// HANDLING

	void TestHandling()
	{
		FKartRace Race;
		const FKartTrack Track = LongStraight();
		Race.Start(Track, Solo());
		FKartInput Full;
		Full.Throttle = 1.0;
		double Top = 0.0;
		const double Dt = Race.Params.StepSeconds;
		for (double T = 0.0; T < 5.0; T += Dt)
		{
			Race.SetInput(0, Full);
			Race.Advance(Dt);
			Top = std::max(Top, Race.Karts()[0].Speed());
		}
		const double Max = Race.Params.MaxSpeed;
		const double At5 = Race.Karts()[0].Speed();
		Check(At5 > 0.9 * Max && Top <= Max * 1.001, "full throttle reaches its top speed in five seconds, and no more",
			Fmt("%.0f cm/s at 5 s, top %.0f, max %.0f", At5, Top, Max));

		const FKartVec Before = Race.Karts()[0].P;
		const double From = Race.Karts()[0].Speed();
		FKartInput Stop;
		Stop.Throttle = -1.0;
		double T = 0.0;
		while (Race.Karts()[0].ForwardSpeed() > 5.0 && T < 5.0)
		{
			Race.SetInput(0, Stop);
			Race.Advance(Dt);
			T += Dt;
		}
		const double Stopped = (Race.Karts()[0].P - Before).Length();
		const double Physics = From * From / (2.0 * Race.Params.Brake);
		Check(std::abs(Stopped - Physics) / Physics < 0.2, "the brakes stop it in the distance the brakes say",
			Fmt("%.0f cm, v^2/2a = %.0f", Stopped, Physics));

		Run(Race, 3.0, Stop);
		const double Back = Race.Karts()[0].ForwardSpeed();
		Check(Back < -100.0 && Back > -Race.Params.MaxReverse * 1.01, "held, the brake becomes reverse, and a slow one",
			Fmt("%.0f cm/s", Back));

		FKartRace Turn;
		Turn.Start(Track, Solo());
		FKartInput Gentle;
		Gentle.Throttle = 0.5;
		Run(Turn, 1.0, Gentle);
		const double H0 = Turn.Karts()[0].Heading;
		FKartInput Left = Gentle;
		Left.Steer = 1.0;
		Run(Turn, 0.5, Left);
		const double Turned = Turn.Karts()[0].Heading - H0;
		Check(Turned > 0.3, "steering left turns it left", Fmt("%.2f rad in half a second", Turned));
	}

	void TestWalls()
	{
		FKartRace Race;
		const FKartTrack Track = LongStraight();
		Race.Start(Track, Solo());
		const FKartState& K = Race.Karts()[0];
		const double S = K.S;
		const FKartVec T = Track.TangentAt(S);
		// Straight at the left-hand wall at full speed, sixty degrees off the road.
		const double Heading = std::atan2(T.Y, T.X) + Pi / 3.0;
		Race.Place(0, Track.PointAt(S), Heading, FKartVec(std::cos(Heading), std::sin(Heading)) * Race.Params.MaxSpeed);
		FKartInput Full;
		Full.Throttle = 1.0;
		double Worst = 0.0;
		bool bHit = false;
		const double Limit = Track.HalfWidth() - Race.Params.KartRadius;
		const double Dt = Race.Params.StepSeconds;
		for (double Time = 0.0; Time < 2.0; Time += Dt)
		{
			Race.SetInput(0, Full);
			Race.Advance(Dt);
			Worst = std::max(Worst, std::abs(K.D) - Limit);
			bHit = bHit || HasEvent(Race.TakeEvents(), EKartEvent::WallHit, 0);
		}
		Check(bHit && Worst < 0.5, "a kart driven into the wall stops at it, not through it", Fmt("worst %.2f cm past", Worst));
	}

	void TestDrift()
	{
		// A round track - a twelve-sided figure whose corners meet, a 15 m circle - so a drift
		// can be held as long as it likes. The driver does what a player does in a drift:
		// steers into it, and eases off when the kart slides toward the inside.
		const FKartTrack Track = MakeTrack({
			{ 1553, 0 }, { 1345, 777 }, { 777, 1345 }, { 0, 1553 }, { -777, 1345 }, { -1345, 777 },
			{ -1553, 0 }, { -1345, -777 }, { -777, -1345 }, { 0, -1553 }, { 777, -1345 }, { 1345, -777 } }, 600.0, 1450.0);
		auto DriftFor = [&Track](double Hold)
		{
			FKartRace Race;
			Race.Start(Track, Solo());
			const FKartState& K = Race.Karts()[0];
			const FKartVec T = Track.TangentAt(K.S);
			Race.Place(0, Track.PointAt(K.S), std::atan2(T.Y, T.X), T * 1400.0);
			Race.TakeEvents();
			const double Dt = Race.Params.StepSeconds;
			for (double Time = 0.0; Time < Hold; Time += Dt)
			{
				FKartInput In;
				In.Throttle = 1.0;
				In.bDrift = true;
				const FKartVec Ahead = Track.TangentAt(K.S + 300.0);
				double Error = std::atan2(Ahead.Y, Ahead.X) - K.Heading;
				while (Error > Pi) Error -= 2.0 * Pi;
				while (Error < -Pi) Error += 2.0 * Pi;
				In.Steer = std::max(-1.0, std::min(1.0, 0.4 + 2.0 * Error - 0.002 * K.D));
				Race.SetInput(0, In);
				Race.Advance(Dt);
			}
			FKartInput Let;
			Let.Throttle = 1.0;
			Run(Race, 0.05, Let);
			double Paid = 0.0;
			for (const FKartEventRecord& E : Race.TakeEvents())
			{
				if (E.Type == EKartEvent::Boosted)
				{
					Paid = E.Value;
				}
			}
			return Paid;
		};
		const double Short = DriftFor(0.4);
		const double Mini = DriftFor(1.1);
		const double Super = DriftFor(2.2);
		Check(Short == 0.0 && Mini > 0.0 && Super > Mini, "a drift pays a boost sized to how long it was held",
			Fmt("0.4 s: %.1f, 1.1 s: %.1f, 2.2 s: %.1f", Short, Mini, Super));
	}

	// -------------------------------------------------------------------------------------
	// RACING

	void TestLaps()
	{
		const FKartTrack Track = LongStraight();
		FKartRace Race;
		Race.Start(Track, Solo(2));
		const FKartState& K = Race.Karts()[0];
		Check(K.Lap == -1 && K.Progress < 0.0, "the grid is behind the line", Fmt("lap %d, progress %.0f", K.Lap, K.Progress));
		FKartInput Go;
		Go.Throttle = 1.0;
		Run(Race, 1.5, Go);
		Check(K.Lap == 0 && K.Progress > 0.0, "over the line, the race has begun", Fmt("lap %d, progress %.0f", K.Lap, K.Progress));

		// Back over it, and forward again: no lap is gained, and progress never jumps.
		FKartInput Back;
		Back.Throttle = -1.0;
		double Last = K.Progress;
		double Jump = 0.0;
		bool bWentBack = false;
		const double Dt = Race.Params.StepSeconds;
		for (double T = 0.0; T < 8.0; T += Dt)
		{
			Race.SetInput(0, Back);
			Race.Advance(Dt);
			Jump = std::max(Jump, std::abs(K.Progress - Last));
			Last = K.Progress;
			bWentBack = bWentBack || K.Lap == -1;
		}
		for (double T = 0.0; T < 4.0; T += Dt)
		{
			Race.SetInput(0, Go);
			Race.Advance(Dt);
			Jump = std::max(Jump, std::abs(K.Progress - Last));
			Last = K.Progress;
		}
		bool bLapEvent = false;
		for (const FKartEventRecord& E : Race.TakeEvents())
		{
			bLapEvent = bLapEvent || E.Type == EKartEvent::Lap;
		}
		Check(bWentBack && K.Lap == 0 && !bLapEvent && Jump < 100.0, "reversing over the line and back gains nothing",
			Fmt("lap %d, biggest step %.1f cm", K.Lap, Jump));
	}

	struct FRaceLog
	{
		bool bOver = false;
		double Clock = 0.0;
		double WorstOutside = -1e9;
		bool bNaN = false;
		int Finished = 0;
		int WallHits = 0;
		int KartHits = 0;
		int Items = 0;
		int Spins = 0;
		double First = 0.0;
		double Last = 0.0;
		std::vector<int> Order;
	};

	template <typename TPlayer>
	FRaceLog RunRace(FKartRace& Race, double Limit, TPlayer Player)
	{
		FRaceLog Log;
		const double Dt = Race.Params.StepSeconds;
		double T = 0.0;
		while (Race.Phase() != EKartPhase::Over && T < Limit)
		{
			Player(Race, T);
			Race.Advance(Dt);
			T += Dt;
			for (const FKartState& K : Race.Karts())
			{
				const double Edge = Race.Track().HalfWidthAt(K.S) - Race.Params.KartRadius;
				Log.WorstOutside = std::max(Log.WorstOutside, std::abs(K.D) - Edge);
				Log.bNaN = Log.bNaN || !(K.P.X == K.P.X) || !(K.P.Y == K.P.Y) || !(K.V.X == K.V.X) || !(K.Heading == K.Heading);
			}
			for (const FKartEventRecord& E : Race.TakeEvents())
			{
				Log.WallHits += E.Type == EKartEvent::WallHit ? 1 : 0;
				Log.KartHits += E.Type == EKartEvent::KartHit ? 1 : 0;
				Log.Items += E.Type == EKartEvent::UsedItem ? 1 : 0;
				Log.Spins += E.Type == EKartEvent::SpunOut ? 1 : 0;
			}
		}
		Log.bOver = Race.Phase() == EKartPhase::Over;
		Log.Clock = Race.Clock();
		Log.First = 1e9;
		for (const FKartState& K : Race.Karts())
		{
			if (K.bFinished)
			{
				++Log.Finished;
				Log.First = std::min(Log.First, K.FinishTime);
				Log.Last = std::max(Log.Last, K.FinishTime);
			}
		}
		Log.Order = Race.Standings();
		return Log;
	}

	void TestAgentRaces()
	{
		const std::pair<const char*, FKartTrack> Tracks[] = {
			{ "square", Square() }, { "city block", Block() }, { "L", Ell() }, { "chicane", Chicane() },
			{ "bottleneck", Bottleneck() },
		};
		for (const auto& Entry : Tracks)
		{
			const FKartTrack& Track = Entry.second;
			FKartRace Race;
			FKartRaceSetup Setup;
			Setup.Player = -1;
			Setup.Seed = 7;
			Setup.Skill = { 1.0, 0.98, 0.96, 0.99, 0.97, 0.95 };
			Race.Start(Track, Setup);
			const auto Clock = std::chrono::steady_clock::now();
			const FRaceLog Log = RunRace(Race, 900.0, [](FKartRace&, double) {});
			const double Ideal = 3.0 * Track.Length() / Race.Params.MaxSpeed;
			Check(Log.bOver && Log.Finished == 6 && !Log.bNaN && Log.WorstOutside < 0.5,
				Fmt("five agents and one more finish three laps of the %s, on the road", Entry.first).c_str(),
				Fmt("%d finished in %.1f-%.1f s (flat out %.1f s), %d wall hits, %d bumps, %d items, %d spins; worst %.2f cm off; %.2f s to run",
					Log.Finished, Log.First, Log.Last, Ideal, Log.WallHits, Log.KartHits, Log.Items, Log.Spins, Log.WorstOutside, Seconds(Clock)));
			Check(Log.First > Ideal * 0.85 && Log.Last < Ideal * 3.0, Fmt("and at a racing pace on the %s", Entry.first).c_str(),
				Fmt("winner %.1f s, last %.1f s, flat out %.1f s", Log.First, Log.Last, Ideal));
		}
	}

	void TestItems()
	{
		const FKartTrack Track = LongStraight();
		const double Dt = 1.0 / 120.0;

		// Sauce: faster than the kart's own best, for a while.
		{
			FKartRace Race;
			Race.Start(Track, Solo());
			FKartInput Full;
			Full.Throttle = 1.0;
			Run(Race, 4.0, Full);
			Race.Give(0, EKartItem::Sauce);
			FKartInput Use = Full;
			Use.bUseItem = true;
			Run(Race, Dt, Use);
			double Top = 0.0;
			for (double T = 0.0; T < 1.4; T += Dt)
			{
				Race.SetInput(0, Full);
				Race.Advance(Dt);
				Top = std::max(Top, Race.Karts()[0].Speed());
			}
			Check(Race.Karts()[0].Item == EKartItem::None && Top > Race.Params.MaxSpeed * 1.1, "Sauce boosts past the kart's own top speed",
				Fmt("%.0f cm/s against %.0f", Top, Race.Params.MaxSpeed));
		}

		// Slap: the kart alongside spins.
		{
			FKartRace Race;
			FKartRaceSetup Setup = Solo();
			Setup.Karts = 2;
			Race.Start(Track, Setup);
			const FKartState& Me = Race.Karts()[0];
			const FKartVec T = Track.TangentAt(Me.S);
			Race.Place(1, Me.P + T * 400.0, Me.Heading, FKartVec());
			Race.Give(0, EKartItem::Slap);
			Race.TakeEvents();
			FKartInput Use;
			Use.bUseItem = true;
			Run(Race, Dt, Use);
			bool bSpun = false;
			for (const FKartEventRecord& E : Race.TakeEvents())
			{
				bSpun = bSpun || (E.Type == EKartEvent::SpunOut && E.Kart == 1 && E.Other == 0);
			}
			Check(bSpun && Race.Karts()[1].SpinLeft > 0.0, "Slap spins out the kart just ahead");
		}

		// Generate: an obstacle behind, and whoever drives into it spins - its maker included.
		{
			FKartRace Race;
			Race.Start(Track, Solo());
			FKartInput Full;
			Full.Throttle = 1.0;
			Run(Race, 2.0, Full);
			Race.Give(0, EKartItem::Generate);
			FKartInput Use = Full;
			Use.bUseItem = true;
			Run(Race, Dt, Use);
			const bool bDropped = Race.Obstacles().size() == 1;
			const FKartVec Rock = bDropped ? Race.Obstacles()[0].P : FKartVec();
			const FKartState& Me = Race.Karts()[0];
			const FKartVec T = Track.TangentAt(Me.S);
			Race.Place(0, Rock - T * 600.0, std::atan2(T.Y, T.X), T * 1500.0);
			Race.TakeEvents();
			Run(Race, 1.0, Full);
			Check(bDropped && HasEvent(Race.TakeEvents(), EKartEvent::SpunOut, 0) && Race.Obstacles().empty(),
				"Generate drops an obstacle that spins out whoever hits it");
		}

		// Refactor: the kart ahead becomes a chest, slow and empty-handed.
		{
			FKartRace Race;
			FKartRaceSetup Setup = Solo();
			Setup.Karts = 2;
			Setup.Player = 1;
			Race.Start(Track, Setup);
			FKartInput Full;
			Full.Throttle = 1.0;
			const double DtStep = Race.Params.StepSeconds;
			for (double T = 0.0; T < 3.0; T += DtStep)
			{
				Race.SetInput(1, Full);
				Race.Advance(DtStep);
			}
			// The agent twenty metres up the road, at speed: she leads.
			const FKartState& Me = Race.Karts()[1];
			const double AheadS = Me.S + 2000.0;
			const FKartVec T = Track.TangentAt(AheadS);
			Race.Place(0, Track.PointAt(AheadS), std::atan2(T.Y, T.X), T * Race.Params.MaxSpeed * 0.9);
			Race.SetInput(1, Full);
			Race.Advance(DtStep);
			const bool bSheLeads = Race.Standings()[0] == 0;
			Race.Give(1, EKartItem::Refactor);
			FKartInput Use = Full;
			Use.bUseItem = true;
			Race.SetInput(1, Use);
			Race.Advance(DtStep);
			for (double Time = 0.0; Time < 1.5; Time += DtStep)
			{
				Race.SetInput(1, Full);
				Race.Advance(DtStep);
			}
			const FKartState& Chest = Race.Karts()[0];
			Check(bSheLeads && Chest.ChestLeft > 0.0 && Chest.Speed() < Race.Params.MaxSpeed * 0.6,
				"Refactor turns the kart ahead into a slow treasure chest",
				Fmt("%.1f s left, %.0f cm/s", Chest.ChestLeft, Chest.Speed()));
		}

		// The boxes: driven through, an item; a few seconds later, the box is back.
		{
			FKartRace Race;
			Race.Start(Track, Solo());
			const FKartBox Box = Race.Boxes()[1];
			const FKartVec T = Track.TangentAt(Box.S);
			Race.Place(0, Box.P - T * 900.0, std::atan2(T.Y, T.X), T * 1500.0);
			Race.TakeEvents();
			FKartInput Full;
			Full.Throttle = 1.0;
			Run(Race, 1.0, Full);
			const bool bGot = HasEvent(Race.TakeEvents(), EKartEvent::Pickup, 0) && Race.Karts()[0].Item != EKartItem::None;
			const bool bGone = Race.Boxes()[1].RespawnLeft > 0.0;
			Run(Race, Race.Params.BoxRespawn + 0.1, Full);
			Check(bGot && bGone && Race.Boxes()[1].RespawnLeft == 0.0, "an item box gives an item, and comes back",
				Fmt("item %s", KartItemName(Race.Karts()[0].Item)));
		}
	}

	void TestBumps()
	{
		const FKartTrack Track = LongStraight();
		FKartRace Race;
		FKartRaceSetup Setup = Solo();
		Setup.Karts = 2;
		Race.Start(Track, Setup);
		const FKartState& A = Race.Karts()[0];
		const FKartVec T = Track.TangentAt(A.S);
		const double H = std::atan2(T.Y, T.X);
		Race.Place(0, Track.PointAt(A.S), H, T * 1200.0);
		Race.Place(1, Track.PointAt(A.S) + T * 150.0, H + Pi, T * -1200.0);
		Race.Advance(Race.Params.StepSeconds);
		const double Gap = (Race.Karts()[1].P - Race.Karts()[0].P).Length();
		const double Closing = (Race.Karts()[1].V - Race.Karts()[0].V).Dot(T);
		Check(Gap >= 2.0 * Race.Params.KartRadius - 1.0 && Closing >= 0.0, "two karts that meet head on bounce apart, never through",
			Fmt("%.1f cm apart, opening at %.0f cm/s", Gap, Closing));
	}

	// -------------------------------------------------------------------------------------
	// WHOLE RACES MUST END - the lesson the pinball's wedge taught.

	void TestRandomRaces()
	{
		const auto Clock = std::chrono::steady_clock::now();
		const FKartTrack Tracks[5] = { Square(), Block(), Ell(), Chicane(), Bottleneck() };
		FLcg Rng{ 20261009u };
		int Ended = 0;
		int Broken = 0;
		int PlayerFinished = 0;
		std::string First;
		const int Races = 60;
		for (int R = 0; R < Races; ++R)
		{
			FKartRace Race;
			FKartRaceSetup Setup;
			Setup.Seed = static_cast<uint32_t>(1000 + R);
			Setup.Player = static_cast<int>(Rng.Next01() * 6.0);
			Setup.AiPace = 0.85 + 0.15 * Rng.Next01();
			Setup.CountdownSeconds = 1.0;
			Race.Start(Tracks[R % 5], Setup);
			FKartInput Held;
			double Change = 0.0;
			const FRaceLog Log = RunRace(Race, 1200.0, [&](FKartRace& InRace, double T)
			{
				if (T >= Change)
				{
					// Mostly forward, often turning, sometimes drifting, now and then backwards.
					Held.Throttle = Rng.Next01() < 0.8 ? 1.0 : -1.0;
					Held.Steer = Rng.Next01() * 2.0 - 1.0;
					Held.bDrift = Rng.Next01() < 0.3;
					Held.bUseItem = Rng.Next01() < 0.3;
					Change = T + 0.2 + 0.8 * Rng.Next01();
				}
				InRace.SetInput(Setup.Player, Held);
			});
			Ended += Log.bOver ? 1 : 0;
			PlayerFinished += Race.Karts()[static_cast<size_t>(Setup.Player)].bFinished ? 1 : 0;
			const bool bBad = Log.bNaN || Log.WorstOutside > 0.5 || !Log.bOver;
			Broken += bBad ? 1 : 0;
			if (bBad && First.empty())
			{
				First = Fmt("race %d: over %d, NaN %d, worst %.2f cm off", R, Log.bOver ? 1 : 0, Log.bNaN ? 1 : 0, Log.WorstOutside);
			}
		}
		Check(Broken == 0 && Ended == Races, "races with a player mashing the controls all end, on the road",
			Fmt("%d of %d ended, the random player finished %d; %.1f s", Ended, Races, PlayerFinished, Seconds(Clock)) + (First.empty() ? "" : " " + First));
	}

	void TestSameSeedSameRace()
	{
		auto RaceOnce = []()
		{
			FKartRace Race;
			FKartRaceSetup Setup;
			Setup.Player = -1;
			Setup.Seed = 99;
			Race.Start(Chicane(), Setup);
			RunRace(Race, 900.0, [](FKartRace&, double) {});
			std::vector<double> Times;
			for (const FKartState& K : Race.Karts())
			{
				Times.push_back(K.FinishTime);
			}
			return Times;
		};
		Check(RaceOnce() == RaceOnce(), "the same seed runs the same race");
	}
}

int main()
{
	TestTrack();
	TestHandling();
	TestWalls();
	TestDrift();
	TestLaps();
	TestBumps();
	TestItems();
	TestAgentRaces();
	TestSameSeedSameRace();
	TestRandomRaces();

	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
