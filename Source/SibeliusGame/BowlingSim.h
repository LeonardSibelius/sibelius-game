// BowlingSim.h
//
// BOWLING — the physics model for the Transhuman Bowling Alley, reached by a door in the
// living room (Walt, 2026-09-27). Ten frames, strikes and spares, scored the real way.
//
// ===========================================================================
// THE HOUSE METHOD AGAIN (FPinballSim, FBilliardsSim): physics in 2D on the plane of the
// lane, rendering in 3D; a pure model, deterministic, standard library only, tested
// standalone by Tools/Tests/run_bowling_sim_tests.ps1. The alley actor only presents it.
//
// SKID, HOOK, ROLL. A bowling ball leaves the hand sliding, spinning about an axis tilted
// away from the direction it is thrown. The front forty feet of a lane are oiled, and on
// oil the ball barely grips: it skids nearly straight. On the dry back end it grips, the
// tilted spin bites, and it hooks into the pins, until the spin matches the motion and it
// simply rolls. That whole shape falls out of one friction model - the pool table's
// sliding-to-rolling model - with the friction coefficient taken from where the ball is
// on the lane. Nothing is scripted.
//
// PINS. A standing pin is anchored: nothing moves it but a hit hard enough to knock it
// over (KnockSpeed), and then it is DOWN for good - a real pin that is hit that hard does
// not stand back up. A down pin slides and scatters across the deck, bounces off the
// kickbacks at the sides, and knocks down any standing pin it reaches hard enough. That
// is "pin action", and it is where strikes come from: the ball itself touches only four
// or five pins of a strike.
//
// Coordinates: X along the lane from the foul line toward the pins, Y across it, +Y to
// the bowler's LEFT (so +X, +Y, up is right-handed, and angles are counter-clockwise).
// Origin: the foul line, at the centre of the lane. Centimetres, seconds, kilograms.
//
// NOT MODELLED: the ball's track flare and core dynamics, carry-down of oil, pins that
// wobble and stay up, pins flying above other pins. A 2D model is a strong model of a
// strike and a fair one of a messenger.

#pragma once

#include <cstdint>
#include <vector>

struct FBowlingVec2
{
	double X = 0.0;
	double Y = 0.0;

	constexpr FBowlingVec2() = default;
	constexpr FBowlingVec2(double InX, double InY) : X(InX), Y(InY) {}
};

inline FBowlingVec2 operator+(FBowlingVec2 A, FBowlingVec2 B) { return FBowlingVec2(A.X + B.X, A.Y + B.Y); }
inline FBowlingVec2 operator-(FBowlingVec2 A, FBowlingVec2 B) { return FBowlingVec2(A.X - B.X, A.Y - B.Y); }
inline FBowlingVec2 operator*(FBowlingVec2 A, double S) { return FBowlingVec2(A.X * S, A.Y * S); }
inline FBowlingVec2 operator*(double S, FBowlingVec2 A) { return FBowlingVec2(A.X * S, A.Y * S); }

struct FBowlingVec3
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
};

// Every number here is USBC regulation unless it says otherwise, converted from inches.
struct FBowlingLaneLayout
{
	double LaneHalfWidth = 52.705;      // 41.5 in wide, gutter to gutter
	double GutterWidth = 23.495;        // 9.25 in each side
	double HeadPinX = 1828.8;           // 60 ft from the foul line to the head pin
	double PinSpacing = 30.48;          // 12 in, centre to centre, in the triangle
	double PitX = 1828.8 + 86.8;        // the deck ends 34 3/16 in behind the head pin
	double KickbackStartX = 1828.8 - 30.0;   // the side plates along the pin deck
	double OilLength = 1219.2;          // a 40 ft house pattern
	double BallRadius = 10.795;         // 8.5 in ball
	double PinRadius = 6.053;           // 4.766 in at the belly, which is ball height
	double PinHeight = 38.1;            // 15 in, for the actor

