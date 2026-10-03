// SpaceSim.cpp — see SpaceSim.h for what this is and why it can be trusted.

#include "SpaceSim.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace SpaceSimDetail
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double TwoPi = 2.0 * Pi;
	constexpr double DegToRad = Pi / 180.0;
	constexpr double GMSun = 1.32712440041279419e11;   // km^3/s^2, JPL
	constexpr double Infinity = std::numeric_limits<double>::infinity();

	// JPL "Keplerian Elements for Approximate Positions of the Major Planets", Table 1,
	// valid 1800-2050 AD, copied from ssd.jpl.nasa.gov/planets/approx_pos.html on
	// 2026-10-02. Mean ecliptic and equinox of J2000. Per row: the value at J2000, then its
	// rate per Julian century. a in au, e plain, the four angles in degrees. Indexed by
	// ESpaceBody; the Sun's row is unused.
	struct FPlanetRow
	{
		double A, E, I, L, LongPeri, LongNode;
		double DA, DE, DI, DL, DLongPeri, DLongNode;
	};

	const FPlanetRow PlanetRows[SpaceBodyCount] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,          // Mercury
		  0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081 },
		{ 0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,         // Venus
		  0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418 },
		{ 1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,                // EM Bary
		  0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0 },
		{ 1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,          // Mars
		  0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343 },
		{ 5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,          // Jupiter
		  -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106 },
		{ 9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,          // Saturn
		  -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794 },
		{ 19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503,        // Uranus
		  -0.00196176, -0.00004397, -0.00242939, 428.48202785, 0.40805281, 0.04240589 },
		{ 30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574,        // Neptune
		  0.00026291, 0.00005105, 0.00035372, 218.45945325, -0.32241464, -0.00508664 },
	};

	// GM from ssd.jpl.nasa.gov/astro_par.html; radii from ssd.jpl.nasa.gov/planets/
	// phys_par.html (both 2026-10-02). Systems where a planet has moons: the GM is the
	// whole system's, the radius the planet's. Earth's GM is the Earth's plus the Moon's.
	// The Sun's radius is the IAU 2015 nominal value.
	FSpaceBodyInfo MakeInfo(const char* Name, double GM, double Radius, double MeanRadius, int Row)
	{
		FSpaceBodyInfo Info;
		Info.Name = Name;
		Info.GM = GM;
		Info.RadiusKm = Radius;
		Info.MeanRadiusKm = MeanRadius;
		Info.SphereOfInfluenceKm = Row > 0 ? PlanetRows[Row].A * SpaceConst::AuKm * std::pow(GM / GMSun, 0.4) : 0.0;
		return Info;
	}

	struct FInfoTable
	{
		FSpaceBodyInfo Of[SpaceBodyCount];

		FInfoTable()
		{
			Of[0] = MakeInfo("Sun", GMSun, 695700.0, 695700.0, 0);
			Of[1] = MakeInfo("Mercury", 22031.868551, 2440.53, 2439.4, 1);
			Of[2] = MakeInfo("Venus", 324858.592, 6051.8, 6051.8, 2);
			Of[3] = MakeInfo("Earth", 398600.435507 + 4902.800118, 6378.1366, 6371.0084, 3);
			Of[4] = MakeInfo("Mars", 42828.375816, 3396.19, 3389.50, 4);
			Of[5] = MakeInfo("Jupiter", 126712764.1, 71492.0, 69911.0, 5);
			Of[6] = MakeInfo("Saturn", 37940584.8418, 60268.0, 58232.0, 6);
			Of[7] = MakeInfo("Uranus", 5794556.4, 25559.0, 25362.0, 7);
			Of[8] = MakeInfo("Neptune", 6836527.10058, 24764.0, 24622.0, 8);
		}
	};

	const FInfoTable& InfoTable()
	{
		static const FInfoTable Table;   // built once, thread-safe since C++11
		return Table;
	}

	double WrapPi(double Angle)
	{
		double A = std::fmod(Angle + Pi, TwoPi);
		if (A < 0.0)
		{
			A += TwoPi;
		}
		return A - Pi;
	}

	// From an orbit's own plane (x toward periapsis) to the ecliptic: Rz(Node) Rx(Inc)
	// Rz(ArgPeri), written out as JPL's approx_pos page writes it.
	FSpaceVec FromPerifocal(double X, double Y, double Node, double Inc, double ArgPeri)
	{
		const double CW = std::cos(ArgPeri), SW = std::sin(ArgPeri);
		const double CN = std::cos(Node), SN = std::sin(Node);
		const double CI = std::cos(Inc), SI = std::sin(Inc);
		return FSpaceVec(
			(CW * CN - SW * SN * CI) * X + (-SW * CN - CW * SN * CI) * Y,
			(CW * SN + SW * CN * CI) * X + (-SW * SN + CW * CN * CI) * Y,
			(SW * SI) * X + (CW * SI) * Y);
	}

	// The angle from A to B, both in the plane whose normal is N, signed by N.
	double PlaneAngle(FSpaceVec A, FSpaceVec B, FSpaceVec N)
	{
		return std::atan2(SpaceMath::Dot(SpaceMath::Cross(A, B), N), SpaceMath::Dot(A, B));
	}


	// Dormand-Prince 5(4) coefficients (Dormand & Prince 1980; Hairer, Norsett & Wanner,
	// Solving ODEs I, table 5.2).
	constexpr double C2 = 1.0 / 5.0, C3 = 3.0 / 10.0, C4 = 4.0 / 5.0, C5 = 8.0 / 9.0;
	constexpr double A21 = 1.0 / 5.0;
	constexpr double A31 = 3.0 / 40.0, A32 = 9.0 / 40.0;
	constexpr double A41 = 44.0 / 45.0, A42 = -56.0 / 15.0, A43 = 32.0 / 9.0;
	constexpr double A51 = 19372.0 / 6561.0, A52 = -25360.0 / 2187.0, A53 = 64448.0 / 6561.0, A54 = -212.0 / 729.0;
	constexpr double A61 = 9017.0 / 3168.0, A62 = -355.0 / 33.0, A63 = 46732.0 / 5247.0, A64 = 49.0 / 176.0, A65 = -5103.0 / 18656.0;
	constexpr double B1 = 35.0 / 384.0, B3 = 500.0 / 1113.0, B4 = 125.0 / 192.0, B5 = -2187.0 / 6784.0, B6 = 11.0 / 84.0;
	// Fifth-order minus fourth-order weights: the error estimate.
	constexpr double E1 = 71.0 / 57600.0, E3 = -71.0 / 16695.0, E4 = 71.0 / 1920.0, E5 = -17253.0 / 339200.0, E6 = 22.0 / 525.0, E7 = -1.0 / 40.0;

	// The state's derivative: d(position)/dt = velocity, d(velocity)/dt = gravity.
	struct FDeriv
	{
		FSpaceVec DPos;
		FSpaceVec DVel;
	};

	FDeriv Derivative(const FSpaceAccelFn& Accel, double T, const FSpaceVec& Pos, const FSpaceVec& Vel)
	{
		return FDeriv{ Vel, Accel(T, Pos) };
	}

	double ComponentError(double Err, double Y0, double Y1, double Abs, double Rel)
	{
		return std::fabs(Err) / (Abs + Rel * (std::max)(std::fabs(Y0), std::fabs(Y1)));
	}
}

