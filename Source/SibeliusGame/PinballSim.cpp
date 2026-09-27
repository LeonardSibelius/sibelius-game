// PinballSim.cpp — see the header for the design, and Tools/Tests/pinball_sim_test.cpp for
// what it is held to.

#include "PinballSim.h"

#include <cmath>
#include <utility>

// NAMED, NOT ANONYMOUS, AND NOT PULLED IN WITH "using". Unity builds paste many .cpp files
// into one translation unit, so a file-scope Dot() or Length() here would collide with any
// other file's (the ProceduralPcm.h lesson). Qualified calls keep this file sealed.
namespace PinMath
{
	constexpr double Pi = 3.14159265358979323846;

	inline double Dot(FPinVec2 A, FPinVec2 B) { return A.X * B.X + A.Y * B.Y; }
	inline double Length(FPinVec2 A) { return std::sqrt(Dot(A, A)); }
	inline FPinVec2 Perp(FPinVec2 A) { return FPinVec2(-A.Y, A.X); }   // 90° counter-clockwise
	inline double Radians(double Degrees) { return Degrees * Pi / 180.0; }
	inline double Clamp01(double V) { return V < 0.0 ? 0.0 : (V > 1.0 ? 1.0 : V); }

	inline FPinVec2 Normalized(FPinVec2 A)
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : FPinVec2(0.0, 1.0);
	}

	// Parameter 0..1 of the point on segment AB closest to P.
	inline double ClosestT(FPinVec2 P, FPinVec2 A, FPinVec2 B)
	{
		const FPinVec2 AB = B - A;
		const double L2 = Dot(AB, AB);
		return L2 > 1e-12 ? Clamp01(Dot(P - A, AB) / L2) : 0.0;
	}
}

// ===========================================================================
// THE LIVING-ROOM TABLE
//
// Built left-side-first and mirrored about the playfield centre, so the two halves cannot
// drift apart when one is tuned. Every number below is centimetres on the table plane.

