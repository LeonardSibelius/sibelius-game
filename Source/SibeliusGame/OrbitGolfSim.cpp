// OrbitGolfSim.cpp — see the header for the design, and Tools/Tests/orbit_golf_test.cpp for what it
// is held to.

#include "OrbitGolfSim.h"

#include <cmath>

// NAMED, NOT ANONYMOUS: the unity-build rule (BilliardsSim.cpp's PoolMath).
namespace OrbitMath
{
	inline double Dot(const FSpaceVec& A, const FSpaceVec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline double Length(const FSpaceVec& A) { return std::sqrt(Dot(A, A)); }
	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline FSpaceVec Unit(const FSpaceVec& A)
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : FSpaceVec(0.0, 0.0, 1.0);
	}

	// Speed into a surface of normal N becomes Restitution of itself back out (nothing, if it
	// was barely moving), and the speed along it loses Friction of the speed into it, plus the
	// rolling resistance of the time in contact.
	inline void Bounce(FSpaceVec& V, const FSpaceVec& N, double Restitution, double Friction, double DragStep)
	{
		const double Vn = Dot(V, N);
		FSpaceVec Along = V - N * Vn;
		const double AlongSpeed = Length(Along);
		const double Into = Vn < 0.0 ? -Vn : 0.0;
		const double Lose = Friction * Into + DragStep;
		Along = AlongSpeed > Lose ? Along * ((AlongSpeed - Lose) / AlongSpeed) : FSpaceVec();
		const double Out = Vn < 0.0 ? (Into > 60.0 ? Into * Restitution : 0.0) : Vn;
		V = Along + N * Out;
	}
}

FSpaceVec FOrbitHole::CupPoint() const
{
	const FOrbitMoon& Moon = Moons[CupMoon];
	return Moon.Center + CupDirection * Moon.Radius;
}

// Elise's course. The platform is the tee; the moons hang 7 to 15 m up, 18 to 40 m out.
FOrbitCourse FOrbitCourse::MakeDefault()
{
	FOrbitCourse C;
	const double G = C.WorldGravity;
	{
		// FIRST LIGHT: one moon, the cup on its crown. Loft it on and stop it there.
		FOrbitHole H;
		H.Name = "First Light";
		H.Par = 2;
		H.Moons.push_back(FOrbitMoon{ FSpaceVec(2500.0, 0.0, 900.0), 400.0, 2.0 * G });
		H.CupMoon = 0;
		H.CupDirection = FSpaceVec(0.0, 0.0, 1.0);
		C.Holes.push_back(H);
	}
	{
		// SLINGSHOT: the cup on the FAR side of a small moon, a big one beside the way there.
		// Swing round the big one, or come down over the small one and run round its curve.
		FOrbitHole H;
		H.Name = "Slingshot";
		H.Par = 3;
		H.Moons.push_back(FOrbitMoon{ FSpaceVec(1800.0, 700.0, 800.0), 550.0, 2.6 * G });
		H.Moons.push_back(FOrbitMoon{ FSpaceVec(3800.0, -300.0, 1100.0), 380.0, 2.0 * G });
		H.CupMoon = 1;
		H.CupDirection = OrbitMath::Unit(FSpaceVec(1.0, 0.2, 0.25));
		C.Holes.push_back(H);
	}
	{
		// UNDERSIDE: the cup at the bottom of a moon that pulls harder than the world, so a ball
		// can lie on it upside down. Come in under it and let it take the ball.
		FOrbitHole H;
		H.Name = "Underside";
		H.Par = 3;
		H.Moons.push_back(FOrbitMoon{ FSpaceVec(2300.0, 0.0, 1500.0), 450.0, 2.8 * G });
		H.CupMoon = 0;
		H.CupDirection = FSpaceVec(0.0, 0.0, -1.0);
		C.Holes.push_back(H);
	}
	return C;
}

FOrbitGolfSim::FOrbitGolfSim(FOrbitCourse InCourse)
	: CourseData(static_cast<FOrbitCourse&&>(InCourse))
{
	// The space game's tolerances are for kilometres; these are for centimetres, and one
	// substep is the most it may take, so no contact can fall between two steps.
	Integrator.Settings.RelTolerance = 1e-9;
	Integrator.Settings.AbsTolKm = 1e-4;
	Integrator.Settings.AbsTolKmPerS = 1e-4;
	Integrator.Settings.MinStepSeconds = 1e-6;
	Integrator.Settings.MaxStepSeconds = SubstepSeconds;
	StartHole(0);
	CurrentPhase = EOrbitPhase::Idle;
	PendingEvents.clear();
}

