// OrbitGolfSim.h
//
// ORBITAL GOLF — a course floating over Elise's grassland (Walt, 2026-10-25: "let's do the
// orbital golf"). The second game beyond the portals for a transhuman to play, after the 4D
// billiards in Aisling's snowfield: golf where the greens are moons hanging in the sky, each
// pulling the ball with its own gravity, on top of the world's own.
//
// ===========================================================================
// THE PHYSICS. One ball, in centimetres and seconds, Z up. It falls under the world's gravity
// (a uniform 981 cm/s^2 down) and every moon of the hole pulls it toward the moon's centre with
// an inverse square, set by the moon's SURFACE gravity: GM = g_surface * R^2. Where a moon pulls
// harder than the world, a ball can lie on its underside, upside down - the third hole's cup is
// there.
//
// THE GRAND TOUR'S INTEGRATOR FLIES IT. FSpaceIntegrator (SpaceSim.h) is the Dormand-Prince 5(4)
// stepper that flew Voyager 2 to Jupiter in the space game Walt set aside; it is generic over the
// force, so here it flies a golf ball. Capped at one substep so no contact is stepped over.
//
// CONTACT is ours, as at the pool tables: a ball meeting a moon or the tee platform bounces off
// its surface with restitution, loses speed along it to friction, and once it is in contact and
// slow it STOPS where it is - a ball comes to rest on a moon the way a putt stops on a green,
// not by rolling to the moon's lowest point. The grass below is out of bounds; so is a ball
// still flying after MaxFlightSeconds - lost in orbit. Either costs a penalty stroke and puts
// the ball back where it was played from.
//
// THE CUP is a point on a moon's surface. A ball holes out when it comes to rest within
// CupRadius of it, or passes over it within that radius moving slowly enough to drop.
//
// THE HOUSE PATTERN (FHyperBilliardsSim, FBilliardsSim): pure, deterministic, no engine headers;
// the actor reads it and drains TakeEvents(). Tools/Tests/run_orbit_golf_tests.ps1 builds it with
// SpaceSim.cpp and the Visual Studio compiler alone.
//
// Course coordinates: origin on the tee platform's top, +X down the course toward the moons.
// A shot is a heading (yaw, radians, counter-clockwise from +X), a pitch (radians above
// horizontal) and a power 0..1.

#pragma once

#include "SpaceSim.h"

#include <cstdint>
#include <string>
#include <vector>

struct FOrbitMoon
{
	FSpaceVec Center;
	double Radius = 400.0;
	double SurfaceGravity = 2.0 * 981.0;   // cm/s^2 at its surface
};

struct FOrbitHole
{
	std::string Name;
	int Par = 3;
	std::vector<FOrbitMoon> Moons;
	int CupMoon = 0;                       // the moon the cup is on
	FSpaceVec CupDirection;                // unit, from that moon's centre
	FSpaceVec CupPoint() const;            // on the surface
};

struct FOrbitCourse
{
	std::vector<FOrbitHole> Holes;
	double PlatformRadius = 300.0;         // the tee platform, its top at Z = 0
	double WorldGravity = 981.0;           // Elise's world's own, straight down
	double BallRadius = 8.0;               // big for a golf ball: it is seen from 30 m away
	double CupRadius = 40.0;
	double OutOfBoundsZ = -60.0;           // below the platform's top and off it: the grass
	double OutOfBoundsRange = 9000.0;      // this far from the tee, sideways or up: gone

	// The three holes of Elise's course: First Light, Slingshot, Underside.
	static FOrbitCourse MakeDefault();
	static FOrbitCourse MakePractice();
};

enum class EOrbitPhase : uint8_t
{
	Idle,       // no round yet
	Aiming,     // the ball lies still; waiting for a shot
	Flying,     // a shot is running
	Holed,      // in the cup; waiting to go on to the next hole
	Finished,   // the last hole is done
};

enum class EOrbitEvent : uint8_t
{
	RoundStarted,
	HoleStarted,        // Index = hole
	ShotTaken,          // Index = strokes on this hole, Value = launch speed
	Bounced,            // Index = moon (-1 the platform), Value = speed into it
	CameToRest,         // Index = moon it rests on (-1 the platform)
	Holed,              // Index = strokes for the hole
	OutOfBounds,        // a penalty stroke; the ball goes back
	LostInOrbit,        // a penalty stroke; the ball goes back
	RoundFinished,      // Index = total strokes, Value = total par
};

