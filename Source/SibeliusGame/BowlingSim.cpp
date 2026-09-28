// BowlingSim.cpp — see the header for the design, and Tools/Tests/bowling_sim_test.cpp for
// what it is held to.

#include "BowlingSim.h"

#include <cmath>

// NAMED, NOT ANONYMOUS: the unity-build rule (see PinballSim.cpp's PinMath).
namespace BowlMath
{
	constexpr double RollingSlip = 1e-6;   // cm/s: below this a ball is rolling

	inline double Dot(FBowlingVec2 A, FBowlingVec2 B) { return A.X * B.X + A.Y * B.Y; }
	inline double Length(FBowlingVec2 A) { return std::sqrt(Dot(A, A)); }
	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	inline FBowlingVec2 Normalized(FBowlingVec2 A, FBowlingVec2 Fallback = FBowlingVec2(1.0, 0.0))
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : Fallback;
	}

	// The same contact-point slip as the pool table's: zero when the ball rolls.
	inline FBowlingVec2 Slip(FBowlingVec2 Vel, FBowlingVec3 Spin, double Radius)
	{
		return FBowlingVec2(Vel.X - Radius * Spin.Y, Vel.Y + Radius * Spin.X);
	}

	inline FBowlingVec3 RollingSpin(FBowlingVec2 Vel, double Radius)
	{
		return FBowlingVec3{ -Vel.Y / Radius, Vel.X / Radius, 0.0 };
	}

	inline FBowlingVec2 ClampSpeed(FBowlingVec2 V, double Max)
	{
		const double S = Length(V);
		return S > Max ? V * (Max / S) : V;
	}
}

// ===========================================================================
// THE LANE — USBC regulation. The pins stand in an equilateral triangle, 12 in apart,
// point toward the bowler: the head pin (1) on the spot 60 ft from the foul line, then
// rows of two, three and four behind it, 10.39 in apart. Pin 7 is at the back left, 10 at
// the back right, as the bowler sees them. The alley pack's own lane has not been
// measured yet: when it is, only these numbers move.

FBowlingLaneLayout FBowlingLaneLayout::MakeRegulation()
{
	FBowlingLaneLayout L;
	L.PlacePinSpots();
	return L;
}

/* THE ALLEY'S LANE — PixelForgeESP's SM_Pist, measured on 2026-09-27 from its OBJ export
   (681 vertices; export axes (x, y-up, z) are Unreal (X, Z, Y)). In the mesh's own space:
     - the lane surface runs X 25..129 (104.0 cm wide, regulation 105.4), top at Z 0
     - gutters X 0..25 and 129..154, 6.6 cm deep, from Y -30 to the end
     - it runs from Y 0 toward -Y and ends at Y -1920
   The gutters start at the foul line, so the foul line is Y -30. The lane mesh itself ends
   1890 cm beyond it, but the PIN DECK runs on past that onto the next mesh: the pack's own
   head pin (SM_Stick40 in L_Bowling, the lane at SM_Pist4) stands 1831 cm from the foul
   line - regulation 60 ft is 1828.8. Our rack stands exactly where the pack's stood (the
   pack's pins are deleted from that lane), and the pit is a regulation deck behind it.
   The first version guessed the deck ended with the mesh, put the head pin 28 cm short,
   and the pack's white pins stood behind ours. ABowlingLane maps this onto the mesh. */
FBowlingLaneLayout FBowlingLaneLayout::MakeRetroBowl()
{
	FBowlingLaneLayout L;
	L.LaneHalfWidth = 52.0;
	L.GutterWidth = 25.0;
	L.HeadPinX = 1831.0;
	L.PitX = L.HeadPinX + 86.8;
	L.KickbackStartX = L.HeadPinX - 30.0;
	L.PlacePinSpots();
	return L;
}

void FBowlingLaneLayout::PlacePinSpots()
{
	FBowlingLaneLayout& L = *this;
	const double S = L.PinSpacing;
	const double Row = S * std::sqrt(3.0) * 0.5;
	const double H = L.HeadPinX;
	const FBowlingVec2 Spots[10] = {
		{ H, 0.0 },
		{ H + Row, S * 0.5 },  { H + Row, -S * 0.5 },
		{ H + 2.0 * Row, S },  { H + 2.0 * Row, 0.0 },  { H + 2.0 * Row, -S },
		{ H + 3.0 * Row, S * 1.5 }, { H + 3.0 * Row, S * 0.5 }, { H + 3.0 * Row, -S * 0.5 }, { H + 3.0 * Row, -S * 1.5 },
	};
	for (int I = 0; I < 10; ++I)
	{
		L.PinSpots[I] = Spots[I];
	}
}

