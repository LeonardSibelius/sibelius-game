// SpaceSim.h
//
// THE GRAND TOUR — the model for the space adventure through the front door of the house
// (Walt, 2026-10-02: "let's make it realistic, not some silly fight with space enemies").
// Leonard leaves his body at the door and goes as a mind running on a probe. The goal is
// the trip Voyager 2 made: slingshot past the giant planets, out to Neptune.
//
// ===========================================================================
// THE HOUSE METHOD (FPinballSim, FBilliardsSim, FBowlingSim, FChessPosition): a pure model,
// standard library only, deterministic, tested standalone by
// Tools/Tests/run_space_sim_tests.ps1. A level and a probe actor will only present it.
//
// WHERE THE PLANETS ARE. NASA JPL's "Keplerian Elements for Approximate Positions of the
// Major Planets" (E M Standish, ssd.jpl.nasa.gov/planets/approx_pos.html), Table 1: six
// elements and their rates for each planet, fitted to JPL's full ephemeris over 1800-2050
// AD. JPL's nominal errors are tens of arcseconds for most planets and 400-600" for
// Jupiter and Saturn, whose pull on each other a Kepler orbit cannot carry; the tests hold
// every planet to that table against JPL Horizons on five dates from 1850 to 2050. (Uranus
// and Neptune turn out to be fitted around the solar system's centre of mass, so the Sun's
// wobble is added back to them; SpaceSim.cpp says how that was found.) Outside 1800-2050
// the elements are extrapolated and drift. Nothing stops a player going there, and nothing
// here claims it is accurate.
//
// HOW THE PROBE MOVES. Real gravity, not a rail: the Sun and all eight planets pull on it
// at once (Newton, plus the "indirect" term, because the frame is centred on a Sun that
// the planets pull too), integrated by an adaptive Dormand-Prince 5(4) Runge-Kutta. The
// step stretches to days in the empty middle of the solar system and shrinks to seconds at
// a close flyby, so a slingshot is not an animation. It is what happens when the probe
// passes close behind a moving planet. The tests hold the integrator to exact two-body
// orbits and to the exact bend of a hyperbolic flyby, and fly Voyager 2 from its real
// September 1977 position (Horizons) out to Jupiter.
//
// THE PROBE has a dry mass, propellant and a specific impulse, and every burn is paid for
// by the rocket equation. That is the whole budget. Fuel is the scarce thing in space, and
// the planets are free fuel.
//
// FRAME AND UNITS. Heliocentric, mean ecliptic and equinox of J2000 (JPL's and Horizons'
// "ECLIPTIC / J2000"): +X toward the vernal equinox, +Z toward the ecliptic north pole, +Y
// completing a right-handed set. Kilometres, seconds, kilograms. Time is a Julian Date in
// TDB, the timescale the elements are fitted in; it runs about 69 s ahead of UTC, which no
// game will notice.
//
// "EARTH" is the Earth-Moon barycentre, as in the JPL table, and its gravity includes the
// Moon's mass. It sits 4,700 km from the centre of the Earth. The Moon itself is not
// modelled, nor is any moon: a probe passing Jupiter feels the mass of the whole Jovian
// system at Jupiter's centre, which is how a mission designer first sketches it too.
//
// NOT MODELLED: relativity, sunlight pressure, planets' oblateness, asteroids, Pluto (Table
// 1 stops at Neptune). None of them would change what a player can do.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

struct FSpaceVec
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;

	constexpr FSpaceVec() = default;
	constexpr FSpaceVec(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

	FSpaceVec& operator+=(FSpaceVec B) { X += B.X; Y += B.Y; Z += B.Z; return *this; }
	FSpaceVec& operator-=(FSpaceVec B) { X -= B.X; Y -= B.Y; Z -= B.Z; return *this; }
};

inline FSpaceVec operator+(FSpaceVec A, FSpaceVec B) { return FSpaceVec(A.X + B.X, A.Y + B.Y, A.Z + B.Z); }
inline FSpaceVec operator-(FSpaceVec A, FSpaceVec B) { return FSpaceVec(A.X - B.X, A.Y - B.Y, A.Z - B.Z); }
inline FSpaceVec operator-(FSpaceVec A) { return FSpaceVec(-A.X, -A.Y, -A.Z); }
inline FSpaceVec operator*(FSpaceVec A, double S) { return FSpaceVec(A.X * S, A.Y * S, A.Z * S); }
inline FSpaceVec operator*(double S, FSpaceVec A) { return FSpaceVec(A.X * S, A.Y * S, A.Z * S); }
inline FSpaceVec operator/(FSpaceVec A, double S) { return FSpaceVec(A.X / S, A.Y / S, A.Z / S); }

