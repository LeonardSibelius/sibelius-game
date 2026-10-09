// KartSim.cpp — see the header for the design, and Tools/Tests/kart_sim_test.cpp for what it
// is held to.

#include "KartSim.h"

#include <algorithm>
#include <cmath>

// NAMED, NOT ANONYMOUS: the unity-build rule (see PinballSim.cpp's PinMath).
namespace KartDetail
{
	constexpr double Pi = 3.14159265358979323846;

	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline double Sign(double V) { return V > 0.0 ? 1.0 : (V < 0.0 ? -1.0 : 0.0); }

	inline double WrapAngle(double A)
	{
		while (A > Pi) A -= 2.0 * Pi;
		while (A < -Pi) A += 2.0 * Pi;
		return A;
	}

	inline FKartVec FromAngle(double A) { return FKartVec(std::cos(A), std::sin(A)); }

	inline FKartVec Rotate(const FKartVec& V, double A)
	{
		const double C = std::cos(A);
		const double S = std::sin(A);
		return FKartVec(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}
}

double FKartVec::Length() const
{
	return std::sqrt(X * X + Y * Y);
}

FKartVec FKartVec::Normalised() const
{
	const double L = Length();
	return L > 1e-12 ? FKartVec(X / L, Y / L) : FKartVec(1.0, 0.0);
}

const char* KartItemName(EKartItem Item)
{
	switch (Item)
	{
	case EKartItem::Sauce:    return "Sauce";
	case EKartItem::Slap:     return "Slap";
	case EKartItem::Generate: return "Generate";
	case EKartItem::Refactor: return "Refactor";
	default:                  return "";
	}
}

double FKartState::ForwardSpeed() const
{
	return V.Dot(KartDetail::FromAngle(Heading));
}

// =====================================================================================
// THE TRACK

bool FKartTrack::Build(const std::vector<FKartTrackPoint>& Corners, double InHalfWidth, double FilletRadius,
	double Step, std::string* Error)
{
	using namespace KartDetail;
	Pts.clear();
	Cum.clear();
	Curv.clear();
	Heights.clear();
	Widths.clear();
	Len = 0.0;
	Half = InHalfWidth;
	MinR = 0.0;
	auto Fail = [Error](const std::string& Why)
	{
		if (Error)
		{
			*Error = Why;
		}
		return false;
	};

	const int N = static_cast<int>(Corners.size());
	if (N < 3)
	{
		return Fail("a track needs at least three corners");
	}
	if (InHalfWidth <= 0.0 || Step <= 1.0)
	{
		return Fail("the half-width and the step must be positive");
	}
	auto At = [&Corners, N](int I) { const FKartTrackPoint& C = Corners[static_cast<size_t>(((I % N) + N) % N)]; return FKartVec(C.X, C.Y); };
	auto ZAt = [&Corners, N](int I) { return Corners[static_cast<size_t>(((I % N) + N) % N)].Z; };
	auto HAt = [&Corners, N, InHalfWidth](int I)
	{
		const double W = Corners[static_cast<size_t>(((I % N) + N) % N)].HalfWidth;
		return W > 0.0 ? W : InHalfWidth;
	};

	// At each corner: the turn, and how far before and after it the arc meets the straights.
	std::vector<double> Turn(static_cast<size_t>(N));
	std::vector<double> Cut(static_cast<size_t>(N));
	for (int I = 0; I < N; ++I)
	{
		const FKartVec In = (At(I) - At(I - 1)).Normalised();
		const FKartVec Out = (At(I + 1) - At(I)).Normalised();
		if ((At(I) - At(I - 1)).Length() < 1.0)
		{
			return Fail("two corners in the same place");
		}
		Turn[static_cast<size_t>(I)] = std::atan2(In.Cross(Out), In.Dot(Out));
		if (std::abs(Turn[static_cast<size_t>(I)]) > Pi * 0.97)
		{
			return Fail("a corner turns all the way back on itself");
		}
		Cut[static_cast<size_t>(I)] = FilletRadius * std::tan(std::abs(Turn[static_cast<size_t>(I)]) * 0.5);
		if (std::abs(Turn[static_cast<size_t>(I)]) > 1e-9 && FilletRadius <= HAt(I))
		{
			return Fail("the corner radius must be wider than the road at corner " + std::to_string(I) + ", or the inside of the corner folds over itself");
		}
		Half = std::max(Half, HAt(I));
	}
	for (int I = 0; I < N; ++I)
	{
		const double Straight = (At(I + 1) - At(I)).Length();
		if (Cut[static_cast<size_t>(I)] + Cut[static_cast<size_t>((I + 1) % N)] > Straight)
		{
			return Fail("corner " + std::to_string(I) + " and the next are too close for their radius");
		}
	}

	// Walk the loop from the middle of the straight after corner 0, so the start line is on a
	// straight and the grid stands on it, behind.
	auto Emit = [this](const FKartVec& P, double Curvature, double Z, double W)
	{
		if (!Pts.empty() && (P - Pts.back()).Length() < 1e-6)
		{
			return;
		}
		Pts.push_back(P);
		Curv.push_back(Curvature);
		Heights.push_back(Z);
		Widths.push_back(W);
	};
	// Where along the straight from corner I to I+1 a point is, 0..1, for its height and width.
	auto Fraction = [&](int I, const FKartVec& P)
	{
		const FKartVec A = At(I);
		const FKartVec B = At(I + 1);
		const double L2 = (B - A).Dot(B - A);
		return L2 > 0.0 ? Clamp((P - A).Dot(B - A) / L2, 0.0, 1.0) : 0.0;
	};
	auto StraightPiece = [&](int I, const FKartVec& A, const FKartVec& B)
	{
		const double L = (B - A).Length();
		const int Pieces = std::max(1, static_cast<int>(std::ceil(L / Step)));
		for (int K = 0; K < Pieces; ++K)
		{
			const FKartVec P = A + (B - A) * (static_cast<double>(K) / Pieces);
			const double T = Fraction(I, P);
			Emit(P, 0.0, ZAt(I) + (ZAt(I + 1) - ZAt(I)) * T, HAt(I) + (HAt(I + 1) - HAt(I)) * T);
		}
	};
	auto Arc = [&](int I)
	{
		const double Th = Turn[static_cast<size_t>(I)];
		if (std::abs(Th) < 1e-9)
		{
			return;
		}
		const FKartVec In = (At(I) - At(I - 1)).Normalised();
		const FKartVec Start = At(I) - In * Cut[static_cast<size_t>(I)];
		const FKartVec Centre = Start + In.Left() * (FilletRadius * Sign(Th));
		// Three times finer than the straights: the corridor's edge on a corner is the arc
		// itself, not a run of chords a kart could feel.
		const int Pieces = std::max(2, static_cast<int>(std::ceil(FilletRadius * std::abs(Th) / (Step / 3.0))));
		for (int K = 0; K < Pieces; ++K)
		{
			const double A = Th * static_cast<double>(K) / Pieces;
			Emit(Centre + Rotate(Start - Centre, A), Sign(Th) / FilletRadius, ZAt(I), HAt(I));
		}
	};
	auto StraightEnds = [&](int I, FKartVec& A, FKartVec& B)
	{
		const FKartVec Dir = (At(I + 1) - At(I)).Normalised();
		A = At(I) + Dir * Cut[static_cast<size_t>(I)];
		B = At(I + 1) - Dir * Cut[static_cast<size_t>((I + 1) % N)];
	};

	FKartVec A0;
	FKartVec B0;
	StraightEnds(0, A0, B0);
	const FKartVec Mid = (A0 + B0) * 0.5;
	StraightPiece(0, Mid, B0);
	for (int I = 1; I <= N; ++I)
	{
		Arc(I % N);
		FKartVec A;
		FKartVec B;
		StraightEnds(I % N, A, B);
		StraightPiece(I % N, A, I < N ? B : Mid);
	}

	const size_t Count = Pts.size();
	if (Count < 3)
	{
		return Fail("the track came out empty");
	}
	Cum.resize(Count);
	double Along = 0.0;
	for (size_t I = 0; I < Count; ++I)
	{
		Cum[I] = Along;
		Along += (Pts[(I + 1) % Count] - Pts[I]).Length();
	}
	Len = Along;
	MinR = FilletRadius;
	return true;
}

double FKartTrack::Wrap(double S) const
{
	if (Len <= 0.0)
	{
		return 0.0;
	}
	double W = std::fmod(S, Len);
	if (W < 0.0)
	{
		W += Len;
	}
	return W >= Len ? 0.0 : W;
}

double FKartTrack::Gap(double From, double To) const
{
	double G = Wrap(To - From);
	if (G > Len * 0.5)
	{
		G -= Len;
	}
	return G;
}

int FKartTrack::SegmentAt(double S) const
{
	const double W = Wrap(S);
	const auto It = std::upper_bound(Cum.begin(), Cum.end(), W);
	const int I = static_cast<int>(It - Cum.begin()) - 1;
	return std::max(0, std::min(I, static_cast<int>(Cum.size()) - 1));
}

FKartVec FKartTrack::PointAt(double S) const
{
	if (Pts.empty())
	{
		return FKartVec();
	}
	const int I = SegmentAt(S);
	const size_t A = static_cast<size_t>(I);
	const size_t B = (A + 1) % Pts.size();
	const double SegLen = (Pts[B] - Pts[A]).Length();
	const double T = SegLen > 0.0 ? KartDetail::Clamp((Wrap(S) - Cum[A]) / SegLen, 0.0, 1.0) : 0.0;
	return Pts[A] + (Pts[B] - Pts[A]) * T;
}

FKartVec FKartTrack::TangentAt(double S) const
{
	if (Pts.size() < 2)
	{
		return FKartVec(1.0, 0.0);
	}
	const size_t A = static_cast<size_t>(SegmentAt(S));
	return (Pts[(A + 1) % Pts.size()] - Pts[A]).Normalised();
}

double FKartTrack::CurvatureAt(double S) const
{
	return Curv.empty() ? 0.0 : Curv[static_cast<size_t>(SegmentAt(S))];
}

double FKartTrack::HeightAt(double S) const
{
	if (Heights.empty())
	{
		return 0.0;
	}
	const size_t A = static_cast<size_t>(SegmentAt(S));
	const size_t B = (A + 1) % Pts.size();
	const double SegLen = (Pts[B] - Pts[A]).Length();
	const double T = SegLen > 0.0 ? KartDetail::Clamp((Wrap(S) - Cum[A]) / SegLen, 0.0, 1.0) : 0.0;
	return Heights[A] + (Heights[B] - Heights[A]) * T;
}

double FKartTrack::HalfWidthAt(double S) const
{
	if (Widths.empty())
	{
		return Half;
	}
	const size_t A = static_cast<size_t>(SegmentAt(S));
	const size_t B = (A + 1) % Pts.size();
	const double SegLen = (Pts[B] - Pts[A]).Length();
	const double T = SegLen > 0.0 ? KartDetail::Clamp((Wrap(S) - Cum[A]) / SegLen, 0.0, 1.0) : 0.0;
	return Widths[A] + (Widths[B] - Widths[A]) * T;
}

double FKartTrack::ProjectOnSegment(int I, const FKartVec& P, double& T) const
{
	const size_t A = static_cast<size_t>(I);
	const size_t B = (A + 1) % Pts.size();
	const FKartVec AB = Pts[B] - Pts[A];
	const double L2 = AB.Dot(AB);
	T = L2 > 0.0 ? KartDetail::Clamp((P - Pts[A]).Dot(AB) / L2, 0.0, 1.0) : 0.0;
	return (P - (Pts[A] + AB * T)).Length();
}

FKartTrack::FFrame FKartTrack::Locate(const FKartVec& P, int Hint) const
{
	FFrame F;
	const int Count = static_cast<int>(Pts.size());
	if (Count < 2)
	{
		return F;
	}
	int Best = 0;
	double BestT = 0.0;
	double BestDist = 1e300;
	auto Try = [&](int I)
	{
		double T = 0.0;
		const double Dist = ProjectOnSegment(I, P, T);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = I;
			BestT = T;
		}
	};
	if (Hint >= 0 && Hint < Count)
	{
		for (int K = -12; K <= 12; ++K)
		{
			Try(((Hint + K) % Count + Count) % Count);
		}
	}
	// A kart is never further than the half-width from the centre line, so a local answer
	// further than that is the wrong stretch of road: look everywhere.
	if (Hint < 0 || BestDist > Half * 1.25)
	{
		for (int I = 0; I < Count; ++I)
		{
			Try(I);
		}
	}
	const size_t A = static_cast<size_t>(Best);
	const size_t B = (A + 1) % Pts.size();
	const FKartVec AB = Pts[B] - Pts[A];
	const FKartVec Foot = Pts[A] + AB * BestT;
	F.Segment = Best;
	F.Tangent = AB.Normalised();
	F.S = Wrap(Cum[A] + AB.Length() * BestT);
	F.D = F.Tangent.Cross(P - Foot);
	return F;
}

