// PinballSim.cpp — see the header for the design, and Tools/Tests/pinball_sim_test.cpp for
// what it is held to.

#include "PinballSim.h"

#include <cmath>
#include <initializer_list>
#include <utility>

// NAMED, NOT ANONYMOUS, AND NOT PULLED IN WITH "using". Unity builds paste many .cpp files
// into one translation unit, so a file-scope Dot() or Length() here would collide with any
// other file's (the ProceduralPcm.h lesson). Qualified calls keep this file sealed.
namespace PinMath
{
	constexpr double Pi = 3.14159265358979323846;

	inline double Dot(FPinVec2 A, FPinVec2 B) { return A.X * B.X + A.Y * B.Y; }
	inline double Length(FPinVec2 A) { return std::sqrt(Dot(A, A)); }
	inline FPinVec2 Perp(FPinVec2 A) { return FPinVec2(-A.Y, A.X); }   // 90° counter-clockwise
	inline double Radians(double Degrees) { return Degrees * Pi / 180.0; }
	inline double Clamp01(double V) { return V < 0.0 ? 0.0 : (V > 1.0 ? 1.0 : V); }

	inline FPinVec2 Normalized(FPinVec2 A)
	{
		const double L = Length(A);
		return L > 1e-12 ? A * (1.0 / L) : FPinVec2(0.0, 1.0);
	}

	// Parameter 0..1 of the point on segment AB closest to P.
	inline double ClosestT(FPinVec2 P, FPinVec2 A, FPinVec2 B)
	{
		const FPinVec2 AB = B - A;
		const double L2 = Dot(AB, AB);
		return L2 > 1e-12 ? Clamp01(Dot(P - A, AB) / L2) : 0.0;
	}
}

// ===========================================================================
// THE TEST TABLE
//
// The first layout, before there was a cabinet to fit: generic, symmetrical, and built
// left-side-first and mirrored so the halves cannot drift apart. The physics tests are
// written against it because its geometry is simple and known. Centimetres throughout.

FPinballTableLayout FPinballTableLayout::MakeTestTable()
{
	FPinballTableLayout L;

	const double LaneX = L.ShooterLaneMinX;   // 46.5: the playfield is 0..LaneX
	const double CenterX = LaneX * 0.5;       // 23.25
	auto MirrorX = [CenterX](double X) { return 2.0 * CenterX - X; };

	auto Wall = [&L](double AX, double AY, double BX, double BY, EPinballSurface Kind = EPinballSurface::Wall, int Index = -1)
	{
		L.Segments.push_back(FPinballSegment{ FPinVec2(AX, AY), FPinVec2(BX, BY), Kind, Index });
	};

	// The outer cabinet: right wall, the top arc, left wall.
	Wall(L.Width, 0.0, L.Width, 85.0);
	{
		// An elliptical arc, 15° segments, from the top of the right wall round to the top
		// of the left. It is what turns a plunged ball left into the playfield. Endpoints are
		// set exactly rather than trusted to cos(pi).
		const double ArcCX = L.Width * 0.5, ArcCY = 85.0, ArcRX = L.Width * 0.5, ArcRY = 21.0;
		FPinVec2 Prev(L.Width, 85.0);
		for (int Step = 1; Step <= 12; ++Step)
		{
			const double Theta = PinMath::Radians(15.0 * Step);
			const FPinVec2 Next = (Step == 12)
				? FPinVec2(0.0, 85.0)
				: FPinVec2(ArcCX + ArcRX * std::cos(Theta), ArcCY + ArcRY * std::sin(Theta));
			Wall(Prev.X, Prev.Y, Next.X, Next.Y);
			Prev = Next;
		}
	}
	Wall(0.0, 85.0, 0.0, 20.0);

	// The guides that funnel the lower playfield onto the flippers. Each ends at its
	// flipper's pivot centre, where the flipper's thick base covers the joint.
	Wall(0.0, 20.0, 14.0, 11.0);
	Wall(LaneX, 20.0, MirrorX(14.0), 11.0);

	// The shooter lane: inner wall, the plunger floor the served ball rests on, and the
	// one-way gate across its top. The gate starts exactly at the top of the inner wall.
	// With any stub of wall standing above the gate's low end, a ball rolling down the gate
	// would be trapped in the corner between them.
	Wall(LaneX, 0.0, LaneX, 80.0);
	Wall(LaneX, 0.0, L.Width, 0.0);
	Wall(LaneX, 80.0, L.Width, 84.0, EPinballSurface::OneWayGate);   // front (left-normal) = above

	// Slingshots: a kicking rubber face toward the centre, closed behind by two rails.
	// The back corner sits at y=21 so the inlane between it and the guide is 4 cm wide —
	// at y=19 it was 2.4 cm and a 2.7 cm ball would have jammed there.
	Wall(6.0, 27.0, 11.5, 17.0, EPinballSurface::Slingshot, 0);
	Wall(6.0, 27.0, 6.0, 21.0);
	Wall(6.0, 21.0, 11.5, 17.0);
	Wall(MirrorX(6.0), 27.0, MirrorX(11.5), 17.0, EPinballSurface::Slingshot, 1);
	Wall(MirrorX(6.0), 27.0, MirrorX(6.0), 21.0);
	Wall(MirrorX(6.0), 21.0, MirrorX(11.5), 17.0);

	L.Bumpers.push_back(FPinballBumper{ FPinVec2(15.0, 68.0), 2.6 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(MirrorX(15.0), 68.0), 2.6 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(CenterX, 78.0), 2.6 });

	L.Rollovers.push_back(FPinballRollover{ FPinVec2(16.0, 95.0), 2.0 });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(CenterX, 97.0), 2.0 });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(MirrorX(16.0), 95.0), 2.0 });

	// Flippers: 30° below horizontal at rest, 25° above when held. The right one is the
	// mirror image, so it rotates clockwise to rise.
	FPinballFlipperDef& Left = L.Flippers[0];
	Left.Pivot = FPinVec2(14.0, 11.0);
	Left.RestAngle = PinMath::Radians(-30.0);
	Left.UpAngle = PinMath::Radians(25.0);

	FPinballFlipperDef& Right = L.Flippers[1];
	Right.Pivot = FPinVec2(MirrorX(14.0), 11.0);
	Right.RestAngle = PinMath::Radians(180.0 + 30.0);
	Right.UpAngle = PinMath::Radians(180.0 - 25.0);

	L.PlungerRest = FPinVec2((LaneX + L.Width) * 0.5, L.BallRadius);
	return L;
}