// ===========================================================================

namespace BowlDetail
{
	// A lying, tumbling pin sweeps more of the deck than its belly's circle: it is 15 in
	// long. A somewhat larger circle stands in for that, which is most of what pin action is.
	constexpr double DownPinRadius = 8.5;
}

FBowlingSim::FBowlingSim(FBowlingLaneLayout InLayout)
	: LaneLayout(InLayout)
{
	ResetRack();
	PendingEvents.clear();
	CurrentPhase = EBowlingPhase::Idle;
}

void FBowlingSim::ResetRack()
{
	for (int I = 0; I < PinCount; ++I)
	{
		Pins[I] = FPin();
		Pins[I].Pos = LaneLayout.PinSpots[I];
	}
	Emit(EBowlingEvent::RackReset);
}

void FBowlingSim::StartGame()
{
	RollHistory.clear();
	CurrentFrame = 0;
	CurrentBallInFrame = 0;
	Ball = FBall();
	Accumulator = 0.0;
	ResetRack();
	CurrentPhase = EBowlingPhase::Ready;
	Emit(EBowlingEvent::GameStarted);
}

bool FBowlingSim::Roll(double StartY, double Angle, double Power, double Hook)
{
	if (CurrentPhase != EBowlingPhase::Ready)
	{
		return false;
	}
	const double R = LaneLayout.BallRadius;
	const double Speed = MinBallSpeed + BowlMath::Clamp(Power, 0.0, 1.0) * (MaxBallSpeed - MinBallSpeed);
	const FBowlingVec2 Dir(std::cos(Angle), std::sin(Angle));
	const double H = BowlMath::Clamp(Hook, -1.0, 1.0);

	// The spin: about an axis turned away from the rolling axis by the axis rotation. The
	// rolling part makes it roll; the part along the direction of travel is what hooks
	// once the lane grips. Its sign picks the side: a right-hander's hook (+) needs spin
	// that is negative about the direction of travel.
	const double Axis = std::fabs(H) * MaxAxisDegrees * 3.14159265358979323846 / 180.0;
	const double Revs = RevsAtFullHook * (0.5 + 0.5 * std::fabs(H));
	const FBowlingVec2 RollAxis(-Dir.Y, Dir.X);
	const double Side = H >= 0.0 ? -1.0 : 1.0;
	const FBowlingVec2 W = RollAxis * (Revs * std::cos(Axis)) + Dir * (Side * Revs * std::sin(Axis));

	Ball = FBall();
	Ball.Pos = FBowlingVec2(R, BowlMath::Clamp(StartY, -(LaneLayout.LaneHalfWidth - R), LaneLayout.LaneHalfWidth - R));
	Ball.Vel = Dir * Speed;
	Ball.Spin = FBowlingVec3{ W.X, W.Y, 0.0 };
	Ball.bActive = true;

	StandingAtRelease = PinsStanding();
	RollSeconds = 0.0;
	SecondsSinceBallGone = 0.0;
	CurrentPhase = EBowlingPhase::Rolling;
	Emit(EBowlingEvent::BallReleased, -1, -1, Speed);
	return true;
}

void FBowlingSim::Advance(double DeltaSeconds)
{
	Accumulator += BowlMath::Clamp(DeltaSeconds, 0.0, MaxFrameSeconds);
	while (Accumulator >= SubstepSeconds)
	{
		Accumulator -= SubstepSeconds;
		Substep();
	}
}

void FBowlingSim::Substep()
{
	StepBall();
	StepPins();
	for (int Pass = 0; Pass < 2; ++Pass)
	{
		CollideBallPins();
		CollidePins();
	}
	CollideKickbacks();

	if (CurrentPhase != EBowlingPhase::Rolling)
	{
		return;
	}
	RollSeconds += SubstepSeconds;
	if (!Ball.bActive)
	{
		SecondsSinceBallGone += SubstepSeconds;
	}
	const bool bSettled = !Ball.bActive && SecondsSinceBallGone >= SettleSeconds && !AnythingMoving();
	if (bSettled || SecondsSinceBallGone > 6.0 || RollSeconds > MaxRollSeconds)
	{
		EndRoll();
	}
}

