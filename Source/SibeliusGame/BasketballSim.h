// BasketballSim.h
//
// THE THREE-POINT CONTEST - the basketball half-court in the Transhuman Chess Park, reached by
// the BASKETBALL door in the living room (Walt, 2026-10-15: "let's do the basketball shooting
// contest"; the court "in the chess park"). The five AI agents shoot in turn, then Leonard.
//
// ===========================================================================
// THE HOUSE METHOD AGAIN (FBowlingSim, FBilliardsSim, FKartSim): a pure model, deterministic,
// standard library only, tested standalone by Tools/Tests/run_basketball_sim_tests.ps1. The
// court actor only presents it. The game decides every shot - never the chat model.
//
// THE CONTEST is the All-Star one: five racks of five balls around the three-point arc,
// corner to corner; the fifth ball of each rack is the MONEY BALL, worth two; sixty seconds
// on the clock, and a ball counts only if it leaves the hand before the buzzer. A perfect
// round is 30. Real shooters rarely finish all twenty-five balls - the clock is the contest.
//
// A SHOT is aimed at the middle of the rim and lands some way off it, at rim height, on the
// way down: an error along the line of the shot (short or long) and an error across it (wide).
// The ball - 12 cm in radius - fits through an 22.86 cm-radius rim with 10.9 cm to spare, so
// an error inside that is a SWISH. Further out the ball touches iron: it rattles in or out,
// more often out the further it is off. Further still it misses the rim altogether.
//   - An agent's errors are random, scaled by her Spread (the 1-sigma error at the arc).
//   - Leonard's come from the timing meter: released early the ball falls SHORT, late LONG,
//     plus a little scatter even when the timing is perfect.
//
// Coordinates: origin on the floor directly under the middle of the rim. +X out from the
// basket toward the top of the key, +Y to the LEFT as seen from the top of the key facing the
// basket, +Z up. Centimetres, seconds. NBA measurements.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FHoopVec3
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;

	constexpr FHoopVec3() = default;
	constexpr FHoopVec3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
};

namespace BasketballCourt
{
	constexpr double Gravity = 981.0;           // cm/s^2
	constexpr double RimHeight = 304.8;         // 10 ft
	constexpr double RimRadius = 22.86;         // 18 in across, inside the iron
	constexpr double BallRadius = 11.94;        // size 7: 29.5 in round
	constexpr double ArcDistance = 723.9;       // 23 ft 9 in from the middle of the rim
	constexpr double CornerDistance = 670.6;    // 22 ft in the corners
	constexpr double RimToBackboard = 38.1;     // middle of the rim to the face of the board
	constexpr double RimToBaseline = 160.0;     // middle of the rim to the baseline

	// The swish window: how far the ball's middle may be from the rim's and touch nothing.
	constexpr double CleanRadius = RimRadius - BallRadius;
	// Beyond this the ball's middle is so far off that it misses the rim altogether.
	constexpr double ContactRadius = RimRadius + BallRadius;
}

enum class EShotResult : uint8_t
{
	Swish,      // through without touching the rim
	RimIn,      // touched the rim and dropped
	RimOut,     // touched the rim and fell out
	Short,      // missed the rim altogether, short
	Long,       // missed the rim altogether, long (off the backboard)
	Wide,       // missed the rim altogether, to the side
};

inline bool IsMade(EShotResult Result) { return Result == EShotResult::Swish || Result == EShotResult::RimIn; }
const char* ShotResultName(EShotResult Result);

// How a shooter shoots. The agents each have one (FBasketballContest::AgentStyles); Leonard's
// Spread is small because the meter decides most of where his ball goes.
struct FShooterStyle
{
	std::string Name;
	double Spread = 20.0;         // cm, 1-sigma error at rim height, at the arc; less is better
	double ShotSeconds = 2.0;     // catch, set and release, ball to ball within a rack
	double MoveSeconds = 2.5;     // walking to the next rack
	double ReleaseHeight = 240.0; // cm, where the ball leaves the hand
	double ArcDegrees = 50.0;     // release angle above the horizontal
};