// =====================================================================================
// THE RACE

bool FKartRace::Start(const FKartTrack& OnTrack, const FKartRaceSetup& InSetup)
{
	if (!OnTrack.IsBuilt() || InSetup.Karts < 1 || InSetup.Karts > 12 || InSetup.Laps < 1
		|| InSetup.Player < -1 || InSetup.Player >= InSetup.Karts)
	{
		return false;
	}
	TheTrack = OnTrack;
	Setup = InSetup;
	const size_t N = static_cast<size_t>(Setup.Karts);
	State.assign(N, FKartState());
	Inputs.assign(N, FKartInput());
	Lane.assign(N, 0.0);
	StuckClock.assign(N, 0.0);
	StuckProgress.assign(N, -1e300);
	RecoverLeft.assign(N, 0.0);
	ItemHold.assign(N, 0.0);
	PrevS.assign(N, 0.0);
	BestLap.assign(N, 0);
	Events.clear();
	ObstacleList.clear();
	BoxList.clear();
	Rng = static_cast<uint64_t>(Setup.Seed) * 2654435761ull + 0x9E3779B97F4A7C15ull;
	if (Rng == 0)
	{
		Rng = 1;
	}

	const double L = TheTrack.Length();
	const double Half = TheTrack.HalfWidth();
	for (size_t I = 0; I < N; ++I)
	{
		FKartState& K = State[I];
		const int Row = static_cast<int>(I) / 2;
		const double S = TheTrack.Wrap(-(700.0 + 650.0 * Row));
		const double Side = std::min(250.0, std::max(0.0, TheTrack.HalfWidthAt(S) - Params.KartRadius - 40.0));
		const double D = (I % 2 == 0) ? Side : -Side;
		const FKartVec T = TheTrack.TangentAt(S);
		K.P = TheTrack.PointAt(S) + T.Left() * D;
		K.Heading = std::atan2(T.Y, T.X);
		const FKartTrack::FFrame F = TheTrack.Locate(K.P);
		K.S = F.S;
		K.D = F.D;
		K.Segment = F.Segment;
		K.Lap = (K.S > L * 0.5) ? -1 : 0;
		K.Progress = K.Lap * L + K.S;
		PrevS[I] = K.S;
		// Each agent keeps to a lane of her own, so the pack spreads across the road.
		Lane[I] = (static_cast<double>((I * 3) % 5) - 2.0) / 2.0 * 0.32 * Half;
	}

	// Item boxes: a row across the road on three straights, never on a corner.
	auto OnStraight = [this](double S) { return TheTrack.CurvatureAt(S) == 0.0 && TheTrack.CurvatureAt(S + 400.0) == 0.0 && TheTrack.CurvatureAt(S - 400.0) == 0.0; };
	const double Rows[3] = { 0.28, 0.58, 0.84 };
	for (const double Fraction : Rows)
	{
		double S = L * Fraction;
		for (int Tries = 0; Tries < 400 && !OnStraight(S); ++Tries)
		{
			S += 150.0;
		}
		for (const double Across : { -0.6, -0.2, 0.2, 0.6 })
		{
			FKartBox Box;
			Box.S = TheTrack.Wrap(S);
			Box.D = Across * (TheTrack.HalfWidthAt(Box.S) - Params.KartRadius);
			Box.P = TheTrack.PointAt(Box.S) + TheTrack.TangentAt(Box.S).Left() * Box.D;
			BoxList.push_back(Box);
		}
	}

	RaceClock = 0.0;
	PlayerDoneClock = -1.0;
	OthersDoneClock = -1.0;
	Pending = 0.0;
	CountdownClock = std::max(0.0, Setup.CountdownSeconds);
	LastCountdownShown = 0;
	CurrentPhase = CountdownClock > 0.0 ? EKartPhase::Countdown : EKartPhase::Racing;
	if (CurrentPhase == EKartPhase::Racing)
	{
		Emit(EKartEvent::Go, -1);
	}
	Rank();
	return true;
}