// ---------------------------------------------------------------------------

namespace SpaceMath
{
	double Dot(FSpaceVec A, FSpaceVec B)
	{
		return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
	}

	FSpaceVec Cross(FSpaceVec A, FSpaceVec B)
	{
		return FSpaceVec(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}

	double Length(FSpaceVec A)
	{
		return std::sqrt(Dot(A, A));
	}

	FSpaceVec Normalized(FSpaceVec A)
	{
		const double L = Length(A);
		return L > 0.0 ? A / L : FSpaceVec();
	}
}

const FSpaceBodyInfo& SpaceBodyInfo(ESpaceBody Body)
{
	return SpaceSimDetail::InfoTable().Of[SpaceBodyIndex(Body)];
}

// ---------------------------------------------------------------------------
// TIME

namespace SpaceTime
{
	double JulianDate(int Year, int Month, int Day, double Hours)
	{
		int Y = Year;
		int M = Month;
		if (M <= 2)
		{
			Y -= 1;
			M += 12;
		}
		const double A = std::floor(Y / 100.0);
		const double B = 2.0 - A + std::floor(A / 4.0);
		return std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + Day + B - 1524.5 + Hours / 24.0;
	}

	FCalendar CalendarFromJulian(double Jd)
	{
		const double Z = std::floor(Jd + 0.5);
		const double F = Jd + 0.5 - Z;
		double A = Z;
		if (Z >= 2299161.0)
		{
			const double Alpha = std::floor((Z - 1867216.25) / 36524.25);
			A = Z + 1.0 + Alpha - std::floor(Alpha / 4.0);
		}
		const double B = A + 1524.0;
		const double C = std::floor((B - 122.1) / 365.25);
		const double D = std::floor(365.25 * C);
		const double E = std::floor((B - D) / 30.6001);

		FCalendar Out;
		const double DayWithFraction = B - D - std::floor(30.6001 * E) + F;
		Out.Day = static_cast<int>(std::floor(DayWithFraction));
		Out.Month = static_cast<int>(E < 14.0 ? E - 1.0 : E - 13.0);
		Out.Year = static_cast<int>(Out.Month > 2 ? C - 4716.0 : C - 4715.0);

		// The time of day to the nearest millisecond, so 14:29 does not come back as
		// 14:28:59.99999 (a Julian Date near 2.4 million holds about 40 microseconds).
		const long long Ms = (std::min)(86399999LL, static_cast<long long>(std::llround((DayWithFraction - Out.Day) * 86400000.0)));
		Out.Hour = static_cast<int>(Ms / 3600000);
		Out.Minute = static_cast<int>((Ms / 60000) % 60);
		Out.Second = static_cast<double>(Ms % 60000) / 1000.0;
		return Out;
	}
}

// ---------------------------------------------------------------------------
// KEPLER

namespace SpaceKepler
{
	double EccentricAnomaly(double MeanAnomaly, double Eccentricity)
	{
		const double M = SpaceSimDetail::WrapPi(MeanAnomaly);
		const double Ecc = Eccentricity;
		// Danby's starting value, from which Newton converges for every e < 1.
		double E = M + 0.85 * Ecc * (M > 0.0 ? 1.0 : (M < 0.0 ? -1.0 : 0.0));
		for (int Iteration = 0; Iteration < 64; ++Iteration)
		{
			const double F = E - Ecc * std::sin(E) - M;
			const double DF = 1.0 - Ecc * std::cos(E);
			const double Step = F / DF;
			E -= Step;
			if (std::fabs(Step) < 1e-15 * (1.0 + std::fabs(E)))
			{
				break;
			}
		}
		return E;
	}

