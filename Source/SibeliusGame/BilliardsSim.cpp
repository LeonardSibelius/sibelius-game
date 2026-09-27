// BilliardsSim.cpp — see the header for the design, and Tools/Tests/billiards_sim_test.cpp
// for what it is held to.

#include "BilliardsSim.h"

#include <cmath>
#include <initializer_list>

// NAMED, NOT ANONYMOUS, AND NOT PULLED IN WITH "using": the unity-build rule (see
// PinballSim.cpp's PinMath). Qualified calls keep this file sealed.
namespace PoolMath
{
	inline double Dot(FBilliardsVec2 A, FBilliardsVec2 B) { return A.X * B.X + A.Y * B.Y; }
	inline double Length(FBilliardsVec2 A) { return std::sqrt(Dot(A, A)); }
	// Slip below this is rolling: RollingSpin leaves rounding-sized slip behind, and a
	// ball that never counts as rolling never feels rolling resistance, so never stops.
	constexpr double RollingSlip = 1e-6;   // cm/s

	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	inline FBilliardsVec2 Normalized(FBilliardsVec2 A, FBilliardsVec2 Fallback = FBilliardsVec2(1.0, 0.0))
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : Fallback;
	}

	inline FBilliardsVec2 ClosestOnSegment(FBilliardsVec2 P, FBilliardsVec2 A, FBilliardsVec2 B)
	{
		const FBilliardsVec2 AB = B - A;
		const double L2 = Dot(AB, AB);
		const double T = L2 > 1e-12 ? Clamp(Dot(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return A + AB * T;
	}

	// The slip of the contact point under a ball: zero when it rolls. A ball spinning about
	// the +Y axis rolls toward +X, so the contact point moves at V - R * (spin x Z).
	inline FBilliardsVec2 Slip(FBilliardsVec2 Vel, FBilliardsVec3 Spin, double Radius)
	{
		return FBilliardsVec2(Vel.X - Radius * Spin.Y, Vel.Y + Radius * Spin.X);
	}

	// The spin that makes a ball of this velocity roll without slipping.
	inline FBilliardsVec3 RollingSpin(FBilliardsVec2 Vel, double Radius)
	{
		return FBilliardsVec3{ -Vel.Y / Radius, Vel.X / Radius, 0.0 };
	}

	// First T >= 0 where P + D*T comes within Radius of C, or -1. D is a unit vector.
	inline double RayCircle(FBilliardsVec2 P, FBilliardsVec2 D, FBilliardsVec2 C, double Radius)
	{
		const FBilliardsVec2 M = P - C;
		const double B = Dot(M, D);
		const double Cc = Dot(M, M) - Radius * Radius;
		if (Cc > 0.0 && B > 0.0)
		{
			return -1.0;   // outside and heading away
		}
		const double Disc = B * B - Cc;
		if (Disc < 0.0)
		{
			return -1.0;
		}
		const double T = -B - std::sqrt(Disc);
		return T < 0.0 ? 0.0 : T;
	}
}

// ===========================================================================
// THE LIVING-ROOM TABLE — PijayArt's Classic Billiards Table (Fab, "Generated with AI:
// No"), measured on 2026-09-27 from its own FBX (SM_Billiard_Table.fbx, 1636 vertices), by
// slicing the mesh at several heights above the cloth:
//
//   - the cloth is flat at 89.01 cm, and the cushion faces are VERTICAL, straight up from
//     the cloth edge to the rail top at 93.20: so the cushion line is the cloth's own
//     outline, 231.06 x 140.10 cm, at +-115.53 and +-70.05
//   - the six pockets are circles cut in the cloth. A least-squares fit of the slice:
//     corners centred at (+-114.29, +-67.89), radius 6.18 (a little inside the cushion
//     corner, as real corner pockets are), sides at (0, +-70.05), radius 6.19, centred ON
//     the cushion line
//   - each ball is 7.00 cm across (SM_White_Billiard_Ball.fbx, +-3.5 on every axis)
//
// So the playing field is 1.65 : 1, wider than a real table's 2 : 1, with balls a size too
// big and pockets 1.77 balls across. The model keeps all of it: it has to match the art.
//
// The mesh is Y-up; imported, the table's long axis is the actor's X and the cloth is
// 89.01 up its Z. The table is symmetric in both axes, so the sign of Y cannot matter.
//
// The spots are the standard ones, a quarter of the length from each end.

FBilliardsTableLayout FBilliardsTableLayout::MakeDefault()
{
	FBilliardsTableLayout L;

	const double HL = L.HalfLength;
	const double HW = L.HalfWidth;

	const double CornerX = 114.29;
	const double CornerY = 67.89;
	const double CornerR = 6.18;
	const double SideR = 6.19;

	// Pockets, numbered for events: 0 foot-left, 1 side-left, 2 head-left, then the right
	// side (-Y) the same way. "Left" is +Y, seen from the head end looking up the table.
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(CornerX, CornerY), CornerR });
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(0.0, HW), SideR });
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(-CornerX, CornerY), CornerR });
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(CornerX, -CornerY), CornerR });
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(0.0, -HW), SideR });
	L.Pockets.push_back(FBilliardsPocket{ FBilliardsVec2(-CornerX, -CornerY), CornerR });

	// The cushions run between the points where each pocket circle cuts the cushion line:
	// those points are the jaws.
	const double CornerJawX = CornerX - std::sqrt(CornerR * CornerR - (HW - CornerY) * (HW - CornerY));   // along a long rail
	const double CornerJawY = CornerY - std::sqrt(CornerR * CornerR - (HL - CornerX) * (HL - CornerX));   // along a short rail
	const double SideJawX = SideR;

	for (const double Sy : { 1.0, -1.0 })
	{
		L.Cushions.push_back(FBilliardsCushion{ FBilliardsVec2(-CornerJawX, Sy * HW), FBilliardsVec2(-SideJawX, Sy * HW) });
		L.Cushions.push_back(FBilliardsCushion{ FBilliardsVec2(SideJawX, Sy * HW), FBilliardsVec2(CornerJawX, Sy * HW) });
	}
	for (const double Sx : { 1.0, -1.0 })
	{
		L.Cushions.push_back(FBilliardsCushion{ FBilliardsVec2(Sx * HL, -CornerJawY), FBilliardsVec2(Sx * HL, CornerJawY) });
	}

	L.FootSpot = FBilliardsVec2(HL * 0.5, 0.0);
	L.HeadSpot = FBilliardsVec2(-HL * 0.5, 0.0);
	return L;
}

