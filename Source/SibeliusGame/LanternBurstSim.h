// LanternBurstSim.h
//
// LANTERN BURST — a lantern of glowing glass orbs hanging over Kaia's jungle (Walt, 2026-10-27:
// "Please make a new game that is addictive and attractive. I don't care what genre or topic it
// is" - and it need not fit Kaia's home or the story). The match-three shooter, rolled into a
// cylinder and hung from a crown on chains: shoot an orb onto the lantern, three or more of a
// colour touching burst, and whatever no longer hangs from the crown FALLS. Cut a ring right round
// the lantern and everything under it comes down at once - the jackpot a flat board cannot give.
//
// ===========================================================================
// THE LATTICE. Columns orbs round, rows counted down from the crown. Odd rows sit half a step
// round from even ones, so every orb touches six: two beside it, two above, two below - and the
// last column touches the first. An orb hangs if a chain of touching orbs reaches row 0, which
// hangs from the crown itself.
//
// A SHOT leaves the launcher under the lantern and climbs the glass in a straight line on the
// lantern unrolled flat - which, rolled up again, is a corkscrew: aimed off the vertical it winds
// round the lantern, round the back if he likes. It sticks to the first orb it comes within
// CollideDistance of (or to the crown), in the empty place beside that orb nearest to it, so what
// is reachable is what lies open from below - holes up inside the lantern are not. (The first
// version let a shot go to any open place; a greedy robot then cleared stage 1 in 8.5 shots and
// never lost. The corkscrew is what makes aiming a skill.) Matching three or more of its colour
// bursts them; anything left hanging from nothing falls. A shot that bursts nothing is a MISS:
// after LowerEvery misses the crown comes down a row. An orb left at or below the line of embers
// (LineRow) ends the game; a lantern emptied ends the stage.
//
// THE UNROLLED LANTERN, the plane shots fly in: X round the lantern in columns (one column is one
// orb's width, so the lantern is Columns wide and X wraps), Y down from row 0's centres in orb
// diameters (rows are RowHeight apart). The launcher sits a row under the embers, so as the crown
// comes down the launcher, like the embers, stays where it is in the world.
//
// THE HAND holds the orb loaded and the next one; SWAP trades them. New orbs come only in colours
// still on the lantern, so no orb is ever dead in the hand.
//
// THE SPARK is the reward for playing well: every fourth burst in a row, and every fall of eight
// or more, puts a spark in the hand. It burns every orb within two steps, whatever its colour -
// the way to cut the neck the colours will not.
//
// SCORE: a burst of n scores 10n; a fall of n scores 10 n(n+1)/2, so one big fall beats many
// small bursts; both times the streak (consecutive shots that burst something, x1 to x5). A
// cleared stage adds 1000 per stage number, and 1000 more if the crown never came down.
//
// THE HOUSE PATTERN (FOrbitGolfSim, FBilliardsSim): pure, deterministic, no engine headers; the
// actor (ALanternBurst) reads it and drains TakeEvents(). Tools/Tests/run_lantern_burst_tests.ps1
// builds it with the Visual Studio compiler alone.

#pragma once

#include <cstdint>
#include <vector>

namespace LanternBurst
{
	constexpr int Columns = 20;
	constexpr int DeathRow = 12;              // the embers' row while the crown is at the top
	constexpr int GridRows = DeathRow + 1;
	constexpr int MaxColors = 6;
	constexpr int Empty = -1;
	constexpr int Spark = MaxColors;          // the seventh kind of orb
	constexpr int SparkReach = 2;             // steps
	constexpr int MaxMultiplier = 5;
	constexpr int MinBurst = 3;
	constexpr int SparkEveryStreak = 4;
	constexpr int SparkForFall = 8;
	constexpr double RowHeight = 0.86602540378;     // rows apart, in orb diameters
	constexpr double CollideDistance = 0.85;        // a hair under touching, so a shot threads a one-orb gap
	constexpr double MaxAimAngle = 1.30899693899;   // 75 degrees either side of straight up
	constexpr double TraceStep = 0.08;              // diameters
	constexpr double MaxTravel = 200.0;
}

/** A point on the unrolled lantern: X round it in columns, Y down from row 0 in diameters. */
struct FLanternPoint
{
	double X = 0.0;
	double Y = 0.0;
};

struct FLanternCell
{
	int Row = 0;
	int Col = 0;
	bool operator==(const FLanternCell& Other) const { return Row == Other.Row && Col == Other.Col; }
};

/** An orb as it stood when something happened to it. */
struct FLanternOrb
{
	int Row = 0;
	int Col = 0;
	double Column = 0.0;   // round the lantern in columns: Col, or Col + 0.5 on an odd row
	int Color = 0;
	int Ring = 0;          // steps from the shot that set it off - the actor ripples bursts by it
};

enum class ELanternPhase : uint8_t
{
	Ready,        // before the first game
	Playing,
	StageClear,   // the lantern is empty; NextStage() hangs the next
	GameOver
};

enum class ELanternEvent : uint8_t
{
	StageStarted,   // Index = the stage
	Placed,         // Orbs[0] = where the shot stuck
	Burst,          // Orbs = the matched colour; Index = the multiplier; Value = points
	Sparked,        // Orbs = what the spark burned, itself included; Index, Value as Burst
	Fell,           // Orbs = what no longer hung; Value = points
	StreakUp,       // Index = the multiplier now earned (2..5)
	SparkEarned,
	Missed,         // Index = misses left before the crown comes down
	Lowered,        // Index = rows the crown has come down this stage
	StageCleared,   // Index = the stage; Value = the bonus
	GameOver        // Value = the final score
};

