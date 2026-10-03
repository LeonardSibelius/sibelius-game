// space_sim_test.cpp — standalone tests for Source/SibeliusGame/SpaceSim.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_space_sim_tests.ps1.
//
// Three kinds of claim. THE SKY: the planets are where NASA says they are, to the accuracy
// NASA says the method has, on five dates from 1850 to 2050 (JPL Horizons, fetched
// 2026-10-02). THE PHYSICS: the integrator flies an orbit the way the exact two-body
// solution says it must, and bends a flyby the way the exact hyperbola says; a slingshot
// behind a planet speeds the probe up and one in front slows it; fuel follows the rocket
// equation. THE HISTORY: Voyager 2, started from where Horizons says it was on 5 September
// 1977, arrives at Jupiter in July 1979.

#include "SpaceSim.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int GPasses = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* Name, const std::string& Detail = std::string())
	{
		std::printf("%s  %s%s%s\n", bOk ? "PASS" : "FAIL", Name, Detail.empty() ? "" : "  -- ", Detail.c_str());
		if (bOk)
		{
			++GPasses;
		}
		else
		{
			++GFailures;
		}
	}

	template <typename... TArgs>
	std::string Fmt(const char* Format, TArgs... Args)
	{
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
		return std::string(Buffer);
	}

	constexpr double Pi = 3.14159265358979323846;
	constexpr double ArcsecPerRad = 180.0 / Pi * 3600.0;
	constexpr double Day = SpaceConst::SecondsPerDay;
	constexpr double Au = SpaceConst::AuKm;

	double Len(FSpaceVec V) { return SpaceMath::Length(V); }

	double AngleBetween(FSpaceVec A, FSpaceVec B)
	{
		return std::atan2(Len(SpaceMath::Cross(A, B)), SpaceMath::Dot(A, B));
	}

	double WrapPi(double A)
	{
		while (A > Pi) A -= 2.0 * Pi;
		while (A < -Pi) A += 2.0 * Pi;
		return A;
	}

	// JPL Horizons: heliocentric (Sun body centre), ECLIPTIC / J2000, au and au/day, TDB.
	// Bodies 1-8 are the planets' system barycentres; 3 is the Earth-Moon barycentre.
	struct FHorizonsRow
	{
		ESpaceBody Body;
		double Jd;
		double X, Y, Z, VX, VY, VZ;
	};

	const FHorizonsRow HorizonsRows[] = {
		{ ESpaceBody::Mercury, 2396758.500000000, 2.567995726398442E-01, -3.372771758137295E-01, -5.115513477534905E-02, 1.678694557552548E-02, 1.843373914447855E-02, -4.505016723162348E-05 },
		{ ESpaceBody::Mercury, 2444064.500000000, -1.945346010017586E-01, -4.209516570830964E-01, -1.651011048541701E-02, 1.987833478237735E-02, -1.042462763040553E-02, -2.676872135445622E-03 },
		{ ESpaceBody::Mercury, 2451545.000000000, -1.300936053754522E-01, -4.472876181353563E-01, -2.459830695805179E-02, 2.136639566801616E-02, -6.447989743836899E-03, -2.487863414837162E-03 },
		{ ESpaceBody::Mercury, 2461315.500000000, 7.961874455161874E-04, -4.621628277399842E-01, -3.784286382359849E-02, 2.249117359693908E-02, 1.490463390029177E-03, -1.941003648679610E-03 },
		{ ESpaceBody::Mercury, 2469807.500000000, -1.795140424844850E-01, 2.678117696735194E-01, 3.834830324254487E-02, -2.904542095821605E-02, -1.457880238639018E-02, 1.470321649527605E-03 },
		{ ESpaceBody::Venus, 2396758.500000000, -2.766106960762407E-01, -6.706343042048769E-01, 7.112230833130810E-03, 1.855843435341337E-02, -7.802359043529595E-03, -1.176824996960574E-03 },
		{ ESpaceBody::Venus, 2444064.500000000, 1.668330376313917E-01, 7.009204897385785E-01, -9.320201028978321E-05, -1.974437873291505E-02, 4.584969122263255E-03, 1.202422870128731E-03 },
		{ ESpaceBody::Venus, 2451545.000000000, -7.183022963453890E-01, -3.265430819980606E-02, 4.101418202684621E-02, 7.981175157753219E-04, -2.029521870223381E-02, -3.234515157278357E-04 },
		{ ESpaceBody::Venus, 2461315.500000000, 7.229432587605678E-01, -6.229911367461402E-02, -4.256863561229107E-02, 1.644520517914084E-03, 2.005944968261177E-02, 1.807302333079956E-04 },
		{ ESpaceBody::Venus, 2469807.500000000, 1.417822235551292E-01, -7.133842157318016E-01, -1.802566774593160E-02, 1.970202520985986E-02, 3.873000003324849E-03, -1.082942722975158E-03 },
		{ ESpaceBody::Earth, 2396758.500000000, -2.109980549495346E-01, 9.603502646357958E-01, 3.212720205316658E-04, -1.708497949195181E-02, -3.753769607070980E-03, -1.771359021091262E-06 },
		{ ESpaceBody::Earth, 2444064.500000000, 3.050445100046674E-01, -9.698135061774356E-01, -4.329098582320237E-05, 1.613147613413460E-02, 5.098224911382690E-03, 3.210375098975082E-07 },
		{ ESpaceBody::Earth, 2451545.000000000, -1.771587841839055E-01, 9.672193524609504E-01, -1.139275508446145E-06, -1.720310905522688E-02, -3.163911860749114E-03, 2.424167134816753E-08 },
		{ ESpaceBody::Earth, 2461315.500000000, 9.901846904815903E-01, 1.471401778075381E-01, -1.492532202786802E-05, -2.807868997763183E-03, 1.695296909174787E-02, -1.013915278402887E-06 },
		{ ESpaceBody::Earth, 2469807.500000000, -1.715829623696938E-01, 9.682682880886148E-01, -1.081487189538922E-04, -1.721987955350517E-02, -3.067159355751021E-03, 5.249905268501170E-07 },
		{ ESpaceBody::Mars, 2396758.500000000, -1.410067703756244E-01, 1.577032262093822E+00, 3.646758199217467E-02, -1.339229039058151E-02, -6.625572974751701E-05, 3.320893619864481E-04 },
		{ ESpaceBody::Mars, 2444064.500000000, 1.030488563823881E+00, 1.037067205150215E+00, -3.653398568399194E-03, -9.388819488492284E-03, 1.105556705256322E-02, 4.626859177740240E-04 },
		{ ESpaceBody::Mars, 2451545.000000000, 1.390715921746287E+00, -1.341631815233992E-02, -3.446766277585426E-02, 6.714995210335850E-04, 1.518724809115030E-02, 3.016517626039734E-04 },
		{ ESpaceBody::Mars, 2461315.500000000, 1.138388319680569E-01, 1.555052058761381E+00, 2.979685411760057E-02, -1.342611557073817E-02, 2.210171750553270E-03, 3.755274295874819E-04 },
		{ ESpaceBody::Mars, 2469807.500000000, -1.543231687779370E+00, -5.035895204729829E-01, 2.720193305006915E-02, 4.861797714874932E-03, -1.210505177414021E-02, -3.727379416628890E-04 },
		{ ESpaceBody::Jupiter, 2396758.500000000, -5.236953241535112E+00, 1.390861883013043E+00, 1.119970000117554E-01, -2.023857770055410E-03, -6.948046922614570E-03, 7.340392880470141E-05 },
		{ ESpaceBody::Jupiter, 2444064.500000000, -3.936941549748474E+00, 3.592903563404502E+00, 7.342069457239318E-02, -5.180845012491611E-03, -5.229617831287475E-03, 1.375809424383218E-04 },
		{ ESpaceBody::Jupiter, 2451545.000000000, 4.001177161126058E+00, 2.938576106966103E+00, -1.017852766337440E-01, -4.568313526752718E-03, 6.443206010221308E-03, 7.557952025774449E-05 },
		{ ESpaceBody::Jupiter, 2461315.500000000, -3.496218743191001E+00, 3.992472880624441E+00, 6.163841583234604E-02, -5.769601379981436E-03, -4.623350259238524E-03, 1.482949360139971E-04 },
		{ ESpaceBody::Jupiter, 2469807.500000000, -2.391046344931990E+00, 4.664074051129192E+00, 3.396564221877785E-02, -6.817675783480790E-03, -3.096749005025743E-03, 1.654141153083914E-04 },
		{ ESpaceBody::Saturn, 2396758.500000000, 9.295725808029005E+00, 1.619518304445104E+00, -3.968876738061549E-01, -1.268785903469936E-03, 5.475834268229660E-03, -4.692588836437783E-05 },
		{ ESpaceBody::Saturn, 2444064.500000000, -9.058025301188898E+00, 2.363360711918467E+00, 3.185845207617167E-01, -1.714070340803484E-03, -5.416875693847710E-03, 1.627420151050462E-04 },
		{ ESpaceBody::Saturn, 2451545.000000000, 6.406408859532809E+00, 6.569989616912557E+00, -3.690764262448534E-01, -4.292351873843250E-03, 3.890315698014864E-03, 1.029478493989804E-04 },
		{ ESpaceBody::Saturn, 2461315.500000000, 9.257798808988221E+00, 1.772168386851378E+00, -3.993802043110860E-01, -1.358309271579008E-03, 5.465244675121139E-03, -4.112409063410775E-05 },
		{ ESpaceBody::Saturn, 2469807.500000000, 4.766225421471267E+00, -8.773719932777603E+00, -3.787676542813846E-02, 4.594510460960312E-03, 2.649472402110743E-03, -2.291045475944737E-04 },
		{ ESpaceBody::Uranus, 2396758.500000000, 1.770199277631338E+01, 9.094969288542160E+00, -1.964037157572909E-01, -1.829573025815457E-03, 3.310664461152775E-03, 3.627965558815186E-05 },
		{ ESpaceBody::Uranus, 2444064.500000000, -1.201161313132108E+01, -1.432708697073158E+01, 1.027420599098129E-01, 2.980461611161733E-03, -2.716370697673550E-03, -4.864540819498478E-05 },
		{ ESpaceBody::Uranus, 2451545.000000000, 1.443185661438178E+01, -1.373432146877096E+01, -2.381416877992503E-01, 2.678105591420150E-03, 2.672695895712686E-03, -2.477206355505103E-05 },
		{ ESpaceBody::Uranus, 2461315.500000000, 8.927211528430075E+00, 1.727016181956445E+01, -5.161194459683142E-02, -3.528847633731332E-03, 1.620448878088429E-03, 5.179157446559894E-05 },
		{ ESpaceBody::Uranus, 2469807.500000000, -1.782323641511405E+01, 4.071483016564170E+00, 2.458503411213354E-01, -9.101967696381948E-04, -4.021537332158423E-03, -3.110845750543546E-06 },
		{ ESpaceBody::Neptune, 2396758.500000000, 2.744042960597269E+01, -1.205868639756535E+01, -3.838282297610034E-01, 1.239914685627185E-03, 2.887217961524908E-03, -8.811149828088357E-05 },
		{ ESpaceBody::Neptune, 2444064.500000000, -5.475135119117250E+00, -2.977365595489961E+01, 7.392873178135944E-01, 3.062312641979403E-03, -5.562322224024622E-04, -5.917723921320597E-05 },
		{ ESpaceBody::Neptune, 2451545.000000000, 1.681204696805288E+01, -2.499176288928469E+01, 1.272228799202476E-01, 2.579274259047737E-03, 1.776900820016126E-03, -9.590961498591593E-05 },
		{ ESpaceBody::Neptune, 2461315.500000000, 2.983739648555049E+01, 1.383305737580365E+00, -7.160802532192254E-01, -1.715409852970363E-04, 3.151891868350340E-03, -6.101716679677210E-05 },
		{ ESpaceBody::Neptune, 2469807.500000000, 1.739819881068304E+01, 2.419778398623797E+01, -8.992418129822297E-01, -2.572492909775959E-03, 1.850343743778257E-03, 2.110076042488964E-05 },
	};

	// JPL's own table of nominal errors for the 1800-2050 elements: longitude and latitude
	// in arcseconds, distance in thousands of km. Indexed by ESpaceBody.
	struct FNominalError
	{
		double LonArcsec, LatArcsec, Dist1000Km;
	};

	const FNominalError NominalErrors[SpaceBodyCount] = {
		{ 0, 0, 0 },
		{ 15, 1, 1 },        // Mercury
		{ 20, 1, 4 },        // Venus
		{ 20, 8, 6 },        // EM Bary
		{ 40, 2, 25 },       // Mars
		{ 400, 10, 600 },    // Jupiter
		{ 600, 25, 1500 },   // Saturn
		{ 50, 2, 1000 },     // Uranus
		{ 10, 1, 200 },      // Neptune
	};

	FSpaceAccelFn PointMass(double GM)
	{
		return [GM](double, const FSpaceVec& P)
		{
			const double R = Len(P);
			return P * (-GM / (R * R * R));
		};
	}

	// ---------------------------------------------------------------------------

	void TestTime()
	{
		std::printf("\n-- time\n");
		struct FDate { int Y, M, D; double Hours; double Jd; };
		const FDate Dates[] = {
			{ 2000, 1, 1, 12.0, 2451545.0 },
			{ 1850, 1, 1, 0.0, 2396758.5 },
			{ 1977, 9, 5, 0.0, 2443391.5 },
			{ 1979, 7, 10, 0.0, 2444064.5 },
			{ 2026, 10, 2, 0.0, 2461315.5 },
			{ 2050, 1, 1, 0.0, 2469807.5 },
			{ 1957, 10, 4, 0.81 * 24.0, 2436116.31 },   // Meeus's example: Sputnik 1
		};
		double Worst = 0.0;
		for (const FDate& D : Dates)
		{
			Worst = std::fmax(Worst, std::fabs(SpaceTime::JulianDate(D.Y, D.M, D.D, D.Hours) - D.Jd));
		}
		Check(Worst < 1e-6, "calendar dates convert to the Julian Dates Horizons prints", Fmt("worst %.2e days", Worst));

		double WorstTrip = 0.0;
		bool bFieldsOk = true;
		for (double Jd = 2378496.5; Jd < 2488069.5; Jd += 37.3)   // 1800 to 2100
		{
			const SpaceTime::FCalendar C = SpaceTime::CalendarFromJulian(Jd);
			const double Back = SpaceTime::JulianDate(C.Year, C.Month, C.Day, C.Hour + C.Minute / 60.0 + C.Second / 3600.0);
			WorstTrip = std::fmax(WorstTrip, std::fabs(Back - Jd));
			bFieldsOk = bFieldsOk && C.Month >= 1 && C.Month <= 12 && C.Day >= 1 && C.Day <= 31 && C.Hour >= 0 && C.Hour < 24;
		}
		Check(WorstTrip < 1e-6 && bFieldsOk, "Julian Date -> calendar -> Julian Date, every 37.3 days 1800-2100", Fmt("worst %.2e days", WorstTrip));

		const SpaceTime::FCalendar Moon = SpaceTime::CalendarFromJulian(2440423.345139);   // Apollo 11 landing, 20:17 TDB-ish
		Check(Moon.Year == 1969 && Moon.Month == 7 && Moon.Day == 20 && Moon.Hour == 20, "1969-07-20 comes back as 1969-07-20, 20h");

		const SpaceTime::FCalendar Launch = SpaceTime::CalendarFromJulian(SpaceGrandTour::StartJd());
		Check(Launch.Year == 1977 && Launch.Month == 8 && Launch.Day == 20 && Launch.Hour == 14 && Launch.Minute == 29,
			"the voyage begins when Voyager 2 did: 1977-08-20 14:29", Fmt("JD %.4f", SpaceGrandTour::StartJd()));
	}

	void TestKepler()
	{
		std::printf("\n-- Kepler's equation\n");
		double Worst = 0.0;
		const double Eccs[] = { 0.0, 0.1, 0.5, 0.9, 0.99, 0.999 };
		for (double E : Eccs)
		{
			for (int Index = -36; Index <= 36; ++Index)
			{
				const double M = Index * Pi / 36.0;
				const double Anom = SpaceKepler::EccentricAnomaly(M, E);
				Worst = std::fmax(Worst, std::fabs(WrapPi(Anom - E * std::sin(Anom) - M)));
			}
		}
		Check(Worst < 1e-13, "elliptic: E - e sin E = M for e up to 0.999", Fmt("worst residual %.2e rad", Worst));

		double WorstH = 0.0;
		const double HEccs[] = { 1.001, 1.1, 2.0, 5.0, 50.0 };
		for (double E : HEccs)
		{
			for (int Index = -50; Index <= 50; ++Index)
			{
				const double M = Index * 1.0;
				const double H = SpaceKepler::HyperbolicAnomaly(M, E);
				WorstH = std::fmax(WorstH, std::fabs(E * std::sinh(H) - H - M) / (1.0 + std::fabs(M)));
			}
		}
		Check(WorstH < 1e-12, "hyperbolic: e sinh H - H = M for e from 1.001 to 50", Fmt("worst relative residual %.2e", WorstH));
	}

	void TestEphemeris()
	{
		std::printf("\n-- the planets against JPL Horizons, 1850-2050\n");
		double WorstRatio[SpaceBodyCount] = {};
		double WorstVel[SpaceBodyCount] = {};
		std::string WorstWhere[SpaceBodyCount];
		for (const FHorizonsRow& Row : HorizonsRows)
		{
			const int B = SpaceBodyIndex(Row.Body);
			const FSpaceVec Truth(Row.X * Au, Row.Y * Au, Row.Z * Au);
			const FSpaceVec TruthV = FSpaceVec(Row.VX, Row.VY, Row.VZ) * (Au / Day);
			const FSpaceState Ours = SpaceEphemeris::PlanetState(Row.Body, Row.Jd);

			const double LonT = std::atan2(Truth.Y, Truth.X), LonO = std::atan2(Ours.Position.Y, Ours.Position.X);
			const double LatT = std::asin(Truth.Z / Len(Truth)), LatO = std::asin(Ours.Position.Z / Len(Ours.Position));
			const double DLon = std::fabs(WrapPi(LonO - LonT)) * ArcsecPerRad;
			const double DLat = std::fabs(LatO - LatT) * ArcsecPerRad;
			const double DDist = std::fabs(Len(Ours.Position) - Len(Truth)) / 1000.0;

			// Measured against JPL's nominal error, so 1.0 means "exactly as good as JPL says".
			const FNominalError& N = NominalErrors[B];
			const double Ratio = std::fmax(DLon / N.LonArcsec, std::fmax(DLat / N.LatArcsec, DDist / N.Dist1000Km));
			if (Ratio > WorstRatio[B])
			{
				const SpaceTime::FCalendar C = SpaceTime::CalendarFromJulian(Row.Jd);
				WorstRatio[B] = Ratio;
				WorstWhere[B] = Fmt("%d: lon %.1f\", lat %.2f\", distance %.0f thousand km", C.Year, DLon, DLat, DDist);
			}
			WorstVel[B] = std::fmax(WorstVel[B], Len(Ours.Velocity - TruthV) / Len(TruthV));
		}
		for (int B = 1; B < SpaceBodyCount; ++B)
		{
			const char* Name = SpaceBodyInfo(SpaceBodyAt(B)).Name;
			// 2.5 x nominal. The edges of the fit are where it is weakest: Uranus in 2050, the
			// last year it covers, is the worst of all forty (2.3 x).
			Check(WorstRatio[B] <= 2.5, (std::string(Name) + " is where Horizons puts it, within 2.5 x JPL's nominal error").c_str(),
				Fmt("worst %.2f x nominal, %s", WorstRatio[B], WorstWhere[B].c_str()));
			Check(WorstVel[B] < 5e-3, (std::string(Name) + "'s velocity agrees with Horizons").c_str(),
				Fmt("worst %.2e relative", WorstVel[B]));
		}

		// The velocity really is the rate of change of the position.
		double WorstDeriv = 0.0;
		for (int B = 1; B < SpaceBodyCount; ++B)
		{
			const double Jd = 2461315.5;
			// 1/1024 day (84 s): a power of two, so Jd +- H are exact and the difference is not
			// rounded; short enough that even Mercury's curve does not blur it.
			const double H = 1.0 / 1024.0;
			const FSpaceVec Fd = (SpaceEphemeris::PlanetPosition(SpaceBodyAt(B), Jd + H) - SpaceEphemeris::PlanetPosition(SpaceBodyAt(B), Jd - H)) / (2.0 * H * Day);
			const FSpaceVec V = SpaceEphemeris::PlanetState(SpaceBodyAt(B), Jd).Velocity;
			WorstDeriv = std::fmax(WorstDeriv, Len(V - Fd) / Len(V));
		}
		Check(WorstDeriv < 1e-8, "each planet's velocity is the derivative of its position", Fmt("worst %.2e relative", WorstDeriv));

		const FSpaceBodyPositions All = SpaceEphemeris::AllPositions(2461315.5);
		Check(Len(All[ESpaceBody::Sun]) == 0.0 && Len(All[ESpaceBody::Jupiter] - SpaceEphemeris::PlanetPosition(ESpaceBody::Jupiter, 2461315.5)) == 0.0,
			"AllPositions puts the Sun at the origin and agrees with PlanetPosition");
	}

	void TestBodies()
	{
		std::printf("\n-- the bodies\n");
		const double EarthSoi = SpaceBodyInfo(ESpaceBody::Earth).SphereOfInfluenceKm;
		const double JupiterSoi = SpaceBodyInfo(ESpaceBody::Jupiter).SphereOfInfluenceKm;
		Check(std::fabs(EarthSoi / 925000.0 - 1.0) < 0.03, "Earth's sphere of influence is about 925,000 km", Fmt("%.0f km", EarthSoi));
		Check(std::fabs(JupiterSoi / 48.2e6 - 1.0) < 0.03, "Jupiter's sphere of influence is about 48 million km", Fmt("%.3g km", JupiterSoi));

		const FSpaceState Earth = SpaceEphemeris::PlanetState(ESpaceBody::Earth, 2461315.5);
		const FSpaceConic Orbit = SpaceConic::FromState(Earth, SpaceBodyInfo(ESpaceBody::Sun).GM + SpaceBodyInfo(ESpaceBody::Earth).GM);
		Check(std::fabs(Orbit.PeriodSeconds / Day - 365.256) < 0.2, "Earth's orbit, read back from its state, takes a year",
			Fmt("%.3f days", Orbit.PeriodSeconds / Day));
		Check(std::fabs(Orbit.Elements.Eccentricity - 0.0167) < 0.001, "and is the right shape", Fmt("e = %.5f", Orbit.Elements.Eccentricity));
	}

	void TestConics()
	{
		std::printf("\n-- conics\n");
		const double GM = SpaceBodyInfo(ESpaceBody::Sun).GM;
		struct FCase { const char* Name; FSpaceElements El; };
		FCase Cases[3];
		Cases[0].Name = "an inclined ellipse";
		Cases[0].El = FSpaceElements{ 2.0 * Au, 0.3, 30.0 * Pi / 180.0, 1.0, 2.0, 0.5 };
		Cases[1].Name = "a retrograde hyperbola";
		Cases[1].El = FSpaceElements{ -1.5 * Au, 1.8, 120.0 * Pi / 180.0, -2.0, 0.7, 1.3 };
		Cases[2].Name = "a nearly circular orbit";
		Cases[2].El = FSpaceElements{ 1.0 * Au, 1e-3, 5.0 * Pi / 180.0, 0.3, -1.0, -2.5 };
		for (const FCase& C : Cases)
		{
			const FSpaceState S = SpaceConic::StateFromElements(C.El, GM);
			const FSpaceConic Back = SpaceConic::FromState(S, GM);
			const FSpaceElements& B = Back.Elements;
			const double ElErr = std::fmax(std::fabs(B.SemiMajorAxisKm / C.El.SemiMajorAxisKm - 1.0),
				std::fmax(std::fabs(B.Eccentricity - C.El.Eccentricity),
					std::fmax(std::fabs(WrapPi(B.InclinationRad - C.El.InclinationRad)),
						std::fmax(std::fabs(WrapPi(B.AscendingNodeRad - C.El.AscendingNodeRad)),
							std::fmax(std::fabs(WrapPi(B.ArgPeriapsisRad - C.El.ArgPeriapsisRad)), std::fabs(WrapPi(B.MeanAnomalyRad - C.El.MeanAnomalyRad)))))));
			Check(ElErr < 1e-9, (std::string(C.Name) + ": elements -> state -> the same elements").c_str(), Fmt("worst %.2e", ElErr));
		}

		// A circular orbit in the ecliptic has no node and no periapsis; its state must still
		// come back exactly.
		const FSpaceState Circle{ FSpaceVec(Au, 0.0, 0.0), FSpaceVec(0.0, std::sqrt(GM / Au), 0.0) };
		const FSpaceState CircleBack = SpaceConic::StateFromElements(SpaceConic::FromState(Circle, GM).Elements, GM);
		Check(Len(CircleBack.Position - Circle.Position) < 1e-3 && Len(CircleBack.Velocity - Circle.Velocity) < 1e-9,
			"a circular orbit in the ecliptic round-trips through its elements");

		const FSpaceState Ellipse = SpaceConic::StateFromElements(Cases[0].El, GM);
		const double Period = SpaceConic::FromState(Ellipse, GM).PeriodSeconds;
		const FSpaceState Lap = SpaceConic::Propagate(Ellipse, GM, Period);
		Check(Len(Lap.Position - Ellipse.Position) < 1e-3, "an ellipse comes back to where it started after one period",
			Fmt("%.2e km", Len(Lap.Position - Ellipse.Position)));
	}

	void TestIntegrator()
	{
		std::printf("\n-- the integrator against exact orbits\n");
		const double GM = SpaceBodyInfo(ESpaceBody::Sun).GM;

		// Earth-like circle, one year.
		{
			FSpaceIntegrator Integrator;
			FSpaceState S{ FSpaceVec(Au, 0.0, 0.0), FSpaceVec(0.0, std::sqrt(GM / Au), 0.0) };
			const FSpaceState Start = S;
			double T = 0.0;
			const double Year = 2.0 * Pi * std::sqrt(Au * Au * Au / GM);
			const int Steps = Integrator.AdvanceTo(S, T, Year, PointMass(GM));
			const double Err = Len(S.Position - Start.Position);
			Check(Err < 1.0, "a circular orbit closes after one period", Fmt("%.3f km off after %d steps", Err, Steps));
		}

		// Halley-like: e = 0.967, perihelion 0.586 au, one 75-year lap, fast at perihelion.
		{
			FSpaceIntegrator Integrator;
			Integrator.Settings.MaxStepSeconds = 30.0 * Day;
			const FSpaceElements El{ 17.8 * Au, 0.967, 162.0 * Pi / 180.0, 1.0, 2.0, Pi * 0.999 };
			FSpaceState S = SpaceConic::StateFromElements(El, GM);
			const FSpaceState Start = S;
			const FSpaceConic Conic = SpaceConic::FromState(S, GM);
			double T = 0.0;
			const int Steps = Integrator.AdvanceTo(S, T, Conic.PeriodSeconds, PointMass(GM));
			const FSpaceState Exact = SpaceConic::Propagate(Start, GM, Conic.PeriodSeconds);
			const double Err = Len(S.Position - Exact.Position);
			const double E0 = 0.5 * SpaceMath::Dot(Start.Velocity, Start.Velocity) - GM / Len(Start.Position);
			const double E1 = 0.5 * SpaceMath::Dot(S.Velocity, S.Velocity) - GM / Len(S.Position);
			Check(Err < 100.0, "a comet orbit (e = 0.967) matches the exact orbit after a 75-year lap", Fmt("%.1f km off after %d steps", Err, Steps));
			Check(std::fabs(E1 / E0 - 1.0) < 1e-9, "and keeps its energy", Fmt("drift %.2e", std::fabs(E1 / E0 - 1.0)));
		}

		// A flyby of a lone Jupiter-mass planet: 10 km/s, periapsis about 1 million km.
		{
			const double GMJ = SpaceBodyInfo(ESpaceBody::Jupiter).GM;
			const double R0 = 1.0e8;
			const double VInf = 10.0;
			const double Rp = 1.0e6;
			const double B = Rp * std::sqrt(1.0 + 2.0 * GMJ / (Rp * VInf * VInf));
			const double Speed = std::sqrt(VInf * VInf + 2.0 * GMJ / R0);
			FSpaceState S{ FSpaceVec(-std::sqrt(R0 * R0 - B * B), B, 0.0), FSpaceVec(Speed, 0.0, 0.0) };
			const FSpaceState Start = S;
			FSpaceIntegrator Integrator;
			double T = 0.0;
			const double Flight = 2.0e7;
			double Closest = 1e300;
			int Steps = 0;
			while (T < Flight)
			{
				Integrator.Step(S, T, std::fmin(Flight - T, 0.05 * Len(S.Position) / Len(S.Velocity)), PointMass(GMJ));
				Closest = std::fmin(Closest, Len(S.Position));
				++Steps;
			}
			const FSpaceState Exact = SpaceConic::Propagate(Start, GMJ, T);
			const FSpaceConic Conic = SpaceConic::FromState(Start, GMJ);
			Check(Len(S.Position - Exact.Position) < 1.0 && Len(S.Velocity - Exact.Velocity) < 1e-6,
				"a flyby of a lone planet follows the exact hyperbola", Fmt("%.3f km, %.1e km/s off after %d steps", Len(S.Position - Exact.Position), Len(S.Velocity - Exact.Velocity), Steps));
			// Aimed for 1,000,000 km with the formula for a start at infinity; from 1e8 km
			// the true periapsis is a little wider, and the exact conic says by how much.
			Check(std::fabs(Conic.PeriapsisKm / Rp - 1.0) < 0.05 && std::fabs(Closest / Conic.PeriapsisKm - 1.0) < 1e-3,
				"and comes as close as its orbit says it will", Fmt("periapsis %.0f km, closest step %.0f km", Conic.PeriapsisKm, Closest));
			const double Turn = AngleBetween(Start.Velocity, S.Velocity);
			const double AsymptoticTurn = 2.0 * std::asin(1.0 / Conic.Elements.Eccentricity);
			Check(std::fabs(Turn - AsymptoticTurn) < 0.02, "and turns through the angle 2 asin(1/e)",
				Fmt("%.2f deg against %.2f deg (the start is not quite at infinity)", Turn * 180.0 / Pi, AsymptoticTurn * 180.0 / Pi));
		}
	}

	// A probe set up near a planet with a chosen v-infinity and aim, at a given date.
	FSpaceSim ApproachJupiter(double Jd, double OffsetAlongJupiterMotion, double VInf, double Distance)
	{
		const FSpaceState J = SpaceEphemeris::PlanetState(ESpaceBody::Jupiter, Jd);
		const FSpaceVec Inward = SpaceMath::Normalized(-J.Position);
		const FSpaceVec Along = SpaceMath::Normalized(J.Velocity - Inward * SpaceMath::Dot(J.Velocity, Inward));
		FSpaceState Probe;
		Probe.Position = J.Position - Inward * Distance + Along * OffsetAlongJupiterMotion;
		Probe.Velocity = J.Velocity + Inward * VInf;
		FSpaceSim Sim;
		Sim.Reset(Jd, Probe, FSpaceCraft());
		return Sim;
	}

	void TestSlingshot()
	{
		std::printf("\n-- slingshots in the full solar system\n");
		const double Jd = 2461315.5;
		FSpaceSim Behind = ApproachJupiter(Jd, -1.5e6, 10.0, 3.0e7);
		FSpaceSim InFront = ApproachJupiter(Jd, 1.5e6, 10.0, 3.0e7);
		const double SpeedBefore = Len(Behind.Probe().Velocity);
		const double SpeedBeforeFront = Len(InFront.Probe().Velocity);
		Behind.Advance(70.0 * Day);
		InFront.Advance(70.0 * Day);
		const double Gain = Len(Behind.Probe().Velocity) - SpeedBefore;
		const double Loss = Len(InFront.Probe().Velocity) - SpeedBeforeFront;
		Check(Gain > 3.0, "passing BEHIND Jupiter speeds the probe up (around the Sun)", Fmt("%+.2f km/s", Gain));
		Check(Loss < -3.0, "passing IN FRONT of it slows the probe down", Fmt("%+.2f km/s", Loss));

		Behind.Advance(80.0 * Day);   // on out of Jupiter's sphere: 48 million km
		const std::vector<FSpaceEventRecord> Events = Behind.TakeEvents();
		int Entered = -1, Closest = -1, Left = -1;
		double Periapsis = 0.0, PeriSpeed = 0.0;
		for (int Index = 0; Index < static_cast<int>(Events.size()); ++Index)
		{
			const FSpaceEventRecord& E = Events[Index];
			if (E.Body != ESpaceBody::Jupiter) continue;
			if (E.Type == ESpaceEvent::EnteredSphere && Entered < 0) Entered = Index;
			if (E.Type == ESpaceEvent::ClosestApproach && Closest < 0) { Closest = Index; Periapsis = E.Value; PeriSpeed = E.Value2; }
			if (E.Type == ESpaceEvent::LeftSphere && Left < 0) Left = Index;
		}
		// Aimed 1.5 million km off with v-infinity 10 km/s: the hyperbola's periapsis is
		// about 696,000 km, and the probe is going about 21 km/s there.
		Check(Entered < 0 && Closest >= 0 && Left > Closest,
			"the flyby reports its closest approach, then leaving Jupiter's sphere (it started inside)",
			Fmt("entered %d, closest %d, left %d", Entered, Closest, Left));
		Check(Periapsis > 5.0e5 && Periapsis < 9.0e5, "the closest approach is the one the aim implies", Fmt("%.0f km at %.1f km/s", Periapsis, PeriSpeed));

		FSpaceSim Bullseye = ApproachJupiter(Jd, 0.0, 10.0, 3.0e7);
		Bullseye.Advance(70.0 * Day);
		Check(Bullseye.HasCrashed() && Bullseye.CrashedInto() == ESpaceBody::Jupiter, "aimed straight at Jupiter, it hits Jupiter");
	}

	void TestLaunchAndBurns()
	{
		std::printf("\n-- launch, burns and fuel\n");
		// A Hohmann transfer to Mars needs 2.94 km/s beyond Earth, along Earth's motion.
		const double Jd = 2461315.5;
		const FSpaceState Earth = SpaceEphemeris::PlanetState(ESpaceBody::Earth, Jd);
		FSpaceSim Sim;
		Sim.LaunchFromEarth(Jd, SpaceMath::Normalized(Earth.Velocity) * 2.945, FSpaceCraft());
		Check(Sim.DominantBody() == ESpaceBody::Sun, "two million km out, the probe is already the Sun's");
		Sim.Advance(30.0 * Day);
		const FSpaceConic Orbit = SpaceConic::FromState(Sim.Probe(), SpaceBodyInfo(ESpaceBody::Sun).GM);
		Check(Orbit.ApoapsisKm / Au > 1.47 && Orbit.ApoapsisKm / Au < 1.58, "2.945 km/s prograde from Earth reaches out to Mars's orbit",
			Fmt("aphelion %.3f au", Orbit.ApoapsisKm / Au));

		FSpaceCraft Craft;
		Craft.DryMassKg = 700.0;
		Craft.PropellantKg = 100.0;
		Craft.IspSeconds = 230.0;
		const double Budget = 230.0 * SpaceConst::StandardGravityKmPerS2 * std::log(800.0 / 700.0);
		Check(std::fabs(Craft.DeltaVRemaining() - Budget) < 1e-12, "the rocket equation gives the budget", Fmt("%.4f km/s", Craft.DeltaVRemaining()));

		FSpaceSim Burner;
		Burner.LaunchFromEarth(Jd, FSpaceVec(0.0, 0.0, 5.0), Craft);
		const FSpaceVec V0 = Burner.Probe().Velocity;
		const FSpaceBurnFrame Frame = Burner.BurnFrame();
		const double Applied = Burner.Burn(Frame.Prograde * 0.1);
		const double Spent = 100.0 - Burner.Craft().PropellantKg;
		const double ExpectSpent = 800.0 * (1.0 - std::exp(-0.1 / (230.0 * SpaceConst::StandardGravityKmPerS2)));
		Check(std::fabs(Applied - 0.1) < 1e-12 && std::fabs(Spent - ExpectSpent) < 1e-9 && std::fabs(Len(Burner.Probe().Velocity - V0) - 0.1) < 1e-12,
			"a 100 m/s burn changes the velocity by 100 m/s and burns what the rocket equation says", Fmt("%.3f kg", Spent));
		const double Second = Burner.Burn(Frame.Normal * 1.0);
		Check(std::fabs(Second - (Budget - 0.1)) < 1e-9 && Burner.Craft().PropellantKg < 1e-9 && Burner.Craft().DeltaVRemaining() < 1e-9,
			"a burn bigger than the tank gives what is left, and empties it", Fmt("%.4f km/s", Second));
		Check(Burner.Burn(Frame.Prograde * 0.1) == 0.0, "an empty tank burns nothing");

		const FSpaceVec Cross = SpaceMath::Cross(Frame.Prograde, Frame.Normal) - Frame.Radial;
		Check(Len(Cross) < 1e-12 && std::fabs(SpaceMath::Dot(Frame.Radial, SpaceMath::Normalized(Burner.Probe().Position))) > 0.0,
			"prograde x normal = radial");
	}

	void TestVoyager()
	{
		std::printf("\n-- Voyager 2, 1977-1979\n");
		// Horizons, body -32, heliocentric ECLIPTIC / J2000, km and km/s, 1977-09-05 00:00 TDB.
		const double StartJd = 2443391.5;
		const FSpaceState Start{
			FSpaceVec(1.469980692081588E+08, -3.275093225097097E+07, 4.277224045490490E+06),
			FSpaceVec(1.058127990296092E+01, 3.735151648887162E+01, 3.144942233467848E+00) };
		FSpaceCraft NoFuel;
		NoFuel.PropellantKg = 0.0;
		FSpaceSim Sim;
		Sim.Reset(StartJd, Start, NoFuel);
		Sim.Advance(SpaceTime::JulianDate(1979, 8, 1) - StartJd > 0.0 ? (SpaceTime::JulianDate(1979, 8, 1) - StartJd) * Day : 0.0);

		// Horizons: Voyager 2's closest approach was 721,375 km from Jupiter's centre at
		// 1979-07-09 22:30 TDB. Our Jupiter is up to ~1.5 million km from the real one (JPL's
		// nominal 400" for this method), and the course after the pass bends accordingly,
		// so the claim is the arrival: the right planet, in the right week.
		const double TrueJd = SpaceTime::JulianDate(1979, 7, 9, 22.5);
		bool bFound = false;
		double When = 0.0, Periapsis = 0.0;
		for (const FSpaceEventRecord& E : Sim.TakeEvents())
		{
			if (E.Type == ESpaceEvent::ClosestApproach && E.Body == ESpaceBody::Jupiter && !bFound)
			{
				bFound = true;
				When = E.Jd;
				Periapsis = E.Value;
			}
		}
		const SpaceTime::FCalendar C = SpaceTime::CalendarFromJulian(When);
		Check(bFound && std::fabs(When - TrueJd) < 3.0 && Periapsis < 3.0e6,
			"Voyager 2, flown from its real 1977 position, passes Jupiter in July 1979",
			Fmt("%04d-%02d-%02d %02d:%02d at %.0f km (real: 1979-07-09 22:30 at 721,375 km)", C.Year, C.Month, C.Day, C.Hour, C.Minute, Periapsis));
		Check(!Sim.HasCrashed(), "and does not hit it");
	}

	void TestSimHousekeeping()
	{
		std::printf("\n-- prediction, determinism, light time\n");
		const double Jd = 2461315.5;
		FSpaceSim A = ApproachJupiter(Jd, -1.5e6, 10.0, 3.0e7);
		FSpaceSim B = A;
		std::vector<FSpacePathPoint> Path;
		const FSpaceVec Before = A.Probe().Position;
		A.Predict(70.0 * Day, Day, Path);
		Check(A.Jd() == Jd && Len(A.Probe().Position - Before) == 0.0, "Predict leaves the sim where it was");
		B.Advance(70.0 * Day);
		Check(!Path.empty() && Len(Path.back().Position - B.Probe().Position) < 1e-6 && std::fabs(Path.back().Jd - B.Jd()) < 1e-9,
			"the predicted path ends where flying it actually goes", Fmt("%zu points", Path.size()));
		int Dense = 0;
		for (size_t Index = 1; Index < Path.size(); ++Index)
		{
			if ((Path[Index].Jd - Path[Index - 1].Jd) < 0.5)
			{
				++Dense;
			}
		}
		Check(Dense >= 10, "the path is drawn finely where it bends round Jupiter", Fmt("%d points closer than half a day apart", Dense));

		FSpaceSim C1 = ApproachJupiter(Jd, -1.5e6, 10.0, 3.0e7);
		FSpaceSim C2 = ApproachJupiter(Jd, -1.5e6, 10.0, 3.0e7);
		C1.Advance(70.0 * Day);
		C2.Advance(70.0 * Day);
		Check(C1.Probe().Position.X == C2.Probe().Position.X && C1.Probe().Velocity.Z == C2.Probe().Velocity.Z, "the same start flies the same path, to the bit");

		FSpaceSim Near = ApproachJupiter(Jd, -1.5e6, 10.0, 3.0e7);
		const double Minutes = Near.LightSecondsToEarth() / 60.0;
		Check(Minutes > 33.0 && Minutes < 54.0, "a message from Jupiter takes 33-54 minutes to reach Earth", Fmt("%.1f minutes today", Minutes));
		Check(Near.DominantBody() == ESpaceBody::Jupiter, "30 million km from Jupiter, the probe is in Jupiter's sphere");
	}
}

int main()
{
	TestTime();
	TestKepler();
	TestEphemeris();
	TestBodies();
	TestConics();
	TestIntegrator();
	TestSlingshot();
	TestLaunchAndBurns();
	TestVoyager();
	TestSimHousekeeping();
	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