// ===========================================================================

FBilliardsSim::FBilliardsSim(FBilliardsTableLayout InLayout)
	: TableLayout(static_cast<FBilliardsTableLayout&&>(InLayout))
{
	Rack();
	CurrentPhase = EBilliardsPhase::Idle;
}

void FBilliardsSim::Rack()
{
	const double R = TableLayout.BallRadius;
	const double Pitch = 2.0 * R + 0.01;   // a hair apart: touching balls would press on
	                                       // each other every substep before the break

	// The usual eight-ball rack, apex toward the head: the 1 on the spot, the 8 in the
	// middle of the third row. Solo play does not care about the order; it looks right.
	static constexpr int Order[15] = { 1, 9, 2, 10, 8, 3, 11, 7, 14, 4, 5, 13, 15, 6, 12 };

	int Next = 0;
	for (int Row = 0; Row < 5; ++Row)
	{
		for (int Col = 0; Col <= Row; ++Col)
		{
			FBall& Ball = Balls[Order[Next++]];
			Ball.Pos = TableLayout.FootSpot + FBilliardsVec2(Row * Pitch * std::sqrt(3.0) * 0.5, (Col - Row * 0.5) * Pitch);
			Ball.Vel = FBilliardsVec2();
			Ball.Spin = FBilliardsVec3();
			Ball.bOnTable = true;
		}
	}

	FBall& Cue = Balls[CueBall];
	Cue.Pos = TableLayout.HeadSpot;
	Cue.Vel = FBilliardsVec2();
	Cue.Spin = FBilliardsVec3();
	Cue.bOnTable = true;
}

void FBilliardsSim::StartGame()
{
	Rack();
	ShotCount = 0;
	PenaltyCount = 0;
	PottedThisShot = 0;
	bCueScratched = false;
	Accumulator = 0.0;
	ShotSeconds = 0.0;
	CurrentPhase = EBilliardsPhase::Aiming;
	Emit(EBilliardsEvent::GameStarted);
}