// Skid, hook, roll: the pool table's friction model, with the friction taken from the lane
// under the ball - oil in front, dry behind.
void FBowlingSim::StepBall()
{
	if (!Ball.bActive)
	{
		return;
	}
	const double R = LaneLayout.BallRadius;
	const double Dt = SubstepSeconds;

	if (Ball.bInGutter)
	{
		// In the channel it just rolls on, beside the pins and into the pit.
		Ball.Vel = FBowlingVec2(std::fmax(Ball.Vel.X - RollingFriction * Gravity * Dt, 0.0), 0.0);
		Ball.Spin = BowlMath::RollingSpin(Ball.Vel, R);
		Ball.Pos = FBowlingVec2(Ball.Pos.X + Ball.Vel.X * Dt, Ball.Pos.Y);
	}
	else
	{
		const double Mu = Ball.Pos.X < LaneLayout.OilLength ? OilFriction : DryFriction;
		const FBowlingVec2 U = BowlMath::Slip(Ball.Vel, Ball.Spin, R);
		const double Slip = BowlMath::Length(U);
		if (Slip > 3.5 * Mu * Gravity * Dt)
		{
			const FBowlingVec2 A = U * (-Mu * Gravity / Slip);
			Ball.Vel = Ball.Vel + A * Dt;
			Ball.Spin.X += 2.5 / R * A.Y * Dt;
			Ball.Spin.Y -= 2.5 / R * A.X * Dt;
		}
		else if (Slip > BowlMath::RollingSlip)
		{
			Ball.Vel = Ball.Vel - U * (2.0 / 7.0);
			Ball.Spin = BowlMath::RollingSpin(Ball.Vel, R);
		}
		else
		{
			const double Speed = BowlMath::Length(Ball.Vel);
			const double Slowed = Speed - RollingFriction * Gravity * Dt;
			Ball.Vel = Slowed > 0.0 ? Ball.Vel * (Slowed / Speed) : FBowlingVec2();
			Ball.Spin = BowlMath::RollingSpin(Ball.Vel, R);
		}
		Ball.Vel = BowlMath::ClampSpeed(Ball.Vel, MaxSpeed);
		Ball.Pos = Ball.Pos + Ball.Vel * Dt;

		// Over the edge: once its centre is past the edge of the lane, it drops in.
		if (std::fabs(Ball.Pos.Y) > LaneLayout.LaneHalfWidth && Ball.Pos.X < LaneLayout.PitX)
		{
			Ball.bInGutter = true;
			Ball.Pos.Y = Ball.Pos.Y > 0.0 ? LaneLayout.GutterCentreY() : -LaneLayout.GutterCentreY();
			Ball.Vel.Y = 0.0;
			Emit(EBowlingEvent::BallInGutter);
		}
	}

	if (Ball.Pos.X > LaneLayout.PitX || Ball.Pos.X < -50.0 || (Ball.Vel.X == 0.0 && Ball.Vel.Y == 0.0))
	{
		Ball.bActive = false;
		Ball.Vel = FBowlingVec2();
		Ball.Spin = FBowlingVec3();
		Emit(EBowlingEvent::BallInPit);
	}
}

void FBowlingSim::StepPins()
{
	const double Dt = SubstepSeconds;
	for (FPin& Pin : Pins)
	{
		if (Pin.State != EPinState::Down)
		{
			continue;
		}
		Pin.SecondsDown += Dt;
		const double Speed = BowlMath::Length(Pin.Vel);
		if (Speed > 0.0)
		{
			const double Slowed = Speed - PinSlideFriction * Gravity * Dt;
			Pin.Vel = Slowed > 0.0 ? BowlMath::ClampSpeed(Pin.Vel * (Slowed / Speed), MaxSpeed) : FBowlingVec2();
			Pin.Pos = Pin.Pos + Pin.Vel * Dt;
		}
		// Into the pit behind the deck, or off the side of the lane short of the kickbacks.
		const bool bInPit = Pin.Pos.X > LaneLayout.PitX;
		const bool bOffTheSide = std::fabs(Pin.Pos.Y) > LaneLayout.KickbackY() && Pin.Pos.X < LaneLayout.KickbackStartX;
		if (bInPit || bOffTheSide)
		{
			Pin.State = EPinState::Gone;
			Pin.Vel = FBowlingVec2();
		}
	}
}

