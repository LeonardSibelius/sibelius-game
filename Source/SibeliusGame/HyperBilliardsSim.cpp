// HyperBilliardsSim.cpp — see the header for the design, and Tools/Tests/hyper_billiards_test.cpp
// for what it is held to.

#include "HyperBilliardsSim.h"

#include <cmath>
#include <initializer_list>

// NAMED, NOT ANONYMOUS, AND NOT PULLED IN WITH "using": the unity-build rule (BilliardsSim.cpp's
// PoolMath). Qualified calls keep this file sealed.
namespace HyperMath
{
	inline double Dot(FHyperVec3 A, FHyperVec3 B) { return A.X * B.X + A.Y * B.Y + A.W * B.W; }
	inline double Length(FHyperVec3 A) { return std::sqrt(Dot(A, A)); }
	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	// Slip below this is rolling, as at the living-room table: snapping leaves rounding-sized
	// slip behind, and a ball that never counts as rolling never feels rolling resistance.
	constexpr double RollingSlip = 1e-6;   // cm/s

	inline FHyperVec3 Normalized(FHyperVec3 A, FHyperVec3 Fallback = FHyperVec3(1.0, 0.0, 0.0))
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : Fallback;
	}

	inline bool IsZero(FHyperVec3 A) { return A.X == 0.0 && A.Y == 0.0 && A.W == 0.0; }

	// The direction of a shot: a heading in the X-Y plane, tilted into W.
	inline FHyperVec3 Direction(double Heading, double Tilt)
	{
		return FHyperVec3(std::cos(Tilt) * std::cos(Heading), std::cos(Tilt) * std::sin(Heading), std::sin(Tilt));
	}

	// First T >= 0 where P + D*T comes within Radius of C, or -1. D is a unit vector.
	inline double RaySphere(FHyperVec3 P, FHyperVec3 D, FHyperVec3 C, double Radius)
	{
		const FHyperVec3 M = P - C;
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

	// The component of A along axis 0 (X), 1 (Y) or 2 (W), by reference.
	inline double& Axis(FHyperVec3& A, int I) { return I == 0 ? A.X : (I == 1 ? A.Y : A.W); }
}

// The table in Aisling's snowfield. Not measured from a mesh, as the living room's is: there
// is no such table to measure, so the actor builds the one this describes. 2.2 m by 1.1 m, like
// a big pool table, and 60 cm into W - six balls deep, so a rack fills half of it and a slice
// shows a fair share of the game without showing all of it.
FHyperTableLayout FHyperTableLayout::MakeDefault()
{
	FHyperTableLayout L;
	const double HL = L.HalfLength;
	const double HW = L.HalfWidth;
	const double HD = L.HalfDepth;
	for (const double X : { -HL, HL })
	{
		for (const double Y : { -HW, HW })
		{
			for (const double W : { -HD, HD })
			{
				L.Pockets.push_back(FHyperVec3(X, Y, W));
			}
		}
	}
	for (const double Y : { -HW, HW })
	{
		for (const double W : { -HD, HD })
		{
			L.Pockets.push_back(FHyperVec3(0.0, Y, W));
		}
	}
	L.FootSpot = FHyperVec3(HL * 0.5, 0.0, 0.0);
	L.HeadSpot = FHyperVec3(-HL * 0.5, 0.0, 0.0);
	return L;
}

FHyperBilliardsSim::FHyperBilliardsSim(FHyperTableLayout InLayout)
	: TableLayout(static_cast<FHyperTableLayout&&>(InLayout))
{
	Rack();
}

void FHyperBilliardsSim::StartGame()
{
	Rack();
	ShotCount = 0;
	PenaltyCount = 0;
	PottedThisShot = 0;
	bCueScratched = false;
	Accumulator = 0.0;
	CurrentPhase = EHyperPhase::Aiming;
	PendingEvents.clear();
	Emit(EHyperEvent::GameStarted);
}

