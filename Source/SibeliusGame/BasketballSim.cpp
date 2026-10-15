// BasketballSim.cpp - see the header.

#include "BasketballSim.h"

#include <algorithm>
#include <cmath>

namespace BasketballSimDetail
{
	constexpr double Pi = 3.14159265358979323846;

	double DegToRad(double Degrees) { return Degrees * Pi / 180.0; }

	// Rack angles, measured from +X (the top of the key) toward +Y (the left): the corners sit
	// just above the baseline, the wings halfway round.
	constexpr double RackAngleDegrees[FBasketballContest::Racks] = { -80.0, -45.0, 0.0, 45.0, 80.0 };
}

const char* ShotResultName(EShotResult Result)
{
	switch (Result)
	{
	case EShotResult::Swish: return "swish";
	case EShotResult::RimIn: return "in off the rim";
	case EShotResult::RimOut: return "rims out";
	case EShotResult::Short: return "short";
	case EShotResult::Long: return "long";
	case EShotResult::Wide: return "wide";
	}
	return "?";
}

bool SolveLaunch(const FHoopVec3& Release, const FHoopVec3& Target, double ArcDegrees,
	FHoopVec3& OutVelocity, double& OutFlightSeconds)
{
	const double Dx = Target.X - Release.X;
	const double Dy = Target.Y - Release.Y;
	const double Distance = std::sqrt(Dx * Dx + Dy * Dy);
	const double Rise = Target.Z - Release.Z;
	const double Theta = BasketballSimDetail::DegToRad(ArcDegrees);
	const double Cos = std::cos(Theta);
	const double Tan = std::tan(Theta);
	// z(d) = d tan(theta) - g d^2 / (2 v^2 cos^2(theta)): solve for v at d = Distance, z = Rise.
	const double Denominator = 2.0 * Cos * Cos * (Distance * Tan - Rise);
	if (Distance < 1.0 || Denominator <= 0.0)
	{
		return false;
	}
	const double Speed = std::sqrt(BasketballCourt::Gravity * Distance * Distance / Denominator);
	const double Horizontal = Speed * Cos;
	OutVelocity = FHoopVec3(Dx / Distance * Horizontal, Dy / Distance * Horizontal, Speed * std::sin(Theta));
	OutFlightSeconds = Distance / Horizontal;
	return true;
}

FHoopVec3 BallAt(const FHoopVec3& Release, const FHoopVec3& Velocity, double T)
{
	return FHoopVec3(Release.X + Velocity.X * T, Release.Y + Velocity.Y * T,
		Release.Z + Velocity.Z * T - 0.5 * BasketballCourt::Gravity * T * T);
}

EShotResult ResolveShot(double ErrorAlong, double ErrorAcross, double Roll01)
{
	using namespace BasketballCourt;
	const double Off = std::sqrt(ErrorAlong * ErrorAlong + ErrorAcross * ErrorAcross);
	if (Off <= CleanRadius)
	{
		return EShotResult::Swish;
	}
	if (Off < ContactRadius)
	{
		// On the iron: near the clean window it nearly always drops, at the far edge never.
		const double DropChance = 1.0 - (Off - CleanRadius) / (ContactRadius - CleanRadius);
		return Roll01 < DropChance * DropChance ? EShotResult::RimIn : EShotResult::RimOut;
	}
	if (std::fabs(ErrorAcross) > std::fabs(ErrorAlong))
	{
		return EShotResult::Wide;
	}
	return ErrorAlong < 0.0 ? EShotResult::Short : EShotResult::Long;
}

std::vector<FShooterStyle> FBasketballContest::AgentStyles()
{
	// Five ways to shoot. Spread is the whole personality: the 1-sigma miss at the arc. Measured
	// by simulation of ResolveShot: 18 cm makes about 52% of her threes, 20 cm 45%, 22 cm 40%,
	// 25 cm a third - a contest shooter's range, from a good day to a cold one. Rhythm decides
	// how many of the twenty-five balls she gets away before the buzzer.
	std::vector<FShooterStyle> Styles(5);
	Styles[0].Name = "Kaia";    Styles[0].Spread = 20.0; Styles[0].ShotSeconds = 1.9; Styles[0].MoveSeconds = 2.3;
	Styles[1].Name = "Nyra";    Styles[1].Spread = 23.0; Styles[1].ShotSeconds = 1.6; Styles[1].MoveSeconds = 2.0;
	Styles[2].Name = "Isla";    Styles[2].Spread = 18.0; Styles[2].ShotSeconds = 2.3; Styles[2].MoveSeconds = 2.6;
	Styles[3].Name = "Aisling"; Styles[3].Spread = 25.0; Styles[3].ShotSeconds = 1.8; Styles[3].MoveSeconds = 2.2;
	Styles[4].Name = "Elise";   Styles[4].Spread = 21.0; Styles[4].ShotSeconds = 2.0; Styles[4].MoveSeconds = 2.4;
	for (FShooterStyle& Style : Styles)
	{
		Style.ReleaseHeight = 235.0;
		Style.ArcDegrees = 50.0;
	}
	return Styles;
}

FHoopVec3 FBasketballContest::RackSpot(int Rack)
{
	const int Clamped = std::clamp(Rack, 0, Racks - 1);
	const double Angle = BasketballSimDetail::DegToRad(BasketballSimDetail::RackAngleDegrees[Clamped]);
	const bool bCorner = Clamped == 0 || Clamped == Racks - 1;
	const double Distance = bCorner ? BasketballCourt::CornerDistance : BasketballCourt::ArcDistance;
	// A little behind the line, where a shooter's feet would be.
	const double Feet = Distance + 15.0;
	return FHoopVec3(Feet * std::cos(Angle), Feet * std::sin(Angle), 0.0);
}