	FBowlingVec2 PinSpots[10];          // numbered 1..10 as index 0..9

	double KickbackY() const { return LaneHalfWidth + GutterWidth; }
	double GutterCentreY() const { return LaneHalfWidth + GutterWidth * 0.5; }

	static FBowlingLaneLayout MakeRegulation();

	// The Retro Bowling Alley pack's own lane (SM_Pist), measured from its mesh: see the
	// note above MakeRetroBowl in BowlingSim.cpp. The one the alley actually plays on.
	static FBowlingLaneLayout MakeRetroBowl();

	// Sets PinSpots from HeadPinX and PinSpacing.
	void PlacePinSpots();
};

enum class EBowlingPhase : uint8_t
{
	Idle,       // no game yet: a full rack, waiting
	Ready,      // waiting for the next ball
	Rolling,    // a ball is on its way, or pins are still moving
	GameOver,
};

enum class EPinState : uint8_t
{
	Standing,
	Down,       // knocked over, still on the deck: sliding or at rest
	Gone,       // in the pit, or swept away between balls
};

enum class EBowlingEvent : uint8_t
{
	GameStarted,
	BallReleased,       // Value = speed, cm/s
	BallInGutter,
	BallHitPin,         // Index = pin (0..9), Value = closing speed
	PinHitPin,          // Index, Other = pins, Value = closing speed
	PinHitKickback,     // Index = pin, Value = speed into it
	PinDown,            // Index = pin
	BallInPit,
	RollEnded,          // Index = pins knocked down by this ball
	Strike,             // Index = frame (0..9)
	Spare,              // Index = frame
	RackReset,          // a full new rack of ten
	FrameEnded,         // Index = frame, Other = running total through it (when known)
	GameOver,           // Index = final score
};

struct FBowlingEvent
{
	EBowlingEvent Type = EBowlingEvent::GameStarted;
	int Index = -1;
	int Other = -1;
	double Value = 0.0;
};

class FBowlingSim
{
public:
	static constexpr int PinCount = 10;
	static constexpr int FrameCount = 10;

	static constexpr double SubstepSeconds = 1.0 / 2000.0;
	static constexpr double MaxSpeed = 1600.0;          // cm/s: a pin kicked by a fast ball
	static constexpr double MaxFrameSeconds = 0.1;

	static_assert(MaxSpeed * SubstepSeconds < 6.053 / 3.0,
		"nothing may move a third of a pin's radius per substep, or it can pass through one");

	static constexpr double Gravity = 981.0;
	static constexpr double BallMass = 6.8;             // a 15 lb ball
	static constexpr double PinMass = 1.55;             // 3 lb 6 oz
	static constexpr double OilFriction = 0.03;         // ball on the oiled front of the lane
	static constexpr double DryFriction = 0.18;         // ball on the dry back end
	static constexpr double RollingFriction = 0.008;    // a rolling ball barely slows
	static constexpr double PinSlideFriction = 0.30;    // a down pin skidding on the deck
	static constexpr double BallPinRestitution = 0.65;
	static constexpr double PinPinRestitution = 0.50;
	static constexpr double KickbackRestitution = 0.55;
	static constexpr double KnockSpeed = 40.0;          // cm/s of closing speed to topple a pin
	static constexpr double EventSpeed = 20.0;

	// The throw.
	static constexpr double MinBallSpeed = 560.0;       // 12.5 mph: always reaches the pins
	static constexpr double MaxBallSpeed = 1000.0;      // 22 mph
	static constexpr double RevsAtFullHook = 32.0;      // rad/s: about 300 rpm, a stroker's
	static constexpr double MaxAxisDegrees = 55.0;      // axis rotation at full hook
	static constexpr double MaxRollSeconds = 15.0;      // a watchdog, never reached in play
	static constexpr double SettleSeconds = 1.2;        // after the ball is gone, pins get this
	                                                    // long to finish moving