FPinballTableLayout FPinballTableLayout::MakeDefault()
{
	FPinballTableLayout L;

	const double LaneX = L.ShooterLaneMinX;   // 46.5: the playfield is 0..LaneX
	const double CenterX = LaneX * 0.5;       // 23.25
	auto MirrorX = [CenterX](double X) { return 2.0 * CenterX - X; };

	auto Wall = [&L](double AX, double AY, double BX, double BY, EPinballSurface Kind = EPinballSurface::Wall, int Index = -1)
	{
		L.Segments.push_back(FPinballSegment{ FPinVec2(AX, AY), FPinVec2(BX, BY), Kind, Index });
	};

	// The outer cabinet: right wall, the top arc, left wall.
	Wall(L.Width, 0.0, L.Width, 85.0);
	{
		// An elliptical arc, 15° segments, from the top of the right wall round to the top
		// of the left. It is what turns a plunged ball left into the playfield. Endpoints are
		// set exactly rather than trusted to cos(pi).
		const double ArcCX = L.Width * 0.5, ArcCY = 85.0, ArcRX = L.Width * 0.5, ArcRY = 21.0;
		FPinVec2 Prev(L.Width, 85.0);
		for (int Step = 1; Step <= 12; ++Step)
		{
			const double Theta = PinMath::Radians(15.0 * Step);
			const FPinVec2 Next = (Step == 12)
				? FPinVec2(0.0, 85.0)
				: FPinVec2(ArcCX + ArcRX * std::cos(Theta), ArcCY + ArcRY * std::sin(Theta));
			Wall(Prev.X, Prev.Y, Next.X, Next.Y);
			Prev = Next;
		}
	}
	Wall(0.0, 85.0, 0.0, 20.0);

	// The guides that funnel the lower playfield onto the flippers. Each ends at its
	// flipper's pivot centre, where the flipper's thick base covers the joint.
	Wall(0.0, 20.0, 14.0, 11.0);
	Wall(LaneX, 20.0, MirrorX(14.0), 11.0);

	// The shooter lane: inner wall, the plunger floor the served ball rests on, and the
	// one-way gate across its top. The gate starts exactly at the top of the inner wall.
	// With any stub of wall standing above the gate's low end, a ball rolling down the gate
	// would be trapped in the corner between them.
	Wall(LaneX, 0.0, LaneX, 80.0);
	Wall(LaneX, 0.0, L.Width, 0.0);
	Wall(LaneX, 80.0, L.Width, 84.0, EPinballSurface::OneWayGate);   // front (left-normal) = above

	// Slingshots: a kicking rubber face toward the centre, closed behind by two rails.
	// The back corner sits at y=21 so the inlane between it and the guide is 4 cm wide —
	// at y=19 it was 2.4 cm and a 2.7 cm ball would have jammed there.
	Wall(6.0, 27.0, 11.5, 17.0, EPinballSurface::Slingshot, 0);
	Wall(6.0, 27.0, 6.0, 21.0);
	Wall(6.0, 21.0, 11.5, 17.0);
	Wall(MirrorX(6.0), 27.0, MirrorX(11.5), 17.0, EPinballSurface::Slingshot, 1);
	Wall(MirrorX(6.0), 27.0, MirrorX(6.0), 21.0);
	Wall(MirrorX(6.0), 21.0, MirrorX(11.5), 17.0);

	L.Bumpers.push_back(FPinballBumper{ FPinVec2(15.0, 68.0), 2.6 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(MirrorX(15.0), 68.0), 2.6 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(CenterX, 78.0), 2.6 });

	L.Rollovers.push_back(FPinballRollover{ FPinVec2(16.0, 95.0), 2.0 });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(CenterX, 97.0), 2.0 });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(MirrorX(16.0), 95.0), 2.0 });

	// Flippers: 30° below horizontal at rest, 25° above when held. The right one is the
	// mirror image, so it rotates clockwise to rise.
	FPinballFlipperDef& Left = L.Flippers[0];
	Left.Pivot = FPinVec2(14.0, 11.0);
	Left.RestAngle = PinMath::Radians(-30.0);
	Left.UpAngle = PinMath::Radians(25.0);

	FPinballFlipperDef& Right = L.Flippers[1];
	Right.Pivot = FPinVec2(MirrorX(14.0), 11.0);
	Right.RestAngle = PinMath::Radians(180.0 + 30.0);
	Right.UpAngle = PinMath::Radians(180.0 - 25.0);

	L.PlungerRest = FPinVec2((LaneX + L.Width) * 0.5, L.BallRadius);
	return L;
}

// ===========================================================================

FPinballSim::FPinballSim(FPinballTableLayout InLayout)
	: TableLayout(std::move(InLayout))
{
	Gravity = 980.665 * std::sin(PinMath::Radians(TableLayout.SlopeDegrees)) * (5.0 / 7.0);

	RolloverLit.assign(TableLayout.Rollovers.size(), 0);
	RolloverInside.assign(TableLayout.Rollovers.size(), 0);
	BumperCooldown.assign(TableLayout.Bumpers.size(), 0.0);

	int MaxSling = -1;
	for (const FPinballSegment& Segment : TableLayout.Segments)
	{
		if (Segment.Kind == EPinballSurface::Slingshot && Segment.Index > MaxSling)
		{
			MaxSling = Segment.Index;
		}
	}
	SlingCooldown.assign(static_cast<size_t>(MaxSling + 1), 0.0);

	for (int Side = 0; Side < 2; ++Side)
	{
		FlipperStates[Side].Angle = TableLayout.Flippers[Side].RestAngle;
	}
	BallPos = TableLayout.PlungerRest;
}

void FPinballSim::StartGame()
{
	CurrentScore = 0;
	CurrentBall = 1;
	RolloverLit.assign(RolloverLit.size(), 0);
	RolloverInside.assign(RolloverInside.size(), 0);
	Emit(EPinballEvent::GameStarted);
	ServeBall();
}

void FPinballSim::ServeBall()
{
	CurrentPhase = EPinballPhase::BallReady;
	BallPos = TableLayout.PlungerRest;
	BallVel = FPinVec2();
	Emit(EPinballEvent::BallServed, CurrentBall);
}

