// BilliardsSim.h
//
// BILLIARDS — the physics model for the pool table in the living room (Walt, 2026-09-27).
// The game is solo "clear the table": fifteen balls racked, and the score is how many
// strokes it takes to pot them all. Fewer is better. Skill, not chance, free play, the
// same brief as the pinball beside it.
//
// ===========================================================================
// PHYSICS IN 2D, RENDERING IN 3D — the pinball's method, which worked. A pool table is
// flat, so the balls are simulated in the plane of the cloth, in centimetres, and a table
// actor (not written yet) draws the result with the pack's real ball meshes. Nothing is
// handed to Chaos: a physics engine's generic contacts are why most engine pool games feel
// like marbles on glass. Every collision here is ours, and every one is tested.
//
// THE NO-TUNNELLING GUARANTEE is the pinball's too: fixed substeps of 1/1200 s and a speed
// cap, so no ball moves a third of its radius in one step (static_assert below). Nothing
// can pass through a cushion or another ball without first overlapping it.
//
// SLIDING, THEN ROLLING — the thing that makes it pool and not air hockey. A struck ball
// starts out SLIDING: its spin does not match its speed, the cloth rubs, and friction
// works on both until the contact point stops slipping. From then on it ROLLS, and slows
// only by rolling resistance, about twenty times gentler. Each ball carries its spin, so:
//   - a centre hit that reaches the object ball still sliding STOPS DEAD (a stun shot)
//   - a low hit carries backspin through the collision and pulls back (a draw shot)
//   - a high hit carries topspin through and follows on (a follow shot)
// None of those are special cases in the code. They fall out of spin being kept through a
// ball-ball collision, which is what real balls do: the contact is too short and too
// slippery to change their spin.
//
// ===========================================================================
// THE HOUSE PATTERN (FPinballSim, FCarouselSim, UPokerGameModel): a pure model,
// deterministic, no UI, no actors, no timers. The table actor reads Layout() to place its
// meshes and drains TakeEvents() each frame for sounds. It never computes a collision or a
// score. The layout lives HERE, measured from the table's own mesh, so the cushion you
// see and the cushion the ball bounces off cannot disagree.
//
// NO ENGINE HEADERS, ON PURPOSE. Tools/Tests/run_billiards_sim_tests.ps1 builds this with
// the Visual Studio compiler alone and runs every test in a couple of seconds.
//
// Coordinates: origin at the centre of the cloth, X along the table's length (+X is the
// FOOT end, where the rack goes), Y across it, centimetres. Angles are radians,
// counter-clockwise from +X.
//
// NOT MODELLED YET: side spin (english), jump and masse shots, throw between balls,
// cushion height. The pack's balls ride a little high on its low cushions, and a 2D
// cushion does not care.

#pragma once

#include <cstdint>
#include <vector>

// The pack's balls are 7.0 cm across, a size up on a real 5.715 cm ball, and its table is
// scaled to match. The model follows the art: what you see is what the cushion does.
inline constexpr double BilliardsDefaultBallRadius = 3.5;

struct FBilliardsVec2
{
	double X = 0.0;
	double Y = 0.0;

	constexpr FBilliardsVec2() = default;
	constexpr FBilliardsVec2(double InX, double InY) : X(InX), Y(InY) {}
};

inline FBilliardsVec2 operator+(FBilliardsVec2 A, FBilliardsVec2 B) { return FBilliardsVec2(A.X + B.X, A.Y + B.Y); }
inline FBilliardsVec2 operator-(FBilliardsVec2 A, FBilliardsVec2 B) { return FBilliardsVec2(A.X - B.X, A.Y - B.Y); }
inline FBilliardsVec2 operator*(FBilliardsVec2 A, double S) { return FBilliardsVec2(A.X * S, A.Y * S); }
inline FBilliardsVec2 operator*(double S, FBilliardsVec2 A) { return FBilliardsVec2(A.X * S, A.Y * S); }