// Ten balls in a tetrahedron, apex on the foot spot pointing at the cue ball: layers of 1, 3
// and 6 marching away along X, each a triangle in the Y-W space. Touching balls in a regular
// tetrahedron sit sqrt(2/3) of a diameter apart in X. The gap keeps them a hair from touching,
// so a break starts from a clean first contact rather than ten at once.
void FHyperBilliardsSim::Rack()
{
	const double R = TableLayout.BallRadius;
	const double Pitch = 2.0 * R + 0.02;
	const double LayerStep = Pitch * std::sqrt(2.0 / 3.0);
	const double RowStep = Pitch * std::sqrt(3.0) * 0.5;

	int Next = 1;
	for (int Layer = 0; Layer < 3; ++Layer)
	{
		const int Side = Layer + 1;
		// The layer's centroid, so its triangle sits on the table's axis.
		double SumY = 0.0;
		double SumW = 0.0;
		int Count = 0;
		for (int Row = 0; Row < Side; ++Row)
		{
			for (int Col = 0; Col < Side - Row; ++Col)
			{
				SumY += Col * Pitch + Row * Pitch * 0.5;
				SumW += Row * RowStep;
				++Count;
			}
		}
		const double CY = SumY / Count;
		const double CW = SumW / Count;
		for (int Row = 0; Row < Side; ++Row)
		{
			for (int Col = 0; Col < Side - Row; ++Col)
			{
				FBall& B = Balls[Next++];
				B.Pos = TableLayout.FootSpot + FHyperVec3(Layer * LayerStep,
					Col * Pitch + Row * Pitch * 0.5 - CY, Row * RowStep - CW);
				B.Vel = FHyperVec3();
				B.Roll = FHyperVec3();
				B.bOnTable = true;
			}
		}
	}

	FBall& Cue = Balls[CueBall];
	Cue.Pos = TableLayout.HeadSpot;
	Cue.Vel = FHyperVec3();
	Cue.Roll = FHyperVec3();
	Cue.bOnTable = true;
}

// A cue striking H above the centre carries the ball at roll = RollGain * V * H / R: the
// impulse-and-torque of a solid 4-ball. H = R / 3 is exactly rolling, which is why spin 2/3 at
// MaxTipOffset 0.5 is natural roll here.
bool FHyperBilliardsSim::Shoot(double Heading, double Tilt, double Power, double Spin)
{
	if (CurrentPhase != EHyperPhase::Aiming || !Balls[CueBall].bOnTable)
	{
		return false;
	}
	const double Speed = MinCueSpeed + HyperMath::Clamp(Power, 0.0, 1.0) * (MaxCueSpeed - MinCueSpeed);
	const FHyperVec3 Dir = HyperMath::Direction(Heading, Tilt);
	const double TipOverR = HyperMath::Clamp(Spin, -1.0, 1.0) * MaxTipOffset;

	FBall& Cue = Balls[CueBall];
	Cue.Vel = Dir * Speed;
	Cue.Roll = Dir * (RollGain * Speed * TipOverR);

	++ShotCount;
	PottedThisShot = 0;
	bCueScratched = false;
	ShotSeconds = 0.0;
	CurrentPhase = EHyperPhase::Rolling;
	Emit(EHyperEvent::ShotTaken, ShotCount, -1, Speed);
	return true;
}

void FHyperBilliardsSim::Advance(double DeltaSeconds)
{
	Accumulator += HyperMath::Clamp(DeltaSeconds, 0.0, MaxFrameSeconds);
	while (Accumulator >= SubstepSeconds)
	{
		Accumulator -= SubstepSeconds;
		Substep();
	}
}

void FHyperBilliardsSim::Substep()
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

	if (CurrentPhase == EHyperPhase::Rolling)
	{
		ShotSeconds += SubstepSeconds;
		if (!AnyBallMoving() || ShotSeconds > MaxShotSeconds)
		{
			EndShot();
		}
	}
}