	explicit FBowlingSim(FBowlingLaneLayout InLayout = FBowlingLaneLayout::MakeRegulation());

	// Controls.
	void StartGame();
	// Throw from StartY on the foul line, at Angle (radians, 0 = straight down the lane,
	// + toward the left), at Power 0..1, with Hook -1..1 (+ curves left, a right-hander's
	// hook; 0 is a straight ball). Only while Ready.
	bool Roll(double StartY, double Angle, double Power, double Hook);

	void Advance(double DeltaSeconds);
	void Substep();

	// Reading the lane.
	const FBowlingLaneLayout& Layout() const { return LaneLayout; }
	EBowlingPhase Phase() const { return CurrentPhase; }
	FBowlingVec2 BallPosition() const { return Ball.Pos; }
	FBowlingVec2 BallVelocity() const { return Ball.Vel; }
	FBowlingVec3 BallSpin() const { return Ball.Spin; }
	bool IsBallOnLane() const { return Ball.bActive; }
	bool IsBallInGutter() const { return Ball.bInGutter; }
	bool IsBallSliding() const;

	EPinState PinState(int Pin) const { return Pins[Pin].State; }
	FBowlingVec2 PinPosition(int Pin) const { return Pins[Pin].Pos; }
	FBowlingVec2 PinVelocity(int Pin) const { return Pins[Pin].Vel; }
	double PinFallDirection(int Pin) const { return Pins[Pin].FallDirection; }   // radians
	double PinSecondsDown(int Pin) const { return Pins[Pin].SecondsDown; }
	int PinsStanding() const;

	int Frame() const { return CurrentFrame; }          // 0..9
	int BallInFrame() const { return CurrentBallInFrame; }   // 0, 1 (2 in the tenth)
	const std::vector<int>& Rolls() const { return RollHistory; }
	int Score() const;                                  // everything scored so far
	// What the board shows while bonuses are still to come: a strike counts its 10 (and
	// whatever bonus balls have been thrown) at once, instead of 0 until both are in.
	// Walt, 2026-09-27, after his first strike: "what should the score say? it is zero".
	int RunningScore() const;

	// The real rules, as a pure function. Frames[i] is the running total through frame i,
	// or -1 while a strike or spare there is still waiting for its bonus balls.
	static int ScoreRolls(const std::vector<int>& Rolls, int OutFrames[FrameCount]);

	std::vector<FBowlingEvent> TakeEvents();

	// Test hooks.
	void PlaceBall(FBowlingVec2 Position, FBowlingVec2 Velocity, FBowlingVec3 Spin);
	void SetPinsStanding(const bool Standing[PinCount]);   // a spare leave, say

private:
	struct FBall
	{
		FBowlingVec2 Pos;
		FBowlingVec2 Vel;
		FBowlingVec3 Spin;
		bool bActive = false;
		bool bInGutter = false;
	};
	struct FPin
	{
		FBowlingVec2 Pos;
		FBowlingVec2 Vel;
		EPinState State = EPinState::Standing;
		double FallDirection = 0.0;
		double SecondsDown = 0.0;
	};

	void ResetRack();
	void StepBall();
	void StepPins();
	void CollideBallPins();
	void CollidePins();
	void CollideKickbacks();
	void Knock(int Pin, FBowlingVec2 Velocity);
	void EndRoll();
	bool AnythingMoving() const;
	void Emit(EBowlingEvent Type, int Index = -1, int Other = -1, double Value = 0.0);

	FBowlingLaneLayout LaneLayout;
	FBall Ball;
	FPin Pins[PinCount];
	EBowlingPhase CurrentPhase = EBowlingPhase::Idle;
	double Accumulator = 0.0;
	double RollSeconds = 0.0;
	double SecondsSinceBallGone = 0.0;
	int StandingAtRelease = PinCount;
	int CurrentFrame = 0;
	int CurrentBallInFrame = 0;
	std::vector<int> RollHistory;
	std::vector<FBowlingEvent> PendingEvents;
};