// ===========================================================================
// THE LIVING-ROOM TABLE: SINGALONG
//
// The Speedbump Games "Singalong" cabinet (Content/SBG_RetroArcade), measured from its own
// mesh on 2026-09-27 (exported to OBJ; the scripts are described in the commit). Two kinds
// of number below, and it matters which is which:
//
//   MODELLED, so exact to a millimetre: every post, the four pop bumpers, the two flippers
//   (pivot, length, both radii, rest angle), the lane guides, the playfield's width.
//
//   PAINTED ONLY, so traced from a straight-down render of the textured mesh at 10 px/cm and
//   good to about half a centimetre: the rubber bands, the outer walls, the rollovers and the
//   eight holes. The mesh is a display prop; none of these exist as geometry in it.
//
// Table coordinates: X across from the player's left edge of the playfield, Y up the table
// from just below the flippers. The cabinet's playfield is 54.9 cm wide; the extra 4.9 cm
// on the right, between the playfield and the cabinet wall, is the shooter lane.
//
// Scoring is the machine's own, from the art: bumpers "10 POINTS", "100 POINTS" in the
// centre, "HOLES SCORE 10 TO 50 POINTS", "10 POINTS WHEN LIT".

FPinballTableLayout FPinballTableLayout::MakeDefault()
{
	FPinballTableLayout L;
	L.Width = 59.8;
	L.Length = 108.8;
	L.ShooterLaneMinX = 54.9;
	L.DrainY = -3.0;
	L.BumperPoints = 10;
	L.SlingshotPoints = 10;
	L.RolloverSetBonus = 500;

	auto Wall = [&L](double AX, double AY, double BX, double BY, EPinballSurface Kind = EPinballSurface::Wall)
	{
		L.Segments.push_back(FPinballSegment{ FPinVec2(AX, AY), FPinVec2(BX, BY), Kind, -1 });
	};

	/* A RUBBER STRETCHED ROUND POSTS, listed counter-clockwise. One segment per side, pushed
	   out by the post radius so the ball meets the rubber and not the line through the post
	   centres. A two-post loop comes out as both long sides of the band, because A->B and
	   B->A have opposite outward normals. The posts themselves are separate circles below,
	   which is what closes the corners. */
	auto Band = [&L](std::initializer_list<FPinVec2> Corners, double Radius, int KickerEdge = -1, int SlingIndex = -1)
	{
		const std::vector<FPinVec2> C(Corners);
		for (size_t I = 0; I < C.size(); ++I)
		{
			const FPinVec2 A = C[I];
			const FPinVec2 B = C[(I + 1) % C.size()];
			const FPinVec2 D = PinMath::Normalized(B - A);
			const FPinVec2 Out(D.Y, -D.X);   // right-hand normal: outward on a counter-clockwise loop
			const bool bKicker = static_cast<int>(I) == KickerEdge;
			L.Segments.push_back(FPinballSegment{ A + Out * Radius, B + Out * Radius,
				bKicker ? EPinballSurface::Slingshot : EPinballSurface::Rubber, bKicker ? SlingIndex : -1 });
		}
	};

	// --- the cabinet: walls, top arch, shooter lane, and the slopes to the drain ---------
	Wall(59.8, 0.0, 59.8, 91.0);                       // the cabinet wall beside the shooter lane
	{
		const double CX = 29.9, CY = 91.0, RX = 29.9, RY = 17.8;
		FPinVec2 Prev(59.8, 91.0);
		for (int Step = 1; Step <= 12; ++Step)
		{
			const double Theta = PinMath::Radians(15.0 * Step);
			const FPinVec2 Next = (Step == 12) ? FPinVec2(0.0, 91.0)
				: FPinVec2(CX + RX * std::cos(Theta), CY + RY * std::sin(Theta));
			Wall(Prev.X, Prev.Y, Next.X, Next.Y);
			Prev = Next;
		}
	}
	Wall(0.0, 91.0, 0.0, 7.0);                         // left side
	/* THE SLOPES TO THE DRAIN END SHORT OF THE FLIPPER-TIP POSTS, with room to spare. The
	   first version ended them 2.58 cm from the right tip post: a 2.7 cm ball wedged in the
	   gap, and seven of ten simulated games never finished (the trap test in
	   Tools/Tests found it). The left side had cleared by 0.03 cm, which is luck, not design.
	   Both gaps are now about 3.5 cm. */
	Wall(0.0, 7.0, 18.0, -1.5);                        // bottom left: the outlane runs under the flipper to the drain
	Wall(54.9, 7.0, 35.6, -1.5);                       // bottom right
	Wall(54.9, 0.0, 54.9, 88.0);                       // shooter lane inner wall
	Wall(54.9, 0.0, 59.8, 0.0);                        // plunger floor
	Wall(54.9, 88.0, 59.8, 90.5, EPinballSurface::OneWayGate);   // front (left-normal) = above
	/* THE TWO UPPER POSTS HARD BY THE SIDE WALLS - (1.53, 82.66) left, (53.03, 82.05) right -
	   stand 0.44 and 0.78 cm off the wall: too narrow for a ball, so a ball coming down the
	   wall sat in the V between post and wall, at rest, for ever. Walt's first ball did,
	   upper left (2026-09-27), and a 400-game hunt found 16 more there and on the right.
	   On the machine a rubber ring fills that gap. Here a short rail runs from the wall down
	   onto each post's inner shoulder, steep enough that nothing rests on it. */
	Wall(0.0, 84.8, 2.30, 83.43);
	Wall(54.9, 84.2, 52.26, 82.82);

	// --- rubber bands ----------------------------------------------------------------------
	Band({ FPinVec2(6.17, 11.45), FPinVec2(14.19, 12.69), FPinVec2(7.40, 23.36) }, 1.0, 1, 0);   // left slingshot
	Band({ FPinVec2(38.67, 12.51), FPinVec2(47.07, 10.92), FPinVec2(45.94, 22.84) }, 1.0, 2, 1); // right slingshot
	Band({ FPinVec2(0.65, 34.92), FPinVec2(4.33, 38.16), FPinVec2(0.0, 41.5) }, 1.09);          // left "3" triangle
	Band({ FPinVec2(49.00, 37.99), FPinVec2(52.59, 35.10), FPinVec2(54.9, 41.3) }, 1.09);       // right "3" triangle
	Band({ FPinVec2(24.04, 66.11), FPinVec2(29.29, 66.19), FPinVec2(26.6, 70.0) }, 1.09);       // over "100 POINTS"
	Band({ FPinVec2(5.91, 50.60), FPinVec2(5.99, 54.81) }, 1.09);                               // left, beside the lower "2" lane
	Band({ FPinVec2(47.25, 50.08), FPinVec2(47.34, 54.54) }, 1.09);                             // right, beside the lower "2" lane
	Band({ FPinVec2(5.29, 65.49), FPinVec2(5.29, 69.78) }, 1.09);                               // left upper lane guide
	Band({ FPinVec2(48.13, 66.11), FPinVec2(48.04, 70.40) }, 1.09);                             // right upper lane guide
	Band({ FPinVec2(20.53, 85.46), FPinVec2(20.62, 90.63) }, 1.09);                             // top lane guides
	Band({ FPinVec2(26.49, 84.24), FPinVec2(26.67, 89.49) }, 1.09);
	Band({ FPinVec2(32.80, 84.67), FPinVec2(32.97, 90.63) }, 1.09);

	// --- every post in the mesh --------------------------------------------------------
	const double Posts[][3] = {
		{ 6.96, 95.89, 1.09 }, { 47.86, 94.75, 1.09 }, { 20.62, 90.63, 1.09 }, { 32.97, 90.63, 1.09 },
		{ 26.67, 89.49, 1.09 }, { 12.83, 87.39, 1.09 }, { 38.75, 86.43, 1.09 }, { 20.53, 85.46, 1.09 },
		{ 32.80, 84.67, 1.09 }, { 26.49, 84.24, 1.09 }, { 1.53, 82.66, 1.09 }, { 53.03, 82.05, 1.09 },
		{ 48.04, 70.40, 1.09 }, { 5.29, 69.78, 1.09 }, { 29.29, 66.19, 1.09 }, { 24.04, 66.11, 1.09 },
		{ 48.13, 66.11, 1.09 }, { 5.29, 65.49, 1.09 }, { 5.99, 54.81, 1.09 }, { 47.34, 54.54, 1.09 },
		{ 5.91, 50.60, 1.09 }, { 47.25, 50.08, 1.09 }, { 4.33, 38.16, 1.09 }, { 49.00, 37.99, 1.09 },
		{ 52.59, 35.10, 1.09 }, { 0.65, 34.92, 1.09 }, { 7.40, 23.36, 1.09 }, { 45.94, 22.84, 1.09 },
		{ 14.19, 12.69, 0.95 }, { 38.67, 12.51, 0.93 }, { 6.17, 11.45, 0.88 }, { 47.07, 10.92, 0.88 },
		{ 21.07, 2.55, 1.38 }, { 32.58, 2.35, 1.38 }, { 26.75, 0.43, 0.82 },
	};
	for (const auto& P : Posts)
	{
		L.Posts.push_back(FPinballPost{ FPinVec2(P[0], P[1]), P[2] });
	}

	// --- pop bumpers: upper pair, lower pair ------------------------------------------------
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(15.3, 70.5), 4.2 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(37.9, 70.7), 4.2 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(18.5, 55.8), 4.2 });
	L.Bumpers.push_back(FPinballBumper{ FPinVec2(34.8, 55.7), 4.2 });

	// --- rollovers ----------------------------------------------------------------------------
	// The four top lanes are the set ("LIGHTS RED / YELLOW BUMPERS"): light all four for the
	// bonus. Everything else pays on every pass.
	for (const double X : { 16.7, 23.6, 29.8, 35.8 })
	{
		L.Rollovers.push_back(FPinballRollover{ FPinVec2(X, 87.0), 1.8, 50, true });
	}
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(26.9, 63.7), 1.6, 100, false });   // "100 POINTS"
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(2.1, 67.5), 1.6, 20, false });     // the four "2" lanes
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(52.0, 68.0), 1.6, 20, false });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(2.4, 52.7), 1.6, 20, false });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(51.6, 52.3), 1.6, 20, false });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(2.9, 17.5), 1.6, 40, false });     // the "4" outlanes
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(51.7, 17.5), 1.6, 40, false });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(17.9, 22.5), 1.6, 10, false });    // "10 POINTS WHEN LIT"
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(35.2, 22.5), 1.6, 10, false });
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(23.9, 11.3), 1.6, 40, false });    // the "4"s between the flippers
	L.Rollovers.push_back(FPinballRollover{ FPinVec2(29.4, 11.3), 1.6, 40, false });

	// --- the eight holes ---------------------------------------------------------------------
	const FPinVec2 DownRight = PinMath::Normalized(FPinVec2(0.5, -0.866));
	const FPinVec2 DownLeft = PinMath::Normalized(FPinVec2(-0.5, -0.866));
	L.Kickouts.push_back(FPinballKickout{ FPinVec2(9.9, 82.5), 1.6, 50, DownRight, 220.0 });   // top, by the "50 POINTS" arrows
	L.Kickouts.push_back(FPinballKickout{ FPinVec2(42.9, 82.3), 1.6, 50, DownLeft, 220.0 });
	const int RowPoints[4] = { 10, 20, 30, 40 };                                                 // "HOLES SCORE 10 TO 50"
	const double RowX[4] = { 14.6, 22.2, 30.4, 38.4 };
	for (int I = 0; I < 4; ++I)
	{
		L.Kickouts.push_back(FPinballKickout{ FPinVec2(RowX[I], 48.0), 1.6, RowPoints[I], FPinVec2(0.0, -1.0), 180.0 });
	}
	L.Kickouts.push_back(FPinballKickout{ FPinVec2(5.2, 34.5), 1.6, 30, PinMath::Normalized(FPinVec2(0.9, -0.44)), 220.0 });   // the "3" holes
	L.Kickouts.push_back(FPinballKickout{ FPinVec2(48.6, 34.5), 1.6, 30, PinMath::Normalized(FPinVec2(-0.9, -0.44)), 220.0 });

	// --- flippers, as modelled ------------------------------------------------------------------
	FPinballFlipperDef& Left = L.Flippers[0];
	Left.Pivot = FPinVec2(15.28, 9.74);
	Left.Length = 8.28;
	Left.BaseRadius = 1.43;
	Left.TipRadius = 0.83;
	Left.RestAngle = PinMath::Radians(-37.5);
	Left.UpAngle = PinMath::Radians(20.0);

	FPinballFlipperDef& Right = L.Flippers[1];
	Right = Left;
	Right.Pivot = FPinVec2(37.61, 9.74);
	Right.RestAngle = PinMath::Radians(-138.4);
	Right.UpAngle = PinMath::Radians(-200.0);   // unwrapped on purpose: it rises clockwise

	L.PlungerRest = FPinVec2((L.ShooterLaneMinX + L.Width) * 0.5, L.BallRadius);
	return L;
}