bool FBilliardsSim::Shoot(double Angle, double Power, double Spin)
{
	if (CurrentPhase != EBilliardsPhase::Aiming || !Balls[CueBall].bOnTable)
	{
		return false;
	}

	const double Speed = MinCueSpeed + PoolMath::Clamp(Power, 0.0, 1.0) * (MaxCueSpeed - MinCueSpeed);
	const FBilliardsVec2 Dir(std::cos(Angle), std::sin(Angle));
	const double R = TableLayout.BallRadius;

	// A cue striking H above the centre gives a spin of 5*V*H / (2*R^2) about the axis
	// across the shot (the impulse-and-torque of a solid sphere). H = 2R/5 is exactly the
	// rolling spin, which is why spin 0.8 at MaxTipOffset 0.5 is natural roll.
	const double H = PoolMath::Clamp(Spin, -1.0, 1.0) * MaxTipOffset * R;
	const double W = 5.0 * Speed * H / (2.0 * R * R);

	FBall& Cue = Balls[CueBall];
	Cue.Vel = Dir * Speed;
	Cue.Spin = FBilliardsVec3{ -Dir.Y * W, Dir.X * W, 0.0 };

	++ShotCount;
	PottedThisShot = 0;
	bCueScratched = false;
	ShotSeconds = 0.0;
	CurrentPhase = EBilliardsPhase::Rolling;
	Emit(EBilliardsEvent::ShotTaken, ShotCount, -1, Speed);
	return true;
}

void FBilliardsSim::Advance(double DeltaSeconds)
{
	Accumulator += PoolMath::Clamp(DeltaSeconds, 0.0, MaxFrameSeconds);
	while (Accumulator >= SubstepSeconds)
	{
		Accumulator -= SubstepSeconds;
		Substep();
	}
}

void FBilliardsSim::Substep()
{
	for (FBall& Ball : Balls)
	{
		if (Ball.bOnTable)
		{
			StepBall(Ball);
		}
	}
	CollideBalls();
	CollideCushions();
	CheckPockets();

	if (CurrentPhase == EBilliardsPhase::Rolling)
	{
		ShotSeconds += SubstepSeconds;
		if (!AnyBallMoving() || ShotSeconds > MaxShotSeconds)
		{
			EndShot();
		}
	}
}

// One ball, one substep of friction. Sliding: the cloth pushes against the slip at
// SlidingFriction*g, and the same force spins the ball up (or down), so the slip dies at
// 7/2 of that rate - the 2/5 in a solid sphere's moment of inertia. When the slip would
// cross zero this step, it snaps to exact rolling instead, keeping 5/7 of the momentum
// change that sliding had left to make. Rolling: only rolling resistance, straight back.
void FBilliardsSim::StepBall(FBall& Ball) const
{
	const double R = TableLayout.BallRadius;
	const double Dt = SubstepSeconds;
	const FBilliardsVec2 U = PoolMath::Slip(Ball.Vel, Ball.Spin, R);
	const double Slip = PoolMath::Length(U);
	const double SlipStep = 3.5 * SlidingFriction * Gravity * Dt;

	if (Slip > SlipStep)
	{
		const FBilliardsVec2 A = U * (-SlidingFriction * Gravity / Slip);
		Ball.Vel = Ball.Vel + A * Dt;
		Ball.Spin.X += 2.5 / R * A.Y * Dt;
		Ball.Spin.Y -= 2.5 / R * A.X * Dt;
	}
	else if (Slip > PoolMath::RollingSlip)
	{
		Ball.Vel = Ball.Vel - U * (2.0 / 7.0);
		Ball.Spin = PoolMath::RollingSpin(Ball.Vel, R);
	}
	else
	{
		const double Speed = PoolMath::Length(Ball.Vel);
		const double Slowed = Speed - RollingFriction * Gravity * Dt;
		if (Slowed <= 0.0)
		{
			Ball.Vel = FBilliardsVec2();
			Ball.Spin = FBilliardsVec3();
		}
		else
		{
			Ball.Vel = Ball.Vel * (Slowed / Speed);
			Ball.Spin = PoolMath::RollingSpin(Ball.Vel, R);
		}
	}

	const double Speed = PoolMath::Length(Ball.Vel);
	if (Speed > MaxBallSpeed)
	{
		Ball.Vel = Ball.Vel * (MaxBallSpeed / Speed);
	}
	Ball.Pos = Ball.Pos + Ball.Vel * Dt;
}

