// LanternBurstSim.cpp — see the header for the rules.

#include "LanternBurstSim.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

FLanternBurstSim::FLanternBurstSim(uint32_t Seed)
	: Cells(LanternBurst::GridRows * LanternBurst::Columns, LanternBurst::Empty)
	, Rng(Seed ? Seed : 1u)
{
}

// Stage 1 is gentle on purpose - three colours in big clumps and a thin waist, so the first
// minutes already bring a ring of the lantern down. After that a row, a colour or a miss less at
// a time, smaller clumps, and no waist to help. Tuned against the greedy robot in the tests.
FLanternStageSpec FLanternBurstSim::StageSpec(int Stage)
{
	static const FLanternStageSpec Table[] = {
		//Rows Colors Lower Waist Fill  Clump
		{ 5,   3,     7,     2,   0.60, 0.40 },
		{ 6,   4,     6,     3,   0.70, 0.35 },
		{ 6,   4,     6,    -1,   1.00, 0.30 },
		{ 7,   5,     5,    -1,   1.00, 0.30 },
		{ 7,   5,     5,    -1,   1.00, 0.25 },
		{ 8,   6,     4,    -1,   1.00, 0.25 },
	};
	const int Count = static_cast<int>(sizeof(Table) / sizeof(Table[0]));
	FLanternStageSpec Spec = Table[std::clamp(Stage, 1, Count) - 1];
	if (Stage > Count + 2)
	{
		Spec.LowerEvery = 3;   // from stage 9 on the crown is as impatient as it gets
		Spec.Clump = 0.2;
	}
	return Spec;
}

uint32_t FLanternBurstSim::NextRandom()
{
	// xorshift32: small, fast, the same on every compiler - the tests replay seeds.
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return Rng;
}

int FLanternBurstSim::RandomBelow(int N)
{
	return N <= 1 ? 0 : static_cast<int>(NextRandom() % static_cast<uint32_t>(N));
}

bool FLanternBurstSim::Chance(double P)
{
	return (NextRandom() >> 8) * (1.0 / 16777216.0) < P;
}

int FLanternBurstSim::Wrap(int Col)
{
	const int M = Col % LanternBurst::Columns;
	return M < 0 ? M + LanternBurst::Columns : M;
}

int FLanternBurstSim::At(int Row, int Col) const
{
	if (Row < 0 || Row >= LanternBurst::GridRows)
	{
		return LanternBurst::Empty;
	}
	return Cells[Row * LanternBurst::Columns + Wrap(Col)];
}

int& FLanternBurstSim::Cell(int Row, int Col)
{
	return Cells[Row * LanternBurst::Columns + Wrap(Col)];
}

void FLanternBurstSim::Neighbours(int Row, int Col, std::vector<FLanternCell>& Out)
{
	Out.clear();
	Out.push_back({ Row, Wrap(Col - 1) });
	Out.push_back({ Row, Wrap(Col + 1) });
	// An odd row sits half a step round, so it touches columns c and c+1 above and below; an
	// even row touches c-1 and c.
	const int Lo = Shifted(Row) ? Col : Col - 1;
	for (int Other : { Row - 1, Row + 1 })
	{
		if (Other < 0 || Other >= LanternBurst::GridRows)
		{
			continue;
		}
		Out.push_back({ Other, Wrap(Lo) });
		Out.push_back({ Other, Wrap(Lo + 1) });
	}
}

// Steps between two places, the short way round. In "doubled" columns (2c, plus one on an odd
// row) a step is two across or one across and one down, so the distance is the rows apart plus
// whatever of the columns apart the diagonal steps do not cover.
int FLanternBurstSim::Distance(FLanternCell A, FLanternCell B)
{
	const int XA = 2 * Wrap(A.Col) + (Shifted(A.Row) ? 1 : 0);
	const int XB = 2 * Wrap(B.Col) + (Shifted(B.Row) ? 1 : 0);
	int DX = std::abs(XA - XB);
	DX = std::min(DX, 2 * LanternBurst::Columns - DX);
	const int DY = std::abs(A.Row - B.Row);
	return DY + std::max(0, (DX - DY) / 2);
}