// ===========================================================================

FPinballSim::FPinballSim(FPinballTableLayout InLayout)
	: TableLayout(std::move(InLayout))
{
	Gravity = 980.665 * std::sin(PinMath::Radians(TableLayout.SlopeDegrees)) * (5.0 / 7.0);

	RolloverLit.assign(TableLayout.Rollovers.size(), 0);
	RolloverInside.assign(TableLayout.Rollovers.size(), 0);
	BumperCooldown.assign(TableLayout.Bumpers.size(), 0.0);
	KickoutCooldown.assign(TableLayout.Kickouts.size(), 0.0);

	int MaxSling = -1;
	for (const FPinballSegment& Segment : TableLayout.Segments)
	{
		if (Segment.Kind == EPinballSurface::Slingshot && Segment.Index > MaxSling)
		{
			MaxSling = Segment.Index;
		}
	}
	SlingCooldown.assign(static_cast<size_t>(MaxSling + 1), 0.0);

	for (int Side = 0; Side < 2; ++Side)
	{
		FlipperStates[Side].Angle = TableLayout.Flippers[Side].RestAngle;
	}
	BallPos = TableLayout.PlungerRest;
}

void FPinballSim::StartGame()
{
	CurrentScore = 0;
	CurrentBall = 1;
	RolloverLit.assign(RolloverLit.size(), 0);
	RolloverInside.assign(RolloverInside.size(), 0);
	Emit(EPinballEvent::GameStarted);
	ServeBall();
}

