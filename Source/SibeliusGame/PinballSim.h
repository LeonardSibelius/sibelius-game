// PinballSim.h
//
// PINBALL — the physics model for the living-room table, beside the poker cabinet
// (Walt, 2026-09-27).
//
// ===========================================================================
// PHYSICS IN 2D, RENDERING IN 3D.
//
// A pinball playfield is flat: a ball on a tilted plane. So the ball is simulated here in
// the plane of the table, in centimetres, and a table actor (not written yet) draws the
// result as a real chrome ball moving over a real 3D table. It does NOT hand the ball to
// Chaos. A fast ball through a physics-engine flipper is the classic way these fail:
// the ball tunnels through, or the flipper jitters. Here, every collision is ours.
//
// THE NO-TUNNELLING GUARANTEE is arithmetic, not hope. The ball moves in fixed substeps
// of 1/1200 s and its speed is capped, so in one substep it travels less than its own
// radius (static_assert below). A ball that cannot move a radius per step cannot cross a
// zero-thickness wall without first overlapping it, and overlap is what we resolve.
//
// A ROLLING BALL, NOT A SLIDING ONE. A solid sphere rolling down an incline accelerates at
// 5/7 of g·sin(slope), because part of the fall goes into spin. Real pinball feels
// slightly floaty for exactly that reason, and the model does the same.
//
// ===========================================================================
// THE HOUSE PATTERN (FCarouselSim, UPokerGameModel): a pure model, deterministic, no UI,
// no actors, no timers. The table actor reads Layout() to place its meshes and drains
// TakeEvents() each frame for sounds and lights. It never computes a collision or a
// score. The layout lives HERE so the thing you see and the thing that bounces the ball
// cannot disagree.
//
// NO ENGINE HEADERS, ON PURPOSE. This file and PinballSim.cpp use only the standard
// library, so Tools/Tests/run_pinball_sim_tests.ps1 can build and test them with the
// Visual Studio compiler alone, in seconds, without launching the editor or cooking
// anything. That was the only safe way to test on the failing system drive, and it stays
// the fastest way afterwards.
//
// Coordinates: X across the table, Y up the table, origin at the bottom-left corner of
// the cabinet, centimetres. Angles are radians, counter-clockwise from +X.
//
// NOT MODELLED YET: ball spin/english, nudge and tilt, ramps, drop targets, multiball.
// Free play only — no sauce rides on pinball. It is a skill toy, and the poker cabinet
// already covers betting.

#pragma once

#include <cstdint>
#include <vector>

// A real pinball is 1 1/16" across — 27 mm.
inline constexpr double PinballDefaultBallRadius = 1.35;

struct FPinVec2
{
	double X = 0.0;
	double Y = 0.0;

	constexpr FPinVec2() = default;
	constexpr FPinVec2(double InX, double InY) : X(InX), Y(InY) {}
};

inline FPinVec2 operator+(FPinVec2 A, FPinVec2 B) { return FPinVec2(A.X + B.X, A.Y + B.Y); }
inline FPinVec2 operator-(FPinVec2 A, FPinVec2 B) { return FPinVec2(A.X - B.X, A.Y - B.Y); }
inline FPinVec2 operator*(FPinVec2 A, double S) { return FPinVec2(A.X * S, A.Y * S); }
inline FPinVec2 operator*(double S, FPinVec2 A) { return FPinVec2(A.X * S, A.Y * S); }

enum class EPinballSurface : uint8_t
{
	Wall,        // rails, the top arc, the lane walls: passive, and fairly dead
	Rubber,      // a rubber band stretched between posts: passive, but lively
	Slingshot,   // a rubber face that kicks the ball away when struck hard enough
	OneWayGate,  // collides only from its front (left-normal) side: lets the plunged ball
	             // out of the shooter lane and never lets it back in
};

struct FPinballSegment
{
	FPinVec2 A;
	FPinVec2 B;
	EPinballSurface Kind = EPinballSurface::Wall;
	int Index = -1;  // slingshot number, for events; -1 for everything else
};

struct FPinballBumper
{
	FPinVec2 Center;
	double Radius = 2.6;
};