void FKartRace::SetInput(int Kart, const FKartInput& Input)
{
	if (Kart >= 0 && Kart < static_cast<int>(Inputs.size()))
	{
		const bool bPendingUse = Inputs[static_cast<size_t>(Kart)].bUseItem;
		Inputs[static_cast<size_t>(Kart)] = Input;
		// A press survives until a step has seen it, however the frames and steps fall.
		Inputs[static_cast<size_t>(Kart)].bUseItem = Input.bUseItem || bPendingUse;
	}
}

void FKartRace::Place(int Kart, const FKartVec& P, double Heading, const FKartVec& V)
{
	if (Kart < 0 || Kart >= static_cast<int>(State.size()))
	{
		return;
	}
	FKartState& K = State[static_cast<size_t>(Kart)];
	K.P = P;
	K.Heading = Heading;
	K.V = V;
	const FKartTrack::FFrame F = TheTrack.Locate(P);
	K.S = F.S;
	K.D = F.D;
	K.Segment = F.Segment;
	PrevS[static_cast<size_t>(Kart)] = K.S;
	K.Progress = K.Lap * TheTrack.Length() + K.S;
}

void FKartRace::Give(int Kart, EKartItem Item)
{
	if (Kart >= 0 && Kart < static_cast<int>(State.size()))
	{
		State[static_cast<size_t>(Kart)].Item = Item;
	}
}