struct FShot
{
	int Rack = 0;                 // 0..4, right corner to left corner
	int Ball = 0;                 // 0..4; ball 4 is the money ball
	bool bMoney = false;
	EShotResult Result = EShotResult::Short;
	int Points = 0;
	double Clock = 0.0;           // seconds into the round when it was released
	double ErrorAlong = 0.0;      // cm at rim height: + long, - short
	double ErrorAcross = 0.0;     // cm at rim height: + to the shooter's left
	FHoopVec3 Release;            // where the ball leaves the hand
	FHoopVec3 Velocity;           // cm/s at release
	FHoopVec3 Target;             // where the ball's middle reaches rim height on the way down
	double FlightSeconds = 0.0;   // release to Target
};

// The arc of a shot: from Release, at ArcDegrees, so that the ball's middle comes down
// through Target. False if no such throw exists (a target above the arc's reach).
bool SolveLaunch(const FHoopVec3& Release, const FHoopVec3& Target, double ArcDegrees,
	FHoopVec3& OutVelocity, double& OutFlightSeconds);

// Where the ball is T seconds after leaving the hand.
FHoopVec3 BallAt(const FHoopVec3& Release, const FHoopVec3& Velocity, double T);

// What an error at rim height comes to. Roll01 decides the rattles: in the touching band a
// ball near the clean window mostly drops, one near the far edge mostly does not.
EShotResult ResolveShot(double ErrorAlong, double ErrorAcross, double Roll01);

// One shooter's sixty seconds.
class FBasketballContest
{
public:
	static constexpr int Racks = 5;
	static constexpr int BallsPerRack = 5;
	static constexpr double TimeLimit = 60.0;
	static constexpr double PlayerScatter = 14.0;     // cm, Leonard's error with perfect timing (~2/3 made)
	static constexpr double PlayerTimingCm = 70.0;    // cm short or long at a timing of -1 or +1

	// The five agents' styles, in shooting order.
	static std::vector<FShooterStyle> AgentStyles();

	// Where rack 0..4 stands: right corner, right wing, top of the key, left wing, left corner.
	static FHoopVec3 RackSpot(int Rack);

	// Starts the round. A player round waits for ShootPlayer; an agent's round shoots by itself.
	void Begin(const FShooterStyle& InStyle, bool bInPlayer, uint64_t Seed);

	// Runs the clock. An agent's shots that come due are appended to OutShots.
	void Advance(double DeltaSeconds, std::vector<FShot>& OutShots);

	// Leonard releases his next ball. Timing: 0 is the sweet spot, -1 far too early (short),
	// +1 far too late (long). False if no ball is ready yet (still picking it up, or walking to
	// the next rack) or the round is over.
	bool ShootPlayer(double Timing, FShot& OutShot);

	bool IsPlayer() const { return bPlayer; }
	bool IsFinished() const { return bFinished; }
	bool IsBallReady() const { return !bFinished && Clock >= NextBallAt; }
	double GetClock() const { return Clock; }
	// When the next ball goes (an agent's) or can go (Leonard's), on the round's clock: the court
	// starts her throw early enough that her hand lets go at exactly this moment.
	double GetNextBallAt() const { return NextBallAt; }
	double GetTimeLeft() const;
	int GetScore() const { return Score; }
	int GetRack() const { return NextRack; }
	int GetBall() const { return NextBall; }
	int GetBallsTaken() const { return BallsTaken; }
	const FShooterStyle& GetStyle() const { return Style; }
	const std::vector<FShot>& GetShots() const { return Shots; }

private:
	FShot TakeShot(double ErrorAlong, double ErrorAcross);
	void StepToNextBall();
	double Random01();
	double Gaussian();

	FShooterStyle Style;
	bool bPlayer = false;
	bool bFinished = true;
	double Clock = 0.0;
	double NextBallAt = 0.0;
	int NextRack = 0;
	int NextBall = 0;
	int BallsTaken = 0;
	int Score = 0;
	uint64_t Rng = 1;
	std::vector<FShot> Shots;
};

// Places for a list of scores, best first: index of the shooter in each place. Ties keep
// shooting order (the earlier shooter places higher - the later one had the scores to beat).
std::vector<int> RankScores(const std::vector<int>& Scores);