// One ball, one substep of friction. Sliding: the cloth pushes against the slip at
// SlidingFriction*g and the same force spins the roll toward the velocity at RollGain times
// that, so the slip dies at (1 + RollGain) of it. When the slip would cross zero this step it
// snaps to exact rolling, keeping the 1/(1 + RollGain) share of the change sliding had left to
// make. Rolling: only rolling resistance, straight back.
void FHyperBilliardsSim::StepBall(FBall& Ball) const
{
	const double Dt = SubstepSeconds;
	const FHyperVec3 Slip = Ball.Vel - Ball.Roll;
	const double SlipSpeed = HyperMath::Length(Slip);
	const double SlipStep = (1.0 + RollGain) * SlidingFriction * Gravity * Dt;

	if (SlipSpeed > SlipStep)
	{
		const FHyperVec3 A = Slip * (-SlidingFriction * Gravity / SlipSpeed);
		Ball.Vel = Ball.Vel + A * Dt;
		Ball.Roll = Ball.Roll - A * (RollGain * Dt);
	}
	else if (SlipSpeed > HyperMath::RollingSlip)
	{
		Ball.Vel = Ball.Vel - Slip * (1.0 / (1.0 + RollGain));
		Ball.Roll = Ball.Vel;
	}
	else
	{
		const double Speed = HyperMath::Length(Ball.Vel);
		const double Slowed = Speed - RollingFriction * Gravity * Dt;
		if (Slowed <= 0.0)
		{
			Ball.Vel = FHyperVec3();
		}
		else
		{
			Ball.Vel = Ball.Vel * (Slowed / Speed);
		}
		Ball.Roll = Ball.Vel;
	}

	const double Speed = HyperMath::Length(Ball.Vel);
	if (Speed > MaxBallSpeed)
	{
		Ball.Vel = Ball.Vel * (MaxBallSpeed / Speed);
	}
	Ball.Pos = Ball.Pos + Ball.Vel * Dt;
}

// Equal masses, a normal impulse only, along the line of centres in all three directions of
// the cloth; the roll is untouched, which is what makes stun, draw and follow work. Several
// passes, because a rack is ten balls touching at once.
void FHyperBilliardsSim::CollideBalls()
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
				if (HyperMath::IsZero(A.Vel) && HyperMath::IsZero(B.Vel))
				{
					continue;   // two balls at rest cannot collide
				}
				const FHyperVec3 D = B.Pos - A.Pos;
				const double Dist2 = HyperMath::Dot(D, D);
				if (Dist2 >= TwoR * TwoR)
				{
					continue;
				}
				const double Dist = std::sqrt(Dist2);
				const FHyperVec3 N = HyperMath::Normalized(D);
				const double Closing = HyperMath::Dot(B.Vel - A.Vel, N);   // < 0: approaching
				if (Closing < 0.0)
				{
					const double Impulse = -0.5 * (1.0 + BallRestitution) * Closing;
					A.Vel = A.Vel - N * Impulse;
					B.Vel = B.Vel + N * Impulse;
					if (-Closing > EventSpeed)
					{
						Emit(EHyperEvent::BallHitBall, I, J, -Closing);
					}
				}
				const double Push = 0.5 * (TwoR - Dist);
				A.Pos = A.Pos - N * Push;
				B.Pos = B.Pos + N * Push;
			}
		}
	}
}

// The six faces of the box. A ball's centre keeps a radius from each; one coming in bounces
// off with CushionRestitution and loses a little of its speed along the face.
void FHyperBilliardsSim::CollideCushions()
{
	const double R = TableLayout.BallRadius;
	const double Limits[3] = { TableLayout.HalfLength - R, TableLayout.HalfWidth - R, TableLayout.HalfDepth - R };
	for (int I = 0; I < BallCount; ++I)
	{
		FBall& Ball = Balls[I];
		if (!Ball.bOnTable)
		{
			continue;
		}
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			double& P = HyperMath::Axis(Ball.Pos, Axis);
			const double Side = P > Limits[Axis] ? 1.0 : (P < -Limits[Axis] ? -1.0 : 0.0);
			if (Side == 0.0)
			{
				continue;
			}
			P = Side * Limits[Axis];
			double& V = HyperMath::Axis(Ball.Vel, Axis);
			const double Into = V * Side;   // > 0: moving into the face
			if (Into > 0.0)
			{
				const double Bounce = Into > RestingImpactSpeed ? Into * CushionRestitution : 0.0;
				for (int Other = 0; Other < 3; ++Other)
				{
					if (Other != Axis)
					{
						HyperMath::Axis(Ball.Vel, Other) *= 1.0 - CushionFriction;
					}
				}
				V = -Side * Bounce;
				if (Into > EventSpeed)
				{
					Emit(EHyperEvent::BallHitCushion, I, -1, Into);
				}
			}
		}
	}
}