	double HyperbolicAnomaly(double MeanAnomaly, double Eccentricity)
	{
		const double M = MeanAnomaly;
		const double Ecc = Eccentricity;
		const double Sign = M < 0.0 ? -1.0 : 1.0;
		// e sinh H - H is convex for H > 0, and this start is close to the root for any M.
		double H = Sign * std::log(2.0 * std::fabs(M) / Ecc + 1.8);
		for (int Iteration = 0; Iteration < 200; ++Iteration)
		{
			const double F = Ecc * std::sinh(H) - H - M;
			const double DF = Ecc * std::cosh(H) - 1.0;
			const double Step = F / DF;
			H -= Step;
			if (std::fabs(Step) < 1e-15 * (1.0 + std::fabs(H)))
			{
				break;
			}
		}
		return H;
	}
}

// ---------------------------------------------------------------------------
// CONICS

namespace SpaceConic
{
	FSpaceConic FromState(const FSpaceState& Relative, double GM)
	{
		const FSpaceVec R = Relative.Position;
		const FSpaceVec V = Relative.Velocity;
		const double RLen = SpaceMath::Length(R);
		const double V2 = SpaceMath::Dot(V, V);

		const FSpaceVec H = SpaceMath::Cross(R, V);
		const double HLen = SpaceMath::Length(H);

		FSpaceConic Out;
		Out.SpecificEnergy = 0.5 * V2 - GM / RLen;

		if (HLen <= 1e-12 * RLen * std::sqrt(V2))
		{
			// Straight toward or away from the body (a launch is set up this way): a
			// degenerate conic with no plane and a periapsis at the centre.
			Out.Elements.Eccentricity = 1.0;
			Out.Elements.SemiMajorAxisKm = Out.SpecificEnergy != 0.0 ? -GM / (2.0 * Out.SpecificEnergy) : SpaceSimDetail::Infinity;
			Out.PeriapsisKm = 0.0;
			Out.bBound = Out.SpecificEnergy < 0.0;
			Out.ApoapsisKm = Out.bBound ? 2.0 * Out.Elements.SemiMajorAxisKm : SpaceSimDetail::Infinity;
			Out.ExcessSpeed = Out.bBound ? 0.0 : std::sqrt(2.0 * Out.SpecificEnergy);
			return Out;
		}

		const FSpaceVec HUnit = H / HLen;
		const FSpaceVec EVec = (R * (V2 - GM / RLen) - V * SpaceMath::Dot(R, V)) / GM;
		const double Ecc = SpaceMath::Length(EVec);
		Out.Elements.Eccentricity = Ecc;
		Out.Elements.InclinationRad = std::acos((std::max)(-1.0, (std::min)(1.0, H.Z / HLen)));

		// The node line points to where the orbit climbs through the ecliptic.
		const FSpaceVec NodeVec(-H.Y, H.X, 0.0);
		const double NodeLen = SpaceMath::Length(NodeVec);
		const bool bEquatorial = NodeLen < 1e-12 * HLen;
		const bool bCircular = Ecc < 1e-11;

		const FSpaceVec Ref = bEquatorial ? FSpaceVec(1.0, 0.0, 0.0) : NodeVec / NodeLen;
		Out.Elements.AscendingNodeRad = bEquatorial ? 0.0 : std::atan2(NodeVec.Y, NodeVec.X);
		Out.Elements.ArgPeriapsisRad = bCircular ? 0.0 : SpaceSimDetail::PlaneAngle(Ref, EVec, HUnit);

		const FSpaceVec PeriDir = bCircular ? Ref : EVec / Ecc;
		const double Nu = SpaceSimDetail::PlaneAngle(PeriDir, R, HUnit);
		Out.TrueAnomalyRad = Nu;

		const double P = HLen * HLen / GM;
		Out.PeriapsisKm = P / (1.0 + Ecc);
		Out.Elements.SemiMajorAxisKm = Out.SpecificEnergy != 0.0 ? -GM / (2.0 * Out.SpecificEnergy) : SpaceSimDetail::Infinity;

		if (Ecc < 1.0)
		{
			const double E = std::atan2(std::sqrt(1.0 - Ecc * Ecc) * std::sin(Nu), Ecc + std::cos(Nu));
			Out.Elements.MeanAnomalyRad = E - Ecc * std::sin(E);
			Out.ApoapsisKm = P / (1.0 - Ecc);
			const double A = Out.Elements.SemiMajorAxisKm;
			Out.PeriodSeconds = SpaceSimDetail::TwoPi * std::sqrt(A * A * A / GM);
			Out.bBound = true;
		}
		else
		{
			const double HAnom = std::asinh(std::sqrt(Ecc * Ecc - 1.0) * std::sin(Nu) / (1.0 + Ecc * std::cos(Nu)));
			Out.Elements.MeanAnomalyRad = Ecc * std::sinh(HAnom) - HAnom;
			Out.ApoapsisKm = SpaceSimDetail::Infinity;
			Out.PeriodSeconds = 0.0;
			Out.ExcessSpeed = std::sqrt((std::max)(0.0, 2.0 * Out.SpecificEnergy));
			Out.bBound = false;
		}
		return Out;
	}