namespace SpaceMath
{
	double Dot(FSpaceVec A, FSpaceVec B);
	FSpaceVec Cross(FSpaceVec A, FSpaceVec B);
	double Length(FSpaceVec A);
	// The zero vector stays zero rather than becoming NaN.
	FSpaceVec Normalized(FSpaceVec A);
}

namespace SpaceConst
{
	constexpr double AuKm = 149597870.7;              // exact, IAU 2012
	constexpr double LightKmPerSecond = 299792.458;   // exact
	constexpr double SecondsPerDay = 86400.0;
	constexpr double DaysPerCentury = 36525.0;
	constexpr double J2000 = 2451545.0;               // 2000-01-01 12:00 TDB
	constexpr double StandardGravityKmPerS2 = 9.80665e-3;   // g0, for the rocket equation
}

enum class ESpaceBody : uint8_t
{
	Sun,
	Mercury,
	Venus,
	Earth,      // the Earth-Moon barycentre: see the header note
	Mars,
	Jupiter,
	Saturn,
	Uranus,
	Neptune,
};

constexpr int SpaceBodyCount = 9;   // the Sun and eight planets, indexed by ESpaceBody

inline int SpaceBodyIndex(ESpaceBody Body) { return static_cast<int>(Body); }
inline ESpaceBody SpaceBodyAt(int Index) { return static_cast<ESpaceBody>(Index); }

struct FSpaceBodyInfo
{
	const char* Name = "";
	double GM = 0.0;                    // km^3/s^2, JPL (ssd.jpl.nasa.gov/astro_par.html)
	double RadiusKm = 0.0;              // equatorial: the cloud tops, for a gas giant
	double MeanRadiusKm = 0.0;          // the sphere of the same volume, for drawing
	double SphereOfInfluenceKm = 0.0;   // a (m/M)^(2/5); 0 for the Sun
};

const FSpaceBodyInfo& SpaceBodyInfo(ESpaceBody Body);

// Every body's position at one moment, indexed by ESpaceBody. The Sun is always at zero.
struct FSpaceBodyPositions
{
	FSpaceVec Of[SpaceBodyCount];

	const FSpaceVec& operator[](ESpaceBody Body) const { return Of[SpaceBodyIndex(Body)]; }
};

struct FSpaceState
{
	FSpaceVec Position;   // km
	FSpaceVec Velocity;   // km/s
};

// Every body's position and velocity at one moment. The Sun is always at rest at zero.
struct FSpaceBodyStates
{
	FSpaceState Of[SpaceBodyCount];

	const FSpaceState& operator[](ESpaceBody Body) const { return Of[SpaceBodyIndex(Body)]; }
};

inline FSpaceState operator-(const FSpaceState& A, const FSpaceState& B)
{
	return FSpaceState{ A.Position - B.Position, A.Velocity - B.Velocity };
}

// ---------------------------------------------------------------------------
// TIME

namespace SpaceTime
{
	// Gregorian calendar to Julian Date (Meeus, Astronomical Algorithms, ch. 7).
	// 2000-01-01 12:00 is 2451545.0.
	double JulianDate(int Year, int Month, int Day, double Hours = 0.0);

	struct FCalendar
	{
		int Year = 2000;
		int Month = 1;
		int Day = 1;
		int Hour = 0;
		int Minute = 0;
		double Second = 0.0;
	};

	FCalendar CalendarFromJulian(double Jd);
}

// ---------------------------------------------------------------------------
// TWO-BODY ORBITS: the conic a body follows when only one other body pulls on it.

namespace SpaceKepler
{
	// E from M = E - e sin E, for 0 <= e < 1. Radians. Converges for every e < 1.
	double EccentricAnomaly(double MeanAnomaly, double Eccentricity);

	// H from M = e sinh H - H, for e > 1. Radians.
	double HyperbolicAnomaly(double MeanAnomaly, double Eccentricity);
}

struct FSpaceElements
{
	double SemiMajorAxisKm = 0.0;   // negative for a hyperbola
	double Eccentricity = 0.0;
	double InclinationRad = 0.0;
	double AscendingNodeRad = 0.0;
	double ArgPeriapsisRad = 0.0;
	double MeanAnomalyRad = 0.0;
};

// What a state means as an orbit around one body: the numbers a mission planner reads.
struct FSpaceConic
{
	FSpaceElements Elements;
	double TrueAnomalyRad = 0.0;
	double PeriapsisKm = 0.0;
	double ApoapsisKm = 0.0;        // infinity for an open orbit
	double PeriodSeconds = 0.0;     // 0 for an open orbit
	double SpecificEnergy = 0.0;    // km^2/s^2: negative bound, positive escaping
	double ExcessSpeed = 0.0;       // v-infinity, km/s, for an open orbit; else 0
	bool bBound = true;
};