void FHyperBilliardsSim::CheckPockets()
{
	const double Capture2 = TableLayout.PocketCapture * TableLayout.PocketCapture;
	for (int I = 0; I < BallCount; ++I)
	{
		if (!Balls[I].bOnTable)
		{
			continue;
		}
		for (int P = 0; P < static_cast<int>(TableLayout.Pockets.size()); ++P)
		{
			const FHyperVec3 D = Balls[I].Pos - TableLayout.Pockets[P];
			if (HyperMath::Dot(D, D) < Capture2)
			{
				Pocket(I, P);
				break;
			}
		}
	}
}

void FHyperBilliardsSim::Pocket(int Ball, int PocketIndex)
{
	FBall& B = Balls[Ball];
	B.bOnTable = false;
	B.Pos = TableLayout.Pockets[PocketIndex];
	B.Vel = FHyperVec3();
	B.Roll = FHyperVec3();

	if (Ball == CueBall)
	{
		if (!bCueScratched)
		{
			bCueScratched = true;
			++PenaltyCount;
			Emit(EHyperEvent::CueBallScratched, CueBall, PocketIndex);
		}
	}
	else
	{
		++PottedThisShot;
		Emit(EHyperEvent::BallPocketed, Ball, PocketIndex);
	}
}

void FHyperBilliardsSim::EndShot()
{
	// The watchdog's case: whatever still creeps is stopped where it is.
	for (FBall& Ball : Balls)
	{
		Ball.Vel = FHyperVec3();
		Ball.Roll = FHyperVec3();
	}

	Emit(EHyperEvent::ShotEnded, PottedThisShot);

	if (BallsLeft() == 0)
	{
		CurrentPhase = EHyperPhase::Cleared;
		Emit(EHyperEvent::TableCleared, Strokes());
		return;
	}
	if (!Balls[CueBall].bOnTable)
	{
		RespotCueBall();
	}
	CurrentPhase = EHyperPhase::Aiming;
}