// A rubber post: a passive circle. The rubber bands between posts are Rubber segments,
// pushed out to the post's edge, so the ball meets rubber rather than the post's centre line.
struct FPinballPost
{
	FPinVec2 Center;
	double Radius = 1.1;
};

// A rollover is a sensor, not an obstacle: it lights when the ball passes over it. Those
// marked bInSet together pay the table's set bonus when every one of them is lit.
struct FPinballRollover
{
	FPinVec2 Center;
	double Radius = 2.0;
	int Points = 50;
	bool bInSet = true;
};

// A kickout hole: a slow ball drops in, is held a moment, scores, and is kicked back out.
// A fast ball rolls straight over it, as it would on the real thing.
struct FPinballKickout
{
	FPinVec2 Center;
	double Radius = 1.6;
	int Points = 30;
	FPinVec2 EjectDirection = FPinVec2(0.0, -1.0);   // unit vector, table coordinates
	double EjectSpeed = 200.0;
};

// A flipper is a tapered capsule rotating about its pivot: thick at the base, thin at the
// tip, like the real bat.
struct FPinballFlipperDef
{
	FPinVec2 Pivot;
	double Length = 7.4;      // pivot centre to tip centre
	double BaseRadius = 1.4;
	double TipRadius = 0.6;
	double RestAngle = 0.0;   // resting: pointing down toward the drain
	double UpAngle = 0.0;     // fully raised
};

struct FPinballTableLayout
{
	double Width = 51.0;      // the whole cabinet, shooter lane included
	double Length = 107.0;
	double SlopeDegrees = 6.5;
	double BallRadius = PinballDefaultBallRadius;

	std::vector<FPinballSegment> Segments;
	std::vector<FPinballBumper> Bumpers;
	std::vector<FPinballPost> Posts;
	std::vector<FPinballRollover> Rollovers;
	std::vector<FPinballKickout> Kickouts;
	FPinballFlipperDef Flippers[2];   // 0 = left, 1 = right

	FPinVec2 PlungerRest;             // where a served ball waits in the shooter lane
	double ShooterLaneMinX = 46.5;    // the lane's inner wall; the playfield is left of it
	double DrainY = -4.0;             // below this the ball is lost

	// Scoring belongs to the table, not the physics: the Singalong's bumper caps say
	// "10 POINTS", and a different table would say something else.
	int BumperPoints = 100;
	int SlingshotPoints = 10;
	int RolloverSetBonus = 1000;

	// The living-room table: the Singalong cabinet's own playfield, measured from its mesh
	// (see the note above MakeDefault in PinballSim.cpp).
	static FPinballTableLayout MakeDefault();

	// A plain generic table - three bumpers, two slingshots, three rollovers - kept because
	// the physics tests are written against its simple, known geometry.
	static FPinballTableLayout MakeTestTable();
};

enum class EPinballPhase : uint8_t
{
	Attract,    // nobody has pressed start
	BallReady,  // a ball is waiting on the plunger
	InPlay,
	GameOver,
};

enum class EPinballEvent : uint8_t
{
	GameStarted,
	BallServed,         // Index = ball number; also sent when a soft plunge rolls back
	BallLaunched,       // Index = ball number
	FlipperUp,          // Index = side
	FlipperDown,        // Index = side
	BumperHit,          // Index = bumper
	SlingshotHit,       // Index = slingshot
	RolloverLit,        // Index = rollover
	RolloversComplete,  // every rollover in the set lit: bonus, and the lights reset
	KickoutCaptured,    // Index = hole, Points = what it paid
	KickoutEjected,     // Index = hole
	BallDrained,        // Index = the ball that was lost
	GameOver,           // Points = final score
};

struct FPinballEvent
{
	EPinballEvent Type = EPinballEvent::GameStarted;
	int Index = -1;
	int Points = 0;
};

class FPinballSim
{
public:
	static constexpr double SubstepSeconds = 1.0 / 1200.0;
	static constexpr double MaxBallSpeed = 1500.0;   // cm/s; real flipper shots peak near 800
	static constexpr double MaxFrameSeconds = 0.1;   // a hitch never becomes a spiral of catch-up
	static constexpr int ContactIterations = 2;      // corners touch two surfaces at once

	static_assert(MaxBallSpeed * SubstepSeconds < PinballDefaultBallRadius,
		"the ball must move less than its own radius per substep, or it can tunnel");