// Spin, in rad/s. X and Y are the ROLLING axes, in the plane of the cloth; Z (side spin)
// is not modelled yet and stays 0, but the actor turns the ball meshes from all three.
struct FBilliardsVec3
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
};

// A cushion face, a straight line between two jaws. The ball's centre keeps a radius
// from it; its two ends are the pocket jaws, and a ball can rattle off an end.
struct FBilliardsCushion
{
	FBilliardsVec2 A;
	FBilliardsVec2 B;
};

// The hole cut in the cloth. A ball whose centre is over the hole falls in.
struct FBilliardsPocket
{
	FBilliardsVec2 Center;
	double Radius = 6.2;
};

struct FBilliardsTableLayout
{
	double HalfLength = 115.53;    // cushion face to the centre, along X
	double HalfWidth = 70.05;      // cushion face to the centre, across Y
	double BallRadius = BilliardsDefaultBallRadius;
	double ClothHeight = 89.01;    // the cloth above the table's pivot, for the actor

	std::vector<FBilliardsCushion> Cushions;
	std::vector<FBilliardsPocket> Pockets;

	FBilliardsVec2 FootSpot;       // the apex of the rack
	FBilliardsVec2 HeadSpot;       // where the cue ball starts, and comes back to

	// The living-room table: PijayArt's Classic Billiards Table, measured from its mesh
	// (see the note above MakeDefault in BilliardsSim.cpp).
	static FBilliardsTableLayout MakeDefault();
};

enum class EBilliardsPhase : uint8_t
{
	Idle,       // no game yet: the table sits racked
	Aiming,     // everything is still; waiting for a stroke
	Rolling,    // a stroke is running
	Cleared,    // all fifteen are down
};

enum class EBilliardsEvent : uint8_t
{
	GameStarted,
	ShotTaken,          // Index = stroke number, Value = cue ball speed (cm/s)
	BallHitBall,        // Index, Other = the two balls, Value = closing speed
	BallHitCushion,     // Index = ball, Value = speed into the cushion
	BallPocketed,       // Index = ball (1..15), Other = pocket
	CueBallScratched,   // Other = pocket; a penalty stroke
	CueBallRespotted,   // the scratched cue ball is back on the table
	ShotEnded,          // Index = balls potted by that stroke
	TableCleared,       // Index = total strokes, penalties included
};

struct FBilliardsEvent
{
	EBilliardsEvent Type = EBilliardsEvent::GameStarted;
	int Index = -1;
	int Other = -1;
	double Value = 0.0;
};

// What the aim line shows: where the cue ball would first touch something, going
// straight, before any friction or spin has had a say.
struct FBilliardsAimPrediction
{
	bool bHitsBall = false;
	int Ball = -1;
	FBilliardsVec2 CueAtContact;       // the cue ball's centre at that moment
	FBilliardsVec2 ObjectDirection;    // where the object ball goes (unit); zero if none
	FBilliardsVec2 CueDeflection;      // where a stunned cue ball goes (unit, the 90-degree
	                                   // rule); zero for a dead-straight hit
};

class FBilliardsSim
{
public:
	static constexpr int BallCount = 16;       // 0 is the cue ball, 1..15 the object balls
	static constexpr int CueBall = 0;

	static constexpr double SubstepSeconds = 1.0 / 1200.0;
	static constexpr double MaxBallSpeed = 1200.0;   // cm/s; a hard break is about 900
	static constexpr double MaxFrameSeconds = 0.1;
	static constexpr int ContactIterations = 4;      // a rack is fifteen touching balls

	static_assert(MaxBallSpeed * SubstepSeconds < BilliardsDefaultBallRadius / 3.0,
		"a ball must move well under its radius per substep, or it can tunnel");

