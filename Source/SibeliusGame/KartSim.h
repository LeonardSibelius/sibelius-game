// KartSim.h
//
// THE TRANSHUMAN GRAND PRIX — a kart race through the streets of Transhuman City, Leonard
// against the five agents (Walt, 2026-10-09: "a Mario Brothers type of game for the living
// room... Mario Kart was the one we played a lot"). Our own race, not Nintendo's: the karts,
// the track and the items are this game's - the items are its powers.
//
// ===========================================================================
// THE HOUSE METHOD (FPinballSim, FBowlingSim, FChessGame): a pure model, standard library
// only, deterministic, tested standalone by Tools/Tests/run_kart_sim_tests.ps1. AKartRace
// only presents it: it reads the karts' places, draws them on the street, and feeds in
// what the player presses.
//
// THE TRACK IS A CORRIDOR, NOT THE CITY'S COLLISION. The city's streets meet at right
// angles, so the track is a closed polyline through the middle of the streets with a
// circular fillet at every corner, and a half-width either side. The model keeps every
// kart inside it, which is what makes a race testable without Unreal and a kart unable to
// end up inside a building. A fillet's radius must exceed the half-width, or the inside
// edge of the corner folds over itself; Build refuses a track like that.
//
// THE HANDLING is arcade, the kind a family can pick up: the throttle pushes along the
// kart's heading, the steering turns the heading, and the tyres' grip pulls the velocity
// round after it - up to a limit, past which the kart slides wide. Holding drift through a
// corner loosens the grip and charges a boost, released when he lets go: the longer the
// drift, the bigger the boost.
//
// THE OTHER DRIVERS follow the middle of the road with a lane each, look ahead for the next
// corner and brake for it in time - the corner's speed is set by its radius and their
// grip - and keep the race close: a little quicker when far behind the player, a little
// slower when far ahead.
//
// THE ITEMS are the game's powers. Sauce of All Knowledge: a boost. Slap: spins out the
// nearest kart beside or ahead. Generate: drops an obstacle on the road behind. Refactor:
// turns the kart just ahead into a treasure chest for a few seconds - heavy, slow, and
// unable to use an item. Item boxes along the road hand them out, kinder to whoever is last.
//
// EVERY RACE ENDS: when everyone has finished, or a while after the player has, or at the
// time limit, whichever comes first.
//
// Units: centimetres, seconds, radians. X and Y are the street plane; heights are the
// table actor's business.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FKartVec
{
	double X = 0.0;
	double Y = 0.0;

	FKartVec() = default;
	FKartVec(double InX, double InY) : X(InX), Y(InY) {}

	FKartVec operator+(const FKartVec& O) const { return FKartVec(X + O.X, Y + O.Y); }
	FKartVec operator-(const FKartVec& O) const { return FKartVec(X - O.X, Y - O.Y); }
	FKartVec operator*(double S) const { return FKartVec(X * S, Y * S); }
	FKartVec& operator+=(const FKartVec& O) { X += O.X; Y += O.Y; return *this; }
	FKartVec& operator-=(const FKartVec& O) { X -= O.X; Y -= O.Y; return *this; }
	double Dot(const FKartVec& O) const { return X * O.X + Y * O.Y; }
	double Cross(const FKartVec& O) const { return X * O.Y - Y * O.X; }
	double Length() const;
	FKartVec Normalised() const;
	FKartVec Left() const { return FKartVec(-Y, X); }   // a quarter turn anticlockwise
};

// One corner of the track's centre line: where it is, and how the corridor turns there.
struct FKartTrackPoint
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;                 // carried for the actor; the model ignores it
	// The road's half-width here; 0 = the track's own. Along a straight it changes evenly
	// from one corner's to the next, so a narrowing is a pair of extra points in a straight
	// line: the road tapers in to the first and out from the second.
	double HalfWidth = 0.0;
};

class FKartTrack
{
public:
	// Builds a closed track through Corners (the last joins the first): straight between
	// them, a circular arc of FilletRadius at each, the whole sampled every Step cm. False,
	// with Error saying why, for fewer than three corners, a fillet that does not fit its
	// two straights, or a fillet no wider than the road at its corner. A point that does not
	// turn (a narrowing's) needs no fillet, so its width is free.
	bool Build(const std::vector<FKartTrackPoint>& Corners, double HalfWidth, double FilletRadius,
		double Step = 150.0, std::string* Error = nullptr);

	bool IsBuilt() const { return Len > 0.0; }
	double Length() const { return Len; }
	double HalfWidth() const { return Half; }        // the widest anywhere
	double HalfWidthAt(double S) const;              // the road's half-width at S
	double MinRadius() const { return MinR; }

	double Wrap(double S) const;                    // into [0, Length)
	double Gap(double From, double To) const;       // To - From around the loop, in (-L/2, L/2]