	FSpaceState StateFromElements(const FSpaceElements& El, double GM)
	{
		const double Ecc = El.Eccentricity;
		const double P = El.SemiMajorAxisKm * (1.0 - Ecc * Ecc);
		double Nu = 0.0;
		if (Ecc < 1.0)
		{
			const double E = SpaceKepler::EccentricAnomaly(El.MeanAnomalyRad, Ecc);
			Nu = 2.0 * std::atan2(std::sqrt(1.0 + Ecc) * std::sin(0.5 * E), std::sqrt(1.0 - Ecc) * std::cos(0.5 * E));
		}
		else
		{
			const double HAnom = SpaceKepler::HyperbolicAnomaly(El.MeanAnomalyRad, Ecc);
			Nu = 2.0 * std::atan2(std::sqrt(Ecc + 1.0) * std::sinh(0.5 * HAnom), std::sqrt(Ecc - 1.0) * std::cosh(0.5 * HAnom));
		}

		const double CNu = std::cos(Nu), SNu = std::sin(Nu);
		const double RLen = P / (1.0 + Ecc * CNu);
		const double VScale = std::sqrt(GM / P);

		FSpaceState Out;
		Out.Position = SpaceSimDetail::FromPerifocal(RLen * CNu, RLen * SNu, El.AscendingNodeRad, El.InclinationRad, El.ArgPeriapsisRad);
		Out.Velocity = SpaceSimDetail::FromPerifocal(-VScale * SNu, VScale * (Ecc + CNu), El.AscendingNodeRad, El.InclinationRad, El.ArgPeriapsisRad);
		return Out;
	}

	FSpaceState Propagate(const FSpaceState& Relative, double GM, double Seconds)
	{
		const FSpaceConic Conic = FromState(Relative, GM);
		FSpaceElements El = Conic.Elements;
		const double A = std::fabs(El.SemiMajorAxisKm);
		El.MeanAnomalyRad += std::sqrt(GM / (A * A * A)) * Seconds;
		return StateFromElements(El, GM);
	}
}

// ---------------------------------------------------------------------------
// EPHEMERIS AND GRAVITY

namespace SpaceEphemeris
{
	FSpaceElements PlanetElements(ESpaceBody Planet, double Jd)
	{
		FSpaceElements El;
		const int Row = SpaceBodyIndex(Planet);
		if (Row <= 0 || Row >= SpaceBodyCount)
		{
			return El;
		}
		const SpaceSimDetail::FPlanetRow& R = SpaceSimDetail::PlanetRows[Row];
		const double T = (Jd - SpaceConst::J2000) / SpaceConst::DaysPerCentury;
		const double D = SpaceSimDetail::DegToRad;

		const double LongPeri = (R.LongPeri + R.DLongPeri * T) * D;
		const double LongNode = (R.LongNode + R.DLongNode * T) * D;
		El.SemiMajorAxisKm = (R.A + R.DA * T) * SpaceConst::AuKm;
		El.Eccentricity = R.E + R.DE * T;
		El.InclinationRad = (R.I + R.DI * T) * D;
		El.AscendingNodeRad = LongNode;
		El.ArgPeriapsisRad = LongPeri - LongNode;
		El.MeanAnomalyRad = SpaceSimDetail::WrapPi((R.L + R.DL * T) * D - LongPeri);
		return El;
	}

	// One planet straight from its elements, before the Sun's wobble (below). The velocity
	// is the full derivative: the planet moving along its orbit, plus the orbit itself
	// slowly turning and changing shape, which for Neptune is a part in a thousand of its
	// speed.
	FSpaceState RawState(int Row, double Jd, bool bWithVelocity)
	{
		FSpaceState Out;
		const SpaceSimDetail::FPlanetRow& R = SpaceSimDetail::PlanetRows[Row];
		const FSpaceElements El = PlanetElements(SpaceBodyAt(Row), Jd);
		const double Ecc = El.Eccentricity;
		const double A = El.SemiMajorAxisKm;
		const double E = SpaceKepler::EccentricAnomaly(El.MeanAnomalyRad, Ecc);
		const double CE = std::cos(E), SE = std::sin(E);
		const double Root = std::sqrt(1.0 - Ecc * Ecc);
		const double Node = El.AscendingNodeRad, Inc = El.InclinationRad, ArgPeri = El.ArgPeriapsisRad;

		Out.Position = SpaceSimDetail::FromPerifocal(A * (CE - Ecc), A * Root * SE, Node, Inc, ArgPeri);
		if (!bWithVelocity)
		{
			return Out;
		}

		// Rates per second.
		const double PerCentury = 1.0 / (SpaceConst::DaysPerCentury * SpaceConst::SecondsPerDay);
		const double D = SpaceSimDetail::DegToRad;
		const double ADot = R.DA * SpaceConst::AuKm * PerCentury;
		const double EccDot = R.DE * PerCentury;
		const double IncDot = R.DI * D * PerCentury;
		const double NodeDot = R.DLongNode * D * PerCentury;
		const double ArgPeriDot = (R.DLongPeri - R.DLongNode) * D * PerCentury;
		const double MDot = (R.DL - R.DLongPeri) * D * PerCentury;

		// Kepler's equation, differentiated: E moves with M and with e.
		const double OneMinus = 1.0 - Ecc * CE;
		const double EDot = MDot / OneMinus + EccDot * SE / OneMinus;
		const double XDot = ADot * (CE - Ecc) + A * (-SE * EDot - EccDot);
		const double YDot = ADot * Root * SE + A * (-Ecc * EccDot / Root * SE + Root * CE * EDot);
		Out.Velocity = SpaceSimDetail::FromPerifocal(XDot, YDot, Node, Inc, ArgPeri);

		// The orbit's frame turns: about the ecliptic pole with the node, about the node line
		// with the inclination, and about the orbit's own pole with the perihelion.
		const FSpaceVec NodeLine(std::cos(Node), std::sin(Node), 0.0);
		const FSpaceVec OrbitPole(std::sin(Inc) * std::sin(Node), -std::sin(Inc) * std::cos(Node), std::cos(Inc));
		const FSpaceVec Spin = FSpaceVec(0.0, 0.0, NodeDot) + NodeLine * IncDot + OrbitPole * ArgPeriDot;
		Out.Velocity += SpaceMath::Cross(Spin, Out.Position);
		return Out;
	}