int FLanternBurstSim::OrbCount() const
{
	int Count = 0;
	for (int Color : Cells)
	{
		Count += Color != LanternBurst::Empty ? 1 : 0;
	}
	return Count;
}

int FLanternBurstSim::LowestRow() const
{
	for (int Row = LanternBurst::GridRows - 1; Row >= 0; --Row)
	{
		for (int Col = 0; Col < LanternBurst::Columns; ++Col)
		{
			if (At(Row, Col) != LanternBurst::Empty)
			{
				return Row;
			}
		}
	}
	return -1;
}

bool FLanternBurstSim::HasColor(int Color) const
{
	return std::find(Cells.begin(), Cells.end(), Color) != Cells.end();
}

bool FLanternBurstSim::CanPlace(int Row, int Col) const
{
	if (CurrentPhase != ELanternPhase::Playing || Row < 0 || Row >= LanternBurst::GridRows || Row > LineRow()
		|| Col < 0 || Col >= LanternBurst::Columns || At(Row, Col) != LanternBurst::Empty)
	{
		return false;
	}
	if (Row == 0)
	{
		return true;   // hung from the crown itself
	}
	std::vector<FLanternCell> Near;
	Neighbours(Row, Col, Near);
	for (const FLanternCell& N : Near)
	{
		if (At(N.Row, N.Col) != LanternBurst::Empty)
		{
			return true;
		}
	}
	return false;
}

std::vector<FLanternCell> FLanternBurstSim::Frontier() const
{
	std::vector<FLanternCell> Out;
	for (int Row = 0; Row < LanternBurst::GridRows; ++Row)
	{
		for (int Col = 0; Col < LanternBurst::Columns; ++Col)
		{
			if (CanPlace(Row, Col))
			{
				Out.push_back({ Row, Col });
			}
		}
	}
	return Out;
}

int FLanternBurstSim::NextMultiplier() const
{
	return std::min(CurrentStreak + 1, LanternBurst::MaxMultiplier);
}

FLanternOrb FLanternBurstSim::OrbAt(int Row, int Col, int Ring) const
{
	FLanternOrb Orb;
	Orb.Row = Row;
	Orb.Col = Wrap(Col);
	Orb.Column = ColumnOf(Row, Wrap(Col));
	Orb.Color = At(Row, Col);
	Orb.Ring = Ring;
	return Orb;
}

void FLanternBurstSim::Push(ELanternEvent Type, int Index, long long Value, std::vector<FLanternOrb> Orbs)
{
	FLanternEvent Event;
	Event.Type = Type;
	Event.Index = Index;
	Event.Value = Value;
	Event.Orbs = std::move(Orbs);
	Events.push_back(std::move(Event));
}

std::vector<FLanternEvent> FLanternBurstSim::TakeEvents()
{
	std::vector<FLanternEvent> Out;
	Out.swap(Events);
	return Out;
}

void FLanternBurstSim::NewGame(uint32_t Seed)
{
	Rng = Seed ? Seed : 1u;
	for (int I = 0; I < 4; ++I)
	{
		NextRandom();   // small seeds start xorshift slowly
	}
	CurrentStage = 0;
	CurrentScore = 0;
	CurrentStreak = 0;
	ShotCount = 0;
	Events.clear();
	NextStage();
}

void FLanternBurstSim::NextStage()
{
	++CurrentStage;
	HangStage();
	CurrentPhase = ELanternPhase::Playing;
	HandCurrent = Draw();
	HandNext = Draw();
	Push(ELanternEvent::StageStarted, CurrentStage);
}