void FPinballSim::ServeBall()
{
	CapturedKickout = -1;
	KickoutHeldSeconds = 0.0;
	CurrentPhase = EPinballPhase::BallReady;
	BallPos = TableLayout.PlungerRest;
	BallVel = FPinVec2();
	Emit(EPinballEvent::BallServed, CurrentBall);
}

void FPinballSim::SetFlipper(int Side, bool bHeld)
{
	if (Side < 0 || Side > 1)
	{
		return;
	}
	FFlipperState& State = FlipperStates[Side];
	if (State.bHeld == bHeld)
	{
		return;
	}
	State.bHeld = bHeld;
	Emit(bHeld ? EPinballEvent::FlipperUp : EPinballEvent::FlipperDown, Side);
}

bool FPinballSim::Launch(double Pull)
{
	if (CurrentPhase != EPinballPhase::BallReady)
	{
		return false;
	}
	const double Speed = PlungerMinSpeed + (PlungerMaxSpeed - PlungerMinSpeed) * PinMath::Clamp01(Pull);
	BallVel = FPinVec2(0.0, Speed);
	CurrentPhase = EPinballPhase::InPlay;
	Emit(EPinballEvent::BallLaunched, CurrentBall);
	return true;
}

void FPinballSim::Advance(double DeltaSeconds)
{
	if (!(DeltaSeconds > 0.0))
	{
		return;
	}
	Accumulator += DeltaSeconds < MaxFrameSeconds ? DeltaSeconds : MaxFrameSeconds;
	while (Accumulator >= SubstepSeconds)
	{
		Substep();
		Accumulator -= SubstepSeconds;
	}
}