// Back on the head spot; if a ball sits there, the nearest free place along X, then across Y,
// then into W. There is always one: ten balls cannot fill the table.
void FHyperBilliardsSim::RespotCueBall()
{
	const double Step = 2.0 * TableLayout.BallRadius + 0.5;
	const FHyperVec3 Spot = TableLayout.HeadSpot;
	FHyperVec3 Chosen = Spot;
	bool bFound = IsSpotFree(Spot);
	for (int Ring = 1; !bFound && Ring < 40; ++Ring)
	{
		const double S = Ring * Step;
		const FHyperVec3 Tries[6] = {
			Spot + FHyperVec3(-S, 0.0, 0.0), Spot + FHyperVec3(S, 0.0, 0.0),
			Spot + FHyperVec3(0.0, S, 0.0), Spot + FHyperVec3(0.0, -S, 0.0),
			Spot + FHyperVec3(0.0, 0.0, S), Spot + FHyperVec3(0.0, 0.0, -S),
		};
		for (const FHyperVec3& Try : Tries)
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
	Cue.Vel = FHyperVec3();
	Cue.Roll = FHyperVec3();
	Cue.bOnTable = true;
	Emit(EHyperEvent::CueBallRespotted);
}

bool FHyperBilliardsSim::IsSpotFree(FHyperVec3 Position) const
{
	const double R = TableLayout.BallRadius;
	if (std::fabs(Position.X) > TableLayout.HalfLength - R || std::fabs(Position.Y) > TableLayout.HalfWidth - R
		|| std::fabs(Position.W) > TableLayout.HalfDepth - R)
	{
		return false;
	}
	for (const FHyperVec3& Hole : TableLayout.Pockets)
	{
		if (HyperMath::Length(Position - Hole) < TableLayout.PocketCapture + R)
		{
			return false;
		}
	}
	for (int I = 1; I < BallCount; ++I)
	{
		if (Balls[I].bOnTable && HyperMath::Length(Position - Balls[I].Pos) < 2.0 * R + 0.1)
		{
			return false;
		}
	}
	return true;
}

bool FHyperBilliardsSim::IsSliding(int Ball) const
{
	return HyperMath::Length(Balls[Ball].Vel - Balls[Ball].Roll) > HyperMath::RollingSlip;
}

// A ball with no velocity but some roll is not at rest: the cloth is about to set it going
// (a stunned ball with backspin, say).
bool FHyperBilliardsSim::AnyBallMoving() const
{
	for (const FBall& Ball : Balls)
	{
		if (Ball.bOnTable && (!HyperMath::IsZero(Ball.Vel) || !HyperMath::IsZero(Ball.Roll)))
		{
			return true;
		}
	}
	return false;
}

int FHyperBilliardsSim::BallsLeft() const
{
	int Left = 0;
	for (int I = 1; I < BallCount; ++I)
	{
		Left += Balls[I].bOnTable ? 1 : 0;
	}
	return Left;
}

double FHyperBilliardsSim::SliceRadius(double BallRadius, double BallW, double SliceW)
{
	const double D = BallW - SliceW;
	const double R2 = BallRadius * BallRadius - D * D;
	return R2 > 0.0 ? std::sqrt(R2) : 0.0;
}

FHyperAimPrediction FHyperBilliardsSim::PredictShot(double Heading, double Tilt) const
{
	FHyperAimPrediction Out;
	const FBall& Cue = Balls[CueBall];
	if (!Cue.bOnTable)
	{
		return Out;
	}
	const FHyperVec3 Dir = HyperMath::Direction(Heading, Tilt);
	const double TwoR = 2.0 * TableLayout.BallRadius;
	double Best = -1.0;
	for (int I = 1; I < BallCount; ++I)
	{
		if (!Balls[I].bOnTable)
		{
			continue;
		}
		const double T = HyperMath::RaySphere(Cue.Pos, Dir, Balls[I].Pos, TwoR);
		if (T >= 0.0 && (Best < 0.0 || T < Best))
		{
			Best = T;
			Out.Ball = I;
		}
	}
	if (Out.Ball >= 0)
	{
		Out.bHitsBall = true;
		Out.CueAtContact = Cue.Pos + Dir * Best;
		Out.ObjectDirection = HyperMath::Normalized(Balls[Out.Ball].Pos - Out.CueAtContact);
	}
	return Out;
}

std::vector<FHyperEvent> FHyperBilliardsSim::TakeEvents()
{
	std::vector<FHyperEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FHyperBilliardsSim::PlaceBall(int Ball, FHyperVec3 Position, FHyperVec3 Velocity, bool bRolling)
{
	FBall& B = Balls[Ball];
	B.Pos = Position;
	B.Vel = Velocity;
	B.Roll = bRolling ? Velocity : FHyperVec3();
	B.bOnTable = true;
	if (CurrentPhase == EHyperPhase::Idle)
	{
		CurrentPhase = EHyperPhase::Aiming;
	}
	if (!HyperMath::IsZero(Velocity))
	{
		CurrentPhase = EHyperPhase::Rolling;
		ShotSeconds = 0.0;
	}
}

void FHyperBilliardsSim::RemoveBall(int Ball)
{
	Balls[Ball].bOnTable = false;
	Balls[Ball].Vel = FHyperVec3();
	Balls[Ball].Roll = FHyperVec3();
}

void FHyperBilliardsSim::ClearTable()
{
	for (int I = 0; I < BallCount; ++I)
	{
		RemoveBall(I);
	}
	CurrentPhase = EHyperPhase::Aiming;
	PendingEvents.clear();
}

void FHyperBilliardsSim::Emit(EHyperEvent Type, int Index, int Other, double Value)
{
	PendingEvents.push_back(FHyperEvent{ Type, Index, Other, Value });
}

void FHyperBilliardsSim::StartLesson(int Lesson)
{
	StartGame();
	for (int I = 2; I < BallCount; ++I) RemoveBall(I);
	const double W = TableLayout.HalfDepth - TableLayout.BallRadius;
	if (Lesson == 0)
	{
		PlaceBall(0, FHyperVec3(0, -25, W), FHyperVec3(), true);
		PlaceBall(1, FHyperVec3(0, TableLayout.HalfWidth - 30, W), FHyperVec3(), true);
	}
	else if (Lesson == 1)
	{
		PlaceBall(0, FHyperVec3(0, -25, -10), FHyperVec3(), true);
		PlaceBall(1, FHyperVec3(0, TableLayout.HalfWidth - 30, 15), FHyperVec3(), true);
	}
	else
	{
		// Align object ball toward a side-pocket image reflected in the end cushion.
		PlaceBall(0, FHyperVec3(-58, -12, W), FHyperVec3(), true);
		PlaceBall(1, FHyperVec3(0, 0, W), FHyperVec3(), true);
	}
}