// Equal masses, a normal impulse only: velocities along the line of centres trade, less
// the restitution; spin is untouched, which is what makes stun, draw and follow work.
// Several passes, because a rack is fifteen balls touching at once.
void FBilliardsSim::CollideBalls()
{
	const double TwoR = 2.0 * TableLayout.BallRadius;
	for (int Pass = 0; Pass < ContactIterations; ++Pass)
	{
		for (int I = 0; I < BallCount; ++I)
		{
			if (!Balls[I].bOnTable)
			{
				continue;
			}
			for (int J = I + 1; J < BallCount; ++J)
			{
				if (!Balls[J].bOnTable)
				{
					continue;
				}
				FBall& A = Balls[I];
				FBall& B = Balls[J];
				// Two balls at rest cannot collide. Most of a stroke is one or two balls
				// rolling among a dozen still ones, so this is most of the work skipped.
				if (A.Vel.X == 0.0 && A.Vel.Y == 0.0 && B.Vel.X == 0.0 && B.Vel.Y == 0.0)
				{
					continue;
				}
				const FBilliardsVec2 D = B.Pos - A.Pos;
				const double Dist2 = PoolMath::Dot(D, D);
				if (Dist2 >= TwoR * TwoR)
				{
					continue;
				}
				const double Dist = std::sqrt(Dist2);
				const FBilliardsVec2 N = PoolMath::Normalized(D);
				const double Closing = PoolMath::Dot(B.Vel - A.Vel, N);   // < 0: approaching
				if (Closing < 0.0)
				{
					const double Impulse = -0.5 * (1.0 + BallRestitution) * Closing;
					A.Vel = A.Vel - N * Impulse;
					B.Vel = B.Vel + N * Impulse;
					if (-Closing > EventSpeed)
					{
						Emit(EBilliardsEvent::BallHitBall, I, J, -Closing);
					}
				}
				const double Push = 0.5 * (TwoR - Dist);
				A.Pos = A.Pos - N * Push;
				B.Pos = B.Pos + N * Push;
			}
		}
	}
}

// Each cushion is a line with rounded ends: the ends are the pocket jaws, and a ball that
// clips one bounces off it at whatever angle the jaw gives.
void FBilliardsSim::CollideCushions()
{
	const double R = TableLayout.BallRadius;
	for (int I = 0; I < BallCount; ++I)
	{
		FBall& Ball = Balls[I];
		if (!Ball.bOnTable)
		{
			continue;
		}
		for (const FBilliardsCushion& Cushion : TableLayout.Cushions)
		{
			const FBilliardsVec2 Q = PoolMath::ClosestOnSegment(Ball.Pos, Cushion.A, Cushion.B);
			const FBilliardsVec2 D = Ball.Pos - Q;
			const double Dist = PoolMath::Length(D);
			if (Dist >= R)
			{
				continue;
			}
			// Dead on the line only happens to a ball already past it; push it back onto
			// the table side, toward the centre.
			const FBilliardsVec2 N = Dist > 1e-9 ? D * (1.0 / Dist) : PoolMath::Normalized(Q * -1.0);
			const double Vn = PoolMath::Dot(Ball.Vel, N);
			if (Vn < 0.0)
			{
				const FBilliardsVec2 Tangent = Ball.Vel - N * Vn;
				const double Bounce = -Vn > RestingImpactSpeed ? -Vn * CushionRestitution : 0.0;
				Ball.Vel = Tangent * (1.0 - CushionFriction) + N * Bounce;
				if (-Vn > EventSpeed)
				{
					Emit(EBilliardsEvent::BallHitCushion, I, -1, -Vn);
				}
			}
			Ball.Pos = Q + N * R;
		}
	}
}

void FBilliardsSim::CheckPockets()
{
	const double HL = TableLayout.HalfLength;
	const double HW = TableLayout.HalfWidth;
	for (int I = 0; I < BallCount; ++I)
	{
		if (!Balls[I].bOnTable)
		{
			continue;
		}
		const FBilliardsVec2 P = Balls[I].Pos;
		for (int K = 0; K < static_cast<int>(TableLayout.Pockets.size()); ++K)
		{
			const FBilliardsPocket& Hole = TableLayout.Pockets[K];
			if (PoolMath::Length(P - Hole.Center) < Hole.Radius)
			{
				this->Pocket(I, K);
				break;
			}
		}
		// The safety net: the cushions leave no way off the cloth except through a pocket
		// mouth, and every mouth is inside its pocket's circle. If a ball is off the cloth
		// anyway, it went down the nearest hole rather than out into the room.
		if (Balls[I].bOnTable && (std::fabs(P.X) > HL || std::fabs(P.Y) > HW))
		{
			this->Pocket(I, NearestPocket(P));
		}
	}
}