void FPinballSim::Substep()
{
	const double H = SubstepSeconds;

	// Flippers move whatever the phase: on a real machine the buttons work in attract mode.
	StepFlippers();
	TickCooldowns();

	if (CurrentPhase != EPinballPhase::InPlay)
	{
		return;
	}

	// In a hole: the ball sits still until the kicker fires.
	if (CapturedKickout >= 0)
	{
		KickoutHeldSeconds += H;
		if (KickoutHeldSeconds >= KickoutHoldSeconds)
		{
			EjectKickout();
		}
		return;
	}

	BallVel.Y -= Gravity * H;
	BallVel = BallVel * (1.0 - RollingDamping * H);
	ClampBallSpeed();
	BallPos = BallPos + BallVel * H;

	for (int Iteration = 0; Iteration < ContactIterations; ++Iteration)
	{
		CollideSegments();
		CollideBumpers();
		CollidePosts();
		CollideFlippers();
	}
	ClampBallSpeed();
	UpdateRollovers();
	UpdateKickouts();
	if (CapturedKickout >= 0)
	{
		return;
	}

	if (BallPos.Y < TableLayout.DrainY)
	{
		DrainBall();
		return;
	}

	// A plunge too soft to clear the gate rolls back down onto the plunger. It is the same
	// ball, served again, and the player gets another pull.
	if (BallPos.X > TableLayout.ShooterLaneMinX
		&& BallPos.Y < TableLayout.PlungerRest.Y + 2.0
		&& PinMath::Length(BallVel) < 3.0)
	{
		ServeBall();
	}
}