	FKartVec PointAt(double S) const;
	FKartVec TangentAt(double S) const;             // unit, the direction of travel
	double CurvatureAt(double S) const;             // 1 / radius, + turning left, - right
	double HeightAt(double S) const;                // the corners' Z, blended along

	// Where P sits: distance along the centre line, offset to the left of it, and the sample
	// segment it is nearest. Hint, a segment from last time, makes it a local search.
	struct FFrame
	{
		double S = 0.0;
		double D = 0.0;
		int Segment = 0;
		FKartVec Tangent;
	};
	FFrame Locate(const FKartVec& P, int Hint = -1) const;

	const std::vector<FKartVec>& Samples() const { return Pts; }

private:
	std::vector<FKartVec> Pts;      // the centre line, every Step cm, closed (no repeat of [0])
	std::vector<double> Cum;        // distance along to each sample
	std::vector<double> Curv;       // curvature at each sample
	std::vector<double> Heights;
	std::vector<double> Widths;       // half-width at each sample
	double Len = 0.0;
	double Half = 0.0;
	double MinR = 0.0;

	int SegmentAt(double S) const;
	double ProjectOnSegment(int I, const FKartVec& P, double& T) const;
};

enum class EKartItem : uint8_t
{
	None,
	Sauce,          // a boost
	Slap,           // spins out the nearest kart beside or ahead
	Generate,       // an obstacle dropped behind
	Refactor,       // the kart ahead becomes a treasure chest for a while
};

const char* KartItemName(EKartItem Item);

struct FKartInput
{
	double Throttle = 0.0;          // -1 brake / reverse .. +1 full
	double Steer = 0.0;             // -1 right .. +1 left
	bool bDrift = false;            // held
	bool bUseItem = false;          // pressed this frame
};

struct FKartParams
{
	double MaxSpeed = 2600.0;           // cm/s: about 94 km/h
	double MaxReverse = 600.0;
	double Accel = 1500.0;
	double Brake = 3200.0;
	double Coast = 220.0;               // what the road takes when nothing is pressed
	double MaxYawRate = 2.4;            // rad/s at a walking pace, falling with speed
	double SteerFalloff = 3200.0;       // the speed at which the steering is half as sharp
	double Grip = 11.0;                 // how fast sideways sliding dies away, per second
	double MaxLateral = 2300.0;         // the most the tyres can pull, cm/s^2
	double DriftGrip = 2.6;
	double DriftLateral = 1100.0;
	double DriftYawBoost = 1.45;
	double DriftMinSpeed = 900.0;
	double MiniBoostCharge = 0.9;       // seconds of drift for the small boost
	double SuperBoostCharge = 1.9;
	double BoostSpeed = 1.32;           // max speed while boosting, as a multiple
	double SauceSeconds = 1.5;
	double KartRadius = 95.0;
	double WallBounce = 0.25;
	double KartBounce = 0.35;
	double SpinSeconds = 1.1;
	double ChestSeconds = 2.8;
	double ChestSpeed = 0.45;
	double SlapRange = 750.0;
	double BoxRadius = 130.0;
	double BoxRespawn = 3.0;
	double ObstacleRadius = 80.0;
	double ObstacleSeconds = 25.0;
	double StepSeconds = 1.0 / 120.0;
};

struct FKartState
{
	FKartVec P;
	FKartVec V;
	double Heading = 0.0;
	double YawRate = 0.0;

	int Lap = -1;                       // laps completed; -1 on the grid, behind the line
	double S = 0.0;                     // along the track
	double D = 0.0;                     // left of the centre line
	int Segment = 0;
	double Progress = 0.0;              // Lap * Length + S: the race order
	bool bFinished = false;
	double FinishTime = 0.0;
	int Place = 0;                      // 1 = leading

	bool bDrifting = false;
	int DriftDir = 0;                   // +1 left, -1 right
	double DriftCharge = 0.0;
	double BoostLeft = 0.0;
	double SpinLeft = 0.0;
	double ChestLeft = 0.0;
	EKartItem Item = EKartItem::None;
	bool bWrongWay = false;
	double WallContact = 0.0;           // how hard it last met a wall, 0..1, this frame

	double Speed() const { return V.Length(); }
	double ForwardSpeed() const;
};

struct FKartBox
{
	double S = 0.0;
	double D = 0.0;
	FKartVec P;
	double RespawnLeft = 0.0;           // 0 = there to be taken
};

struct FKartObstacle
{
	FKartVec P;
	double Left = 0.0;                  // seconds until it goes
	int Owner = -1;
};