void FPinballSim::SetFlipper(int Side, bool bHeld)
{
	if (Side < 0 || Side > 1)
	{
		return;
	}
	FFlipperState& State = FlipperStates[Side];
	if (State.bHeld == bHeld)
	{
		return;
	}
	State.bHeld = bHeld;
	Emit(bHeld ? EPinballEvent::FlipperUp : EPinballEvent::FlipperDown, Side);
}

bool FPinballSim::Launch(double Pull)
{
	if (CurrentPhase != EPinballPhase::BallReady)
	{
		return false;
	}
	const double Speed = PlungerMinSpeed + (PlungerMaxSpeed - PlungerMinSpeed) * PinMath::Clamp01(Pull);
	BallVel = FPinVec2(0.0, Speed);
	CurrentPhase = EPinballPhase::InPlay;
	Emit(EPinballEvent::BallLaunched, CurrentBall);
	return true;
}

void FPinballSim::Advance(double DeltaSeconds)
{
	if (!(DeltaSeconds > 0.0))
	{
		return;
	}
	Accumulator += DeltaSeconds < MaxFrameSeconds ? DeltaSeconds : MaxFrameSeconds;
	while (Accumulator >= SubstepSeconds)
	{
		Substep();
		Accumulator -= SubstepSeconds;
	}
}

void FPinballSim::Substep()
{
	const double H = SubstepSeconds;

	// Flippers move whatever the phase: on a real machine the buttons work in attract mode.
	StepFlippers();
	TickCooldowns();

	if (CurrentPhase != EPinballPhase::InPlay)
	{
		return;
	}

	BallVel.Y -= Gravity * H;
	BallVel = BallVel * (1.0 - RollingDamping * H);
	ClampBallSpeed();
	BallPos = BallPos + BallVel * H;

	for (int Iteration = 0; Iteration < ContactIterations; ++Iteration)
	{
		CollideSegments();
		CollideBumpers();
		CollideFlippers();
	}
	ClampBallSpeed();
	UpdateRollovers();

	if (BallPos.Y < TableLayout.DrainY)
	{
		DrainBall();
		return;
	}

	// A plunge too soft to clear the gate rolls back down onto the plunger. It is the same
	// ball, served again, and the player gets another pull.
	if (BallPos.X > TableLayout.ShooterLaneMinX
		&& BallPos.Y < TableLayout.PlungerRest.Y + 2.0
		&& PinMath::Length(BallVel) < 3.0)
	{
		ServeBall();
	}
}

void FPinballSim::StepFlippers()
{
	for (int Side = 0; Side < 2; ++Side)
	{
		FFlipperState& State = FlipperStates[Side];
		const FPinballFlipperDef& Def = TableLayout.Flippers[Side];

		const double Target = State.bHeld ? Def.UpAngle : Def.RestAngle;
		const double MaxStep = (State.bHeld ? FlipperUpSpeed : FlipperDownSpeed) * SubstepSeconds;
		const double Delta = Target - State.Angle;
		const double StepAngle = std::fabs(Delta) <= MaxStep ? Delta : (Delta > 0.0 ? MaxStep : -MaxStep);

		State.Angle += StepAngle;
		// The surface velocity the ball feels. Zero once the flipper reaches its stop, so a
		// ball cradled on a held flipper is resting on something still.
		State.AngularVelocity = StepAngle / SubstepSeconds;
	}
}

void FPinballSim::TickCooldowns()
{
	for (double& Cooldown : BumperCooldown)
	{
		Cooldown = Cooldown > SubstepSeconds ? Cooldown - SubstepSeconds : 0.0;
	}
	for (double& Cooldown : SlingCooldown)
	{
		Cooldown = Cooldown > SubstepSeconds ? Cooldown - SubstepSeconds : 0.0;
	}
}