struct FLanternEvent
{
	ELanternEvent Type = ELanternEvent::Placed;
	int Index = 0;
	long long Value = 0;
	std::vector<FLanternOrb> Orbs;
};

struct FLanternTrace
{
	bool bLands = false;
	FLanternCell Cell;
	std::vector<FLanternPoint> Path;   // the launcher first, where it stuck last (X not wrapped)
};

struct FLanternStageSpec
{
	int Rows = 5;            // filled when the stage is hung
	int Colors = 3;
	int LowerEvery = 7;      // misses before the crown comes down a row
	int WaistRow = 2;        // a thinly filled row, so a ring can be cut early; -1 for none
	double WaistFill = 0.5;
	double Clump = 0.4;      // how often a hung orb copies a neighbour's colour: more, easier pairs
};

class FLanternBurstSim
{
public:
	explicit FLanternBurstSim(uint32_t Seed = 1);

	void NewGame(uint32_t Seed);
	void NextStage();
	static FLanternStageSpec StageSpec(int Stage);

	ELanternPhase Phase() const { return CurrentPhase; }
	int Stage() const { return CurrentStage; }
	long long Score() const { return CurrentScore; }
	int Streak() const { return CurrentStreak; }
	/** What the next burst is worth if it keeps the streak going. */
	int NextMultiplier() const;
	int MissesBeforeLower() const { return MissesLeft; }
	int Lowered() const { return LoweredRows; }
	/** An orb in this row or below it is over the embers. */
	int LineRow() const { return LanternBurst::DeathRow - LoweredRows; }
	int Shots() const { return ShotCount; }

	// The lattice.
	int At(int Row, int Col) const;
	static bool Shifted(int Row) { return (Row & 1) != 0; }
	static int Wrap(int Col);
	static double ColumnOf(int Row, int Col) { return Col + (Shifted(Row) ? 0.5 : 0.0); }
	static FLanternPoint CenterOf(int Row, int Col) { return { ColumnOf(Row, Col), Row * LanternBurst::RowHeight }; }
	/** X apart the short way round, in [-Columns/2, Columns/2]. */
	static double WrapDelta(double DX);
	static void Neighbours(int Row, int Col, std::vector<FLanternCell>& Out);
	static int Distance(FLanternCell A, FLanternCell B);
	int OrbCount() const;
	int LowestRow() const;   // -1 when the lantern is empty
	bool HasColor(int Color) const;
	bool CanPlace(int Row, int Col) const;
	std::vector<FLanternCell> Frontier() const;

	// The hand.
	int Current() const { return HandCurrent; }
	int Next() const { return HandNext; }
	void Swap();

	// Shooting.
	/** Where the launcher is, below the embers: Y on the unrolled lantern. */
	double LauncherY() const { return (LineRow() + 1) * LanternBurst::RowHeight; }
	/** Where a shot from LaunchColumn at Angle (radians off straight up, + toward higher X)
	    would stick, and the way it goes. Angle is clamped to MaxAimAngle. */
	FLanternTrace Trace(double LaunchColumn, double Angle, bool bWithPath = true) const;

	struct FPreview
	{
		bool bLegal = false;
		FLanternCell Cell;
		int Burst = 0;       // orbs the shot would burst (or burn)
		int Fall = 0;        // and bring down
		bool bOverLine = false;
		bool bClears = false;
	};
	/** What the loaded orb would do - played on a copy. */
	FPreview Preview(double LaunchColumn, double Angle) const;
	FPreview PreviewAt(int Row, int Col) const;

	/** Fires the loaded orb. False, and nothing changes, if the game is not being played. */
	bool Shoot(double LaunchColumn, double Angle);
	/** Puts the loaded orb straight into (Row, Col) - what a shot does once it has stuck; for the
	    tests and the dev commands. False, and nothing changes, if it cannot go there. */
	bool PlaceAt(int Row, int Col);

	std::vector<FLanternEvent> TakeEvents();

	// For the tests and the actor's dev commands: lay out a lantern by hand.
	void ClearLantern();
	void SetCell(int Row, int Col, int Color);
	void SetHand(int Current, int Next);

private:
	uint32_t NextRandom();
	int RandomBelow(int N);
	bool Chance(double P);

	int& Cell(int Row, int Col);
	void HangStage();
	std::vector<FLanternCell> GroupOf(int Row, int Col) const;
	std::vector<FLanternOrb> RemoveUnhung();
	FLanternOrb OrbAt(int Row, int Col, int Ring) const;
	int Draw();
	void RefreshHand();
	void Push(ELanternEvent Type, int Index = 0, long long Value = 0, std::vector<FLanternOrb> Orbs = {});

	std::vector<int> Cells;          // GridRows x Columns, row major
	ELanternPhase CurrentPhase = ELanternPhase::Ready;
	int CurrentStage = 0;
	long long CurrentScore = 0;
	int CurrentStreak = 0;
	int MissesLeft = 0;
	int LoweredRows = 0;
	int ShotCount = 0;
	int HandCurrent = 0;
	int HandNext = 0;
	uint32_t Rng = 1;
	std::vector<FLanternEvent> Events;
};