// Rows filled from the crown down, colours in clumps (Clump of the orbs copy a neighbour already
// hung), the last two rows and the waist thinner - then anything that does not hang is taken
// off, so the stage starts as the rules would leave it.
void FLanternBurstSim::HangStage()
{
	const FLanternStageSpec Spec = StageSpec(CurrentStage);
	std::fill(Cells.begin(), Cells.end(), LanternBurst::Empty);
	LoweredRows = 0;
	MissesLeft = Spec.LowerEvery;
	std::vector<FLanternCell> Near;
	for (int Row = 0; Row < Spec.Rows && Row < LanternBurst::GridRows; ++Row)
	{
		double Fill = 1.0;
		if (Row == Spec.Rows - 1)
		{
			Fill = 0.7;
		}
		else if (Row == Spec.Rows - 2)
		{
			Fill = 0.9;
		}
		if (Row == Spec.WaistRow)
		{
			Fill = std::min(Fill, Spec.WaistFill);
		}
		for (int Col = 0; Col < LanternBurst::Columns; ++Col)
		{
			if (Row > 0 && !Chance(Fill))
			{
				continue;
			}
			Neighbours(Row, Col, Near);
			int Choices[6];
			int ChoiceCount = 0;
			for (const FLanternCell& N : Near)
			{
				const bool bBefore = N.Row < Row || (N.Row == Row && N.Col == Wrap(Col - 1));
				if (bBefore && At(N.Row, N.Col) != LanternBurst::Empty)
				{
					Choices[ChoiceCount++] = At(N.Row, N.Col);
				}
			}
			Cell(Row, Col) = (ChoiceCount > 0 && Chance(Spec.Clump)) ? Choices[RandomBelow(ChoiceCount)] : RandomBelow(Spec.Colors);
		}
	}
	RemoveUnhung();
}

std::vector<FLanternCell> FLanternBurstSim::GroupOf(int Row, int Col) const
{
	std::vector<FLanternCell> Group;
	const int Color = At(Row, Col);
	if (Color == LanternBurst::Empty || Color == LanternBurst::Spark)
	{
		return Group;
	}
	std::vector<char> Seen(Cells.size(), 0);
	std::vector<FLanternCell> Near;
	Group.push_back({ Row, Wrap(Col) });
	Seen[Row * LanternBurst::Columns + Wrap(Col)] = 1;
	for (size_t I = 0; I < Group.size(); ++I)
	{
		Neighbours(Group[I].Row, Group[I].Col, Near);
		for (const FLanternCell& N : Near)
		{
			char& Mark = Seen[N.Row * LanternBurst::Columns + N.Col];
			if (!Mark && At(N.Row, N.Col) == Color)
			{
				Mark = 1;
				Group.push_back(N);
			}
		}
	}
	return Group;
}

std::vector<FLanternOrb> FLanternBurstSim::RemoveUnhung()
{
	std::vector<char> Hung(Cells.size(), 0);
	std::vector<FLanternCell> Queue;
	for (int Col = 0; Col < LanternBurst::Columns; ++Col)
	{
		if (At(0, Col) != LanternBurst::Empty)
		{
			Hung[Col] = 1;
			Queue.push_back({ 0, Col });
		}
	}
	std::vector<FLanternCell> Near;
	for (size_t I = 0; I < Queue.size(); ++I)
	{
		Neighbours(Queue[I].Row, Queue[I].Col, Near);
		for (const FLanternCell& N : Near)
		{
			char& Mark = Hung[N.Row * LanternBurst::Columns + N.Col];
			if (!Mark && At(N.Row, N.Col) != LanternBurst::Empty)
			{
				Mark = 1;
				Queue.push_back(N);
			}
		}
	}
	std::vector<FLanternOrb> Fallen;
	for (int Row = 0; Row < LanternBurst::GridRows; ++Row)
	{
		for (int Col = 0; Col < LanternBurst::Columns; ++Col)
		{
			if (At(Row, Col) != LanternBurst::Empty && !Hung[Row * LanternBurst::Columns + Col])
			{
				Fallen.push_back(OrbAt(Row, Col, 0));
				Cell(Row, Col) = LanternBurst::Empty;
			}
		}
	}
	return Fallen;
}

// A colour still on the lantern - or, on an empty one, any of the stage's.
int FLanternBurstSim::Draw()
{
	int Present[LanternBurst::MaxColors];
	int Count = 0;
	for (int Color = 0; Color < LanternBurst::MaxColors; ++Color)
	{
		if (HasColor(Color))
		{
			Present[Count++] = Color;
		}
	}
	if (Count == 0)
	{
		return RandomBelow(StageSpec(std::max(CurrentStage, 1)).Colors);
	}
	return Present[RandomBelow(Count)];
}