std::vector<FKartEventRecord> FKartRace::TakeEvents()
{
	std::vector<FKartEventRecord> Out;
	Out.swap(Events);
	return Out;
}

void FKartRace::Emit(EKartEvent Type, int Kart, int Other, double Value, EKartItem Item)
{
	FKartEventRecord E;
	E.Type = Type;
	E.Kart = Kart;
	E.Other = Other;
	E.Value = Value;
	E.Item = Item;
	Events.push_back(E);
	if (Events.size() > 512)
	{
		Events.erase(Events.begin(), Events.begin() + 256);   // nobody is reading: keep the newest
	}
}

double FKartRace::Random01()
{
	// xorshift64*
	Rng ^= Rng >> 12;
	Rng ^= Rng << 25;
	Rng ^= Rng >> 27;
	return static_cast<double>((Rng * 2685821657736338717ull) >> 11) / 9007199254740992.0;
}

std::vector<int> FKartRace::Standings() const
{
	std::vector<int> Order(State.size());
	for (size_t I = 0; I < Order.size(); ++I)
	{
		Order[I] = static_cast<int>(I);
	}
	std::stable_sort(Order.begin(), Order.end(), [this](int A, int B)
	{
		const FKartState& KA = State[static_cast<size_t>(A)];
		const FKartState& KB = State[static_cast<size_t>(B)];
		if (KA.bFinished != KB.bFinished)
		{
			return KA.bFinished;
		}
		if (KA.bFinished)
		{
			return KA.FinishTime < KB.FinishTime;
		}
		return KA.Progress > KB.Progress;
	});
	return Order;
}

void FKartRace::Rank()
{
	const std::vector<int> Order = Standings();
	for (size_t I = 0; I < Order.size(); ++I)
	{
		State[static_cast<size_t>(Order[I])].Place = static_cast<int>(I) + 1;
	}
}

bool FKartRace::IsDrivenBySim(int Kart) const
{
	return Kart != Setup.Player || State[static_cast<size_t>(Kart)].bFinished || CurrentPhase == EKartPhase::Over;
}

double FKartRace::PaceOf(int Kart) const
{
	if (Kart == Setup.Player && !State[static_cast<size_t>(Kart)].bFinished)
	{
		return 1.0;
	}
	double Pace = Setup.AiPace;
	if (static_cast<size_t>(Kart) < Setup.Skill.size())
	{
		Pace *= Setup.Skill[static_cast<size_t>(Kart)];
	}
	// THE CATCH-UP: a family race should stay a race. Far behind him, an agent finds a little
	// more; far ahead, she eases off. Never more than a few per cent either way.
	if (Setup.Player >= 0 && CurrentPhase == EKartPhase::Racing && !State[static_cast<size_t>(Setup.Player)].bFinished)
	{
		const double Behind = State[static_cast<size_t>(Setup.Player)].Progress - State[static_cast<size_t>(Kart)].Progress;
		Pace *= 1.0 + KartDetail::Clamp(Behind / 4000.0 * 0.04, -0.07, 0.08);
	}
	return Pace;
}

double FKartRace::CornerSpeedAt(double S) const
{
	const double K = std::abs(TheTrack.CurvatureAt(S));
	if (K < 1e-9)
	{
		return 1e9;
	}
	return std::sqrt(Params.MaxLateral * 0.8 / K);
}

double FKartRace::TargetSpeedAt(double S, double Pace) const
{
	double Best = Params.MaxSpeed * Pace;
	for (double Ahead = 0.0; Ahead <= 6000.0; Ahead += 150.0)
	{
		const double Corner = CornerSpeedAt(S + Ahead);
		if (Corner > 1e8)
		{
			continue;
		}
		// The speed from which braking at four fifths of full gets down to it in time.
		Best = std::min(Best, std::sqrt(Corner * Corner + 2.0 * Params.Brake * 0.8 * Ahead));
	}
	return Best;
}

void FKartRace::Advance(double Seconds)
{
	if (CurrentPhase == EKartPhase::Idle)
	{
		return;
	}
	Pending += std::min(std::max(0.0, Seconds), 0.25);   // a hitch never becomes a lurch
	const double Dt = Params.StepSeconds;
	while (Pending >= Dt)
	{
		Step(Dt);
		Pending -= Dt;
	}
}