enum class EKartEvent : uint8_t
{
	Countdown,      // Value = 3, 2, 1
	Go,
	WallHit,        // Value = 0..1, how hard
	KartHit,        // Other = the kart met
	Pickup,         // Item
	UsedItem,       // Item; Other = the target, -1 for none
	Boosted,        // Value = seconds
	SpunOut,        // Other = who did it; Value 1 = slapped, 2 = hit an obstacle
	Refactored,     // Other = who did it
	Lap,            // Value = the lap now started, 2..Laps
	Finished,       // Value = place
	RaceOver,
};

struct FKartEventRecord
{
	EKartEvent Type = EKartEvent::Go;
	int Kart = -1;
	int Other = -1;
	double Value = 0.0;
	EKartItem Item = EKartItem::None;
};

enum class EKartPhase : uint8_t
{
	Idle,
	Countdown,
	Racing,
	Over,
};

struct FKartRaceSetup
{
	int Karts = 6;
	int Laps = 3;
	int Player = 5;                     // which kart he drives; -1 = all computer (tests)
	uint32_t Seed = 1;
	double AiPace = 0.95;               // the agents' speed, as a multiple of the karts' best
	double CountdownSeconds = 3.0;
	double TimeLimit = 480.0;
	double AfterPlayerFinish = 20.0;    // the race goes on this long once he is home
	double AfterOthersFinish = 45.0;    // and this long for him once all the agents are
	std::vector<double> Skill;          // per kart, a multiple on AiPace; empty = all 1
};

class FKartRace
{
public:
	FKartParams Params;

	// Lines the karts up two by two behind the start line, kart 0 at the front. False if
	// the track is not built or the setup makes no sense.
	bool Start(const FKartTrack& Track, const FKartRaceSetup& Setup);

	void SetInput(int Kart, const FKartInput& Input);    // the player's; the rest drive themselves
	void SetAiPace(double Pace) { Setup.AiPace = Pace; }
	void Advance(double Seconds);                        // in fixed steps

	EKartPhase Phase() const { return CurrentPhase; }
	double Clock() const { return RaceClock; }           // seconds since GO
	double CountdownLeft() const { return CountdownClock; }
	int Laps() const { return Setup.Laps; }
	const FKartTrack& Track() const { return TheTrack; }
	const std::vector<FKartState>& Karts() const { return State; }
	const std::vector<FKartBox>& Boxes() const { return BoxList; }
	const std::vector<FKartObstacle>& Obstacles() const { return ObstacleList; }
	std::vector<int> Standings() const;                  // kart indices, the leader first

	// What happened since the last TakeEvents: sounds and messages for the table.
	std::vector<FKartEventRecord> TakeEvents();

	// The agents' line, for Code Vision: the speed to be doing at S, and where to be.
	double CornerSpeedAt(double S) const;
	double TargetSpeedAt(double S, double Pace) const;

	// For tests, and nothing else: put a kart somewhere, or hand it an item.
	void Place(int Kart, const FKartVec& P, double Heading, const FKartVec& V);
	void Give(int Kart, EKartItem Item);

	// How fast this kart may go: the player 1, an agent its pace with the catch-up applied.
	double PaceOf(int Kart) const;
	bool IsDrivenBySim(int Kart) const;      // the agents, and the player once he is home

private:
	FKartTrack TheTrack;
	FKartRaceSetup Setup;
	std::vector<FKartState> State;
	std::vector<FKartInput> Inputs;
	std::vector<double> Lane;                // each agent's preferred offset
	std::vector<double> StuckClock;
	std::vector<double> StuckProgress;
	std::vector<double> RecoverLeft;
	std::vector<double> ItemHold;            // an agent waits a moment before using an item
	std::vector<double> PrevS;
	std::vector<int> BestLap;                // so a lap reversed over and driven again is not announced twice
	std::vector<FKartBox> BoxList;
	std::vector<FKartObstacle> ObstacleList;
	std::vector<FKartEventRecord> Events;
	EKartPhase CurrentPhase = EKartPhase::Idle;
	double CountdownClock = 0.0;
	double RaceClock = 0.0;
	double PlayerDoneClock = -1.0;
	double OthersDoneClock = -1.0;
	double Pending = 0.0;
	int LastCountdownShown = 0;
	uint64_t Rng = 1;

	void Step(double Dt);
	FKartInput Drive(int Kart);              // an agent's hands on the wheel
	void Physics(int Kart, const FKartInput& Input, double Dt);
	void KeepInside(int Kart);
	void Collide();
	void CountLaps(int Kart);
	void Items(int Kart, const FKartInput& Input, double Dt);
	void UseItem(int Kart);
	void SpinOut(int Kart, int By, double Cause);
	void Rank();
	EKartItem RollItem(int Place);
	double Random01();
	int NearestTarget(int Kart, bool bAheadOnly) const;
	int KartJustAhead(int Kart) const;
	void Emit(EKartEvent Type, int Kart, int Other = -1, double Value = 0.0, EKartItem Item = EKartItem::None);
};