void FLanternBurstSim::RefreshHand()
{
	if (OrbCount() == 0)
	{
		return;   // the stage is over; NextStage() deals a fresh hand
	}
	if (HandCurrent != LanternBurst::Spark && !HasColor(HandCurrent))
	{
		HandCurrent = Draw();
	}
	if (HandNext != LanternBurst::Spark && !HasColor(HandNext))
	{
		HandNext = Draw();
	}
}

void FLanternBurstSim::Swap()
{
	if (CurrentPhase == ELanternPhase::Playing)
	{
		std::swap(HandCurrent, HandNext);
	}
}

double FLanternBurstSim::WrapDelta(double DX)
{
	// A shot's X is never more than a few turns out, so a loop beats fmod in the trace's inner loop.
	const double W = static_cast<double>(LanternBurst::Columns);
	while (DX > W * 0.5)
	{
		DX -= W;
	}
	while (DX < -W * 0.5)
	{
		DX += W;
	}
	return DX;
}

// Up the glass in small steps until the shot comes within CollideDistance of an orb, or reaches
// row 0's line under the crown. Then into the nearest empty place beside what it met.
FLanternTrace FLanternBurstSim::Trace(double LaunchColumn, double Angle, bool bWithPath) const
{
	FLanternTrace Result;
	if (CurrentPhase != ELanternPhase::Playing)
	{
		return Result;
	}
	Angle = std::clamp(Angle, -LanternBurst::MaxAimAngle, LanternBurst::MaxAimAngle);
	const double DX = std::sin(Angle) * LanternBurst::TraceStep;
	const double DY = -std::cos(Angle) * LanternBurst::TraceStep;
	double X = LaunchColumn;
	double Y = LauncherY();
	if (bWithPath)
	{
		Result.Path.push_back({ X, Y });
	}
	auto Nearest = [&](const std::vector<FLanternCell>& Choices) -> bool
	{
		double Best = 1e9;
		for (const FLanternCell& C : Choices)
		{
			if (!CanPlace(C.Row, C.Col))
			{
				continue;
			}
			const FLanternPoint P = CenterOf(C.Row, C.Col);
			const double DXc = WrapDelta(P.X - X);
			const double D2 = DXc * DXc + (P.Y - Y) * (P.Y - Y);
			if (D2 < Best)
			{
				Best = D2;
				Result.Cell = C;
				Result.bLands = true;
			}
		}
		return Result.bLands;
	};

	// Nothing to meet below the lowest row: fly straight up to it, leaving points on the way for
	// the actor to draw and follow.
	double Travelled = 0.0;
	const int Lowest = LowestRow();
	const double Clear = Lowest < 0 ? 0.0 : Lowest * LanternBurst::RowHeight + LanternBurst::CollideDistance + LanternBurst::TraceStep;
	if (Y > Clear)
	{
		const double Length = (Y - Clear) / std::cos(Angle);
		const int Legs = std::max(1, static_cast<int>(std::ceil(Length / 0.25)));
		const double X0 = X, Y0 = Y;
		for (int Leg = 1; Leg <= Legs; ++Leg)
		{
			const double U = static_cast<double>(Leg) / Legs;
			X = X0 + std::sin(Angle) * Length * U;
			Y = Y0 + (Clear - Y0) * U;
			if (bWithPath && Leg < Legs)
			{
				Result.Path.push_back({ X, Y });
			}
		}
		Travelled = Length;
	}

	const double Collide2 = LanternBurst::CollideDistance * LanternBurst::CollideDistance;
	std::vector<FLanternCell> Choices;
	for (; Travelled < LanternBurst::MaxTravel; Travelled += LanternBurst::TraceStep)
	{
		X += DX;
		Y += DY;
		if (Y <= 0.0)
		{
			Y = 0.0;
			Choices.clear();
			for (int Col = 0; Col < LanternBurst::Columns; ++Col)
			{
				Choices.push_back({ 0, Col });
			}
			Nearest(Choices);
			break;
		}
		const int RowLo = std::max(0, static_cast<int>(Y / LanternBurst::RowHeight) - 1);
		const int RowHi = std::min(LanternBurst::GridRows - 1, RowLo + 3);
		const int ColMid = static_cast<int>(std::floor(X));
		int HitRow = -1, HitCol = 0;
		double HitD2 = Collide2;
		for (int Row = RowLo; Row <= RowHi; ++Row)
		{
			for (int Col = ColMid - 2; Col <= ColMid + 2; ++Col)
			{
				if (At(Row, Col) == LanternBurst::Empty)
				{
					continue;
				}
				const FLanternPoint P = CenterOf(Row, Wrap(Col));
				const double DXc = WrapDelta(P.X - X);
				const double D2 = DXc * DXc + (P.Y - Y) * (P.Y - Y);
				if (D2 < HitD2)
				{
					HitD2 = D2;
					HitRow = Row;
					HitCol = Wrap(Col);
				}
			}
		}
		if (HitRow < 0)
		{
			if (bWithPath)
			{
				Result.Path.push_back({ X, Y });
			}
			continue;
		}
		Neighbours(HitRow, HitCol, Choices);
		if (!Nearest(Choices))
		{
			// Every place beside it is taken (the shot grazed it past a full shoulder): any open
			// place within reach of where the shot is.
			Choices.clear();
			for (int Row = std::max(0, RowLo - 1); Row <= std::min(LanternBurst::GridRows - 1, RowHi + 1); ++Row)
			{
				for (int Col = ColMid - 2; Col <= ColMid + 2; ++Col)
				{
					Choices.push_back({ Row, Wrap(Col) });
				}
			}
			Nearest(Choices);
		}
		break;
	}
	if (bWithPath && Result.bLands)
	{
		// The last leg: from where it stopped to the centre of its place, unwrapped to follow on.
		const FLanternPoint End = CenterOf(Result.Cell.Row, Result.Cell.Col);
		Result.Path.push_back({ X + WrapDelta(End.X - X), End.Y });
	}
	return Result;
}