void FKartRace::Step(double Dt)
{
	if (CurrentPhase == EKartPhase::Countdown)
	{
		CountdownClock -= Dt;
		const int Showing = static_cast<int>(std::ceil(CountdownClock));
		if (Showing > 0 && Showing != LastCountdownShown)
		{
			LastCountdownShown = Showing;
			Emit(EKartEvent::Countdown, -1, -1, Showing);
		}
		if (CountdownClock <= 0.0)
		{
			CountdownClock = 0.0;
			CurrentPhase = EKartPhase::Racing;
			Emit(EKartEvent::Go, -1);
		}
		return;
	}

	if (CurrentPhase == EKartPhase::Racing)
	{
		RaceClock += Dt;
	}

	const size_t N = State.size();
	for (size_t I = 0; I < N; ++I)
	{
		const int K = static_cast<int>(I);
		PrevS[I] = State[I].S;
		State[I].WallContact = 0.0;
		const FKartInput In = IsDrivenBySim(K) ? Drive(K) : Inputs[I];
		Items(K, In, Dt);
		Physics(K, In, Dt);
		KeepInside(K);
	}
	if (Setup.Player >= 0)
	{
		Inputs[static_cast<size_t>(Setup.Player)].bUseItem = false;
	}
	Collide();
	for (size_t I = 0; I < N; ++I)
	{
		CountLaps(static_cast<int>(I));
	}
	Rank();

	for (FKartBox& Box : BoxList)
	{
		Box.RespawnLeft = std::max(0.0, Box.RespawnLeft - Dt);
	}
	for (FKartObstacle& O : ObstacleList)
	{
		O.Left -= Dt;
	}
	ObstacleList.erase(std::remove_if(ObstacleList.begin(), ObstacleList.end(),
		[](const FKartObstacle& O) { return O.Left <= 0.0; }), ObstacleList.end());

	// The agents' patience: one that has made no progress for a while backs off and tries again.
	for (size_t I = 0; I < N; ++I)
	{
		FKartState& K = State[I];
		RecoverLeft[I] = std::max(0.0, RecoverLeft[I] - Dt);
		ItemHold[I] = std::max(0.0, ItemHold[I] - Dt);
		if (!IsDrivenBySim(static_cast<int>(I)) || K.SpinLeft > 0.0 || RecoverLeft[I] > 0.0)
		{
			StuckClock[I] = 0.0;
			StuckProgress[I] = K.Progress;
			continue;
		}
		if (K.Progress > StuckProgress[I] + 250.0)
		{
			StuckProgress[I] = K.Progress;
			StuckClock[I] = 0.0;
		}
		else
		{
			StuckClock[I] += Dt;
			if (StuckClock[I] > 1.6)
			{
				RecoverLeft[I] = 0.9;
				StuckClock[I] = 0.0;
				StuckProgress[I] = K.Progress;
			}
		}
	}

	if (CurrentPhase == EKartPhase::Racing)
	{
		bool bAllDone = true;
		bool bOthersDone = true;
		for (size_t I = 0; I < N; ++I)
		{
			bAllDone = bAllDone && State[I].bFinished;
			bOthersDone = bOthersDone && (State[I].bFinished || static_cast<int>(I) == Setup.Player);
		}
		if (Setup.Player >= 0 && State[static_cast<size_t>(Setup.Player)].bFinished && PlayerDoneClock < 0.0)
		{
			PlayerDoneClock = RaceClock;
		}
		if (Setup.Player >= 0 && bOthersDone && OthersDoneClock < 0.0)
		{
			OthersDoneClock = RaceClock;
		}
		const bool bWaitedForHim = PlayerDoneClock >= 0.0 && RaceClock - PlayerDoneClock >= Setup.AfterPlayerFinish;
		const bool bWaitedForThem = OthersDoneClock >= 0.0 && RaceClock - OthersDoneClock >= Setup.AfterOthersFinish;
		if (bAllDone || bWaitedForHim || bWaitedForThem || RaceClock >= Setup.TimeLimit)
		{
			CurrentPhase = EKartPhase::Over;
			Emit(EKartEvent::RaceOver, -1);
		}
	}
}

FKartInput FKartRace::Drive(int Kart)
{
	using namespace KartDetail;
	FKartInput In;
	const size_t I = static_cast<size_t>(Kart);
	const FKartState& K = State[I];
	const double Half = TheTrack.HalfWidthAt(K.S);
	const double Edge = std::max(0.0, Half - Params.KartRadius - 60.0);
	const FKartVec Facing = FromAngle(K.Heading);

	if (RecoverLeft[I] > 0.0)
	{
		// Backing off a wall: reverse, steering so the nose swings round to the road.
		const FKartVec T = TheTrack.TangentAt(K.S);
		In.Throttle = -1.0;
		In.Steer = -Sign(Facing.Cross(T));
		return In;
	}

	const double Speed = K.Speed();
	const double Look = 350.0 + Speed * 0.45;
	double Want = Lane[I];

	// On a corner, take the inside of it.
	const double Bend = TheTrack.CurvatureAt(K.S + Look);
	if (Bend != 0.0)
	{
		Want = Sign(Bend) * 0.45 * TheTrack.HalfWidthAt(K.S + Look);
	}
	// An item box ahead, with nothing in hand: drive through it.
	if (K.Item == EKartItem::None && K.ChestLeft <= 0.0)
	{
		double Nearest = 1e9;
		for (const FKartBox& Box : BoxList)
		{
			const double Ahead = TheTrack.Gap(K.S, Box.S);
			if (Box.RespawnLeft <= 0.0 && Ahead > 200.0 && Ahead < 1600.0 && Ahead < Nearest)
			{
				Nearest = Ahead;
				Want = Box.D;
			}
		}
	}
	// A kart or an obstacle in the lane ahead: go round it, on whichever side has more room.
	auto Avoid = [&](double Ahead, double D, double Clearance)
	{
		if (Ahead > 0.0 && Ahead < 1400.0 && std::abs(Want - D) < Clearance)
		{
			const double Left = D + Clearance;
			const double Right = D - Clearance;
			Want = (std::abs(Left) <= Edge && (std::abs(Right) > Edge || std::abs(Left - Want) < std::abs(Right - Want))) ? Left : Right;
		}
	};
	for (size_t J = 0; J < State.size(); ++J)
	{
		if (J != I)
		{
			Avoid(TheTrack.Gap(K.S, State[J].S), State[J].D, 2.4 * Params.KartRadius);
		}
	}
	for (const FKartObstacle& O : ObstacleList)
	{
		const FKartTrack::FFrame F = TheTrack.Locate(O.P, K.Segment);
		Avoid(TheTrack.Gap(K.S, F.S), F.D, Params.KartRadius + Params.ObstacleRadius + 80.0);
	}
	Want = Clamp(Want, -Edge, Edge);

	const double AimS = K.S + Look;
	const FKartVec Aim = TheTrack.PointAt(AimS) + TheTrack.TangentAt(AimS).Left() * Want;
	const FKartVec To = Aim - K.P;
	const double Error = WrapAngle(std::atan2(To.Y, To.X) - K.Heading);
	In.Steer = Clamp(Error * 2.6, -1.0, 1.0);

	const double Target = TargetSpeedAt(K.S, PaceOf(Kart));
	const double Forward = K.ForwardSpeed();
	if (std::abs(Error) > 1.2)
	{
		In.Throttle = 0.4;                      // facing the wrong way: come round gently
	}
	else if (Forward < Target - 60.0)
	{
		In.Throttle = 1.0;
	}
	else if (Forward > Target + 120.0)
	{
		In.Throttle = -1.0;
	}
	else
	{
		In.Throttle = Clamp((Target - Forward) / 120.0, -1.0, 1.0);
	}

	// Items: a moment's thought first, then use it when it will do something.
	if (K.Item != EKartItem::None && ItemHold[I] <= 0.0)
	{
		bool bUse = false;
		switch (K.Item)
		{
		case EKartItem::Sauce:
		{
			bool bClear = true;
			for (double Ahead = 0.0; Ahead < 1800.0 && bClear; Ahead += 150.0)
			{
				bClear = TheTrack.CurvatureAt(K.S + Ahead) == 0.0;
			}
			bUse = bClear;
			break;
		}
		case EKartItem::Slap:
			bUse = NearestTarget(Kart, false) >= 0;
			break;
		case EKartItem::Generate:
			for (size_t J = 0; J < State.size(); ++J)
			{
				const double Behind = TheTrack.Gap(State[J].S, K.S);
				bUse = bUse || (J != I && Behind > 0.0 && Behind < 1500.0 && std::abs(State[J].D - K.D) < Half * 0.7);
			}
			break;
		case EKartItem::Refactor:
			bUse = K.Place > 1 || NearestTarget(Kart, false) >= 0;
			break;
		default:
			break;
		}
		In.bUseItem = bUse;
	}
	return In;
}