FSpaceVec FOrbitGolfSim::Gravity(const FSpaceVec& P) const
{
	FSpaceVec A(0.0, 0.0, -CourseData.WorldGravity);
	for (const FOrbitMoon& Moon : CurrentHole().Moons)
	{
		const FSpaceVec D = Moon.Center - P;
		const double R2 = OrbitMath::Dot(D, D);
		const double R = std::sqrt(R2);
		const double GM = Moon.SurfaceGravity * Moon.Radius * Moon.Radius;
		A = A + D * (GM / (R2 * R));
	}
	return A;
}

FSpaceVec FOrbitGolfSim::ShotDirection(double Yaw, double Pitch)
{
	return FSpaceVec(std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch));
}

// The square of the power, as a golf swing feels: half power is a quarter of the speed, so the
// soft end - a putt along a moon's curve - has room to be played.
double FOrbitGolfSim::LaunchSpeed(double Power)
{
	const double P = OrbitMath::Clamp(Power, 0.0, 1.0);
	return MinLaunchSpeed + P * P * (MaxLaunchSpeed - MinLaunchSpeed);
}

void FOrbitGolfSim::StartRound()
{
	Total = 0;
	ParSoFar = 0;
	ScoreCard.clear();
	PendingEvents.clear();
	Emit(EOrbitEvent::RoundStarted);
	StartHole(0);
}

void FOrbitGolfSim::StartHole(int Index)
{
	HoleIndex = Index;
	Strokes = 0;
	Lie = FSpaceVec(0.0, 0.0, CourseData.BallRadius);
	Ball.Position = Lie;
	Ball.Velocity = FSpaceVec();
	Flight = 0.0;
	SlowTouching = 0.0;
	Accumulator = 0.0;
	CurrentPhase = EOrbitPhase::Aiming;
	Emit(EOrbitEvent::HoleStarted, Index);
}

void FOrbitGolfSim::NextHole()
{
	if (CurrentPhase != EOrbitPhase::Holed)
	{
		return;
	}
	if (HoleIndex + 1 < static_cast<int>(CourseData.Holes.size()))
	{
		StartHole(HoleIndex + 1);
		return;
	}
	CurrentPhase = EOrbitPhase::Finished;
	Emit(EOrbitEvent::RoundFinished, Total, ParSoFar);
}

bool FOrbitGolfSim::Shoot(double Yaw, double Pitch, double Power)
{
	if (CurrentPhase != EOrbitPhase::Aiming)
	{
		return false;
	}
	const double Speed = LaunchSpeed(Power);
	Lie = Ball.Position;
	Ball.Velocity = ShotDirection(Yaw, Pitch) * Speed;
	++Strokes;
	Flight = 0.0;
	SlowTouching = 0.0;
	Integrator.Restart();
	CurrentPhase = EOrbitPhase::Flying;
	Emit(EOrbitEvent::ShotTaken, Strokes, Speed);
	return true;
}

void FOrbitGolfSim::Advance(double DeltaSeconds)
{
	Accumulator += OrbitMath::Clamp(DeltaSeconds, 0.0, MaxFrameSeconds);
	while (Accumulator >= SubstepSeconds)
	{
		Accumulator -= SubstepSeconds;
		Substep();
	}
}

void FOrbitGolfSim::Substep()
{
	if (CurrentPhase != EOrbitPhase::Flying)
	{
		return;
	}
	const FSpaceAccelFn Accel = [this](double, const FSpaceVec& P) { return Gravity(P); };
	Integrator.AdvanceTo(Ball, Clock, Clock + SubstepSeconds, Accel);
	Flight += SubstepSeconds;

	const bool bTouching = Collide();
	CheckCup();
	if (CurrentPhase != EOrbitPhase::Flying)
	{
		return;
	}

	const FSpaceVec P = Ball.Position;
	const double Sideways = std::sqrt(P.X * P.X + P.Y * P.Y);
	if ((P.Z < CourseData.OutOfBoundsZ && Sideways > CourseData.PlatformRadius)
		|| Sideways > CourseData.OutOfBoundsRange || P.Z > CourseData.OutOfBoundsRange)
	{
		Penalty(EOrbitEvent::OutOfBounds);
		return;
	}
	if (Flight > MaxFlightSeconds)
	{
		Penalty(EOrbitEvent::LostInOrbit);
		return;
	}

	SlowTouching = (bTouching && OrbitMath::Length(Ball.Velocity) < RestSpeed) ? SlowTouching + SubstepSeconds : 0.0;
	if (SlowTouching >= 0.15)
	{
		Ball.Velocity = FSpaceVec();
		CurrentPhase = EOrbitPhase::Aiming;
		int On = -1;
		double Nearest = 1e30;
		const std::vector<FOrbitMoon>& Moons = CurrentHole().Moons;
		for (int I = 0; I < static_cast<int>(Moons.size()); ++I)
		{
			const double Gap = OrbitMath::Length(Ball.Position - Moons[I].Center) - Moons[I].Radius;
			if (Gap < Nearest)
			{
				Nearest = Gap;
				On = I;
			}
		}
		Emit(EOrbitEvent::CameToRest, Nearest < CourseData.BallRadius + 2.0 ? On : -1);
	}
}