void FBasketballContest::Begin(const FShooterStyle& InStyle, bool bInPlayer, uint64_t Seed)
{
	Style = InStyle;
	bPlayer = bInPlayer;
	bFinished = false;
	Clock = 0.0;
	NextBallAt = Style.ShotSeconds;      // the first ball has to be picked up too
	NextRack = 0;
	NextBall = 0;
	BallsTaken = 0;
	Score = 0;
	Rng = Seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull;
	Shots.clear();
}

double FBasketballContest::GetTimeLeft() const
{
	return std::max(0.0, TimeLimit - Clock);
}

void FBasketballContest::Advance(double DeltaSeconds, std::vector<FShot>& OutShots)
{
	if (bFinished || DeltaSeconds <= 0.0)
	{
		return;
	}
	const double Until = Clock + DeltaSeconds;
	// An agent shoots every ball that comes due inside this step, each at its own moment.
	while (!bPlayer && !bFinished && NextBallAt <= Until && NextBallAt <= TimeLimit)
	{
		Clock = NextBallAt;
		const double Scale = Style.Spread;
		const FShot Shot = TakeShot(Gaussian() * Scale, Gaussian() * Scale * 0.7);
		OutShots.push_back(Shot);
	}
	Clock = std::min(Until, TimeLimit);
	if (Until >= TimeLimit)
	{
		bFinished = true;
	}
}

bool FBasketballContest::ShootPlayer(double Timing, FShot& OutShot)
{
	if (!bPlayer || !IsBallReady())
	{
		return false;
	}
	const double Clamped = std::clamp(Timing, -1.0, 1.0);
	OutShot = TakeShot(Clamped * PlayerTimingCm + Gaussian() * PlayerScatter, Gaussian() * PlayerScatter * 0.7);
	return true;
}

FShot FBasketballContest::TakeShot(double ErrorAlong, double ErrorAcross)
{
	FShot Shot;
	Shot.Rack = NextRack;
	Shot.Ball = NextBall;
	Shot.bMoney = NextBall == BallsPerRack - 1;
	Shot.Clock = Clock;

	// Errors scale with distance: the same wobble of the wrist misses by more from further out,
	// so the shorter corner three is a little kinder than the top of the key.
	const FHoopVec3 Spot = RackSpot(NextRack);
	const double Distance = std::sqrt(Spot.X * Spot.X + Spot.Y * Spot.Y);
	const double Scale = Distance / BasketballCourt::ArcDistance;
	Shot.ErrorAlong = ErrorAlong * Scale;
	Shot.ErrorAcross = ErrorAcross * Scale;
	Shot.Result = ResolveShot(Shot.ErrorAlong, Shot.ErrorAcross, Random01());
	Shot.Points = IsMade(Shot.Result) ? (Shot.bMoney ? 2 : 1) : 0;

	// The flight, for the court to draw: from the hand toward the point at rim height that the
	// errors came to (along = away from the shooter, across = to the shooter's left).
	const double Ax = -Spot.X / Distance;   // unit vector from the spot toward the rim
	const double Ay = -Spot.Y / Distance;
	const double Lx = -Ay;                  // to the shooter's left, facing the rim
	const double Ly = Ax;
	Shot.Release = FHoopVec3(Spot.X + Ax * 30.0, Spot.Y + Ay * 30.0, Style.ReleaseHeight);
	Shot.Target = FHoopVec3(Ax * Shot.ErrorAlong + Lx * Shot.ErrorAcross,
		Ay * Shot.ErrorAlong + Ly * Shot.ErrorAcross, BasketballCourt::RimHeight);
	if (!SolveLaunch(Shot.Release, Shot.Target, Style.ArcDegrees, Shot.Velocity, Shot.FlightSeconds))
	{
		Shot.Velocity = FHoopVec3();
		Shot.FlightSeconds = 0.0;
	}

	Score += Shot.Points;
	++BallsTaken;
	Shots.push_back(Shot);
	StepToNextBall();
	return Shot;
}

void FBasketballContest::StepToNextBall()
{
	if (++NextBall >= BallsPerRack)
	{
		NextBall = 0;
		if (++NextRack >= Racks)
		{
			bFinished = true;   // all twenty-five away before the buzzer
			return;
		}
		NextBallAt = Clock + Style.MoveSeconds + Style.ShotSeconds;
		return;
	}
	NextBallAt = Clock + Style.ShotSeconds;
}

double FBasketballContest::Random01()
{
	// splitmix64: small, fast, and the same on every compiler.
	uint64_t Z = (Rng += 0x9E3779B97F4A7C15ull);
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
	Z ^= Z >> 31;
	return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
}

double FBasketballContest::Gaussian()
{
	// Box-Muller; the 1e-12 keeps the log finite.
	const double U1 = std::max(Random01(), 1e-12);
	const double U2 = Random01();
	return std::sqrt(-2.0 * std::log(U1)) * std::cos(2.0 * BasketballSimDetail::Pi * U2);
}

std::vector<int> RankScores(const std::vector<int>& Scores)
{
	std::vector<int> Order(Scores.size());
	for (size_t Index = 0; Index < Order.size(); ++Index)
	{
		Order[Index] = static_cast<int>(Index);
	}
	std::stable_sort(Order.begin(), Order.end(), [&Scores](int A, int B) { return Scores[A] > Scores[B]; });
	return Order;
}