void FBowlingSim::Knock(int PinIndex, FBowlingVec2 Velocity)
{
	FPin& Pin = Pins[PinIndex];
	Pin.State = EPinState::Down;
	Pin.Vel = BowlMath::ClampSpeed(Velocity, MaxSpeed);
	Pin.FallDirection = std::atan2(Velocity.Y, Velocity.X);
	Pin.SecondsDown = 0.0;
	Emit(EBowlingEvent::PinDown, PinIndex);
}

void FBowlingSim::CollideBallPins()
{
	if (!Ball.bActive || Ball.bInGutter)
	{
		return;
	}
	const double InvBall = 1.0 / BallMass;
	const double InvPin = 1.0 / PinMass;
	for (int I = 0; I < PinCount; ++I)
	{
		FPin& Pin = Pins[I];
		if (Pin.State == EPinState::Gone)
		{
			continue;
		}
		const double Reach = LaneLayout.BallRadius + (Pin.State == EPinState::Standing ? LaneLayout.PinRadius : BowlDetail::DownPinRadius);
		const FBowlingVec2 D = Pin.Pos - Ball.Pos;
		const double Dist = BowlMath::Length(D);
		if (Dist >= Reach)
		{
			continue;
		}
		const FBowlingVec2 N = BowlMath::Normalized(D);
		const double Closing = BowlMath::Dot(Pin.Vel - Ball.Vel, N);   // < 0: approaching
		const bool bKnocks = Pin.State == EPinState::Down || -Closing > KnockSpeed;
		if (Closing < 0.0)
		{
			if (bKnocks)
			{
				const double J = -(1.0 + BallPinRestitution) * Closing / (InvBall + InvPin);
				Ball.Vel = Ball.Vel - N * (J * InvBall);
				const FBowlingVec2 PinVel = Pin.Vel + N * (J * InvPin);
				if (-Closing > EventSpeed)
				{
					Emit(EBowlingEvent::BallHitPin, I, -1, -Closing);
				}
				if (Pin.State == EPinState::Standing)
				{
					Knock(I, PinVel);
				}
				else
				{
					Pin.Vel = BowlMath::ClampSpeed(PinVel, MaxSpeed);
				}
			}
			else
			{
				// Too soft to topple it: the ball comes off a standing pin as off a post.
				Ball.Vel = Ball.Vel - N * ((1.0 + BallPinRestitution) * BowlMath::Dot(Ball.Vel, N));
			}
		}
		// Separate: a standing pin does not move, so the ball takes all of it.
		const double Overlap = Reach - Dist;
		if (Pin.State == EPinState::Standing)
		{
			Ball.Pos = Ball.Pos - N * Overlap;
		}
		else
		{
			const double Share = InvPin / (InvBall + InvPin);
			Pin.Pos = Pin.Pos + N * (Overlap * Share);
			Ball.Pos = Ball.Pos - N * (Overlap * (1.0 - Share));
		}
	}
}

void FBowlingSim::CollidePins()
{
	for (int I = 0; I < PinCount; ++I)
	{
		for (int J = I + 1; J < PinCount; ++J)
		{
			FPin& A = Pins[I];
			FPin& B = Pins[J];
			if (A.State == EPinState::Gone || B.State == EPinState::Gone)
			{
				continue;
			}
			if (A.State == EPinState::Standing && B.State == EPinState::Standing)
			{
				continue;   // anchored, and never touching
			}
			const double Ra = A.State == EPinState::Standing ? LaneLayout.PinRadius : BowlDetail::DownPinRadius;
			const double Rb = B.State == EPinState::Standing ? LaneLayout.PinRadius : BowlDetail::DownPinRadius;
			const FBowlingVec2 D = B.Pos - A.Pos;
			const double Dist = BowlMath::Length(D);
			if (Dist >= Ra + Rb)
			{
				continue;
			}
			const FBowlingVec2 N = BowlMath::Normalized(D);
			const double Closing = BowlMath::Dot(B.Vel - A.Vel, N);
			const bool bAStanding = A.State == EPinState::Standing;
			const bool bBStanding = B.State == EPinState::Standing;

			if (Closing < 0.0)
			{
				if ((bAStanding || bBStanding) && -Closing <= KnockSpeed)
				{
					// A slow pin bumping a standing one: it bounces; the standing pin stays up.
					FPin& Mover = bAStanding ? B : A;
					const FBowlingVec2 Out = bAStanding ? N : N * -1.0;
					const double Vn = BowlMath::Dot(Mover.Vel, Out);
					if (Vn < 0.0)
					{
						Mover.Vel = Mover.Vel - Out * ((1.0 + PinPinRestitution) * Vn);
					}
				}
				else
				{
					const double Impulse = -0.5 * (1.0 + PinPinRestitution) * Closing;   // equal masses
					const FBowlingVec2 NewA = A.Vel - N * Impulse;
					const FBowlingVec2 NewB = B.Vel + N * Impulse;
					if (-Closing > EventSpeed)
					{
						Emit(EBowlingEvent::PinHitPin, I, J, -Closing);
					}
					if (bAStanding) { Knock(I, NewA); } else { A.Vel = BowlMath::ClampSpeed(NewA, MaxSpeed); }
					if (bBStanding) { Knock(J, NewB); } else { B.Vel = BowlMath::ClampSpeed(NewB, MaxSpeed); }
				}
			}

			const double Overlap = Ra + Rb - Dist;
			if (A.State == EPinState::Standing)
			{
				B.Pos = B.Pos + N * Overlap;
			}
			else if (B.State == EPinState::Standing)
			{
				A.Pos = A.Pos - N * Overlap;
			}
			else
			{
				A.Pos = A.Pos - N * (Overlap * 0.5);
				B.Pos = B.Pos + N * (Overlap * 0.5);
			}
		}
	}
}