void FPinballSim::StepFlippers()
{
	for (int Side = 0; Side < 2; ++Side)
	{
		FFlipperState& State = FlipperStates[Side];
		const FPinballFlipperDef& Def = TableLayout.Flippers[Side];

		const double Target = State.bHeld ? Def.UpAngle : Def.RestAngle;
		const double MaxStep = (State.bHeld ? FlipperUpSpeed : FlipperDownSpeed) * SubstepSeconds;
		const double Delta = Target - State.Angle;
		const double StepAngle = std::fabs(Delta) <= MaxStep ? Delta : (Delta > 0.0 ? MaxStep : -MaxStep);

		State.Angle += StepAngle;
		// The surface velocity the ball feels. Zero once the flipper reaches its stop, so a
		// ball cradled on a held flipper is resting on something still.
		State.AngularVelocity = StepAngle / SubstepSeconds;
	}
}

void FPinballSim::TickCooldowns()
{
	for (double& Cooldown : BumperCooldown)
	{
		Cooldown = Cooldown > SubstepSeconds ? Cooldown - SubstepSeconds : 0.0;
	}
	for (double& Cooldown : SlingCooldown)
	{
		Cooldown = Cooldown > SubstepSeconds ? Cooldown - SubstepSeconds : 0.0;
	}
	for (double& Cooldown : KickoutCooldown)
	{
		Cooldown = Cooldown > SubstepSeconds ? Cooldown - SubstepSeconds : 0.0;
	}
}

// Push the ball out along Normal, and reflect its velocity RELATIVE TO THE SURFACE, so a
// moving flipper hands its speed to the ball. Returns the approach speed into the surface
// (0 if the ball was already moving away), which is what decides whether a kicker fires.
double FPinballSim::Resolve(FPinVec2 Normal, double Penetration, FPinVec2 SurfaceVelocity, double Restitution)
{
	BallPos = BallPos + Normal * Penetration;

	FPinVec2 Relative = BallVel - SurfaceVelocity;
	const double Vn = PinMath::Dot(Relative, Normal);
	if (Vn >= 0.0)
	{
		return 0.0;
	}

	const double Approach = -Vn;
	const bool bRealImpact = Approach >= RestingSpeed;
	const double E = bRealImpact ? Restitution : 0.0;
	Relative = Relative - Normal * ((1.0 + E) * Vn);

	// Friction on real impacts only. Applied every substep it would brake a ball rolling
	// along a rail 1200 times a second.
	if (bRealImpact)
	{
		const FPinVec2 Tangent = Relative - Normal * PinMath::Dot(Relative, Normal);
		Relative = Relative - Tangent * ImpactFriction;
	}

	BallVel = Relative + SurfaceVelocity;
	return Approach;
}

void FPinballSim::Kick(FPinVec2 Normal, double Speed)
{
	const double Vn = PinMath::Dot(BallVel, Normal);
	if (Vn < Speed)
	{
		BallVel = BallVel + Normal * (Speed - Vn);
	}
}

void FPinballSim::ClampBallSpeed()
{
	const double Speed = PinMath::Length(BallVel);
	if (Speed > MaxBallSpeed)
	{
		BallVel = BallVel * (MaxBallSpeed / Speed);
	}
}