	// URANUS AND NEPTUNE, measured, 2026-10-02. The table's elements put them where Horizons
	// puts them relative to the solar system's CENTRE OF MASS, not the Sun: Neptune misses the
	// Sun-centred position by up to 45" (JPL's nominal is 10") and the barycentric one by 11"
	// at most. That makes physical sense. Out there, a planet circles the Sun and Jupiter and
	// Saturn together, and the Sun itself circles their common centre by up to a solar
	// diameter. So the Sun's offset from the centre of mass, from all eight planets' masses,
	// is added back to those two. Jupiter and Saturn are fitted Sun-centred already: the same
	// correction makes them worse, so they do not get it.
	bool FollowsBarycentre(int Row)
	{
		return Row == SpaceBodyIndex(ESpaceBody::Uranus) || Row == SpaceBodyIndex(ESpaceBody::Neptune);
	}

	FSpaceBodyStates States(double Jd, bool bWithVelocity)
	{
		FSpaceBodyStates Out;
		FSpaceVec WeightedPos, WeightedVel;
		double TotalGM = SpaceBodyInfo(ESpaceBody::Sun).GM;
		for (int Row = 1; Row < SpaceBodyCount; ++Row)
		{
			Out.Of[Row] = RawState(Row, Jd, bWithVelocity);
			const double GM = SpaceBodyInfo(SpaceBodyAt(Row)).GM;
			WeightedPos += Out.Of[Row].Position * GM;
			WeightedVel += Out.Of[Row].Velocity * GM;
			TotalGM += GM;
		}
		// The Sun sits at -WeightedPos / TotalGM from the centre of mass; a position measured
		// from that centre is moved onto the Sun by adding WeightedPos / TotalGM.
		for (int Row = 1; Row < SpaceBodyCount; ++Row)
		{
			if (FollowsBarycentre(Row))
			{
				Out.Of[Row].Position += WeightedPos / TotalGM;
				Out.Of[Row].Velocity += WeightedVel / TotalGM;
			}
		}
		return Out;
	}

	FSpaceState PlanetState(ESpaceBody Planet, double Jd)
	{
		const int Row = SpaceBodyIndex(Planet);
		if (Row <= 0 || Row >= SpaceBodyCount)
		{
			return FSpaceState();
		}
		return FollowsBarycentre(Row) ? States(Jd, true).Of[Row] : RawState(Row, Jd, true);
	}

	FSpaceVec PlanetPosition(ESpaceBody Planet, double Jd)
	{
		const int Row = SpaceBodyIndex(Planet);
		if (Row <= 0 || Row >= SpaceBodyCount)
		{
			return FSpaceVec();
		}
		return FollowsBarycentre(Row) ? States(Jd, false).Of[Row].Position : RawState(Row, Jd, false).Position;
	}

	FSpaceBodyStates AllStates(double Jd)
	{
		return States(Jd, true);
	}