namespace SpaceConic
{
	// Relative state (body to probe) -> the orbit around a body of gravity GM.
	// A circular orbit takes its argument of periapsis as 0 and an equatorial one its node
	// as 0, so the angles stay defined; the state round-trips either way.
	FSpaceConic FromState(const FSpaceState& Relative, double GM);

	FSpaceState StateFromElements(const FSpaceElements& Elements, double GM);

	// The exact two-body motion: where Relative is after Seconds (either sign). Not for a
	// near-parabolic orbit (e within about 1e-8 of 1), where the elements break down.
	FSpaceState Propagate(const FSpaceState& Relative, double GM, double Seconds);
}

// ---------------------------------------------------------------------------
// WHERE THE PLANETS ARE (JPL Table 1). The Sun is the origin.

namespace SpaceEphemeris
{
	// The planet's heliocentric elements at Jd (Sun: all zero). Elliptic, a in km.
	FSpaceElements PlanetElements(ESpaceBody Planet, double Jd);

	FSpaceVec PlanetPosition(ESpaceBody Planet, double Jd);

	// Position and velocity. The velocity is the exact derivative of PlanetPosition,
	// including the slow turning of the orbit itself.
	FSpaceState PlanetState(ESpaceBody Planet, double Jd);

	// All of them at once: cheaper than one at a time, because Uranus and Neptune need the
	// others (see SpaceSim.cpp, "URANUS AND NEPTUNE").
	FSpaceBodyPositions AllPositions(double Jd);
	FSpaceBodyStates AllStates(double Jd);
}

namespace SpaceGravity
{
	// Acceleration of a massless probe at Position (heliocentric), km/s^2: the Sun, every
	// planet, and the indirect term for the planets' pull on the Sun.
	FSpaceVec Acceleration(const FSpaceVec& Position, const FSpaceBodyPositions& Bodies);
}

// ---------------------------------------------------------------------------
// THE INTEGRATOR: Dormand-Prince 5(4) with step-size control, the method behind MATLAB's
// ode45. Generic over the force so the tests can fly it past a lone planet.

// Acceleration (km/s^2) at a time (seconds from the integrator's epoch) and a position.
using FSpaceAccelFn = std::function<FSpaceVec(double Seconds, const FSpaceVec& Position)>;

struct FSpaceIntegratorSettings
{
	double RelTolerance = 1e-11;
	double AbsTolKm = 1e-4;           // 10 cm
	double AbsTolKmPerS = 1e-10;      // 0.1 micrometre a second
	double MinStepSeconds = 1e-3;
	double MaxStepSeconds = 2.0 * SpaceConst::SecondsPerDay;
};

class FSpaceIntegrator
{
public:
	FSpaceIntegratorSettings Settings;

	// One accepted step, of at most MaxSeconds (> 0). Moves State and Seconds forward and
	// returns the step taken. Retries smaller steps internally until the error estimate
	// passes; a step already at MinStepSeconds is accepted regardless.
	double Step(FSpaceState& State, double& Seconds, double MaxSeconds, const FSpaceAccelFn& Accel);

	// Steps until Seconds reaches Until (> Seconds) or MaxSteps is used. Returns the steps.
	int AdvanceTo(FSpaceState& State, double& Seconds, double Until, const FSpaceAccelFn& Accel, int MaxSteps = 1000000);

	// Forgets the step size learned so far, for a state that jumped (a burn, a reset).
	void Restart() { NextStep = 0.0; }

private:
	double NextStep = 0.0;
};

// ---------------------------------------------------------------------------
// THE VOYAGE (Walt, 2026-10-02: "go with 1977, and let the player know the voyage begins on
// that date"). The front door opens on the day Voyager 2 left Earth, 20 August 1977, 14:29
// UTC. That summer the four giant planets stood where one probe could swing from each to
// the next, an alignment that comes round about once in 175 years; today's sky does not
// allow it. The level tells the player the date as the voyage begins.

namespace SpaceGrandTour
{
	constexpr int StartYear = 1977;
	constexpr int StartMonth = 8;
	constexpr int StartDay = 20;
	constexpr double StartHours = 14.0 + 29.0 / 60.0;   // UTC; TDB is under a minute ahead

	inline double StartJd() { return SpaceTime::JulianDate(StartYear, StartMonth, StartDay, StartHours); }
}

// ---------------------------------------------------------------------------
// THE PROBE AND ITS FLIGHT

struct FSpaceCraft
{
	double DryMassKg = 500.0;
	double PropellantKg = 100.0;
	double IspSeconds = 230.0;        // hydrazine monopropellant, Voyager's kind

	double MassKg() const { return DryMassKg + PropellantKg; }
	double ExhaustSpeedKmPerS() const { return IspSeconds * SpaceConst::StandardGravityKmPerS2; }
	// The rocket equation: all the speed change the propellant left can still buy.
	double DeltaVRemaining() const;
};