void FPinballSim::CollideSegments()
{
	const double R = TableLayout.BallRadius;

	for (const FPinballSegment& Segment : TableLayout.Segments)
	{
		const FPinVec2 AB = Segment.B - Segment.A;

		// A gate stops only a ball whose centre is on its front side. The plunged ball comes
		// from behind and passes; a ball falling onto it from the playfield is on the front.
		if (Segment.Kind == EPinballSurface::OneWayGate
			&& PinMath::Dot(BallPos - Segment.A, PinMath::Perp(AB)) <= 0.0)
		{
			continue;
		}

		const double T = PinMath::ClosestT(BallPos, Segment.A, Segment.B);
		const FPinVec2 Offset = BallPos - (Segment.A + AB * T);
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= R * R)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : PinMath::Normalized(PinMath::Perp(AB));
		const bool bRubber = Segment.Kind == EPinballSurface::Rubber || Segment.Kind == EPinballSurface::Slingshot;
		const double Approach = Resolve(Normal, R - Dist, FPinVec2(), bRubber ? RubberRestitution : WallRestitution);

		if (Segment.Kind == EPinballSurface::Slingshot
			&& Approach >= SlingTriggerSpeed
			&& Segment.Index >= 0
			&& static_cast<size_t>(Segment.Index) < SlingCooldown.size()
			&& SlingCooldown[static_cast<size_t>(Segment.Index)] <= 0.0)
		{
			Kick(Normal, SlingKickSpeed);
			SlingCooldown[static_cast<size_t>(Segment.Index)] = KickerCooldownSeconds;
			CurrentScore += TableLayout.SlingshotPoints;
			Emit(EPinballEvent::SlingshotHit, Segment.Index, TableLayout.SlingshotPoints);
		}
	}
}

void FPinballSim::CollideBumpers()
{
	const double R = TableLayout.BallRadius;

	for (size_t I = 0; I < TableLayout.Bumpers.size(); ++I)
	{
		const FPinballBumper& Bumper = TableLayout.Bumpers[I];
		const FPinVec2 Offset = BallPos - Bumper.Center;
		const double MinDist = R + Bumper.Radius;
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= MinDist * MinDist)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : FPinVec2(0.0, 1.0);
		Resolve(Normal, MinDist - Dist, FPinVec2(), BumperRestitution);

		// A pop bumper fires on any touch — its skirt is the switch — but only once per
		// contact, or a ball grazing it would score every substep.
		if (BumperCooldown[I] <= 0.0)
		{
			Kick(Normal, BumperKickSpeed);
			BumperCooldown[I] = KickerCooldownSeconds;
			CurrentScore += TableLayout.BumperPoints;
			Emit(EPinballEvent::BumperHit, static_cast<int>(I), TableLayout.BumperPoints);
		}
	}
}

void FPinballSim::CollidePosts()
{
	const double R = TableLayout.BallRadius;

	for (const FPinballPost& Post : TableLayout.Posts)
	{
		const FPinVec2 Offset = BallPos - Post.Center;
		const double MinDist = R + Post.Radius;
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= MinDist * MinDist)
		{
			continue;
		}
		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : FPinVec2(0.0, 1.0);
		Resolve(Normal, MinDist - Dist, FPinVec2(), RubberRestitution);
	}
}

void FPinballSim::CollideFlippers()
{
	const double R = TableLayout.BallRadius;

	for (int Side = 0; Side < 2; ++Side)
	{
		const FPinballFlipperDef& Def = TableLayout.Flippers[Side];
		const FPinVec2 Tip = FlipperTip(Side);

		const double T = PinMath::ClosestT(BallPos, Def.Pivot, Tip);
		const FPinVec2 Closest = Def.Pivot + (Tip - Def.Pivot) * T;
		const double FlipperRadius = Def.BaseRadius + (Def.TipRadius - Def.BaseRadius) * T;

		const FPinVec2 Offset = BallPos - Closest;
		const double MinDist = R + FlipperRadius;
		const double Dist2 = PinMath::Dot(Offset, Offset);
		if (Dist2 >= MinDist * MinDist)
		{
			continue;
		}

		const double Dist = std::sqrt(Dist2);
		const FPinVec2 Normal = Dist > 1e-9 ? Offset * (1.0 / Dist) : FPinVec2(0.0, 1.0);

		// The velocity of the flipper's surface at the point of contact: omega × r. This is
		// the whole of what makes a flipper shot — the ball is struck by a moving surface,
		// harder the further out along the bat it sits.
		const FPinVec2 Contact = Closest + Normal * FlipperRadius;
		const FPinVec2 SurfaceVelocity = PinMath::Perp(Contact - Def.Pivot) * FlipperStates[Side].AngularVelocity;

		Resolve(Normal, MinDist - Dist, SurfaceVelocity, FlipperRestitution);
	}
}