FLanternBurstSim::FPreview FLanternBurstSim::Preview(double LaunchColumn, double Angle) const
{
	const FLanternTrace Shot = Trace(LaunchColumn, Angle, false);
	if (!Shot.bLands)
	{
		return FPreview();
	}
	return PreviewAt(Shot.Cell.Row, Shot.Cell.Col);
}

bool FLanternBurstSim::Shoot(double LaunchColumn, double Angle)
{
	const FLanternTrace Shot = Trace(LaunchColumn, Angle, false);
	return Shot.bLands && PlaceAt(Shot.Cell.Row, Shot.Cell.Col);
}

FLanternBurstSim::FPreview FLanternBurstSim::PreviewAt(int Row, int Col) const
{
	FPreview Result;
	if (!CanPlace(Row, Col))
	{
		return Result;
	}
	Result.Cell = { Row, Col };
	FLanternBurstSim Trial = *this;
	Trial.Events.clear();
	Result.bLegal = Trial.PlaceAt(Row, Col);
	for (const FLanternEvent& Event : Trial.Events)
	{
		if (Event.Type == ELanternEvent::Burst || Event.Type == ELanternEvent::Sparked)
		{
			Result.Burst += static_cast<int>(Event.Orbs.size());
		}
		else if (Event.Type == ELanternEvent::Fell)
		{
			Result.Fall += static_cast<int>(Event.Orbs.size());
		}
	}
	Result.bOverLine = Trial.CurrentPhase == ELanternPhase::GameOver;
	Result.bClears = Trial.CurrentPhase == ELanternPhase::StageClear;
	return Result;
}

