// HyperBilliardsSim.h
//
// 4D BILLIARDS — pool played on a table one dimension bigger than ours, in Aisling's snowfield
// (Walt, 2026-10-25: "let's do the 4D billiards in Aisling's snowfield"). He wanted the games
// beyond the portals to be ones a transhuman would play; a table no human eye can take in
// whole is that, and it sits on the game he plays most.
//
// ===========================================================================
// WHAT IS FOUR-DIMENSIONAL ABOUT IT. Our table's cloth is a flat plane and the balls roll on
// it in two directions. One dimension up, the cloth is a SPACE - length X, width Y and a third
// direction, W, that no human has a word for (mathematicians say ana and kata) - and the balls
// roll in all three. The balls are 4D spheres resting on it.
//
// The player sees one 3D SLICE of that world, the one at his W (the table actor's job): the
// cloth looks like an ordinary cloth, and each ball shows as its cross-section there - a full
// ball when it lies in his slice, smaller as it lies further off in W, gone when it is more
// than a radius away. Balls swell, shrink and vanish as they roll; two balls that seem to pass
// through each other were a ball's width apart in W. Everything here is the true geometry:
// the model knows nothing about slices.
//
// SO THE MODEL IS THE LIVING ROOM'S, ONE DIMENSION UP. FBilliardsSim simulates in the plane of
// its cloth; this one simulates in the space of this cloth, with 3-vectors (X, Y, W), the same
// way: fixed substeps, a speed cap against tunnelling, and every collision its own.
//
// SLIDING, THEN ROLLING, as at the living-room table, so stun, draw and follow come out of the
// physics and are not special cases. A spinning ball is carried by its spin at some velocity -
// its ROLL - and the cloth rubs wherever its true velocity differs (the slip), until the two
// agree and it rolls. Spin in four dimensions turns in a plane, not about an axis, but all the
// cloth ever feels is that roll velocity, so the model keeps the roll and needs no more.
//   - A 4D ball's moment of inertia is 2/(n+2) M R^2 with n = 4: a third of M R^2, where ours
//     is two fifths. So the cloth turns slip into roll faster, and a centre-struck ball settles
//     to rolling at THREE QUARTERS of its speed, not five sevenths. The tests hold it to that.
//   - A collision is too short to change spin, so a ball keeps its roll through one: a centre
//     hit that is still sliding stops dead, backspin pulls back, topspin follows on.
//
// THE TABLE is a box in (X, Y, W): six cushions, the faces, and twelve pockets - its eight
// corners and the middles of its four long edges, the long-edge middle pockets being what a
// side pocket becomes. The rack is ten balls in a tetrahedron, because one dimension up the
// triangle of a rack is a pyramid: layers of 1, 3 and 6 along X, apex toward the cue ball.
// Solo "clear the table", like the living room's: the score is the strokes, fewer is better,
// and a scratch is a penalty stroke.
//
// THE HOUSE PATTERN (FBilliardsSim): pure, deterministic, no engine headers. The actor reads
// it and drains TakeEvents(); it never computes a collision. Tools/Tests/
// run_hyper_billiards_tests.ps1 builds it with the Visual Studio compiler alone.
//
// Coordinates: origin at the centre of the table, X along its length (+X the FOOT end, where
// the rack goes), Y across it, W the hidden direction; centimetres. The aim is a heading in
// the X-Y plane (radians, counter-clockwise from +X) and a tilt into W (radians, + toward +W).

#pragma once

#include <cstdint>
#include <vector>

struct FHyperVec3
{
	double X = 0.0;
	double Y = 0.0;
	double W = 0.0;

	constexpr FHyperVec3() = default;
	constexpr FHyperVec3(double InX, double InY, double InW) : X(InX), Y(InY), W(InW) {}
};

inline FHyperVec3 operator+(FHyperVec3 A, FHyperVec3 B) { return FHyperVec3(A.X + B.X, A.Y + B.Y, A.W + B.W); }
inline FHyperVec3 operator-(FHyperVec3 A, FHyperVec3 B) { return FHyperVec3(A.X - B.X, A.Y - B.Y, A.W - B.W); }
inline FHyperVec3 operator*(FHyperVec3 A, double S) { return FHyperVec3(A.X * S, A.Y * S, A.W * S); }
inline FHyperVec3 operator*(double S, FHyperVec3 A) { return FHyperVec3(A.X * S, A.Y * S, A.W * S); }

struct FHyperTableLayout
{
	double HalfLength = 110.0;   // cushion face to the centre, along X
	double HalfWidth = 55.0;     // across Y
	double HalfDepth = 30.0;     // into W: six balls deep
	double BallRadius = 5.0;     // bigger than the living room's 3.5: a slice is smaller still
	// A ball whose centre comes this close to a pocket's point falls in. The cushions keep a
	// centre one radius from each face, so the nearest it can come to a corner is sqrt(3) R
	// and to an edge sqrt(2) R: the capture reaches past both, as a pocket's mouth does.
	double PocketCapture = 12.0;

	std::vector<FHyperVec3> Pockets;   // the eight corners, then the four long-edge middles
	FHyperVec3 FootSpot;               // the apex of the rack
	FHyperVec3 HeadSpot;               // where the cue ball starts, and comes back to

	static FHyperTableLayout MakeDefault();
};

enum class EHyperPhase : uint8_t
{
	Idle,       // no game yet: the table sits racked
	Aiming,     // everything is still; waiting for a stroke
	Rolling,    // a stroke is running
	Cleared,    // all ten are down
};