void FKartRace::Physics(int Kart, const FKartInput& InRaw, double Dt)
{
	using namespace KartDetail;
	FKartState& K = State[static_cast<size_t>(Kart)];
	FKartInput In = InRaw;
	In.Throttle = Clamp(In.Throttle, -1.0, 1.0);
	In.Steer = Clamp(In.Steer, -1.0, 1.0);
	const bool bSpinning = K.SpinLeft > 0.0;
	if (bSpinning)
	{
		In = FKartInput();
	}

	FKartVec Facing = FromAngle(K.Heading);
	double Forward = K.V.Dot(Facing);
	const double Cap = Params.MaxSpeed * PaceOf(Kart) * (K.BoostLeft > 0.0 ? Params.BoostSpeed : 1.0)
		* (K.ChestLeft > 0.0 ? Params.ChestSpeed : 1.0);

	// --- the drift -----------------------------------------------------------------------
	if (!K.bDrifting && In.bDrift && std::abs(In.Steer) > 0.3 && Forward > Params.DriftMinSpeed && K.ChestLeft <= 0.0)
	{
		K.bDrifting = true;
		K.DriftDir = In.Steer > 0.0 ? 1 : -1;
		K.DriftCharge = 0.0;
	}
	if (K.bDrifting)
	{
		const bool bLetGo = !In.bDrift;
		if (bLetGo || Forward < Params.DriftMinSpeed * 0.7 || bSpinning)
		{
			if (bLetGo)
			{
				const double Boost = K.DriftCharge >= Params.SuperBoostCharge ? 1.2 : (K.DriftCharge >= Params.MiniBoostCharge ? 0.6 : 0.0);
				if (Boost > 0.0)
				{
					K.BoostLeft = std::max(K.BoostLeft, Boost);
					Emit(EKartEvent::Boosted, Kart, -1, Boost);
				}
			}
			K.bDrifting = false;
			K.DriftDir = 0;
			K.DriftCharge = 0.0;
		}
	}

	// --- steering: a yaw rate, sharp when slow, gentler when fast --------------------------
	const double Moving = std::min(1.0, std::abs(Forward) / 350.0);
	const double Agility = Params.MaxYawRate * Moving / (1.0 + std::abs(Forward) / Params.SteerFalloff);
	double YawTarget = In.Steer * Agility * (Forward >= 0.0 ? 1.0 : -1.0);
	if (K.bDrifting)
	{
		YawTarget = K.DriftDir * Agility * Params.DriftYawBoost * (0.55 + 0.45 * In.Steer * K.DriftDir);
		K.DriftCharge += Dt;   // plain seconds held: about one for the small boost, two for the big

	}
	if (bSpinning)
	{
		YawTarget = 11.0;
	}
	K.YawRate += (YawTarget - K.YawRate) * std::min(1.0, Dt / 0.07);
	K.Heading = WrapAngle(K.Heading + K.YawRate * Dt);
	Facing = FromAngle(K.Heading);

	// --- grip: the tyres pull the velocity round to where the kart points, up to a limit ---
	const double Speed = K.V.Length();
	if (Speed > 1.0)
	{
		const FKartVec Along = K.V.Dot(Facing) >= 0.0 ? Facing : Facing * -1.0;
		const double Slip = WrapAngle(std::atan2(K.V.Y, K.V.X) - std::atan2(Along.Y, Along.X));
		const double Grip = bSpinning ? 1.5 : (K.bDrifting ? Params.DriftGrip : Params.Grip);
		const double Lateral = bSpinning ? 600.0 : (K.bDrifting ? Params.DriftLateral : Params.MaxLateral);
		const double Turn = std::min(std::abs(Slip) * std::min(1.0, Grip * Dt), Lateral * Dt / Speed);
		K.V = Rotate(K.V, -Sign(Slip) * Turn);
		K.V = K.V * (1.0 - 0.6 * std::abs(std::sin(Slip)) * Dt);   // sliding costs a little speed
	}

	// --- throttle, brake, reverse ---------------------------------------------------------
	Forward = K.V.Dot(Facing);
	double Accel = 0.0;
	if (In.Throttle > 0.0)
	{
		Accel = Forward < 0.0 ? In.Throttle * Params.Brake
			: In.Throttle * Params.Accel * (K.BoostLeft > 0.0 ? 1.6 : 1.0) * std::max(0.0, 1.0 - Forward / Cap);
	}
	else if (In.Throttle < 0.0)
	{
		Accel = Forward > 50.0 ? In.Throttle * Params.Brake
			: In.Throttle * Params.Accel * 0.6 * std::max(0.0, 1.0 + Forward / Params.MaxReverse);
	}
	K.V += Facing * (Accel * Dt);
	if (std::abs(In.Throttle) < 0.05 || bSpinning)
	{
		const double Now = K.V.Length();
		const double Drop = (bSpinning ? 1800.0 : Params.Coast) * Dt;
		K.V = Now > Drop ? K.V * ((Now - Drop) / Now) : FKartVec();
	}
	const double After = K.V.Length();
	if (After > Cap && After > 1.0)
	{
		K.V = K.V * std::max(Cap / After, 1.0 - 2.5 * Dt);   // a boost or a chest ending: ease to the new limit
	}

	K.P += K.V * Dt;
	K.BoostLeft = std::max(0.0, K.BoostLeft - Dt);
	K.SpinLeft = std::max(0.0, K.SpinLeft - Dt);
	K.ChestLeft = std::max(0.0, K.ChestLeft - Dt);
}