// Push the ball out along Normal, and reflect its velocity RELATIVE TO THE SURFACE, so a
// moving flipper hands its speed to the ball. Returns the approach speed into the surface
// (0 if the ball was already moving away), which is what decides whether a kicker fires.
double FPinballSim::Resolve(FPinVec2 Normal, double Penetration, FPinVec2 SurfaceVelocity, double Restitution)
{
	BallPos = BallPos + Normal * Penetration;

	FPinVec2 Relative = BallVel - SurfaceVelocity;
	const double Vn = PinMath::Dot(Relative, Normal);
	if (Vn >= 0.0)
	{
		return 0.0;
	}

	const double Approach = -Vn;
	const bool bRealImpact = Approach >= RestingSpeed;
	const double E = bRealImpact ? Restitution : 0.0;
	Relative = Relative - Normal * ((1.0 + E) * Vn);

	// Friction on real impacts only. Applied every substep it would brake a ball rolling
	// along a rail 1200 times a second.
	if (bRealImpact)
	{
		const FPinVec2 Tangent = Relative - Normal * PinMath::Dot(Relative, Normal);
		Relative = Relative - Tangent * ImpactFriction;
	}

	BallVel = Relative + SurfaceVelocity;
	return Approach;
}

void FPinballSim::Kick(FPinVec2 Normal, double Speed)
{
	const double Vn = PinMath::Dot(BallVel, Normal);
	if (Vn < Speed)
	{
		BallVel = BallVel + Normal * (Speed - Vn);
	}
}

void FPinballSim::ClampBallSpeed()
{
	const double Speed = PinMath::Length(BallVel);
	if (Speed > MaxBallSpeed)
	{
		BallVel = BallVel * (MaxBallSpeed / Speed);
	}
}

void FPinballSim::CollideSegments()
{
	const double R = TableLayout.BallRadius;

	for (const FPinballSegment& Segment : TableLayout.Segments)
	{
		const FPinVec2 AB = Segment.B - Segment.A;

		// A gate stops only a ball whose centre is on its front side. The plunged ball comes
		// from behind and passes; a ball falling onto it from the playfield is on the front.
		if (Segment.Kind == EPinballSurface::OneWayGate
			&& PinMath::Dot(BallPos - Segment.A, PinMath::Perp(AB)) <= 0.0)
		{
			continue;
		}

		const double T = PinMath::ClosestT(BallPos, Segment.A, Segment.B);
		const FPinVec2 Offset = BallPos - (Segment.A + AB * T);
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= R * R)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : PinMath::Normalized(PinMath::Perp(AB));
		const double Approach = Resolve(Normal, R - Dist, FPinVec2(), WallRestitution);

		if (Segment.Kind == EPinballSurface::Slingshot
			&& Approach >= SlingTriggerSpeed
			&& Segment.Index >= 0
			&& static_cast<size_t>(Segment.Index) < SlingCooldown.size()
			&& SlingCooldown[static_cast<size_t>(Segment.Index)] <= 0.0)
		{
			Kick(Normal, SlingKickSpeed);
			SlingCooldown[static_cast<size_t>(Segment.Index)] = KickerCooldownSeconds;
			CurrentScore += SlingshotPoints;
			Emit(EPinballEvent::SlingshotHit, Segment.Index, SlingshotPoints);
		}
	}
}

void FPinballSim::CollideBumpers()
{
	const double R = TableLayout.BallRadius;

	for (size_t I = 0; I < TableLayout.Bumpers.size(); ++I)
	{
		const FPinballBumper& Bumper = TableLayout.Bumpers[I];
		const FPinVec2 Offset = BallPos - Bumper.Center;
		const double MinDist = R + Bumper.Radius;
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= MinDist * MinDist)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : FPinVec2(0.0, 1.0);
		Resolve(Normal, MinDist - Dist, FPinVec2(), BumperRestitution);

		// A pop bumper fires on any touch — its skirt is the switch — but only once per
		// contact, or a ball grazing it would score every substep.
		if (BumperCooldown[I] <= 0.0)
		{
			Kick(Normal, BumperKickSpeed);
			BumperCooldown[I] = KickerCooldownSeconds;
			CurrentScore += BumperPoints;
			Emit(EPinballEvent::BumperHit, static_cast<int>(I), BumperPoints);
		}
	}
}