// The side plates of the pin deck: down pins bounce off them back into the rack, which
// is how a pin from one side takes out the other.
void FBowlingSim::CollideKickbacks()
{
	const double Wall = LaneLayout.KickbackY();
	for (int I = 0; I < PinCount; ++I)
	{
		FPin& Pin = Pins[I];
		if (Pin.State != EPinState::Down || Pin.Pos.X < LaneLayout.KickbackStartX)
		{
			continue;
		}
		const double Limit = Wall - BowlDetail::DownPinRadius;
		if (std::fabs(Pin.Pos.Y) > Limit)
		{
			const double Side = Pin.Pos.Y > 0.0 ? 1.0 : -1.0;
			if (Pin.Vel.Y * Side > 0.0)
			{
				if (Pin.Vel.Y * Side > EventSpeed)
				{
					Emit(EBowlingEvent::PinHitKickback, I, -1, Pin.Vel.Y * Side);
				}
				Pin.Vel.Y = -Pin.Vel.Y * KickbackRestitution;
			}
			Pin.Pos.Y = Side * Limit;
		}
	}
}

bool FBowlingSim::AnythingMoving() const
{
	if (Ball.bActive)
	{
		return true;
	}
	for (const FPin& Pin : Pins)
	{
		if (Pin.State == EPinState::Down && (Pin.Vel.X != 0.0 || Pin.Vel.Y != 0.0))
		{
			return true;
		}
	}
	return false;
}

int FBowlingSim::PinsStanding() const
{
	int Count = 0;
	for (const FPin& Pin : Pins)
	{
		Count += Pin.State == EPinState::Standing ? 1 : 0;
	}
	return Count;
}

bool FBowlingSim::IsBallSliding() const
{
	return BowlMath::Length(BowlMath::Slip(Ball.Vel, Ball.Spin, LaneLayout.BallRadius)) > BowlMath::RollingSlip;
}