void FKartRace::KeepInside(int Kart)
{
	FKartState& K = State[static_cast<size_t>(Kart)];
	FKartTrack::FFrame F = TheTrack.Locate(K.P, K.Segment);
	const double Limit = std::max(0.0, TheTrack.HalfWidthAt(F.S) - Params.KartRadius);
	if (std::abs(F.D) > Limit)
	{
		const FKartVec Out = F.Tangent.Left() * KartDetail::Sign(F.D);
		// Back inside, along the nearest stretch's normal. On a corner that stretch can change
		// as the kart moves, so it is measured again and pushed again until it is in.
		K.P -= Out * (std::abs(F.D) - Limit);
		for (int Pass = 0; Pass < 4; ++Pass)
		{
			const FKartTrack::FFrame Again = TheTrack.Locate(K.P, F.Segment);
			if (std::abs(Again.D) <= Limit)
			{
				break;
			}
			K.P -= Again.Tangent.Left() * (KartDetail::Sign(Again.D) * (std::abs(Again.D) - Limit + 0.01));
		}
		const double Into = K.V.Dot(Out);
		if (Into > 0.0)
		{
			K.V -= Out * (Into * (1.0 + Params.WallBounce));
			const double Scrape = 1.0 - std::min(0.35, Into / 2500.0);
			const FKartVec Normal = Out * K.V.Dot(Out);
			K.V = Normal + (K.V - Normal) * Scrape;
			K.WallContact = std::min(1.0, std::max(K.WallContact, Into / 1500.0));
			if (Into > 250.0)
			{
				Emit(EKartEvent::WallHit, Kart, -1, std::min(1.0, Into / 1500.0));
			}
			if (K.bDrifting && Into > 400.0)
			{
				K.bDrifting = false;   // a drift into the wall earns nothing
				K.DriftDir = 0;
				K.DriftCharge = 0.0;
			}
		}
		F = TheTrack.Locate(K.P, F.Segment);
	}
	K.S = F.S;
	K.D = F.D;
	K.Segment = F.Segment;
	const FKartVec Facing = KartDetail::FromAngle(K.Heading);
	K.bWrongWay = Facing.Dot(F.Tangent) < -0.3 && K.V.Dot(F.Tangent) < -200.0;
}

void FKartRace::Collide()
{
	const double MinGap = 2.0 * Params.KartRadius;
	for (size_t I = 0; I < State.size(); ++I)
	{
		for (size_t J = I + 1; J < State.size(); ++J)
		{
			FKartState& A = State[I];
			FKartState& B = State[J];
			FKartVec Between = B.P - A.P;
			double Dist = Between.Length();
			if (Dist >= MinGap)
			{
				continue;
			}
			if (Dist < 1e-6)
			{
				Between = KartDetail::FromAngle(A.Heading).Left();   // exactly on top: part them sideways
				Dist = 1e-6;
			}
			const FKartVec Normal = Between * (1.0 / Dist);
			const double Overlap = MinGap - Dist;
			A.P -= Normal * (Overlap * 0.5);
			B.P += Normal * (Overlap * 0.5);
			const double Closing = (B.V - A.V).Dot(Normal);
			if (Closing < 0.0)
			{
				const double Impulse = -(1.0 + Params.KartBounce) * Closing * 0.5;
				A.V -= Normal * Impulse;
				B.V += Normal * Impulse;
				if (Closing < -200.0)
				{
					Emit(EKartEvent::KartHit, static_cast<int>(I), static_cast<int>(J), std::min(1.0, -Closing / 1500.0));
				}
			}
			KeepInside(static_cast<int>(I));
			KeepInside(static_cast<int>(J));
		}
	}
}

void FKartRace::CountLaps(int Kart)
{
	const size_t I = static_cast<size_t>(Kart);
	FKartState& K = State[I];
	const double L = TheTrack.Length();
	const double Moved = K.S - PrevS[I];
	if (Moved < -L * 0.5)
	{
		++K.Lap;                // over the line, the right way
	}
	else if (Moved > L * 0.5)
	{
		--K.Lap;                // back over it
	}
	K.Progress = K.Lap * L + K.S;

	if (CurrentPhase != EKartPhase::Racing || K.bFinished)
	{
		return;
	}
	if (K.Lap > BestLap[I])
	{
		BestLap[I] = K.Lap;
		if (K.Lap < Setup.Laps)
		{
			Emit(EKartEvent::Lap, Kart, -1, K.Lap + 1);
		}
	}
	if (K.Progress >= Setup.Laps * L)
	{
		K.bFinished = true;
		K.FinishTime = RaceClock;
		int Finishing = 1;
		for (size_t J = 0; J < State.size(); ++J)
		{
			Finishing += (J != I && State[J].bFinished) ? 1 : 0;
		}
		K.Place = Finishing;
		Emit(EKartEvent::Finished, Kart, -1, Finishing);
	}
}

// =====================================================================================
// ITEMS