	static constexpr double Gravity = 981.0;               // cm/s^2
	static constexpr double SlidingFriction = 0.2;         // cloth on a slipping ball
	static constexpr double RollingFriction = 0.012;       // a rolling ball: 12 cm/s^2
	static constexpr double BallRestitution = 0.93;        // phenolic resin, nearly elastic
	static constexpr double CushionRestitution = 0.75;
	static constexpr double CushionFriction = 0.05;        // along the rail, per impact
	static constexpr double RestingImpactSpeed = 2.0;      // slower impacts do not bounce
	static constexpr double EventSpeed = 5.0;              // quieter contacts make no sound

	static constexpr double MinCueSpeed = 15.0;            // power 0 still moves it
	static constexpr double MaxCueSpeed = 900.0;           // power 1: a full break
	static constexpr double MaxTipOffset = 0.5;            // spin +-1 is half a radius off
	                                                       // centre; beyond that, a miscue
	static constexpr double MaxShotSeconds = 60.0;         // a watchdog, never reached in play

	explicit FBilliardsSim(FBilliardsTableLayout InLayout = FBilliardsTableLayout::MakeDefault());

	// Controls.
	void StartGame();                   // rack fifteen, cue ball on the head spot
	// Angle: the cue ball's direction. Power 0..1. Spin -1 (draw) .. +1 (follow); 0.8 is
	// natural roll, where the ball rolls from the first instant. Only while Aiming.
	bool Shoot(double Angle, double Power, double Spin);

	// Time. Advance runs whole fixed substeps and carries the remainder to the next frame.
	void Advance(double DeltaSeconds);
	void Substep();

	// Reading the table.
	const FBilliardsTableLayout& Layout() const { return TableLayout; }
	EBilliardsPhase Phase() const { return CurrentPhase; }
	FBilliardsVec2 BallPosition(int Ball) const { return Balls[Ball].Pos; }
	FBilliardsVec2 BallVelocity(int Ball) const { return Balls[Ball].Vel; }
	FBilliardsVec3 BallSpin(int Ball) const { return Balls[Ball].Spin; }
	bool IsOnTable(int Ball) const { return Balls[Ball].bOnTable; }
	bool IsSliding(int Ball) const;
	bool AnyBallMoving() const;
	int BallsLeft() const;              // object balls still on the table
	int Strokes() const { return ShotCount + PenaltyCount; }
	int Shots() const { return ShotCount; }
	int Penalties() const { return PenaltyCount; }

	FBilliardsAimPrediction PredictShot(double Angle) const;

	// Everything that happened since the last call, oldest first.
	std::vector<FBilliardsEvent> TakeEvents();

	// Test hooks. PlaceBall puts one ball anywhere, moving any way; bRolling gives it the
	// spin that matches its speed, so it starts out rolling rather than sliding.
	void PlaceBall(int Ball, FBilliardsVec2 Position, FBilliardsVec2 Velocity, bool bRolling);
	void RemoveBall(int Ball);
	void ClearTable();                  // every ball off, phase Aiming: for one-ball tests

private:
	struct FBall
	{
		FBilliardsVec2 Pos;
		FBilliardsVec2 Vel;
		FBilliardsVec3 Spin;
		bool bOnTable = false;
	};

	void Rack();
	void StepBall(FBall& Ball) const;
	void CollideBalls();
	void CollideCushions();
	void CheckPockets();
	void Pocket(int Ball, int PocketIndex);
	int NearestPocket(FBilliardsVec2 Position) const;
	void EndShot();
	void RespotCueBall();
	bool IsSpotFree(FBilliardsVec2 Position) const;
	void Emit(EBilliardsEvent Type, int Index = -1, int Other = -1, double Value = 0.0);

	FBilliardsTableLayout TableLayout;
	FBall Balls[BallCount];
	double Accumulator = 0.0;
	double ShotSeconds = 0.0;
	EBilliardsPhase CurrentPhase = EBilliardsPhase::Idle;
	int ShotCount = 0;
	int PenaltyCount = 0;
	int PottedThisShot = 0;
	bool bCueScratched = false;
	std::vector<FBilliardsEvent> PendingEvents;
};