int FBilliardsSim::NearestPocket(FBilliardsVec2 Position) const
{
	int Best = 0;
	double BestDist = 1e300;
	for (int K = 0; K < static_cast<int>(TableLayout.Pockets.size()); ++K)
	{
		const double Dist = PoolMath::Length(Position - TableLayout.Pockets[K].Center);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = K;
		}
	}
	return Best;
}

void FBilliardsSim::Pocket(int Ball, int PocketIndex)
{
	FBall& B = Balls[Ball];
	B.bOnTable = false;
	B.Pos = TableLayout.Pockets[PocketIndex].Center;
	B.Vel = FBilliardsVec2();
	B.Spin = FBilliardsVec3();

	if (Ball == CueBall)
	{
		if (!bCueScratched)
		{
			bCueScratched = true;
			++PenaltyCount;
			Emit(EBilliardsEvent::CueBallScratched, CueBall, PocketIndex);
		}
	}
	else
	{
		++PottedThisShot;
		Emit(EBilliardsEvent::BallPocketed, Ball, PocketIndex);
	}
}

void FBilliardsSim::EndShot()
{
	// The watchdog's case: whatever still creeps is stopped where it is.
	for (FBall& Ball : Balls)
	{
		Ball.Vel = FBilliardsVec2();
		Ball.Spin = FBilliardsVec3();
	}

	Emit(EBilliardsEvent::ShotEnded, PottedThisShot);

	if (BallsLeft() == 0)
	{
		CurrentPhase = EBilliardsPhase::Cleared;
		Emit(EBilliardsEvent::TableCleared, Strokes());
		return;
	}
	if (!Balls[CueBall].bOnTable)
	{
		RespotCueBall();
	}
	CurrentPhase = EBilliardsPhase::Aiming;
}

// Back on the head spot; if a ball sits there, the nearest free place behind it on the
// long axis, then either side. There is always one: fifteen balls cannot cover the table.
void FBilliardsSim::RespotCueBall()
{
	const double Step = 2.0 * TableLayout.BallRadius + 0.5;
	const FBilliardsVec2 Spot = TableLayout.HeadSpot;
	FBilliardsVec2 Chosen = Spot;
	bool bFound = IsSpotFree(Spot);
	for (int Ring = 1; !bFound && Ring < 40; ++Ring)
	{
		const FBilliardsVec2 Tries[4] = {
			Spot + FBilliardsVec2(-Ring * Step, 0.0),
			Spot + FBilliardsVec2(Ring * Step, 0.0),
			Spot + FBilliardsVec2(0.0, Ring * Step),
			Spot + FBilliardsVec2(0.0, -Ring * Step),
		};
		for (const FBilliardsVec2& Try : Tries)
		{
			if (IsSpotFree(Try))
			{
				Chosen = Try;
				bFound = true;
				break;
			}
		}
	}

	FBall& Cue = Balls[CueBall];
	Cue.Pos = Chosen;
	Cue.Vel = FBilliardsVec2();
	Cue.Spin = FBilliardsVec3();
	Cue.bOnTable = true;
	Emit(EBilliardsEvent::CueBallRespotted);
}

bool FBilliardsSim::IsSpotFree(FBilliardsVec2 Position) const
{
	const double R = TableLayout.BallRadius;
	if (std::fabs(Position.X) > TableLayout.HalfLength - R || std::fabs(Position.Y) > TableLayout.HalfWidth - R)
	{
		return false;
	}
	for (const FBilliardsPocket& Hole : TableLayout.Pockets)
	{
		if (PoolMath::Length(Position - Hole.Center) < Hole.Radius + R)
		{
			return false;
		}
	}
	for (int I = 1; I < BallCount; ++I)
	{
		if (Balls[I].bOnTable && PoolMath::Length(Position - Balls[I].Pos) < 2.0 * R + 0.1)
		{
			return false;
		}
	}
	return true;
}

bool FBilliardsSim::IsSliding(int Ball) const
{
	const FBall& B = Balls[Ball];
	return PoolMath::Length(PoolMath::Slip(B.Vel, B.Spin, TableLayout.BallRadius)) > PoolMath::RollingSlip;
}