	FSpaceBodyPositions AllPositions(double Jd)
	{
		const FSpaceBodyStates All = States(Jd, false);
		FSpaceBodyPositions Out;
		for (int Row = 1; Row < SpaceBodyCount; ++Row)
		{
			Out.Of[Row] = All.Of[Row].Position;
		}
		return Out;
	}
}
namespace SpaceGravity
{
	FSpaceVec Acceleration(const FSpaceVec& Position, const FSpaceBodyPositions& Bodies)
	{
		const SpaceSimDetail::FInfoTable& Info = SpaceSimDetail::InfoTable();
		const double R = SpaceMath::Length(Position);
		FSpaceVec Accel = Position * (-Info.Of[0].GM / (R * R * R));
		for (int Index = 1; Index < SpaceBodyCount; ++Index)
		{
			const FSpaceVec PlanetPos = Bodies.Of[Index];
			const FSpaceVec ToPlanet = PlanetPos - Position;
			const double D = SpaceMath::Length(ToPlanet);
			const double RP = SpaceMath::Length(PlanetPos);
			const double GM = Info.Of[Index].GM;
			// The planet pulls the probe, and pulls the Sun too; in a frame that rides with
			// the Sun the second pull shows up as the probe being pulled the other way.
			Accel += ToPlanet * (GM / (D * D * D));
			Accel -= PlanetPos * (GM / (RP * RP * RP));
		}
		return Accel;
	}
}

// ---------------------------------------------------------------------------
// INTEGRATOR

double FSpaceIntegrator::Step(FSpaceState& State, double& Seconds, double MaxSeconds, const FSpaceAccelFn& Accel)
{
	namespace D = SpaceSimDetail;
	const FSpaceIntegratorSettings& S = Settings;

	const double Limit = (std::min)(MaxSeconds, S.MaxStepSeconds);
	double H = NextStep > 0.0 ? NextStep : 60.0;
	H = (std::min)((std::max)(H, S.MinStepSeconds), Limit);

	const FSpaceVec R0 = State.Position;
	const FSpaceVec V0 = State.Velocity;
	const double T0 = Seconds;
	const D::FDeriv K1 = D::Derivative(Accel, T0, R0, V0);

	for (;;)
	{
		const D::FDeriv K2 = D::Derivative(Accel, T0 + D::C2 * H,
			R0 + H * (D::A21 * K1.DPos),
			V0 + H * (D::A21 * K1.DVel));
		const D::FDeriv K3 = D::Derivative(Accel, T0 + D::C3 * H,
			R0 + H * (D::A31 * K1.DPos + D::A32 * K2.DPos),
			V0 + H * (D::A31 * K1.DVel + D::A32 * K2.DVel));
		const D::FDeriv K4 = D::Derivative(Accel, T0 + D::C4 * H,
			R0 + H * (D::A41 * K1.DPos + D::A42 * K2.DPos + D::A43 * K3.DPos),
			V0 + H * (D::A41 * K1.DVel + D::A42 * K2.DVel + D::A43 * K3.DVel));
		const D::FDeriv K5 = D::Derivative(Accel, T0 + D::C5 * H,
			R0 + H * (D::A51 * K1.DPos + D::A52 * K2.DPos + D::A53 * K3.DPos + D::A54 * K4.DPos),
			V0 + H * (D::A51 * K1.DVel + D::A52 * K2.DVel + D::A53 * K3.DVel + D::A54 * K4.DVel));
		const D::FDeriv K6 = D::Derivative(Accel, T0 + H,
			R0 + H * (D::A61 * K1.DPos + D::A62 * K2.DPos + D::A63 * K3.DPos + D::A64 * K4.DPos + D::A65 * K5.DPos),
			V0 + H * (D::A61 * K1.DVel + D::A62 * K2.DVel + D::A63 * K3.DVel + D::A64 * K4.DVel + D::A65 * K5.DVel));

		const FSpaceVec R1 = R0 + H * (D::B1 * K1.DPos + D::B3 * K3.DPos + D::B4 * K4.DPos + D::B5 * K5.DPos + D::B6 * K6.DPos);
		const FSpaceVec V1 = V0 + H * (D::B1 * K1.DVel + D::B3 * K3.DVel + D::B4 * K4.DVel + D::B5 * K5.DVel + D::B6 * K6.DVel);
		const D::FDeriv K7 = D::Derivative(Accel, T0 + H, R1, V1);

		const FSpaceVec ErrR = H * (D::E1 * K1.DPos + D::E3 * K3.DPos + D::E4 * K4.DPos + D::E5 * K5.DPos + D::E6 * K6.DPos + D::E7 * K7.DPos);
		const FSpaceVec ErrV = H * (D::E1 * K1.DVel + D::E3 * K3.DVel + D::E4 * K4.DVel + D::E5 * K5.DVel + D::E6 * K6.DVel + D::E7 * K7.DVel);

		double Err = 0.0;
		Err = (std::max)(Err, D::ComponentError(ErrR.X, R0.X, R1.X, S.AbsTolKm, S.RelTolerance));
		Err = (std::max)(Err, D::ComponentError(ErrR.Y, R0.Y, R1.Y, S.AbsTolKm, S.RelTolerance));
		Err = (std::max)(Err, D::ComponentError(ErrR.Z, R0.Z, R1.Z, S.AbsTolKm, S.RelTolerance));
		Err = (std::max)(Err, D::ComponentError(ErrV.X, V0.X, V1.X, S.AbsTolKmPerS, S.RelTolerance));
		Err = (std::max)(Err, D::ComponentError(ErrV.Y, V0.Y, V1.Y, S.AbsTolKmPerS, S.RelTolerance));
		Err = (std::max)(Err, D::ComponentError(ErrV.Z, V0.Z, V1.Z, S.AbsTolKmPerS, S.RelTolerance));

		// The classic controller: scale by the fifth root of the error, cautiously.
		const double Factor = Err > 0.0 ? 0.9 * std::pow(Err, -0.2) : 5.0;
		if (Err <= 1.0 || H <= S.MinStepSeconds)
		{
			State.Position = R1;
			State.Velocity = V1;
			Seconds = T0 + H;
			const double Grown = H * (std::min)(5.0, (std::max)(0.2, Factor));
			// A step clipped short by the caller (a sample boundary, a planet ahead) says
			// nothing about what step the error would allow, so it does not shrink the next.
			NextStep = (H < Limit || H >= NextStep) ? Grown : (std::max)(NextStep, Grown);
			NextStep = (std::min)(NextStep, S.MaxStepSeconds);
			return H;
		}
		H = (std::max)(S.MinStepSeconds, H * (std::max)(0.2, Factor));
	}
}

int FSpaceIntegrator::AdvanceTo(FSpaceState& State, double& Seconds, double Until, const FSpaceAccelFn& Accel, int MaxSteps)
{
	int Steps = 0;
	while (Seconds < Until && Steps < MaxSteps)
	{
		Step(State, Seconds, Until - Seconds, Accel);
		++Steps;
		if (Until - Seconds < 1e-9)
		{
			Seconds = Until;
		}
	}
	return Steps;
}

// ---------------------------------------------------------------------------
// THE PROBE

double FSpaceCraft::DeltaVRemaining() const
{
	if (DryMassKg <= 0.0)
	{
		return SpaceSimDetail::Infinity;
	}
	return ExhaustSpeedKmPerS() * std::log(MassKg() / DryMassKg);
}

void FSpaceSim::Reset(double StartJd, const FSpaceState& StartProbe, const FSpaceCraft& StartCraft)
{
	EpochJd = StartJd;
	Seconds = 0.0;
	ProbeState = StartProbe;
	CraftState = StartCraft;
	bCrashed = false;
	CrashBody = ESpaceBody::Sun;
	Events.clear();
	Integrator.Restart();
	UpdateEvents(false);
}

void FSpaceSim::LaunchFromEarth(double StartJd, const FSpaceVec& VInfinity, const FSpaceCraft& StartCraft, double DistanceKm)
{
	const FSpaceState Earth = SpaceEphemeris::PlanetState(ESpaceBody::Earth, StartJd);
	const FSpaceVec Dir = SpaceMath::Normalized(VInfinity);
	const double VInf = SpaceMath::Length(VInfinity);
	// Energy conservation against Earth alone: v^2 = v_inf^2 + 2 GM / r.
	const double Speed = std::sqrt(VInf * VInf + 2.0 * SpaceBodyInfo(ESpaceBody::Earth).GM / DistanceKm);
	FSpaceState Start;
	Start.Position = Earth.Position + Dir * DistanceKm;
	Start.Velocity = Earth.Velocity + Dir * Speed;
	Reset(StartJd, Start, StartCraft);
}

double FSpaceSim::Jd() const
{
	return EpochJd + Seconds / SpaceConst::SecondsPerDay;
}

FSpaceAccelFn FSpaceSim::GravityFn() const
{
	const double Epoch = EpochJd;
	return [Epoch](double T, const FSpaceVec& Position)
	{
		return SpaceGravity::Acceleration(Position, SpaceEphemeris::AllPositions(Epoch + T / SpaceConst::SecondsPerDay));
	};
}

double FSpaceSim::SafeStepSeconds() const
{
	// No step may carry the probe more than a twentieth of the way to any body, so a close
	// pass can never fall between two steps where the error estimate cannot see it.
	double Safe = Integrator.Settings.MaxStepSeconds;
	const FSpaceBodyStates Bodies = SpaceEphemeris::AllStates(Jd());
	for (int Index = 0; Index < SpaceBodyCount; ++Index)
	{
		const FSpaceState Rel = ProbeState - Bodies.Of[Index];
		const double Distance = SpaceMath::Length(Rel.Position);
		const double Speed = SpaceMath::Length(Rel.Velocity);
		if (Speed > 0.0)
		{
			Safe = (std::min)(Safe, 0.05 * Distance / Speed);
		}
	}
	return (std::max)(Safe, Integrator.Settings.MinStepSeconds);
}

double FSpaceSim::StepOnce(double MaxSeconds, const FSpaceAccelFn& Accel)
{
	const double Taken = Integrator.Step(ProbeState, Seconds, (std::min)(MaxSeconds, SafeStepSeconds()), Accel);
	UpdateEvents(true);
	return Taken;
}

void FSpaceSim::UpdateEvents(bool bEmit)
{
	const FSpaceBodyStates Bodies = SpaceEphemeris::AllStates(Jd());
	for (int Index = 0; Index < SpaceBodyCount; ++Index)
	{
		const ESpaceBody Body = SpaceBodyAt(Index);
		const FSpaceBodyInfo& Info = SpaceBodyInfo(Body);
		const FSpaceState Rel = ProbeState - Bodies.Of[Index];
		const double Distance = SpaceMath::Length(Rel.Position);

		if (Distance < Info.RadiusKm)
		{
			if (bEmit && !bCrashed)
			{
				bCrashed = true;
				CrashBody = Body;
				Emit(ESpaceEvent::Crashed, Body, Distance);
			}
			continue;
		}

		const bool bInside = Index > 0 && Distance < Info.SphereOfInfluenceKm;
		const bool bCloser = SpaceMath::Dot(Rel.Position, Rel.Velocity) < 0.0;
		if (bEmit)
		{
			if (bInside != bInSphere[Index])
			{
				Emit(bInside ? ESpaceEvent::EnteredSphere : ESpaceEvent::LeftSphere, Body, Distance);
			}
			if (bInside && bApproaching[Index] && !bCloser)
			{
				// The pass has just been made. Inside the sphere of influence the path is a
				// hyperbola around the planet to a good approximation, so its periapsis is
				// the closest the probe came, between this step and the last.
				const FSpaceConic Conic = SpaceConic::FromState(Rel, Info.GM);
				const double A = Conic.Elements.SemiMajorAxisKm;
				const double SpeedThere = std::sqrt((std::max)(0.0, Info.GM * (2.0 / Conic.PeriapsisKm - 1.0 / A)));
				Emit(ESpaceEvent::ClosestApproach, Body, Conic.PeriapsisKm, SpeedThere);
			}
		}
		bInSphere[Index] = bInside;
		bApproaching[Index] = bCloser;
	}

	const double R = SpaceMath::Length(ProbeState.Position);
	const double V2 = SpaceMath::Dot(ProbeState.Velocity, ProbeState.Velocity);
	const double Energy = 0.5 * V2 - SpaceBodyInfo(ESpaceBody::Sun).GM / R;
	const bool bNowEscaping = Energy > 0.0;
	if (bEmit && bNowEscaping && !bEscaping)
	{
		Emit(ESpaceEvent::Escaping, ESpaceBody::Sun, std::sqrt(2.0 * Energy));
	}
	bEscaping = bNowEscaping;
}

void FSpaceSim::Emit(ESpaceEvent Type, ESpaceBody Body, double Value, double Value2)
{
	FSpaceEventRecord Record;
	Record.Type = Type;
	Record.Body = Body;
	Record.Jd = Jd();
	Record.Value = Value;
	Record.Value2 = Value2;
	Events.push_back(Record);
}

double FSpaceSim::Advance(double FlightSeconds, int MaxSteps)
{
	if (bCrashed || FlightSeconds <= 0.0)
	{
		return 0.0;
	}
	const FSpaceAccelFn Accel = GravityFn();
	const double Start = Seconds;
	const double Target = Seconds + FlightSeconds;
	int Steps = 0;
	while (Seconds < Target && Steps < MaxSteps && !bCrashed)
	{
		StepOnce(Target - Seconds, Accel);
		++Steps;
		if (Target - Seconds < 1e-9)
		{
			Seconds = Target;
		}
	}
	return Seconds - Start;
}

double FSpaceSim::Burn(const FSpaceVec& DeltaV)
{
	const double Wanted = SpaceMath::Length(DeltaV);
	if (bCrashed || Wanted <= 0.0)
	{
		return 0.0;
	}
	const double Applied = (std::min)(Wanted, CraftState.DeltaVRemaining());
	if (Applied <= 0.0)
	{
		return 0.0;
	}
	const double MassBefore = CraftState.MassKg();
	const double MassAfter = MassBefore / std::exp(Applied / CraftState.ExhaustSpeedKmPerS());
	const double Spent = (std::min)(CraftState.PropellantKg, MassBefore - MassAfter);
	CraftState.PropellantKg -= Spent;
	ProbeState.Velocity += DeltaV * (Applied / Wanted);
	Integrator.Restart();
	// A burn can turn the probe away from a planet; that is not a flyby, so the flags are
	// brought up to date without events.
	UpdateEvents(false);
	Emit(ESpaceEvent::Burn, ESpaceBody::Sun, Applied, Spent);
	return Applied;
}

FSpaceBurnFrame FSpaceSim::BurnFrame(ESpaceBody Body) const
{
	const FSpaceState Rel = RelativeTo(Body);
	FSpaceBurnFrame Frame;
	Frame.Prograde = SpaceMath::Normalized(Rel.Velocity);
	Frame.Normal = SpaceMath::Normalized(SpaceMath::Cross(Rel.Position, Rel.Velocity));
	Frame.Radial = SpaceMath::Cross(Frame.Prograde, Frame.Normal);
	return Frame;
}

FSpaceVec FSpaceSim::BodyPosition(ESpaceBody Body) const
{
	return SpaceEphemeris::PlanetPosition(Body, Jd());
}

FSpaceState FSpaceSim::BodyState(ESpaceBody Body) const
{
	return SpaceEphemeris::PlanetState(Body, Jd());
}

FSpaceState FSpaceSim::RelativeTo(ESpaceBody Body) const
{
	return ProbeState - BodyState(Body);
}

double FSpaceSim::DistanceTo(ESpaceBody Body) const
{
	return SpaceMath::Length(ProbeState.Position - BodyPosition(Body));
}

ESpaceBody FSpaceSim::DominantBody() const
{
	ESpaceBody Best = ESpaceBody::Sun;
	double BestDistance = SpaceSimDetail::Infinity;
	for (int Index = 1; Index < SpaceBodyCount; ++Index)
	{
		const ESpaceBody Body = SpaceBodyAt(Index);
		const double Distance = DistanceTo(Body);
		if (Distance < SpaceBodyInfo(Body).SphereOfInfluenceKm && Distance < BestDistance)
		{
			Best = Body;
			BestDistance = Distance;
		}
	}
	return Best;
}

double FSpaceSim::LightSecondsToEarth() const
{
	return DistanceTo(ESpaceBody::Earth) / SpaceConst::LightKmPerSecond;
}

std::vector<FSpaceEventRecord> FSpaceSim::TakeEvents()
{
	std::vector<FSpaceEventRecord> Out;
	Out.swap(Events);
	return Out;
}

void FSpaceSim::Predict(double FlightSeconds, double SampleSeconds, std::vector<FSpacePathPoint>& Out, int MaxSteps) const
{
	Out.clear();
	FSpaceSim Copy = *this;
	Copy.Events.clear();
	Out.push_back(FSpacePathPoint{ Copy.Jd(), Copy.ProbeState.Position });
	if (bCrashed || FlightSeconds <= 0.0)
	{
		return;
	}

	const FSpaceAccelFn Accel = Copy.GravityFn();
	const double Target = Copy.Seconds + FlightSeconds;
	double LastSampleSeconds = Copy.Seconds;
	FSpaceVec LastDir;
	int Steps = 0;
	while (Copy.Seconds < Target && Steps < MaxSteps && !Copy.bCrashed)
	{
		Copy.StepOnce(Target - Copy.Seconds, Accel);
		++Steps;
		if (Target - Copy.Seconds < 1e-9)
		{
			Copy.Seconds = Target;
		}
		Copy.Events.clear();

		// A point every SampleSeconds, and more where the path bends (a flyby takes hours
		// and turns the path through tens of degrees): wherever it has turned 3 degrees.
		const FSpaceVec Dir = SpaceMath::Normalized(Copy.ProbeState.Position - Out.back().Position);
		const bool bTurned = SpaceMath::Dot(Dir, LastDir) < 0.99863 && SpaceMath::Length(LastDir) > 0.0;
		const bool bDue = Copy.Seconds - LastSampleSeconds >= SampleSeconds;
		const bool bLast = Copy.Seconds >= Target || Copy.bCrashed || Steps >= MaxSteps;
		if (bTurned || bDue || bLast)
		{
			Out.push_back(FSpacePathPoint{ Copy.Jd(), Copy.ProbeState.Position });
			LastSampleSeconds = Copy.Seconds;
			LastDir = Dir;
		}
		else if (SpaceMath::Length(LastDir) == 0.0)
		{
			LastDir = Dir;
		}
	}
}