bool FLanternBurstSim::PlaceAt(int Row, int Col)
{
	if (!CanPlace(Row, Col))
	{
		return false;
	}
	++ShotCount;
	const int Color = HandCurrent;
	const FLanternCell Shot{ Row, Col };
	Cell(Row, Col) = Color;
	Push(ELanternEvent::Placed, 0, 0, { OrbAt(Row, Col, 0) });

	// What goes: everything the spark reaches, or the shot's colour if it makes three.
	bool bEarnedSpark = false;
	std::vector<FLanternOrb> Gone;
	if (Color == LanternBurst::Spark)
	{
		for (int R = std::max(0, Row - LanternBurst::SparkReach); R <= std::min(LanternBurst::GridRows - 1, Row + LanternBurst::SparkReach); ++R)
		{
			for (int C = 0; C < LanternBurst::Columns; ++C)
			{
				const int D = Distance(Shot, { R, C });
				if (D <= LanternBurst::SparkReach && At(R, C) != LanternBurst::Empty)
				{
					Gone.push_back(OrbAt(R, C, D));
				}
			}
		}
	}
	else
	{
		const std::vector<FLanternCell> Group = GroupOf(Row, Col);
		if (static_cast<int>(Group.size()) >= LanternBurst::MinBurst)
		{
			for (const FLanternCell& G : Group)
			{
				Gone.push_back(OrbAt(G.Row, G.Col, Distance(Shot, G)));
			}
		}
	}

	if (!Gone.empty())
	{
		++CurrentStreak;
		const int Multiplier = std::min(CurrentStreak, LanternBurst::MaxMultiplier);
		for (const FLanternOrb& Orb : Gone)
		{
			Cell(Orb.Row, Orb.Col) = LanternBurst::Empty;
		}
		const long long BurstPoints = 10LL * static_cast<long long>(Gone.size()) * Multiplier;
		CurrentScore += BurstPoints;
		Push(Color == LanternBurst::Spark ? ELanternEvent::Sparked : ELanternEvent::Burst, Multiplier, BurstPoints, Gone);

		std::vector<FLanternOrb> Fallen = RemoveUnhung();
		const long long N = static_cast<long long>(Fallen.size());
		if (N > 0)
		{
			const long long FallPoints = 10LL * N * (N + 1) / 2 * Multiplier;
			CurrentScore += FallPoints;
			Push(ELanternEvent::Fell, 0, FallPoints, std::move(Fallen));
		}
		if (CurrentStreak >= 2 && CurrentStreak <= LanternBurst::MaxMultiplier)
		{
			Push(ELanternEvent::StreakUp, Multiplier);
		}
		bEarnedSpark = (CurrentStreak % LanternBurst::SparkEveryStreak == 0) || N >= LanternBurst::SparkForFall;
	}
	else
	{
		CurrentStreak = 0;
		--MissesLeft;
		if (MissesLeft <= 0)
		{
			++LoweredRows;
			MissesLeft = StageSpec(CurrentStage).LowerEvery;
			Push(ELanternEvent::Lowered, LoweredRows);
		}
		else
		{
			Push(ELanternEvent::Missed, MissesLeft);
		}
	}

	// The hand moves up; an earned spark arrives as the NEXT orb, so he sees it coming and can
	// swap it in now or keep it for the neck he means to cut.
	HandCurrent = HandNext;
	HandNext = Draw();
	RefreshHand();
	if (bEarnedSpark && HandCurrent != LanternBurst::Spark && OrbCount() > 0)
	{
		HandNext = LanternBurst::Spark;
		Push(ELanternEvent::SparkEarned);
	}

	if (OrbCount() == 0)
	{
		const long long Bonus = 1000LL * CurrentStage + (LoweredRows == 0 ? 1000LL : 0LL);
		CurrentScore += Bonus;
		CurrentPhase = ELanternPhase::StageClear;
		Push(ELanternEvent::StageCleared, CurrentStage, Bonus);
	}
	else if (LowestRow() >= LineRow())
	{
		CurrentPhase = ELanternPhase::GameOver;
		Push(ELanternEvent::GameOver, CurrentStage, CurrentScore);
	}
	return true;
}

void FLanternBurstSim::ClearLantern()
{
	std::fill(Cells.begin(), Cells.end(), LanternBurst::Empty);
	LoweredRows = 0;
	MissesLeft = StageSpec(std::max(CurrentStage, 1)).LowerEvery;
	if (CurrentStage == 0)
	{
		CurrentStage = 1;
	}
	CurrentPhase = ELanternPhase::Playing;
}

void FLanternBurstSim::SetCell(int Row, int Col, int Color)
{
	if (Row >= 0 && Row < LanternBurst::GridRows)
	{
		Cell(Row, Col) = Color;
	}
}

void FLanternBurstSim::SetHand(int Current, int Next)
{
	HandCurrent = Current;
	HandNext = Next;
}