bool FOrbitGolfSim::Collide()
{
	const double Rb = CourseData.BallRadius;
	const double DragStep = ContactDrag * SubstepSeconds;
	bool bTouching = false;

	const std::vector<FOrbitMoon>& Moons = CurrentHole().Moons;
	for (int I = 0; I < static_cast<int>(Moons.size()); ++I)
	{
		const FOrbitMoon& Moon = Moons[I];
		const FSpaceVec D = Ball.Position - Moon.Center;
		const double Dist = OrbitMath::Length(D);
		if (Dist >= Moon.Radius + Rb + 0.5)
		{
			continue;
		}
		const FSpaceVec N = OrbitMath::Unit(D);
		const double Into = -OrbitMath::Dot(Ball.Velocity, N);
		if (Dist < Moon.Radius + Rb)
		{
			Ball.Position = Moon.Center + N * (Moon.Radius + Rb);
		}
		OrbitMath::Bounce(Ball.Velocity, N, Restitution, SurfaceFriction, DragStep);
		if (Into > 50.0)
		{
			Emit(EOrbitEvent::Bounced, I, Into);
			Integrator.Restart();
		}
		bTouching = true;
	}

	// The tee platform: a flat disc, its top at Z = 0.
	const FSpaceVec P = Ball.Position;
	if (std::sqrt(P.X * P.X + P.Y * P.Y) <= CourseData.PlatformRadius && P.Z < Rb + 0.5 && P.Z > -Rb)
	{
		const double Into = -Ball.Velocity.Z;
		if (P.Z < Rb)
		{
			Ball.Position.Z = Rb;
		}
		OrbitMath::Bounce(Ball.Velocity, FSpaceVec(0.0, 0.0, 1.0), Restitution, SurfaceFriction, DragStep);
		if (Into > 50.0)
		{
			Emit(EOrbitEvent::Bounced, -1, Into);
			Integrator.Restart();
		}
		bTouching = true;
	}
	return bTouching;
}

void FOrbitGolfSim::CheckCup()
{
	const FSpaceVec Cup = CurrentHole().CupPoint();
	const double Reach = CourseData.CupRadius + CourseData.BallRadius;
	if (OrbitMath::Length(Ball.Position - Cup) > Reach || OrbitMath::Length(Ball.Velocity) > DropSpeed)
	{
		return;
	}
	Ball.Velocity = FSpaceVec();
	Ball.Position = Cup;
	CurrentPhase = EOrbitPhase::Holed;
	Total += Strokes;
	ParSoFar += CurrentHole().Par;
	ScoreCard.push_back(Strokes);
	Emit(EOrbitEvent::Holed, Strokes);
}

void FOrbitGolfSim::Penalty(EOrbitEvent Why)
{
	++Strokes;
	Ball.Position = Lie;
	Ball.Velocity = FSpaceVec();
	Flight = 0.0;
	SlowTouching = 0.0;
	CurrentPhase = EOrbitPhase::Aiming;
	Emit(Why, Strokes);
}