void FPinballSim::CollideFlippers()
{
	const double R = TableLayout.BallRadius;

	for (int Side = 0; Side < 2; ++Side)
	{
		const FPinballFlipperDef& Def = TableLayout.Flippers[Side];
		const FPinVec2 Tip = FlipperTip(Side);

		const double T = PinMath::ClosestT(BallPos, Def.Pivot, Tip);
		const FPinVec2 Closest = Def.Pivot + (Tip - Def.Pivot) * T;
		const double FlipperRadius = Def.BaseRadius + (Def.TipRadius - Def.BaseRadius) * T;

		const FPinVec2 Offset = BallPos - Closest;
		const double MinDist = R + FlipperRadius;
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= MinDist * MinDist)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : FPinVec2(0.0, 1.0);

		// The velocity of the flipper's surface at the point of contact: omega × r. This is
		// the whole of what makes a flipper shot — the ball is struck by a moving surface,
		// harder the further out along the bat it sits.
		const FPinVec2 Contact = Closest + Normal * FlipperRadius;
		const FPinVec2 SurfaceVelocity = PinMath::Perp(Contact - Def.Pivot) * FlipperStates[Side].AngularVelocity;

		Resolve(Normal, MinDist - Dist, SurfaceVelocity, FlipperRestitution);
	}
}

void FPinballSim::UpdateRollovers()
{
	bool bNewlyLit = false;

	for (size_t I = 0; I < TableLayout.Rollovers.size(); ++I)
	{
		const FPinballRollover& Rollover = TableLayout.Rollovers[I];
		const FPinVec2 Offset = BallPos - Rollover.Center;
		const bool bInside = PinMath::Dot(Offset, Offset) < Rollover.Radius * Rollover.Radius;

		// On entry only: a ball sitting on a rollover lights it once, not every substep.
		if (bInside && RolloverInside[I] == 0 && RolloverLit[I] == 0)
		{
			RolloverLit[I] = 1;
			CurrentScore += RolloverPoints;
			Emit(EPinballEvent::RolloverLit, static_cast<int>(I), RolloverPoints);
			bNewlyLit = true;
		}
		RolloverInside[I] = bInside ? 1 : 0;
	}

	if (!bNewlyLit || RolloverLit.empty())
	{
		return;
	}
	for (uint8_t Lit : RolloverLit)
	{
		if (Lit == 0)
		{
			return;
		}
	}
	CurrentScore += RolloverSetBonus;
	Emit(EPinballEvent::RolloversComplete, -1, RolloverSetBonus);
	RolloverLit.assign(RolloverLit.size(), 0);
}

void FPinballSim::DrainBall()
{
	Emit(EPinballEvent::BallDrained, CurrentBall);
	if (CurrentBall >= BallsPerGame)
	{
		CurrentPhase = EPinballPhase::GameOver;
		BallVel = FPinVec2();
		Emit(EPinballEvent::GameOver, -1, CurrentScore);
		return;
	}
	++CurrentBall;
	ServeBall();
}

double FPinballSim::FlipperAngle(int Side) const
{
	return (Side == 0 || Side == 1) ? FlipperStates[Side].Angle : 0.0;
}

FPinVec2 FPinballSim::FlipperTip(int Side) const
{
	if (Side != 0 && Side != 1)
	{
		return FPinVec2();
	}
	const FPinballFlipperDef& Def = TableLayout.Flippers[Side];
	const double Angle = FlipperStates[Side].Angle;
	return Def.Pivot + FPinVec2(std::cos(Angle), std::sin(Angle)) * Def.Length;
}

bool FPinballSim::IsRolloverLit(size_t Index) const
{
	return Index < RolloverLit.size() && RolloverLit[Index] != 0;
}

std::vector<FPinballEvent> FPinballSim::TakeEvents()
{
	std::vector<FPinballEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FPinballSim::PlaceBall(FPinVec2 Position, FPinVec2 Velocity)
{
	BallPos = Position;
	BallVel = Velocity;
	CurrentPhase = EPinballPhase::InPlay;
	if (CurrentBall == 0)
	{
		CurrentBall = 1;
	}
}

void FPinballSim::Emit(EPinballEvent Type, int Index, int Points)
{
	PendingEvents.push_back(FPinballEvent{ Type, Index, Points });
}