	static constexpr int BallsPerGame = 3;

	static constexpr double WallRestitution = 0.45;
	static constexpr double RubberRestitution = 0.6;
	static constexpr double BumperRestitution = 0.5;
	static constexpr double FlipperRestitution = 0.55;
	static constexpr double BumperKickSpeed = 300.0;
	static constexpr double SlingKickSpeed = 250.0;
	static constexpr double SlingTriggerSpeed = 20.0;   // a ball resting on a sling does not fire it
	static constexpr double KickerCooldownSeconds = 0.1;
	static constexpr double RestingSpeed = 8.0;         // slower impacts are fully inelastic, so a
	                                                    // resting ball rests instead of buzzing
	static constexpr double RollingDamping = 0.03;      // per second
	static constexpr double ImpactFriction = 0.02;      // tangential loss per real impact
	static constexpr double FlipperUpSpeed = 35.0;      // rad/s: a full stroke in ~27 ms
	static constexpr double FlipperDownSpeed = 18.0;
	static constexpr double PlungerMinSpeed = 80.0;     // too soft to clear the gate
	static constexpr double PlungerMaxSpeed = 700.0;
	static constexpr double KickoutCaptureSpeed = 180.0; // faster than this rolls over the hole
	static constexpr double KickoutHoldSeconds = 0.9;
	static constexpr double KickoutRearmSeconds = 0.6;   // the kicked ball is not re-swallowed

	explicit FPinballSim(FPinballTableLayout InLayout = FPinballTableLayout::MakeDefault());

	// Controls.
	void StartGame();
	void SetFlipper(int Side, bool bHeld);
	bool Launch(double Pull);                // 0..1; only while a ball is on the plunger

	// Time. Advance runs whole fixed substeps and carries the remainder to the next frame.
	void Advance(double DeltaSeconds);
	void Substep();

	// Reading the table.
	const FPinballTableLayout& Layout() const { return TableLayout; }
	EPinballPhase Phase() const { return CurrentPhase; }
	FPinVec2 BallPosition() const { return BallPos; }
	FPinVec2 BallVelocity() const { return BallVel; }
	double FlipperAngle(int Side) const;
	FPinVec2 FlipperTip(int Side) const;
	int Score() const { return CurrentScore; }
	int BallNumber() const { return CurrentBall; }
	bool IsRolloverLit(size_t Index) const;
	int HeldInKickout() const { return CapturedKickout; }   // -1 when the ball is free
	double EffectiveGravity() const { return Gravity; }

	// Everything that happened since the last call, oldest first.
	std::vector<FPinballEvent> TakeEvents();

	// Test hook: put the ball anywhere, moving any way, in play.
	void PlaceBall(FPinVec2 Position, FPinVec2 Velocity);

private:
	struct FFlipperState
	{
		double Angle = 0.0;
		double AngularVelocity = 0.0;
		bool bHeld = false;
	};

	void ServeBall();
	void StepFlippers();
	void TickCooldowns();
	void CollideSegments();
	void CollideBumpers();
	void CollidePosts();
	void CollideFlippers();
	void UpdateRollovers();
	void UpdateKickouts();
	void EjectKickout();
	void DrainBall();
	double Resolve(FPinVec2 Normal, double Penetration, FPinVec2 SurfaceVelocity, double Restitution);
	void Kick(FPinVec2 Normal, double Speed);
	void ClampBallSpeed();
	void Emit(EPinballEvent Type, int Index = -1, int Points = 0);

	FPinballTableLayout TableLayout;
	double Gravity = 0.0;
	double Accumulator = 0.0;
	EPinballPhase CurrentPhase = EPinballPhase::Attract;
	FPinVec2 BallPos;
	FPinVec2 BallVel;
	FFlipperState FlipperStates[2];
	std::vector<uint8_t> RolloverLit;
	std::vector<uint8_t> RolloverInside;
	std::vector<double> BumperCooldown;
	std::vector<double> SlingCooldown;
	std::vector<double> KickoutCooldown;
	int CapturedKickout = -1;
	double KickoutHeldSeconds = 0.0;
	int CurrentScore = 0;
	int CurrentBall = 0;
	std::vector<FPinballEvent> PendingEvents;
};