EKartItem FKartRace::RollItem(int Place)
{
	// Kinder to whoever is behind: the leader mostly gets things to defend with, the last
	// kart mostly gets things to catch up with.
	const double Back = State.size() > 1 ? static_cast<double>(Place - 1) / static_cast<double>(State.size() - 1) : 0.0;
	const double Weights[4] = {
		0.15 + 0.35 * Back,     // Sauce
		0.35 - 0.05 * Back,     // Slap
		0.40 - 0.30 * Back,     // Generate
		0.08 + 0.32 * Back,     // Refactor
	};
	const EKartItem Kinds[4] = { EKartItem::Sauce, EKartItem::Slap, EKartItem::Generate, EKartItem::Refactor };
	double Total = 0.0;
	for (const double W : Weights)
	{
		Total += W;
	}
	double Roll = Random01() * Total;
	for (int I = 0; I < 4; ++I)
	{
		Roll -= Weights[I];
		if (Roll < 0.0)
		{
			return Kinds[I];
		}
	}
	return EKartItem::Sauce;
}

int FKartRace::NearestTarget(int Kart, bool bAheadOnly) const
{
	const FKartState& K = State[static_cast<size_t>(Kart)];
	const FKartVec Facing = KartDetail::FromAngle(K.Heading);
	int Best = -1;
	double BestDist = Params.SlapRange;
	for (size_t J = 0; J < State.size(); ++J)
	{
		if (static_cast<int>(J) == Kart || State[J].SpinLeft > 0.0)
		{
			continue;
		}
		const FKartVec Rel = State[J].P - K.P;
		const double Dist = Rel.Length();
		const double Ahead = Rel.Dot(Facing);
		if (Dist < BestDist && (bAheadOnly ? Ahead > 0.0 : Ahead > -Params.KartRadius * 1.5))
		{
			BestDist = Dist;
			Best = static_cast<int>(J);
		}
	}
	return Best;
}

int FKartRace::KartJustAhead(int Kart) const
{
	const int Mine = State[static_cast<size_t>(Kart)].Place;
	const int Wanted = Mine > 1 ? Mine - 1 : Mine + 1;
	for (size_t J = 0; J < State.size(); ++J)
	{
		if (State[J].Place == Wanted && !State[J].bFinished)
		{
			return static_cast<int>(J);
		}
	}
	return -1;
}

void FKartRace::SpinOut(int Kart, int By, double Cause)
{
	FKartState& K = State[static_cast<size_t>(Kart)];
	if (K.SpinLeft > 0.0)
	{
		return;
	}
	K.SpinLeft = Params.SpinSeconds;
	K.bDrifting = false;
	K.DriftDir = 0;
	K.DriftCharge = 0.0;
	K.BoostLeft = 0.0;
	Emit(EKartEvent::SpunOut, Kart, By, Cause);
}

void FKartRace::Items(int Kart, const FKartInput& In, double Dt)
{
	(void)Dt;
	const size_t I = static_cast<size_t>(Kart);
	FKartState& K = State[I];
	if (CurrentPhase != EKartPhase::Racing)
	{
		return;
	}

	if (K.Item == EKartItem::None && K.ChestLeft <= 0.0)
	{
		for (FKartBox& Box : BoxList)
		{
			if (Box.RespawnLeft <= 0.0 && (Box.P - K.P).Length() < Params.BoxRadius + Params.KartRadius * 0.5)
			{
				K.Item = RollItem(K.Place);
				Box.RespawnLeft = Params.BoxRespawn;
				ItemHold[I] = 0.6 + 1.8 * Random01();
				Emit(EKartEvent::Pickup, Kart, -1, 0.0, K.Item);
				break;
			}
		}
	}

	for (size_t O = 0; O < ObstacleList.size(); ++O)
	{
		if ((ObstacleList[O].P - K.P).Length() < Params.ObstacleRadius + Params.KartRadius * 0.8)
		{
			const int Owner = ObstacleList[O].Owner;
			ObstacleList.erase(ObstacleList.begin() + static_cast<std::ptrdiff_t>(O));
			SpinOut(Kart, Owner, 2.0);
			break;
		}
	}

	if (In.bUseItem && K.Item != EKartItem::None && K.SpinLeft <= 0.0 && K.ChestLeft <= 0.0)
	{
		UseItem(Kart);
	}
}

void FKartRace::UseItem(int Kart)
{
	FKartState& K = State[static_cast<size_t>(Kart)];
	const EKartItem Item = K.Item;
	K.Item = EKartItem::None;
	int Target = -1;
	switch (Item)
	{
	case EKartItem::Sauce:
		K.BoostLeft = std::max(K.BoostLeft, Params.SauceSeconds);
		Emit(EKartEvent::Boosted, Kart, -1, Params.SauceSeconds);
		break;
	case EKartItem::Slap:
		Target = NearestTarget(Kart, false);
		if (Target >= 0)
		{
			SpinOut(Target, Kart, 1.0);
		}
		break;
	case EKartItem::Generate:
	{
		FKartObstacle O;
		const FKartVec Facing = KartDetail::FromAngle(K.Heading);
		O.P = K.P - Facing * (Params.KartRadius * 2.6);
		const FKartTrack::FFrame F = TheTrack.Locate(O.P, K.Segment);
		const double Limit = std::max(0.0, TheTrack.HalfWidthAt(F.S) - Params.ObstacleRadius - 20.0);
		if (std::abs(F.D) > Limit)
		{
			O.P -= F.Tangent.Left() * (KartDetail::Sign(F.D) * (std::abs(F.D) - Limit));
		}
		O.Left = Params.ObstacleSeconds;
		O.Owner = Kart;
		ObstacleList.push_back(O);
		break;
	}
	case EKartItem::Refactor:
		Target = KartJustAhead(Kart);
		if (Target >= 0)
		{
			FKartState& T = State[static_cast<size_t>(Target)];
			T.ChestLeft = Params.ChestSeconds;
			T.Item = EKartItem::None;
			T.bDrifting = false;
			T.DriftDir = 0;
			T.DriftCharge = 0.0;
			T.BoostLeft = 0.0;
			Emit(EKartEvent::Refactored, Target, Kart);
		}
		break;
	default:
		break;
	}
	Emit(EKartEvent::UsedItem, Kart, Target, 0.0, Item);
}