enum class ESpaceEvent : uint8_t
{
	Burn,               // Value = delta-v applied, km/s; Value2 = propellant spent, kg
	EnteredSphere,      // Body: the probe crossed into its sphere of influence
	LeftSphere,
	ClosestApproach,    // Body; Value = periapsis distance from its centre, km; Value2 = speed there relative to it, km/s
	Crashed,            // Body
	Escaping,           // the probe is now fast enough to leave the Sun for good; Value = v-infinity, km/s
};

struct FSpaceEventRecord
{
	ESpaceEvent Type = ESpaceEvent::Burn;
	ESpaceBody Body = ESpaceBody::Sun;
	double Jd = 0.0;
	double Value = 0.0;
	double Value2 = 0.0;
};

// Unit vectors for steering a burn, relative to one body: along the motion, out of the
// orbit's plane, and outward in it. Prograde x Normal = Radial.
struct FSpaceBurnFrame
{
	FSpaceVec Prograde;
	FSpaceVec Normal;
	FSpaceVec Radial;
};

struct FSpacePathPoint
{
	double Jd = 0.0;
	FSpaceVec Position;   // heliocentric
};

class FSpaceSim
{
public:
	FSpaceIntegratorSettings& IntegratorSettings() { return Integrator.Settings; }

	void Reset(double StartJd, const FSpaceState& StartProbe, const FSpaceCraft& StartCraft);

	// Starts the probe already free of Earth: DistanceKm out along the departure direction,
	// heading away with the speed that leaves exactly VInfinity (km/s) once Earth's pull is
	// spent. The way a launch is specified: by its v-infinity, the speed beyond Earth.
	void LaunchFromEarth(double StartJd, const FSpaceVec& VInfinity, const FSpaceCraft& StartCraft, double DistanceKm = 2.0e6);

	double Jd() const;
	const FSpaceState& Probe() const { return ProbeState; }
	const FSpaceCraft& Craft() const { return CraftState; }
	bool HasCrashed() const { return bCrashed; }
	ESpaceBody CrashedInto() const { return CrashBody; }

	// Moves the sim forward by Seconds (>= 0) of flight time, in as many steps as gravity
	// asks for. Returns the seconds actually flown: less if MaxSteps ran out (a very close
	// flyby at a very high time warp) or the probe crashed.
	double Advance(double Seconds, int MaxSteps = 200000);

	// An impulsive burn. Returns the delta-v actually applied (km/s): all of it, or as much
	// as the propellant left can buy, in the same direction.
	double Burn(const FSpaceVec& DeltaV);

	FSpaceBurnFrame BurnFrame(ESpaceBody Body = ESpaceBody::Sun) const;

	FSpaceVec BodyPosition(ESpaceBody Body) const;
	FSpaceState BodyState(ESpaceBody Body) const;
	FSpaceState RelativeTo(ESpaceBody Body) const;   // the probe as seen from the body
	double DistanceTo(ESpaceBody Body) const;

	// The planet whose sphere of influence holds the probe, or the Sun.
	ESpaceBody DominantBody() const;

	// How old the news is: the one-way light time between the probe and Earth.
	double LightSecondsToEarth() const;

	// Events since the last call, oldest first.
	std::vector<FSpaceEventRecord> TakeEvents();

	// Flies a copy forward and records where it goes, a point every SampleSeconds, for the
	// line drawn ahead of the probe. Stops early on a crash. The sim itself is untouched,
	// so a worker thread can run this on a copy while the game draws.
	void Predict(double Seconds, double SampleSeconds, std::vector<FSpacePathPoint>& Out, int MaxSteps = 200000) const;

private:
	FSpaceAccelFn GravityFn() const;
	// One integrator step of at most MaxSeconds, then the event checks. Returns the step.
	double StepOnce(double MaxSeconds, const FSpaceAccelFn& Accel);
	// The longest step that cannot carry the probe past a planet unseen.
	double SafeStepSeconds() const;
	void UpdateEvents(bool bEmit);
	void Emit(ESpaceEvent Type, ESpaceBody Body, double Value = 0.0, double Value2 = 0.0);

	FSpaceIntegrator Integrator;
	double EpochJd = SpaceConst::J2000;
	double Seconds = 0.0;   // since EpochJd
	FSpaceState ProbeState;
	FSpaceCraft CraftState;
	bool bCrashed = false;
	ESpaceBody CrashBody = ESpaceBody::Sun;
	bool bInSphere[SpaceBodyCount] = {};
	bool bApproaching[SpaceBodyCount] = {};
	bool bEscaping = false;
	std::vector<FSpaceEventRecord> Events;
};