enum class EHyperEvent : uint8_t
{
	GameStarted,
	ShotTaken,          // Index = stroke number, Value = cue ball speed (cm/s)
	BallHitBall,        // Index, Other = the two balls, Value = closing speed
	BallHitCushion,     // Index = ball, Value = speed into the cushion
	BallPocketed,       // Index = ball (1..10), Other = pocket
	CueBallScratched,   // Other = pocket; a penalty stroke
	CueBallRespotted,
	ShotEnded,          // Index = balls potted by that stroke
	TableCleared,       // Index = total strokes, penalties included
};

struct FHyperEvent
{
	EHyperEvent Type = EHyperEvent::GameStarted;
	int Index = -1;
	int Other = -1;
	double Value = 0.0;
};

// Where the cue ball would first touch another ball going straight, before friction or spin
// have their say - which may be a ball out of the player's slice, so the actor shows it.
struct FHyperAimPrediction
{
	bool bHitsBall = false;
	int Ball = -1;
	FHyperVec3 CueAtContact;      // the cue ball's centre at that moment
	FHyperVec3 ObjectDirection;   // where the object ball goes (unit); zero if none
};

class FHyperBilliardsSim
{
public:
	static constexpr int BallCount = 11;       // 0 is the cue ball, 1..10 the object balls
	static constexpr int CueBall = 0;

	static constexpr double SubstepSeconds = 1.0 / 1200.0;
	static constexpr double MaxBallSpeed = 1200.0;
	static constexpr double MaxFrameSeconds = 0.1;
	static constexpr int ContactIterations = 4;

	static_assert(MaxBallSpeed * SubstepSeconds < 5.0 / 3.0,
		"a ball must move well under its radius per substep, or it can tunnel");

	static constexpr double Gravity = 981.0;
	static constexpr double SlidingFriction = 0.2;     // the living room's cloth
	static constexpr double RollingFriction = 0.012;
	// d(roll)/dt per unit of sliding deceleration: M R^2 / I = (n + 2) / 2 for an n-ball, 3 in
	// four dimensions (2.5 in ours).
	static constexpr double RollGain = 3.0;
	static constexpr double BallRestitution = 0.93;
	static constexpr double CushionRestitution = 0.75;
	static constexpr double CushionFriction = 0.05;
	static constexpr double RestingImpactSpeed = 2.0;
	static constexpr double EventSpeed = 5.0;

	static constexpr double MinCueSpeed = 15.0;
	static constexpr double MaxCueSpeed = 900.0;
	static constexpr double MaxTipOffset = 0.5;        // spin +-1 is half a radius off centre
	static constexpr double MaxShotSeconds = 60.0;

	explicit FHyperBilliardsSim(FHyperTableLayout InLayout = FHyperTableLayout::MakeDefault());

	// Controls.
	void StartGame();
	// Heading in X-Y, Tilt into W (both radians), Power 0..1, Spin -1 (draw) .. +1 (follow).
	// Only while Aiming. The natural-roll spin here is 2/3 (0.8 at the living-room table).
	bool Shoot(double Heading, double Tilt, double Power, double Spin);

	void Advance(double DeltaSeconds);
	void Substep();

	// Reading the table.
	const FHyperTableLayout& Layout() const { return TableLayout; }
	EHyperPhase Phase() const { return CurrentPhase; }
	FHyperVec3 BallPosition(int Ball) const { return Balls[Ball].Pos; }
	FHyperVec3 BallVelocity(int Ball) const { return Balls[Ball].Vel; }
	FHyperVec3 BallRoll(int Ball) const { return Balls[Ball].Roll; }
	bool IsOnTable(int Ball) const { return Balls[Ball].bOnTable; }
	bool IsSliding(int Ball) const;
	bool AnyBallMoving() const;
	int BallsLeft() const;
	int Strokes() const { return ShotCount + PenaltyCount; }
	int Shots() const { return ShotCount; }
	int Penalties() const { return PenaltyCount; }

	// The radius of a ball's cross-section in the slice at W, or 0 where the slice misses it.
	static double SliceRadius(double BallRadius, double BallW, double SliceW);

	FHyperAimPrediction PredictShot(double Heading, double Tilt) const;

	std::vector<FHyperEvent> TakeEvents();

	// Test hooks, as FBilliardsSim's. bRolling gives a ball the roll that matches its speed.
	void PlaceBall(int Ball, FHyperVec3 Position, FHyperVec3 Velocity, bool bRolling);
	void RemoveBall(int Ball);
	void ClearTable();

private:
	struct FBall
	{
		FHyperVec3 Pos;
		FHyperVec3 Vel;
		FHyperVec3 Roll;   // the velocity its spin alone would carry it at
		bool bOnTable = false;
	};

	void Rack();
	void StepBall(FBall& Ball) const;
	void CollideBalls();
	void CollideCushions();
	void CheckPockets();
	void Pocket(int Ball, int PocketIndex);
	void EndShot();
	void RespotCueBall();
	bool IsSpotFree(FHyperVec3 Position) const;
	void Emit(EHyperEvent Type, int Index = -1, int Other = -1, double Value = 0.0);

	FHyperTableLayout TableLayout;
	FBall Balls[BallCount];
	double Accumulator = 0.0;
	double ShotSeconds = 0.0;
	EHyperPhase CurrentPhase = EHyperPhase::Idle;
	int ShotCount = 0;
	int PenaltyCount = 0;
	int PottedThisShot = 0;
	bool bCueScratched = false;
	std::vector<FHyperEvent> PendingEvents;
};