// The pinsetter's cycle, and the rules. A strike or the second ball of a frame brings a
// fresh rack; after a first ball only the fallen wood is swept, and the leave stays put.
void FBowlingSim::EndRoll()
{
	Ball = FBall();
	const int Knocked = StandingAtRelease - PinsStanding();
	RollHistory.push_back(Knocked);
	Emit(EBowlingEvent::RollEnded, Knocked);

	for (FPin& Pin : Pins)
	{
		if (Pin.State != EPinState::Standing)
		{
			Pin.State = EPinState::Gone;
			Pin.Vel = FBowlingVec2();
		}
	}

	int Frames[FrameCount];
	auto FinishFrame = [this, &Frames](bool bLast)
	{
		ScoreRolls(RollHistory, Frames);
		Emit(EBowlingEvent::FrameEnded, CurrentFrame, Frames[CurrentFrame]);
		if (bLast)
		{
			CurrentPhase = EBowlingPhase::GameOver;
			Emit(EBowlingEvent::GameOver, ScoreRolls(RollHistory, Frames));
			return;
		}
		++CurrentFrame;
		CurrentBallInFrame = 0;
		ResetRack();
	};

	const bool bCleared = PinsStanding() == 0;
	CurrentPhase = EBowlingPhase::Ready;

	if (CurrentFrame < FrameCount - 1)
	{
		if (CurrentBallInFrame == 0 && bCleared)
		{
			Emit(EBowlingEvent::Strike, CurrentFrame);
			FinishFrame(false);
		}
		else if (CurrentBallInFrame == 0)
		{
			CurrentBallInFrame = 1;
		}
		else
		{
			if (bCleared)
			{
				Emit(EBowlingEvent::Spare, CurrentFrame);
			}
			FinishFrame(false);
		}
		return;
	}

	// The tenth frame: a strike or a spare earns more balls, each against a fresh rack
	// when the last one cleared the deck.
	const size_t N = RollHistory.size();
	if (CurrentBallInFrame == 0)
	{
		if (bCleared)
		{
			Emit(EBowlingEvent::Strike, CurrentFrame);
			ResetRack();
		}
		CurrentBallInFrame = 1;
	}
	else if (CurrentBallInFrame == 1)
	{
		const int First = RollHistory[N - 2];
		if (First == 10)
		{
			if (bCleared)
			{
				Emit(EBowlingEvent::Strike, CurrentFrame);
				ResetRack();
			}
			CurrentBallInFrame = 2;
		}
		else if (bCleared)
		{
			Emit(EBowlingEvent::Spare, CurrentFrame);
			ResetRack();
			CurrentBallInFrame = 2;
		}
		else
		{
			FinishFrame(true);
		}
	}
	else
	{
		FinishFrame(true);
	}
}

int FBowlingSim::ScoreRolls(const std::vector<int>& Rolls, int OutFrames[FrameCount])
{
	for (int F = 0; F < FrameCount; ++F)
	{
		OutFrames[F] = -1;
	}
	const size_t N = Rolls.size();
	size_t I = 0;
	int Total = 0;
	for (int F = 0; F < FrameCount; ++F)
	{
		if (I >= N)
		{
			break;
		}
		if (Rolls[I] == 10)
		{
			if (I + 2 >= N)
			{
				break;   // the strike's two bonus balls are not thrown yet
			}
			Total += 10 + Rolls[I + 1] + Rolls[I + 2];
			I += 1;
		}
		else
		{
			if (I + 1 >= N)
			{
				break;
			}
			if (Rolls[I] + Rolls[I + 1] == 10)
			{
				if (I + 2 >= N)
				{
					break;
				}
				Total += 10 + Rolls[I + 2];
			}
			else
			{
				Total += Rolls[I] + Rolls[I + 1];
			}
			I += 2;
		}
		OutFrames[F] = Total;
	}
	return Total;
}

int FBowlingSim::Score() const
{
	int Frames[FrameCount];
	return ScoreRolls(RollHistory, Frames);
}

int FBowlingSim::RunningScore() const
{
	// The bonus balls not yet thrown count as nothing: pad with two gutter balls and score.
	std::vector<int> Padded = RollHistory;
	Padded.push_back(0);
	Padded.push_back(0);
	int Frames[FrameCount];
	return ScoreRolls(Padded, Frames);
}

std::vector<FBowlingEvent> FBowlingSim::TakeEvents()
{
	std::vector<FBowlingEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FBowlingSim::PlaceBall(FBowlingVec2 Position, FBowlingVec2 Velocity, FBowlingVec3 Spin)
{
	Ball = FBall();
	Ball.Pos = Position;
	Ball.Vel = Velocity;
	Ball.Spin = Spin;
	Ball.bActive = true;
	StandingAtRelease = PinsStanding();
	RollSeconds = 0.0;
	SecondsSinceBallGone = 0.0;
	if (CurrentPhase == EBowlingPhase::Ready || CurrentPhase == EBowlingPhase::Idle)
	{
		CurrentPhase = EBowlingPhase::Rolling;
	}
}

void FBowlingSim::SetPinsStanding(const bool Standing[PinCount])
{
	for (int I = 0; I < PinCount; ++I)
	{
		Pins[I] = FPin();
		Pins[I].Pos = LaneLayout.PinSpots[I];
		Pins[I].State = Standing[I] ? EPinState::Standing : EPinState::Gone;
	}
}

void FBowlingSim::Emit(EBowlingEvent Type, int Index, int Other, double Value)
{
	PendingEvents.push_back(FBowlingEvent{ Type, Index, Other, Value });
}