struct FOrbitEvent
{
	EOrbitEvent Type = EOrbitEvent::RoundStarted;
	int Index = -1;
	double Value = 0.0;
};

class FOrbitGolfSim
{
public:
	static constexpr double SubstepSeconds = 1.0 / 480.0;
	static constexpr double MaxFrameSeconds = 0.1;
	static constexpr double MinLaunchSpeed = 60.0;
	static constexpr double MaxLaunchSpeed = 3600.0;
	static constexpr double Restitution = 0.35;
	static constexpr double SurfaceFriction = 0.35;      // speed lost along a surface per impact, of the normal speed
	static constexpr double ContactDrag = 350.0;         // cm/s^2 of rolling resistance while touching
	static constexpr double RestSpeed = 25.0;            // touching and slower than this: at rest
	static constexpr double DropSpeed = 400.0;           // over the cup slower than this: in
	static constexpr double MaxFlightSeconds = 20.0;

	explicit FOrbitGolfSim(FOrbitCourse InCourse = FOrbitCourse::MakeDefault());

	void StartRound();
	void NextHole();                       // only when Holed
	bool Shoot(double Yaw, double Pitch, double Power);   // only when Aiming

	void Advance(double DeltaSeconds);
	void Substep();

	// Reading.
	const FOrbitCourse& Course() const { return CourseData; }
	const FOrbitHole& CurrentHole() const { return CourseData.Holes[HoleIndex]; }
	int HoleNumber() const { return HoleIndex; }
	EOrbitPhase Phase() const { return CurrentPhase; }
	FSpaceVec BallPosition() const { return Ball.Position; }
	FSpaceVec BallVelocity() const { return Ball.Velocity; }
	int HoleStrokes() const { return Strokes; }
	int TotalStrokes() const { return Total; }
	int TotalPar() const { return ParSoFar; }
	const std::vector<int>& Card() const { return ScoreCard; }   // strokes per finished hole
	double FlightSeconds() const { return Flight; }

	// The acceleration on a ball at P: the world's gravity and every moon of the current hole.
	FSpaceVec Gravity(const FSpaceVec& P) const;

	// Predict the actual shot including bounces; optional outcome describes its end.
	std::vector<FSpaceVec> PredictPath(double Yaw, double Pitch, double Power, double Seconds, int Points, EOrbitEvent* Outcome = nullptr) const;

	// Local surface frame: loft always points away from the supporting moon (or tee).
	FSpaceVec SurfaceUp() const;
	FSpaceVec SurfaceDirection(double Heading, double Loft) const;
	double SurfaceHeadingTo(FSpaceVec Target) const;
	static double ClubPower(double Power, bool bPutt);
	static FSpaceVec ShotDirection(double Yaw, double Pitch);
	static double LaunchSpeed(double Power);

	std::vector<FOrbitEvent> TakeEvents();

	// Test hooks: put the ball anywhere, moving any way, as if a shot were running.
	void PlaceBall(FSpaceVec Position, FSpaceVec Velocity);
	FOrbitCourse& MutableCourse() { return CourseData; }

private:
	void StartHole(int Index);
	bool Collide();                        // true if the ball touched something this substep
	void CheckCup();
	void Penalty(EOrbitEvent Why);
	void Emit(EOrbitEvent Type, int Index = -1, double Value = 0.0);

	FOrbitCourse CourseData;
	FSpaceState Ball;
	FSpaceVec Lie;                         // where the ball was last played from
	FSpaceIntegrator Integrator;
	double Clock = 0.0;
	double Accumulator = 0.0;
	double Flight = 0.0;
	double SlowTouching = 0.0;             // seconds in contact and slow
	EOrbitPhase CurrentPhase = EOrbitPhase::Idle;
	int HoleIndex = 0;
	int Strokes = 0;
	int Total = 0;
	int ParSoFar = 0;
	std::vector<int> ScoreCard;
	std::vector<FOrbitEvent> PendingEvents;
};