void FPinballSim::UpdateRollovers()
{
	bool bSetMemberLit = false;

	for (size_t I = 0; I < TableLayout.Rollovers.size(); ++I)
	{
		const FPinballRollover& Rollover = TableLayout.Rollovers[I];
		const FPinVec2 Offset = BallPos - Rollover.Center;
		const bool bInside = PinMath::Dot(Offset, Offset) < Rollover.Radius * Rollover.Radius;

		// On entry only: a ball sitting on a rollover scores once, not every substep. A set
		// member then stays lit until the set is complete; anything else pays every pass.
		if (bInside && RolloverInside[I] == 0 && (!Rollover.bInSet || RolloverLit[I] == 0))
		{
			CurrentScore += Rollover.Points;
			Emit(EPinballEvent::RolloverLit, static_cast<int>(I), Rollover.Points);
			if (Rollover.bInSet)
			{
				RolloverLit[I] = 1;
				bSetMemberLit = true;
			}
		}
		RolloverInside[I] = bInside ? 1 : 0;
	}

	if (!bSetMemberLit)
	{
		return;
	}
	for (size_t I = 0; I < TableLayout.Rollovers.size(); ++I)
	{
		if (TableLayout.Rollovers[I].bInSet && RolloverLit[I] == 0)
		{
			return;
		}
	}
	CurrentScore += TableLayout.RolloverSetBonus;
	Emit(EPinballEvent::RolloversComplete, -1, TableLayout.RolloverSetBonus);
	for (size_t I = 0; I < TableLayout.Rollovers.size(); ++I)
	{
		if (TableLayout.Rollovers[I].bInSet)
		{
			RolloverLit[I] = 0;
		}
	}
}

void FPinballSim::UpdateKickouts()
{
	// Only a slow ball drops in. A fast one skims straight over the hole, as on the real thing.
	if (CapturedKickout >= 0 || PinMath::Length(BallVel) > KickoutCaptureSpeed)
	{
		return;
	}
	for (size_t I = 0; I < TableLayout.Kickouts.size(); ++I)
	{
		if (KickoutCooldown[I] > 0.0)
		{
			continue;   // the ball it has just kicked out is not swallowed again on the way past
		}
		const FPinballKickout& Hole = TableLayout.Kickouts[I];
		const FPinVec2 Offset = BallPos - Hole.Center;
		if (PinMath::Dot(Offset, Offset) >= Hole.Radius * Hole.Radius)
		{
			continue;
		}
		CapturedKickout = static_cast<int>(I);
		KickoutHeldSeconds = 0.0;
		BallPos = Hole.Center;
		BallVel = FPinVec2();
		CurrentScore += Hole.Points;
		Emit(EPinballEvent::KickoutCaptured, static_cast<int>(I), Hole.Points);
		return;
	}
}

void FPinballSim::EjectKickout()
{
	const size_t I = static_cast<size_t>(CapturedKickout);
	const FPinballKickout& Hole = TableLayout.Kickouts[I];
	BallPos = Hole.Center;
	BallVel = PinMath::Normalized(Hole.EjectDirection) * Hole.EjectSpeed;
	KickoutCooldown[I] = KickoutRearmSeconds;
	Emit(EPinballEvent::KickoutEjected, CapturedKickout);
	CapturedKickout = -1;
	KickoutHeldSeconds = 0.0;
}

void FPinballSim::DrainBall()
{
	Emit(EPinballEvent::BallDrained, CurrentBall);
	if (CurrentBall >= BallsPerGame)
	{
		CurrentPhase = EPinballPhase::GameOver;
		BallVel = FPinVec2();
		Emit(EPinballEvent::GameOver, -1, CurrentScore);
		return;
	}
	++CurrentBall;
	ServeBall();
}

double FPinballSim::FlipperAngle(int Side) const
{
	return (Side == 0 || Side == 1) ? FlipperStates[Side].Angle : 0.0;
}

FPinVec2 FPinballSim::FlipperTip(int Side) const
{
	if (Side != 0 && Side != 1)
	{
		return FPinVec2();
	}
	const FPinballFlipperDef& Def = TableLayout.Flippers[Side];
	const double Angle = FlipperStates[Side].Angle;
	return Def.Pivot + FPinVec2(std::cos(Angle), std::sin(Angle)) * Def.Length;
}

bool FPinballSim::IsRolloverLit(size_t Index) const
{
	return Index < RolloverLit.size() && RolloverLit[Index] != 0;
}

std::vector<FPinballEvent> FPinballSim::TakeEvents()
{
	std::vector<FPinballEvent> Out;
	Out.swap(PendingEvents);
	return Out;
}

void FPinballSim::PlaceBall(FPinVec2 Position, FPinVec2 Velocity)
{
	BallPos = Position;
	BallVel = Velocity;
	CapturedKickout = -1;
	KickoutHeldSeconds = 0.0;
	CurrentPhase = EPinballPhase::InPlay;
	if (CurrentBall == 0)
	{
		CurrentBall = 1;
	}
}

void FPinballSim::Emit(EPinballEvent Type, int Index, int Points)
{
	PendingEvents.push_back(FPinballEvent{ Type, Index, Points });
}