std::vector<FSpaceVec> FOrbitGolfSim::PredictPath(double Yaw, double Pitch, double Power, double Seconds, int Points, EOrbitEvent* Outcome) const
{
	std::vector<FSpaceVec> Out;
	if (Outcome) *Outcome = EOrbitEvent::ShotTaken;
	if (Points < 2 || !std::isfinite(Seconds) || Seconds <= 0.0) return Out;
	FOrbitGolfSim Copy = *this;
	Copy.TakeEvents();
	if (!Copy.Shoot(Yaw, Pitch, Power)) return Out;
	Copy.TakeEvents();
	Out.push_back(Copy.BallPosition());
	const double Duration = std::fmin(Seconds, MaxFlightSeconds + SubstepSeconds * 2.0);
	const double Every = Duration / (Points - 1);
	double Next = Every;
	for (double T = SubstepSeconds; T <= Duration + 1e-9; T += SubstepSeconds)
	{
		const FSpaceVec Previous = Copy.BallPosition();
		Copy.Substep();
		bool Failed = false;
		for (const auto& Event : Copy.TakeEvents())
		{
			if (Event.Type == EOrbitEvent::OutOfBounds || Event.Type == EOrbitEvent::LostInOrbit || Event.Type == EOrbitEvent::Holed || Event.Type == EOrbitEvent::CameToRest)
			{ if (Outcome) *Outcome = Event.Type; }
			Failed = Failed || Event.Type == EOrbitEvent::OutOfBounds || Event.Type == EOrbitEvent::LostInOrbit;
		}
		if (Copy.Phase() != EOrbitPhase::Flying)
		{ Out.push_back(Failed ? Previous : Copy.BallPosition()); break; }
		if (T + 1e-9 >= Next) { Out.push_back(Copy.BallPosition()); Next += Every; }
	}
	return Out;
}

std::vector<FOrbitEvent> FOrbitGolfSim::TakeEvents()
{
	std::vector<FOrbitEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FOrbitGolfSim::PlaceBall(FSpaceVec Position, FSpaceVec Velocity)
{
	Ball.Position = Position;
	Ball.Velocity = Velocity;
	Lie = Position;
	Flight = 0.0;
	SlowTouching = 0.0;
	Integrator.Restart();
	CurrentPhase = EOrbitPhase::Flying;
}

void FOrbitGolfSim::Emit(EOrbitEvent Type, int Index, double Value)
{
	PendingEvents.push_back(FOrbitEvent{ Type, Index, Value });
}

FOrbitCourse FOrbitCourse::MakePractice()
{
	FOrbitCourse C = MakeDefault();
	C.CupRadius = 140.0;
	C.Holes[0].Name = "Lesson 1 - Land on a moon";
	C.Holes[1].Name = "Lesson 2 - Bend with gravity";
	C.Holes[2].Name = "Lesson 3 - The underside";
	return C;
}

FSpaceVec FOrbitGolfSim::SurfaceUp() const
{
	// Use the supporting geometry, not net gravity: world gravity can tilt the latter.
	double BestGap = CourseData.BallRadius + 3.0;
	FSpaceVec Up(0.0, 0.0, 1.0);
	for (const auto& Moon : CurrentHole().Moons)
	{
		const FSpaceVec Offset = Ball.Position - Moon.Center;
		const double Gap = SpaceMath::Length(Offset) - Moon.Radius;
		if (std::abs(Gap - CourseData.BallRadius) < 3.0 && Gap < BestGap)
		{ Up = SpaceMath::Normalized(Offset); BestGap = Gap; }
	}
	return Up;
}

FSpaceVec FOrbitGolfSim::SurfaceDirection(double Heading, double Loft) const
{
	const FSpaceVec Up = SurfaceUp();
	const FSpaceVec Reference = std::abs(Up.X) < 0.9 ? FSpaceVec(1, 0, 0) : FSpaceVec(0, 1, 0);
	const FSpaceVec Forward = SpaceMath::Normalized(Reference - Up * SpaceMath::Dot(Reference, Up));
	const FSpaceVec Right = SpaceMath::Cross(Up, Forward);
	return (Forward * std::cos(Heading) + Right * std::sin(Heading)) * std::cos(Loft) + Up * std::sin(Loft);
}

double FOrbitGolfSim::SurfaceHeadingTo(FSpaceVec Target) const
{
	const FSpaceVec D = Target - Ball.Position;
	const FSpaceVec Forward = SurfaceDirection(0.0, 0.0);
	const FSpaceVec Right = SurfaceDirection(1.5707963267948966, 0.0);
	return std::atan2(SpaceMath::Dot(D, Right), SpaceMath::Dot(D, Forward));
}

double FOrbitGolfSim::ClubPower(double Power, bool bPutt)
{
	const double P = OrbitMath::Clamp(Power, 0.0, 1.0);
	// Putter has a linear, gentle speed range; convert to the existing launch curve.
	return bPutt ? std::sqrt(P * (650.0 - MinLaunchSpeed) / (MaxLaunchSpeed - MinLaunchSpeed)) : P;
}