bool FBilliardsSim::AnyBallMoving() const
{
	for (const FBall& Ball : Balls)
	{
		if (Ball.bOnTable && (Ball.Vel.X != 0.0 || Ball.Vel.Y != 0.0 || Ball.Spin.X != 0.0 || Ball.Spin.Y != 0.0))
		{
			return true;
		}
	}
	return false;
}

int FBilliardsSim::BallsLeft() const
{
	int Count = 0;
	for (int I = 1; I < BallCount; ++I)
	{
		if (Balls[I].bOnTable)
		{
			++Count;
		}
	}
	return Count;
}

FBilliardsAimPrediction FBilliardsSim::PredictShot(double Angle) const
{
	FBilliardsAimPrediction Out;
	const FBall& Cue = Balls[CueBall];
	if (!Cue.bOnTable)
	{
		return Out;
	}
	const double R = TableLayout.BallRadius;
	const FBilliardsVec2 P = Cue.Pos;
	const FBilliardsVec2 D(std::cos(Angle), std::sin(Angle));

	// Where it meets the cushion line, inset by a radius: the far wall of the box the
	// centre can reach.
	double TMax = 1e300;
	const double BoxX = TableLayout.HalfLength - R;
	const double BoxY = TableLayout.HalfWidth - R;
	if (D.X > 1e-12) { TMax = std::fmin(TMax, (BoxX - P.X) / D.X); }
	if (D.X < -1e-12) { TMax = std::fmin(TMax, (-BoxX - P.X) / D.X); }
	if (D.Y > 1e-12) { TMax = std::fmin(TMax, (BoxY - P.Y) / D.Y); }
	if (D.Y < -1e-12) { TMax = std::fmin(TMax, (-BoxY - P.Y) / D.Y); }
	TMax = std::fmax(TMax, 0.0);

	double Best = TMax;
	for (int I = 1; I < BallCount; ++I)
	{
		if (!Balls[I].bOnTable)
		{
			continue;
		}
		const double T = PoolMath::RayCircle(P, D, Balls[I].Pos, 2.0 * R);
		if (T >= 0.0 && T < Best)
		{
			Best = T;
			Out.bHitsBall = true;
			Out.Ball = I;
		}
	}

	Out.CueAtContact = P + D * Best;
	if (Out.bHitsBall)
	{
		const FBilliardsVec2 N = PoolMath::Normalized(Balls[Out.Ball].Pos - Out.CueAtContact);
		Out.ObjectDirection = N;
		const FBilliardsVec2 Tangent = D - N * PoolMath::Dot(D, N);
		Out.CueDeflection = PoolMath::Length(Tangent) > 1e-3 ? PoolMath::Normalized(Tangent) : FBilliardsVec2();
	}
	else
	{
		Out.Ball = -1;
	}
	return Out;
}

std::vector<FBilliardsEvent> FBilliardsSim::TakeEvents()
{
	std::vector<FBilliardsEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FBilliardsSim::PlaceBall(int Ball, FBilliardsVec2 Position, FBilliardsVec2 Velocity, bool bRolling)
{
	FBall& B = Balls[Ball];
	B.Pos = Position;
	B.Vel = Velocity;
	B.Spin = bRolling ? PoolMath::RollingSpin(Velocity, TableLayout.BallRadius) : FBilliardsVec3();
	B.bOnTable = true;
	if (CurrentPhase != EBilliardsPhase::Cleared && (Velocity.X != 0.0 || Velocity.Y != 0.0))
	{
		CurrentPhase = EBilliardsPhase::Rolling;
		ShotSeconds = 0.0;
	}
}

void FBilliardsSim::RemoveBall(int Ball)
{
	Balls[Ball].bOnTable = false;
	Balls[Ball].Vel = FBilliardsVec2();
	Balls[Ball].Spin = FBilliardsVec3();
}

void FBilliardsSim::ClearTable()
{
	for (int I = 0; I < BallCount; ++I)
	{
		RemoveBall(I);
	}
	PendingEvents.clear();
	CurrentPhase = EBilliardsPhase::Aiming;
}

void FBilliardsSim::Emit(EBilliardsEvent Type, int Index, int Other, double Value)
{
	PendingEvents.push_back(FBilliardsEvent{ Type, Index, Other, Value });
}
