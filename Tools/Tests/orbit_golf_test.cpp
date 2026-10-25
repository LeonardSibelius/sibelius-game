// orbit_golf_test.cpp — standalone tests for Source/SibeliusGame/OrbitGolfSim, the orbital golf
// course over Elise's grassland. Run with Tools/Tests/run_orbit_golf_tests.ps1.
//
// PHYSICAL CLAIMS, as at the pool tables: energy is kept in flight, a circular orbit stays
// circular, a ball dropped on a moon stays on it, a ball under a strong moon falls UP onto its
// underside - and a robot golfer, aiming by brute force, finishes every hole, so no hole of the
// course is impossible.

#include "OrbitGolfSim.h"

#include <cmath>
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
		(bOk ? GPasses : GFailures)++;
	}

	template <typename... TArgs>
	std::string Fmt(const char* Format, TArgs... Args)
	{
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	double Dot(FSpaceVec A, FSpaceVec B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	double Len(FSpaceVec A) { return std::sqrt(Dot(A, A)); }

	bool Has(const std::vector<FOrbitEvent>& Events, EOrbitEvent Type)
	{
		for (const FOrbitEvent& E : Events)
		{
			if (E.Type == Type)
			{
				return true;
			}
		}
		return false;
	}

	// Runs until the ball stops flying (or Seconds), collecting the events.
	std::vector<FOrbitEvent> Fly(FOrbitGolfSim& Sim, double Seconds = 25.0)
	{
		std::vector<FOrbitEvent> All;
		for (double T = 0.0; T < Seconds && Sim.Phase() == EOrbitPhase::Flying; T += FOrbitGolfSim::SubstepSeconds)
		{
			Sim.Substep();
			for (const FOrbitEvent& E : Sim.TakeEvents())
			{
				All.push_back(E);
			}
		}
		return All;
	}

	// One hole's course alone, the world's gravity as given.
	FOrbitGolfSim OneMoon(FOrbitMoon Moon, double WorldGravity)
	{
		FOrbitCourse C = FOrbitCourse::MakeDefault();
		C.WorldGravity = WorldGravity;
		C.Holes.resize(1);
		C.Holes[0].Moons = { Moon };
		C.Holes[0].CupMoon = 0;
		C.Holes[0].CupDirection = FSpaceVec(1.0, 0.0, 0.0);
		FOrbitGolfSim Sim(C);
		Sim.StartRound();
		Sim.TakeEvents();
		return Sim;
	}
}

static void TestGravity()
{
	const FOrbitMoon Moon{ FSpaceVec(2000.0, 0.0, 1000.0), 400.0, 2.0 * 981.0 };
	FOrbitGolfSim Sim = OneMoon(Moon, 981.0);
	const FSpaceVec Top = Sim.Gravity(FSpaceVec(2000.0, 0.0, 1400.0));
	const FSpaceVec Bottom = Sim.Gravity(FSpaceVec(2000.0, 0.0, 600.0));
	Check(std::fabs(Top.Z + 3.0 * 981.0) < 1e-6 && std::fabs(Bottom.Z - 981.0) < 1e-6,
		"on top of a 2g moon the pull is 3g down; underneath it, 1g up", Fmt("top %.1f, bottom %.1f", Top.Z, Bottom.Z));
}

static void TestEnergy()
{
	const FOrbitMoon Moon{ FSpaceVec(2500.0, 0.0, 1500.0), 400.0, 2.0 * 981.0 };
	FOrbitGolfSim Sim = OneMoon(Moon, 981.0);
	auto Energy = [&](FSpaceVec P, FSpaceVec V)
	{
		const double GM = Moon.SurfaceGravity * Moon.Radius * Moon.Radius;
		return 0.5 * Dot(V, V) + 981.0 * P.Z - GM / Len(P - Moon.Center);
	};
	Sim.PlaceBall(FSpaceVec(0.0, 0.0, 600.0), FSpaceVec(900.0, 200.0, 700.0));
	const double E0 = Energy(Sim.BallPosition(), Sim.BallVelocity());
	double Worst = 0.0;
	for (int S = 0; S < 480 * 2; ++S)
	{
		Sim.Substep();
		Worst = std::fmax(Worst, std::fabs(Energy(Sim.BallPosition(), Sim.BallVelocity()) - E0) / std::fabs(E0));
	}
	Check(Worst < 1e-7, "in free flight the Grand Tour's integrator keeps the energy", Fmt("worst drift %.2g", Worst));
}

static void TestOrbit()
{
	const FOrbitMoon Moon{ FSpaceVec(0.0, 0.0, 3000.0), 300.0, 2.0 * 981.0 };
	FOrbitGolfSim Sim = OneMoon(Moon, 0.0);
	const double R = 900.0;
	const double GM = Moon.SurfaceGravity * Moon.Radius * Moon.Radius;
	const double V = std::sqrt(GM / R);
	const double Period = 2.0 * Pi * R / V;
	Sim.PlaceBall(Moon.Center + FSpaceVec(R, 0.0, 0.0), FSpaceVec(0.0, V, 0.0));
	double Worst = 0.0;
	for (double T = 0.0; T < Period; T += FOrbitGolfSim::SubstepSeconds)
	{
		Sim.Substep();
		Worst = std::fmax(Worst, std::fabs(Len(Sim.BallPosition() - Moon.Center) - R));
	}
	Check(Worst < R * 0.001, "with the world's gravity off, a circular orbit stays circular for a whole turn",
		Fmt("worst radius error %.3f cm over %.1f s", Worst, Period));
}

static void TestRest()
{
	const FOrbitMoon Moon{ FSpaceVec(2000.0, 0.0, 1000.0), 400.0, 2.0 * 981.0 };
	{
		FOrbitGolfSim Sim = OneMoon(Moon, 981.0);
		Sim.PlaceBall(FSpaceVec(2000.0, 0.0, 1700.0), FSpaceVec());
		const std::vector<FOrbitEvent> Events = Fly(Sim);
		const FSpaceVec P = Sim.BallPosition();
		Check(Sim.Phase() == EOrbitPhase::Aiming && Has(Events, EOrbitEvent::CameToRest) && P.Z > Moon.Center.Z + Moon.Radius * 0.9,
			"a ball dropped on a moon's crown comes to rest on it", Fmt("rests at Z %.1f", P.Z));
	}
	{
		// Its pull beats the world's within R * sqrt(2.8) of its centre: let go 2 m below its
		// bottom, inside that.
		const FOrbitMoon Strong{ FSpaceVec(2000.0, 0.0, 1500.0), 450.0, 2.8 * 981.0 };
		FOrbitGolfSim Sim = OneMoon(Strong, 981.0);
		Sim.PlaceBall(FSpaceVec(2000.0, 30.0, 850.0), FSpaceVec());
		const std::vector<FOrbitEvent> Events = Fly(Sim);
		const FSpaceVec P = Sim.BallPosition();
		const double Bottom = Strong.Center.Z - Strong.Radius;
		Check(Sim.Phase() == EOrbitPhase::Aiming && !Has(Events, EOrbitEvent::OutOfBounds) && P.Z > 850.0 && P.Z < Bottom
				&& P.Z > Bottom - 2.0 * 8.0,
			"a ball let go under a moon stronger than the world falls UP, and rests on its underside", Fmt("rests at Z %.1f", P.Z));
	}
}

static void TestBounce()
{
	FOrbitGolfSim Sim = OneMoon(FOrbitMoon{ FSpaceVec(5000.0, 0.0, 5000.0), 100.0, 981.0 }, 981.0);
	Sim.PlaceBall(FSpaceVec(0.0, 0.0, 200.0), FSpaceVec());
	double Into = 0.0;
	double Out = 0.0;
	for (int S = 0; S < 480 && Out == 0.0; ++S)
	{
		const double Before = Sim.BallVelocity().Z;
		Sim.Substep();
		if (Before < 0.0 && Sim.BallVelocity().Z > 0.0)
		{
			Into = -Before;
			Out = Sim.BallVelocity().Z;
		}
	}
	Check(Out > 0.0 && std::fabs(Out / Into - FOrbitGolfSim::Restitution) < 0.03, "a drop on the tee bounces back at the restitution",
		Fmt("%.1f in, %.1f out", Into, Out));
}

static void TestPenalties()
{
	{
		FOrbitGolfSim Sim;
		Sim.StartRound();
		Sim.TakeEvents();
		Sim.Shoot(0.0, -0.05, 0.6);   // skimmed off the front of the tee and down into the grass
		const std::vector<FOrbitEvent> Events = Fly(Sim);
		Check(Has(Events, EOrbitEvent::OutOfBounds) && Sim.HoleStrokes() == 2 && Len(Sim.BallPosition() - FSpaceVec(0.0, 0.0, 8.0)) < 1e-9,
			"into the grass: a penalty stroke, and the ball goes back to the tee");
	}
	{
		const FOrbitMoon Moon{ FSpaceVec(0.0, 0.0, 3000.0), 300.0, 2.0 * 981.0 };
		FOrbitGolfSim Sim = OneMoon(Moon, 0.0);
		const double R = 900.0;
		Sim.PlaceBall(Moon.Center + FSpaceVec(R, 0.0, 0.0), FSpaceVec(0.0, std::sqrt(Moon.SurfaceGravity * Moon.Radius * Moon.Radius / R), 0.0));
		const std::vector<FOrbitEvent> Events = Fly(Sim, 25.0);
		Check(Has(Events, EOrbitEvent::LostInOrbit) && Sim.Phase() == EOrbitPhase::Aiming,
			"a ball still circling a moon after twenty seconds is lost in orbit: a penalty stroke");
	}
}

static void TestCupAndPrediction()
{
	{
		FOrbitGolfSim Sim;
		Sim.StartRound();
		const FOrbitHole& H = Sim.CurrentHole();
		Sim.PlaceBall(H.CupPoint() + H.CupDirection * 30.0, H.CupDirection * -50.0);
		const std::vector<FOrbitEvent> Events = Fly(Sim);
		Check(Sim.Phase() == EOrbitPhase::Holed && Has(Events, EOrbitEvent::Holed), "a ball dropping slowly onto the cup is in");
	}
	{
		FOrbitGolfSim Sim;
		Sim.StartRound();
		const std::vector<FSpaceVec> Guide = Sim.PredictPath(0.1, 0.6, 0.55, 1.0, 11);
		Sim.Shoot(0.1, 0.6, 0.55);
		double Worst = 0.0;
		int Next = 1;
		for (int S = 1; S <= 480 && Next < static_cast<int>(Guide.size()); ++S)
		{
			Sim.Substep();
			if (S % 48 == 0)
			{
				Worst = std::fmax(Worst, Len(Sim.BallPosition() - Guide[Next++]));
			}
		}
		Check(Guide.size() == 11 && Worst < 0.5, "the dotted guide is where the shot really goes", Fmt("worst %.4f cm", Worst));
	}
}

// The robot golfer: every stroke, try a grid of shots on copies of the game and play the one
// that ends nearest the cup - a hole-in-one if it finds one. Finishing every hole within twice
// its par plus two says the course can be played.
static void TestRobotGolfer()
{
	FOrbitGolfSim Sim;
	Sim.StartRound();
	bool bAllDone = true;
	std::string Card;
	for (int Hole = 0; Hole < static_cast<int>(Sim.Course().Holes.size()); ++Hole)
	{
		const int Par = Sim.CurrentHole().Par;
		for (int Stroke = 0; Stroke < 2 * Par + 2 && Sim.Phase() == EOrbitPhase::Aiming; ++Stroke)
		{
			const FSpaceVec Cup = Sim.CurrentHole().CupPoint();
			const FSpaceVec From = Sim.BallPosition();
			const double Toward = std::atan2(Cup.Y - From.Y, Cup.X - From.X);
			double BestScore = 1e30;
			double BestYaw = Toward, BestPitch = 0.5, BestPower = 0.5;
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				const double YawSpan = Pass == 0 ? 0.7 : 0.09;
				const double PitchLo = Pass == 0 ? -1.2 : BestPitch - 0.09;
				const double PitchSpan = Pass == 0 ? 2.6 : 0.18;
				const double PowerLo = Pass == 0 ? 0.05 : BestPower - 0.06;
				const double PowerSpan = Pass == 0 ? 0.95 : 0.12;
				const double CenterYaw = Pass == 0 ? Toward : BestYaw;
				const int N = Pass == 0 ? 9 : 5;
				for (int A = 0; A < N; ++A)
				{
					for (int B = 0; B < (Pass == 0 ? 14 : 5); ++B)
					{
						for (int C = 0; C < (Pass == 0 ? 8 : 5); ++C)
						{
							const double Yaw = CenterYaw - YawSpan + 2.0 * YawSpan * A / (N - 1);
							const double Pitch = PitchLo + PitchSpan * B / ((Pass == 0 ? 14 : 5) - 1);
							const double Power = PowerLo + PowerSpan * C / ((Pass == 0 ? 8 : 5) - 1);
							FOrbitGolfSim Try = Sim;
							if (!Try.Shoot(Yaw, Pitch, Power))
							{
								continue;
							}
							const std::vector<FOrbitEvent> Events = Fly(Try);
							double Score = Try.Phase() == EOrbitPhase::Holed ? -1.0 : Len(Try.BallPosition() - Cup);
							if (Has(Events, EOrbitEvent::OutOfBounds) || Has(Events, EOrbitEvent::LostInOrbit))
							{
								Score += 1e6;
							}
							if (Score < BestScore)
							{
								BestScore = Score;
								BestYaw = Yaw;
								BestPitch = Pitch;
								BestPower = Power;
							}
						}
					}
				}
			}
			Sim.Shoot(BestYaw, BestPitch, BestPower);
			Fly(Sim);
		}
		const bool bDone = Sim.Phase() == EOrbitPhase::Holed;
		bAllDone = bAllDone && bDone;
		Card += Fmt("%s %s (par %d)  ", Sim.CurrentHole().Name.c_str(), bDone ? std::to_string(Sim.HoleStrokes()).c_str() : "--", Par);
		if (!bDone)
		{
			break;
		}
		Sim.NextHole();
	}
	Check(bAllDone && Sim.Phase() == EOrbitPhase::Finished, "a robot golfer aiming by brute force finishes every hole", Card);
}

int main()
{
	TestGravity();
	TestEnergy();
	TestOrbit();
	TestRest();
	TestBounce();
	TestPenalties();
	TestCupAndPrediction();
	TestRobotGolfer();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
