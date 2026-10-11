// GoEngine.cpp — see the header for the design, and Tools/Tests/go_engine_test.cpp for what
// it is held to.

#include "GoEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// NAMED, NOT ANONYMOUS: the unity-build rule (see PinballSim.cpp's PinMath).
namespace GoDetail
{
	constexpr uint8_t EmptyC = 0;
	constexpr uint8_t BlackC = 1;
	constexpr uint8_t WhiteC = 2;
	constexpr uint8_t EdgeC = 3;
	constexpr int MaxPadded = (GoMaxSize + 2) * (GoMaxSize + 2);

	inline uint8_t Code(EGoColor C) { return static_cast<uint8_t>(C); }
	inline bool IsStone(uint8_t C) { return C == BlackC || C == WhiteC; }
	inline int SideOf(uint8_t C) { return C == BlackC ? 0 : 1; }
	inline uint8_t OtherOf(uint8_t C) { return C == BlackC ? WhiteC : BlackC; }

	inline uint64_t SplitMix(uint64_t& State)
	{
		uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	struct FKeys
	{
		uint64_t Stone[2][MaxPadded];
	};

	inline FKeys MakeKeys()
	{
		FKeys K;
		uint64_t State = 0x5EED60A1u;
		for (int C = 0; C < 2; ++C)
		{
			for (int P = 0; P < MaxPadded; ++P)
			{
				K.Stone[C][P] = SplitMix(State);
			}
		}
		return K;
	}

	// Built once, on first use; C++ makes that thread-safe.
	inline const FKeys& Keys()
	{
		static const FKeys K = MakeKeys();
		return K;
	}

	// Small, fast and deterministic: every game the search plays is the seed's.
	struct FRng
	{
		uint64_t State;
		explicit FRng(uint64_t Seed) : State(Seed ? Seed : 0x2545F4914F6CDD1Dull) {}
		uint32_t Next()
		{
			State ^= State << 13;
			State ^= State >> 7;
			State ^= State << 17;
			return static_cast<uint32_t>(State >> 16);
		}
		int Below(int Count) { return Count > 0 ? static_cast<int>(Next() % static_cast<uint32_t>(Count)) : 0; }
		float Unit() { return static_cast<float>(Next() & 0xFFFFFF) / 16777216.0f; }
	};

	// A bit per padded point, on the stack: no shared marks, so a board can be read from any thread.
	struct FMarks
	{
		uint64_t Bits[(MaxPadded + 63) / 64] = {};
		bool Test(int P) const { return (Bits[P >> 6] >> (P & 63)) & 1u; }
		void Set(int P) { Bits[P >> 6] |= uint64_t(1) << (P & 63); }
	};
}

char GoColumnLetter(int X)
{
	using namespace GoDetail;
	return static_cast<char>('A' + X + (X >= 8 ? 1 : 0));
}

// =====================================================================================
// THE BOARD

FGoBoard::FGoBoard(int InSize)
{
	using namespace GoDetail;
	N = std::min(std::max(InSize, GoMinSize), GoMaxSize);
	W = N + 2;
	std::memset(Color, EdgeC, sizeof(Color));
	for (int Y = 0; Y < N; ++Y)
	{
		for (int X = 0; X < N; ++X)
		{
			Color[(Y + 1) * W + X + 1] = EmptyC;
		}
	}
	Rebuild();
}

FGoBoard FGoBoard::FromDiagram(const std::vector<std::string>& Rows, EGoColor ToMove)
{
	using namespace GoDetail;
	std::vector<std::string> Clean;
	for (const std::string& Row : Rows)
	{
		std::string R;
		for (char Ch : Row)
		{
			if (Ch != ' ')
			{
				R += Ch;
			}
		}
		Clean.push_back(R);
	}
	const int Lines = static_cast<int>(Clean.size());
	if (Lines < GoMinSize || Lines > GoMaxSize)
	{
		return FGoBoard(9);
	}
	FGoBoard B(Lines);
	for (int Row = 0; Row < Lines; ++Row)
	{
		if (static_cast<int>(Clean[Row].size()) != Lines)
		{
			return FGoBoard(9);
		}
		const int Y = Lines - 1 - Row;
		for (int X = 0; X < Lines; ++X)
		{
			const char Ch = Clean[Row][X];
			B.Color[(Y + 1) * B.W + X + 1] = Ch == 'X' ? BlackC : Ch == 'O' ? WhiteC : EmptyC;
		}
	}
	B.Mover = ToMove == EGoColor::White ? EGoColor::White : EGoColor::Black;
	B.Rebuild();
	return B;
}

void FGoBoard::Rebuild()
{
	using namespace GoDetail;
	EmptyCount = 0;
	StoneHash = 0;
	for (int P = 0; P < W * W; ++P)
	{
		Head[P] = -1;
		NextStone[P] = P;
		GroupSize[P] = 0;
		PseudoLibs[P] = 0;
		EmptySlot[P] = -1;
	}
	for (int Y = 0; Y < N; ++Y)
	{
		for (int X = 0; X < N; ++X)
		{
			const int P = (Y + 1) * W + X + 1;
			if (Color[P] == EmptyC)
			{
				AddEmpty(P);
			}
		}
	}
	const int Dirs[4] = { 1, -1, W, -W };
	int Stack[MaxPadded];
	for (int Y = 0; Y < N; ++Y)
	{
		for (int X = 0; X < N; ++X)
		{
			const int P = (Y + 1) * W + X + 1;
			if (!IsStone(Color[P]) || Head[P] >= 0)
			{
				continue;
			}
			const uint8_t C = Color[P];
			int Top = 0;
			Stack[Top++] = P;
			Head[P] = P;
			int Stones = 0;
			int Libs = 0;
			while (Top > 0)
			{
				const int S = Stack[--Top];
				++Stones;
				StoneHash ^= Keys().Stone[SideOf(C)][S];
				for (int D : Dirs)
				{
					const int Q = S + D;
					if (Color[Q] == EmptyC)
					{
						++Libs;
					}
					else if (Color[Q] == C && Head[Q] < 0)
					{
						Head[Q] = P;
						NextStone[Q] = NextStone[P];
						NextStone[P] = Q;
						Stack[Top++] = Q;
					}
				}
			}
			GroupSize[P] = Stones;
			PseudoLibs[P] = Libs;
		}
	}
}

bool FGoBoard::SameBookkeeping(const FGoBoard& Other) const
{
	using namespace GoDetail;
	if (N != Other.N || StoneHash != Other.StoneHash || EmptyCount != Other.EmptyCount)
	{
		return false;
	}
	const int Dirs[4] = { 1, -1, W, -W };
	for (int Y = 0; Y < N; ++Y)
	{
		for (int X = 0; X < N; ++X)
		{
			const int P = (Y + 1) * W + X + 1;
			if (Color[P] != Other.Color[P])
			{
				return false;
			}
			if (Color[P] == EmptyC)
			{
				if (EmptySlot[P] < 0 || EmptySlot[P] >= EmptyCount || Empties[EmptySlot[P]] != P)
				{
					return false;
				}
				continue;
			}
			const int H = Head[P];
			const int OH = Other.Head[P];
			if (H < 0 || Color[H] != Color[P] || GroupSize[H] != Other.GroupSize[OH] || PseudoLibs[H] != Other.PseudoLibs[OH])
			{
				return false;
			}
			for (int D : Dirs)
			{
				if (Color[P + D] == Color[P] && Head[P + D] != H)
				{
					return false;
				}
			}
			if (H == P)
			{
				int Count = 0;
				int S = P;
				do
				{
					if (Head[S] != P || ++Count > GroupSize[P])
					{
						return false;
					}
					S = NextStone[S];
				} while (S != P);
				if (Count != GroupSize[P])
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FGoBoard::AddEmpty(int P)
{
	using namespace GoDetail;
	EmptySlot[P] = EmptyCount;
	Empties[EmptyCount++] = P;
}

void FGoBoard::RemoveEmpty(int P)
{
	using namespace GoDetail;
	const int Slot = EmptySlot[P];
	const int Moved = Empties[--EmptyCount];
	Empties[Slot] = Moved;
	EmptySlot[Moved] = Slot;
	EmptySlot[P] = -1;
}

int FGoBoard::CountAdjacentOf(int P, int GroupHead) const
{
	using namespace GoDetail;
	int Count = 0;
	const int Dirs[4] = { 1, -1, W, -W };
	for (int D : Dirs)
	{
		if (IsStone(Color[P + D]) && Head[P + D] == GroupHead)
		{
			++Count;
		}
	}
	return Count;
}

/* Legal unless it is occupied, the ko point, or suicide. Suicide is decided without walking
   any group: a neighbouring group's only liberty is P exactly when every one of its
   pseudo-liberties is one of its stones touching P. */
bool FGoBoard::LegalAt(int P, EGoColor C) const
{
	using namespace GoDetail;
	if (Color[P] != EmptyC || P == Ko)
	{
		return false;
	}
	const int Dirs[4] = { 1, -1, W, -W };
	for (int D : Dirs)
	{
		if (Color[P + D] == EmptyC)
		{
			return true;
		}
	}
	const uint8_t Own = Code(C);
	for (int D : Dirs)
	{
		const int Q = P + D;
		if (!IsStone(Color[Q]))
		{
			continue;
		}
		const int H = Head[Q];
		const bool bOnlyHere = PseudoLibs[H] == CountAdjacentOf(P, H);
		if (Color[Q] == Own ? !bOnlyHere : bOnlyHere)
		{
			return true;   // joins a group with another liberty, or captures
		}
	}
	return false;
}

bool FGoBoard::IsLegal(int Point) const
{
	using namespace GoDetail;
	if (Point == GoPass)
	{
		return true;
	}
	return Point >= 0 && Point < N * N && LegalAt(Pad(Point), Mover);
}

void FGoBoard::PlaceStone(int P, EGoColor C)
{
	using namespace GoDetail;
	RemoveEmpty(P);
	Color[P] = Code(C);
	Head[P] = P;
	NextStone[P] = P;
	GroupSize[P] = 1;
	PseudoLibs[P] = 0;
	StoneHash ^= Keys().Stone[C == EGoColor::Black ? 0 : 1][P];
	const int Dirs[4] = { 1, -1, W, -W };
	for (int D : Dirs)
	{
		const uint8_t Q = Color[P + D];
		if (Q == EmptyC)
		{
			++PseudoLibs[P];
		}
		else if (IsStone(Q))
		{
			--PseudoLibs[Head[P + D]];   // P was one of its liberties, once per stone touching it
		}
	}
}

void FGoBoard::MergeGroups(int A, int B)
{
	using namespace GoDetail;
	if (GroupSize[A] < GroupSize[B])
	{
		std::swap(A, B);
	}
	int S = B;
	do
	{
		Head[S] = A;
		S = NextStone[S];
	} while (S != B);
	std::swap(NextStone[A], NextStone[B]);
	GroupSize[A] += GroupSize[B];
	PseudoLibs[A] += PseudoLibs[B];
}

int FGoBoard::RemoveGroup(int GroupHead)
{
	using namespace GoDetail;
	const uint8_t C = Color[GroupHead];
	const int Side = SideOf(C);
	int Count = 0;
	int S = GroupHead;
	do
	{
		Color[S] = EmptyC;
		AddEmpty(S);
		StoneHash ^= Keys().Stone[Side][S];
		++Count;
		S = NextStone[S];
	} while (S != GroupHead);

	// Only once every stone is gone: a stone's neighbour in the same group is not a liberty to give.
	const int Dirs[4] = { 1, -1, W, -W };
	S = GroupHead;
	do
	{
		for (int D : Dirs)
		{
			if (IsStone(Color[S + D]))
			{
				++PseudoLibs[Head[S + D]];
			}
		}
		S = NextStone[S];
	} while (S != GroupHead);
	return Count;
}

int FGoBoard::PlayAt(int P)
{
	using namespace GoDetail;
	const EGoColor C = Mover;
	if (!LegalAt(P, C))
	{
		return -1;
	}
	PlaceStone(P, C);
	const uint8_t Own = Code(C);
	const uint8_t Opp = OtherOf(Own);
	const int Dirs[4] = { 1, -1, W, -W };
	for (int D : Dirs)
	{
		const int Q = P + D;
		if (Color[Q] == Own && Head[Q] != Head[P])
		{
			MergeGroups(Head[P], Head[Q]);
		}
	}
	int Captured = 0;
	int Lone = -1;
	for (int D : Dirs)
	{
		const int Q = P + D;
		if (Color[Q] == Opp && PseudoLibs[Head[Q]] == 0)
		{
			const int Count = RemoveGroup(Head[Q]);
			Captured += Count;
			Lone = Count == 1 ? Q : Lone;
		}
	}
	// Ko: one stone taken by one stone that is left with that point as its only liberty.
	Ko = (Captured == 1 && GroupSize[Head[P]] == 1 && PseudoLibs[Head[P]] == 1) ? Lone : -1;
	Taken[Own == BlackC ? 0 : 1] += Captured;
	Mover = GoOpponent(C);
	Last = Unpad(P);
	Passes = 0;
	return Captured;
}

int FGoBoard::Play(int Point)
{
	using namespace GoDetail;
	if (Point == GoPass)
	{
		Ko = -1;
		Mover = GoOpponent(Mover);
		Last = GoPass;
		++Passes;
		return 0;
	}
	if (Point < 0 || Point >= N * N)
	{
		return -1;
	}
	return PlayAt(Pad(Point));
}

int FGoBoard::LibertiesAt(int P, int Cap) const
{
	using namespace GoDetail;
	if (!IsStone(Color[P]))
	{
		return 0;
	}
	FMarks Marks;
	const int Dirs[4] = { 1, -1, W, -W };
	int Count = 0;
	const int H = Head[P];
	int S = H;
	do
	{
		for (int D : Dirs)
		{
			const int Q = S + D;
			if (Color[Q] == EmptyC && !Marks.Test(Q))
			{
				Marks.Set(Q);
				if (++Count >= Cap)
				{
					return Count;
				}
			}
		}
		S = NextStone[S];
	} while (S != H);
	return Count;
}

int FGoBoard::Liberties(int Point, int Cap) const
{
	using namespace GoDetail;
	return (Point >= 0 && Point < N * N) ? LibertiesAt(Pad(Point), Cap) : 0;
}

void FGoBoard::Group(int Point, std::vector<int>& Stones, std::vector<int>* Libs) const
{
	using namespace GoDetail;
	Stones.clear();
	if (Libs)
	{
		Libs->clear();
	}
	if (Point < 0 || Point >= N * N)
	{
		return;
	}
	const int P = Pad(Point);
	if (!IsStone(Color[P]))
	{
		return;
	}
	FMarks Marks;
	const int Dirs[4] = { 1, -1, W, -W };
	const int H = Head[P];
	int S = H;
	do
	{
		Stones.push_back(Unpad(S));
		if (Libs)
		{
			for (int D : Dirs)
			{
				const int Q = S + D;
				if (Color[Q] == EmptyC && !Marks.Test(Q))
				{
					Marks.Set(Q);
					Libs->push_back(Unpad(Q));
				}
			}
		}
		S = NextStone[S];
	} while (S != H);
}

int FGoBoard::LibertiesAfterAt(int P, EGoColor C, int Cap, int* OutSize) const
{
	using namespace GoDetail;
	FMarks Marks;
	Marks.Set(P);   // the stone's own point is not a liberty
	const int Dirs[4] = { 1, -1, W, -W };
	const uint8_t Own = Code(C);
	int Heads[4] = { -1, -1, -1, -1 };
	int NumHeads = 0;
	int Joined = 1;
	for (int D : Dirs)
	{
		const int Q = P + D;
		if (Color[Q] == Own)
		{
			const int H = Head[Q];
			if (std::find(Heads, Heads + NumHeads, H) == Heads + NumHeads)
			{
				Heads[NumHeads++] = H;
				Joined += GroupSize[H];
			}
		}
	}
	if (OutSize)
	{
		*OutSize = Joined;
	}

	int Count = 0;
	for (int D : Dirs)
	{
		const int Q = P + D;
		const uint8_t QC = Color[Q];
		// An empty neighbour, or a neighbour this move captures, which will be empty.
		const bool bFree = QC == EmptyC || (IsStone(QC) && QC != Own && PseudoLibs[Head[Q]] == CountAdjacentOf(P, Head[Q]));
		if (bFree && !Marks.Test(Q))
		{
			Marks.Set(Q);
			if (++Count >= Cap)
			{
				return Count;
			}
		}
	}
	for (int I = 0; I < NumHeads; ++I)
	{
		int S = Heads[I];
		do
		{
			for (int D : Dirs)
			{
				const int Q = S + D;
				if (Color[Q] == EmptyC && !Marks.Test(Q))
				{
					Marks.Set(Q);
					if (++Count >= Cap)
					{
						return Count;
					}
				}
			}
			S = NextStone[S];
		} while (S != Heads[I]);
	}
	return Count;
}

int FGoBoard::LibertiesAfter(int Point, int Cap, int* OutSize) const
{
	using namespace GoDetail;
	return (Point >= 0 && Point < N * N) ? LibertiesAfterAt(Pad(Point), Mover, Cap, OutSize) : 0;
}

int FGoBoard::WouldCapture(int Point) const
{
	using namespace GoDetail;
	if (Point < 0 || Point >= N * N)
	{
		return 0;
	}
	const int P = Pad(Point);
	const uint8_t Opp = OtherOf(Code(Mover));
	const int Dirs[4] = { 1, -1, W, -W };
	int Heads[4] = { -1, -1, -1, -1 };
	int NumHeads = 0;
	int Count = 0;
	for (int D : Dirs)
	{
		const int Q = P + D;
		if (Color[Q] != Opp)
		{
			continue;
		}
		const int H = Head[Q];
		if (std::find(Heads, Heads + NumHeads, H) == Heads + NumHeads)
		{
			Heads[NumHeads++] = H;
			if (PseudoLibs[H] == CountAdjacentOf(P, H))
			{
				Count += GroupSize[H];
			}
		}
	}
	return Count;
}

bool FGoBoard::IsEyeAt(int P, EGoColor C) const
{
	using namespace GoDetail;
	if (Color[P] != EmptyC)
	{
		return false;
	}
	const uint8_t Own = Code(C);
	const int Dirs[4] = { 1, -1, W, -W };
	for (int D : Dirs)
	{
		if (Color[P + D] != Own && Color[P + D] != EdgeC)
		{
			return false;
		}
	}
	// A false eye: enough of its diagonals belong to the other side that it can be cut.
	const uint8_t Opp = OtherOf(Own);
	const int Diagonals[4] = { W + 1, W - 1, -W + 1, -W - 1 };
	int Theirs = 0;
	int Off = 0;
	for (int D : Diagonals)
	{
		const uint8_t Q = Color[P + D];
		Off += Q == EdgeC ? 1 : 0;
		Theirs += Q == Opp ? 1 : 0;
	}
	return Off > 0 ? Theirs == 0 : Theirs < 2;
}

bool FGoBoard::IsEye(int Point, EGoColor Owner) const
{
	using namespace GoDetail;
	return Point >= 0 && Point < N * N && IsEyeAt(Pad(Point), Owner);
}

int FGoBoard::AreaDifference() const
{
	using namespace GoDetail;
	int Black = 0;
	int White = 0;
	FMarks Seen;
	int Stack[MaxPadded];
	const int Dirs[4] = { 1, -1, W, -W };
	for (int Y = 0; Y < N; ++Y)
	{
		for (int X = 0; X < N; ++X)
		{
			const int P = (Y + 1) * W + X + 1;
			if (Color[P] == BlackC)
			{
				++Black;
				continue;
			}
			if (Color[P] == WhiteC)
			{
				++White;
				continue;
			}
			if (Seen.Test(P))
			{
				continue;
			}
			bool bTouchesBlack = false;
			bool bTouchesWhite = false;
			int Region = 0;
			int Top = 0;
			Stack[Top++] = P;
			Seen.Set(P);
			while (Top > 0)
			{
				const int S = Stack[--Top];
				++Region;
				for (int D : Dirs)
				{
					const int Q = S + D;
					const uint8_t QC = Color[Q];
					if (QC == EmptyC && !Seen.Test(Q))
					{
						Seen.Set(Q);
						Stack[Top++] = Q;
					}
					bTouchesBlack |= QC == BlackC;
					bTouchesWhite |= QC == WhiteC;
				}
			}
			if (bTouchesBlack != bTouchesWhite)
			{
				(bTouchesBlack ? Black : White) += Region;
			}
		}
	}
	return Black - White;
}

void FGoBoard::RemoveForCount(int Point)
{
	using namespace GoDetail;
	if (Point >= 0 && Point < N * N && IsStone(Color[Pad(Point)]))
	{
		Color[Pad(Point)] = EmptyC;
		Rebuild();
	}
}

// =====================================================================================
// THE SEARCH

FGoStrength FGoStrength::Beginner()
{
	using namespace GoDetail;
	FGoStrength S;
	S.Playouts = 300;
	S.Temperature = 1.0f;
	return S;
}

FGoStrength FGoStrength::Hustler()
{
	using namespace GoDetail;
	FGoStrength S;
	S.Playouts = 3000;
	S.Temperature = 0.25f;
	return S;
}

FGoStrength FGoStrength::Master()
{
	using namespace GoDetail;
	FGoStrength S;
	S.Playouts = 25000;
	S.Temperature = 0.0f;
	return S;
}

FGoStrength FGoStrength::Coach()
{
	using namespace GoDetail;
	FGoStrength S;
	S.Playouts = 15000;
	S.Temperature = 0.0f;
	return S;
}

class FGoSearch
{
public:
	FGoSearch(const FGoBoard& InRoot, EGoMode InMode, float InKomi, const std::vector<uint64_t>* InSeen, uint32_t Seed)
		: Root(InRoot), Mode(InMode), Komi(InKomi), Seen(InSeen), Rng(0x9E3779B97F4A7C15ull ^ (uint64_t(Seed) * 0x100000001B3ull))
	{
		ExpandAt = InRoot.Size() >= 19 ? 8 : InRoot.Size() >= 13 ? 5 : 3;
	}

	FGoThought Run(int Playouts, float Temperature);

	// Who owns each point at the end of quick games from the root: +1 Black every time, -1 White.
	void Ownership(int Playouts, std::vector<float>& Out);

private:
	struct FNode
	{
		int Move = GoNoMove;            // padded point, or GoPass
		int FirstChild = -1;
		int NumChildren = 0;
		int Count = 0;                  // real visits
		float Visits = 0.0f;            // with the prior's
		float Wins = 0.0f;              // for the side that played Move
		float RaveVisits = 0.0f;
		float RaveWins = 0.0f;
		uint8_t TerminalWinner = 0;     // FirstCapture: the move took a stone and won
		bool bExpanded = false;
	};

	struct FPlayed
	{
		int P;
		uint8_t C;
	};

	// A node grows its children after this many visits. A 19x19 node has four times a 9x9's
	// children, so it waits longer: the tree stays tens of megabytes, not hundreds.
	int ExpandAt = 3;
	static constexpr int MaxNodes = 1500000;
	static constexpr float RaveBias = 0.001f;
	static constexpr float Explore = 0.12f;

	int ChooseMove(FGoBoard& B);
	uint8_t Playout(FGoBoard& B, bool bAmaf);
	void Expand(int NodeIndex, const FGoBoard& B, bool bRoot);
	int Select(int NodeIndex) const;
	void Simulate();
	int FindLiberty(const FGoBoard& B, int P) const;
	uint8_t Winner(const FGoBoard& B) const;
	void AddOwnership(const FGoBoard& B);

	FGoBoard Root;
	EGoMode Mode;
	float Komi;
	const std::vector<uint64_t>* Seen;
	GoDetail::FRng Rng;
	std::vector<FNode> Nodes;
	std::vector<FPlayed> Sequence;
	std::vector<int> Path;
	std::vector<float> OwnerSum;
	int OwnerGames = 0;
};

int FGoSearch::FindLiberty(const FGoBoard& B, int P) const
{
	using namespace GoDetail;
	const int Dirs[4] = { 1, -1, B.W, -B.W };
	const int H = B.Head[P];
	int S = H;
	do
	{
		for (int D : Dirs)
		{
			if (B.Color[S + D] == EmptyC)
			{
				return S + D;
			}
		}
		S = B.NextStone[S];
	} while (S != H);
	return -1;
}

/* The quick game's move: take what the last move left in atari, run with what it put in
   atari, and otherwise anything legal that does not fill one of our own eyes or throw a
   group of two or more into atari. In FirstCapture a stone in atari is a lost game, so a
   single stone is not thrown in either. */
int FGoSearch::ChooseMove(FGoBoard& B)
{
	using namespace GoDetail;
	const EGoColor C = B.Mover;
	const uint8_t Own = Code(C);
	const uint8_t Opp = OtherOf(Own);
	const bool bCapture = Mode == EGoMode::FirstCapture;
	const int Dirs[4] = { 1, -1, B.W, -B.W };

	if (B.Last >= 0)
	{
		const int L = B.Pad(B.Last);
		if (B.Color[L] == Opp && (bCapture || Rng.Unit() < 0.9f) && B.LibertiesAt(L, 2) == 1)
		{
			const int Lib = FindLiberty(B, L);
			if (Lib >= 0 && B.LegalAt(Lib, C))
			{
				return Lib;
			}
		}
		if (bCapture || Rng.Unit() < 0.9f)
		{
			for (int D : Dirs)
			{
				const int Q = L + D;
				if (B.Color[Q] == Own && B.LibertiesAt(Q, 2) == 1)
				{
					const int Lib = FindLiberty(B, Q);
					if (Lib >= 0 && B.LegalAt(Lib, C) && B.LibertiesAfterAt(Lib, C, 2, nullptr) >= 2)
					{
						return Lib;
					}
				}
			}
		}
	}

	const int Count = B.EmptyCount;
	const int Start = Rng.Below(Count);
	for (int K = 0; K < Count; ++K)
	{
		int Slot = Start + K;
		Slot -= Slot >= Count ? Count : 0;
		const int P = B.Empties[Slot];
		if (!B.LegalAt(P, C) || B.IsEyeAt(P, C))
		{
			continue;
		}
		int Size = 1;
		if (B.LibertiesAfterAt(P, C, 2, &Size) == 1 && (bCapture || Size >= 2))
		{
			continue;   // into atari: a capture still frees a liberty, so it is not counted here
		}
		return P;
	}
	return GoPass;
}

uint8_t FGoSearch::Winner(const FGoBoard& B) const
{
	using namespace GoDetail;
	return static_cast<float>(B.AreaDifference()) - Komi > 0.0f ? BlackC : WhiteC;
}

void FGoSearch::AddOwnership(const FGoBoard& B)
{
	using namespace GoDetail;
	const int N = B.N;
	if (OwnerSum.empty())
	{
		OwnerSum.assign(N * N, 0.0f);
	}
	const int Dirs[4] = { 1, -1, B.W, -B.W };
	for (int I = 0; I < N * N; ++I)
	{
		const int P = B.Pad(I);
		uint8_t C = B.Color[P];
		if (C == EmptyC)
		{
			// In a finished quick game an empty point is an eye, or between two groups in seki.
			uint8_t Seen1 = EmptyC;
			for (int D : Dirs)
			{
				const uint8_t Q = B.Color[P + D];
				if (IsStone(Q))
				{
					Seen1 = (Seen1 == EmptyC || Seen1 == Q) ? Q : EdgeC;
				}
			}
			C = Seen1 == EdgeC ? EmptyC : Seen1;
		}
		OwnerSum[I] += C == BlackC ? 1.0f : C == WhiteC ? -1.0f : 0.0f;
	}
	++OwnerGames;
}

uint8_t FGoSearch::Playout(FGoBoard& B, bool bAmaf)
{
	using namespace GoDetail;
	const bool bCapture = Mode == EGoMode::FirstCapture;
	const int Limit = 3 * B.N * B.N;
	int PassesInARow = 0;
	for (int M = 0; M < Limit; ++M)
	{
		const int P = ChooseMove(B);
		if (P == GoPass)
		{
			B.Play(GoPass);
			if (++PassesInARow >= 2)
			{
				break;
			}
			continue;
		}
		PassesInARow = 0;
		const uint8_t Mover = Code(B.Mover);
		const int Captured = B.PlayAt(P);
		if (bAmaf)
		{
			Sequence.push_back({ P, Mover });
		}
		if (bCapture && Captured > 0)
		{
			return Mover;
		}
	}
	if (!bCapture)
	{
		AddOwnership(B);
	}
	return Winner(B);
}

void FGoSearch::Ownership(int Playouts, std::vector<float>& Out)
{
	using namespace GoDetail;
	for (int I = 0; I < Playouts; ++I)
	{
		FGoBoard B = Root;
		Playout(B, false);
	}
	Out.assign(Root.N * Root.N, 0.0f);
	for (int I = 0; I < static_cast<int>(OwnerSum.size()) && OwnerGames > 0; ++I)
	{
		Out[I] = OwnerSum[I] / static_cast<float>(OwnerGames);
	}
}

/* Every legal move but filling our own eye, and a pass. The prior is what a player knows
   before reading anything out: taking stones is good, saving stones in atari is good,
   putting your own stones into atari is bad, and the first line is wasted early on. */
void FGoSearch::Expand(int NodeIndex, const FGoBoard& B, bool bRoot)
{
	using namespace GoDetail;
	if (static_cast<int>(Nodes.size()) > MaxNodes)
	{
		return;
	}
	const EGoColor C = B.Mover;
	const uint8_t Own = Code(C);
	const int N = B.N;
	const int Dirs[4] = { 1, -1, B.W, -B.W };
	const bool bCapture = Mode == EGoMode::FirstCapture;
	const int Stones = N * N - B.EmptyCount;
	const bool bOpening = Stones * 3 < N * N;
	const bool bEarly = Stones * 8 < N * N;          // on 19x19, the first forty-odd moves
	const bool bBigBoard = N >= 13;
	const bool bFirstMoves = bBigBoard && Stones < 8;   // the corners are still being shared out
	// At the root - the move actually about to be played - what a player knows before reading
	// anything weighs five times as much. On 19x19 one stone is a small part of the board's
	// score and the quick games are noisy, so without this the ghost left a stone in atari
	// untaken and opened on the fifth line (2026-10-10). The search can still overturn it - a
	// capture that is really a trap loses its games - it just has to prove it.
	const float Weight = bRoot ? 5.0f : 1.0f;

	const int First = static_cast<int>(Nodes.size());
	auto Add = [this](int Move, float PriorVisits, float PriorWinRate)
	{
		FNode Child;
		Child.Move = Move;
		Child.Visits = PriorVisits;
		Child.Wins = PriorVisits * PriorWinRate;
		Nodes.push_back(Child);
	};

	for (int I = 0; I < B.EmptyCount; ++I)
	{
		const int P = B.Empties[I];
		if (!B.LegalAt(P, C) || B.IsEyeAt(P, C))
		{
			continue;
		}
		if (bRoot && Seen)
		{
			FGoBoard After = B;
			After.PlayAt(P);
			if (std::find(Seen->begin(), Seen->end(), After.Hash()) != Seen->end())
			{
				continue;   // superko: that whole-board position has been seen before
			}
		}
		int Size = 1;
		const int LibsAfter = B.LibertiesAfterAt(P, C, 3, &Size);
		const int X = (P % B.W) - 1;
		const int Y = (P / B.W) - 1;
		const int Line = std::min(std::min(X, Y), std::min(N - 1 - X, N - 1 - Y));   // 0 = the edge
		bool bSaves = false;
		bool bTakes = false;
		for (int D : Dirs)
		{
			const int Q = P + D;
			if (B.Color[Q] == Own && B.LibertiesAt(Q, 2) == 1)
			{
				bSaves = LibsAfter >= 2;
			}
			else if (IsStone(B.Color[Q]) && B.Color[Q] != Own && B.PseudoLibs[B.Head[Q]] == B.CountAdjacentOf(P, B.Head[Q]))
			{
				bTakes = true;
			}
		}
		// Taking and saving outweigh the opening's corners: "a ponnuki is worth thirty points".
		if (bTakes)
		{
			Add(P, (bCapture ? 50.0f : 40.0f) * Weight, bCapture ? 1.0f : 0.9f);
		}
		else if (bSaves)
		{
			Add(P, 30.0f * Weight, 0.85f);
		}
		else if (LibsAfter == 1 && (bCapture || Size >= 2))
		{
			Add(P, 20.0f * Weight, 0.1f);
		}
		else if (bOpening && Line == 0)
		{
			Add(P, 10.0f * Weight, 0.2f);
		}
		// On a big board the opening is the third and fourth lines - territory on the third,
		// influence on the fourth - and the second line is too low to be worth a move yet.
		else if (bEarly && bBigBoard && Line == 1)
		{
			Add(P, 8.0f * Weight, 0.3f);
		}
		else if (bEarly && bBigBoard && (Line == 2 || Line == 3))
		{
			// Corners first, then sides: the proverb, because a corner needs the fewest stones.
			const bool bCorner = std::min(X, N - 1 - X) <= 4 && std::min(Y, N - 1 - Y) <= 4;
			if (bFirstMoves && bCorner)
			{
				Add(P, 20.0f * Weight, 0.68f);   // the quick games favour the centre; players never do
			}
			else
			{
				Add(P, (bCorner ? 6.0f : 4.0f) * Weight, bCorner ? 0.66f : 0.6f);
			}
		}
		else if (bFirstMoves && Line >= 4)
		{
			Add(P, 6.0f * Weight, 0.4f);
		}
		else
		{
			Add(P, 2.0f, 0.5f);
		}
	}
	Add(GoPass, 10.0f, bCapture ? 0.1f : 0.3f);

	Nodes[NodeIndex].FirstChild = First;
	Nodes[NodeIndex].NumChildren = static_cast<int>(Nodes.size()) - First;
	Nodes[NodeIndex].bExpanded = true;
}

int FGoSearch::Select(int NodeIndex) const
{
	using namespace GoDetail;
	const FNode& Parent = Nodes[NodeIndex];
	const float LogN = std::log(static_cast<float>(Parent.Count) + 1.0f);
	int Best = -1;
	float BestValue = -1.0e9f;
	for (int I = 0; I < Parent.NumChildren; ++I)
	{
		const int Index = Parent.FirstChild + I;
		const FNode& Child = Nodes[Index];
		float Value = Child.Wins / Child.Visits;
		if (Child.RaveVisits > 0.0f)
		{
			const float Beta = Child.RaveVisits / (Child.RaveVisits + Child.Visits + Child.RaveVisits * Child.Visits * RaveBias);
			Value = (1.0f - Beta) * Value + Beta * (Child.RaveWins / Child.RaveVisits);
		}
		Value += Explore * std::sqrt(LogN / Child.Visits);
		if (Value > BestValue)
		{
			BestValue = Value;
			Best = Index;
		}
	}
	return Best;
}

void FGoSearch::Simulate()
{
	using namespace GoDetail;
	FGoBoard B = Root;
	Path.clear();
	Sequence.clear();
	Path.push_back(0);
	int Node = 0;
	uint8_t Result = 0;
	while (Nodes[Node].bExpanded && Nodes[Node].NumChildren > 0)
	{
		const int Child = Select(Node);
		const int Move = Nodes[Child].Move;
		const uint8_t Mover = Code(B.Mover);
		const int Captured = Move == GoPass ? B.Play(GoPass) : B.PlayAt(Move);
		Sequence.push_back({ Move, Mover });
		Path.push_back(Child);
		Node = Child;
		if (Nodes[Node].TerminalWinner)
		{
			Result = Nodes[Node].TerminalWinner;
			break;
		}
		if (Mode == EGoMode::FirstCapture && Captured > 0)
		{
			Nodes[Node].TerminalWinner = Mover;
			Result = Mover;
			break;
		}
		if (!Nodes[Node].bExpanded)
		{
			if (Nodes[Node].Count + 1 < ExpandAt)
			{
				break;
			}
			Expand(Node, B, false);
		}
	}
	if (!Result)
	{
		Result = Playout(B, true);
	}

	// Backup: each node's wins are for the side that played into it.
	for (int D = 0; D < static_cast<int>(Path.size()); ++D)
	{
		FNode& Visited = Nodes[Path[D]];
		++Visited.Count;
		Visited.Visits += 1.0f;
		if (D > 0 && Sequence[D - 1].C == Result)
		{
			Visited.Wins += 1.0f;
		}
	}

	// RAVE: every move the side to move at a node went on to play first, anywhere later in
	// this game, counts as a trial of that move from the node.
	uint8_t FirstBy[MaxPadded] = {};
	for (int I = static_cast<int>(Sequence.size()) - 1; I >= 0; --I)
	{
		if (Sequence[I].P >= 0)
		{
			FirstBy[Sequence[I].P] = Sequence[I].C;
		}
		if (I < static_cast<int>(Path.size()))
		{
			const FNode& At = Nodes[Path[I]];
			const uint8_t ToMove = Sequence[I].C;
			for (int K = 0; K < At.NumChildren; ++K)
			{
				FNode& Child = Nodes[At.FirstChild + K];
				if (Child.Move >= 0 && FirstBy[Child.Move] == ToMove)
				{
					Child.RaveVisits += 1.0f;
					Child.RaveWins += ToMove == Result ? 1.0f : 0.0f;
				}
			}
		}
	}
}

FGoThought FGoSearch::Run(int Playouts, float Temperature)
{
	using namespace GoDetail;
	FGoThought Out;
	Nodes.clear();
	Nodes.reserve(static_cast<size_t>(std::min(MaxNodes, std::max(1024, Playouts / ExpandAt * 60))));
	Nodes.push_back(FNode());
	Expand(0, Root, true);
	if (Nodes[0].NumChildren == 0)
	{
		return Out;
	}
	for (int I = 0; I < Playouts; ++I)
	{
		Simulate();
	}

	const FNode& RootNode = Nodes[0];
	int Best = RootNode.FirstChild;
	for (int K = 0; K < RootNode.NumChildren; ++K)
	{
		if (Nodes[RootNode.FirstChild + K].Count > Nodes[Best].Count)
		{
			Best = RootNode.FirstChild + K;
		}
	}
	int Chosen = Best;
	if (Temperature > 0.0f && Nodes[Best].Count > 0)
	{
		// Among the moves it took seriously, weighted by how seriously.
		std::vector<double> Weights(RootNode.NumChildren, 0.0);
		double Total = 0.0;
		for (int K = 0; K < RootNode.NumChildren; ++K)
		{
			const FNode& Child = Nodes[RootNode.FirstChild + K];
			if (Child.Count * 10 >= Nodes[Best].Count)
			{
				Weights[K] = std::pow(static_cast<double>(Child.Count), 1.0 / Temperature);
				Total += Weights[K];
			}
		}
		double Pick = static_cast<double>(Rng.Unit()) * Total;
		for (int K = 0; K < RootNode.NumChildren && Total > 0.0; ++K)
		{
			Pick -= Weights[K];
			if (Weights[K] > 0.0 && Pick <= 0.0)
			{
				Chosen = RootNode.FirstChild + K;
				break;
			}
		}
	}

	auto ToPoint = [this](int Move) { return Move == GoPass ? GoPass : Root.Unpad(Move); };
	Out.Move = ToPoint(Nodes[Chosen].Move);
	Out.WinRate = Nodes[Chosen].Wins / Nodes[Chosen].Visits;
	Out.Playouts = Playouts;
	Out.Line.push_back(Out.Move);
	int Node = Chosen;
	while (Nodes[Node].bExpanded && Nodes[Node].NumChildren > 0 && Out.Line.size() < 10)
	{
		int Next = Nodes[Node].FirstChild;
		for (int K = 0; K < Nodes[Node].NumChildren; ++K)
		{
			if (Nodes[Nodes[Node].FirstChild + K].Count > Nodes[Next].Count)
			{
				Next = Nodes[Node].FirstChild + K;
			}
		}
		if (Nodes[Next].Count < 3)
		{
			break;
		}
		Out.Line.push_back(ToPoint(Nodes[Next].Move));
		Node = Next;
	}
	Out.Ownership.assign(Root.N * Root.N, 0.0f);
	for (int I = 0; I < static_cast<int>(OwnerSum.size()) && OwnerGames > 0; ++I)
	{
		Out.Ownership[I] = OwnerSum[I] / static_cast<float>(OwnerGames);
	}
	return Out;
}

// =====================================================================================
// THE GAME

FGoGame::FGoGame(int InSize, EGoMode InMode, float InKomi)
	: Initial(InSize), Current(InSize), GameMode(InMode), KomiPoints(InKomi)
{
	Reset();
}

FGoGame FGoGame::FromBoard(const FGoBoard& Start, EGoMode InMode, float InKomi)
{
	using namespace GoDetail;
	FGoGame Game(Start.Size(), InMode, InKomi);
	Game.Initial = Start;
	Game.Reset();
	return Game;
}

void FGoGame::Reset()
{
	using namespace GoDetail;
	Current = Initial;
	Moves.clear();
	Seen.assign(1, Initial.Hash());
	Outcome = FGoResult();
	Resigned = EGoColor::Empty;
}

void FGoGame::Reset(int InSize, EGoMode InMode, float InKomi)
{
	using namespace GoDetail;
	Initial = FGoBoard(InSize);
	GameMode = InMode;
	KomiPoints = InKomi;
	Reset();
}

bool FGoGame::IsLegal(int Point) const
{
	using namespace GoDetail;
	if (Outcome.IsOver())
	{
		return false;
	}
	if (Point == GoPass)
	{
		return true;
	}
	if (!Current.IsLegal(Point))
	{
		return false;
	}
	FGoBoard After = Current;
	After.Play(Point);
	return std::find(Seen.begin(), Seen.end(), After.Hash()) == Seen.end();
}

bool FGoGame::Play(int Point)
{
	using namespace GoDetail;
	if (!IsLegal(Point))
	{
		return false;
	}
	const EGoColor Mover = Current.ToMove();
	const int Captured = Current.Play(Point);
	Moves.push_back(Point);
	Seen.push_back(Current.Hash());
	if (GameMode == EGoMode::FirstCapture && Captured > 0)
	{
		Finish(EGoOutcome::FirstCapture, Mover);
	}
	else if (Current.ConsecutivePasses() >= 2)
	{
		Finish(EGoOutcome::Counted, EGoColor::Empty);
	}
	return true;
}

void FGoGame::Finish(EGoOutcome How, EGoColor Winner)
{
	using namespace GoDetail;
	Outcome = FGoResult();
	Outcome.Outcome = How;
	Outcome.Winner = Winner;
	if (How == EGoOutcome::Counted)
	{
		const uint64_t H = Current.Hash();
		Outcome.Count = Estimate(800, static_cast<uint32_t>(H ^ (H >> 32)) | 1u);
		Outcome.Winner = Outcome.Count.Leader;
		Outcome.Margin = Outcome.Count.Margin;
	}
}

void FGoGame::Replay()
{
	using namespace GoDetail;
	Current = Initial;
	Seen.assign(1, Initial.Hash());
	Outcome = FGoResult();
	for (int Move : Moves)
	{
		Current.Play(Move);
		Seen.push_back(Current.Hash());
	}
}

bool FGoGame::Undo()
{
	using namespace GoDetail;
	if (Resigned != EGoColor::Empty)
	{
		Resigned = EGoColor::Empty;
		Outcome = FGoResult();
		return true;
	}
	if (Moves.empty())
	{
		return false;
	}
	Moves.pop_back();
	Replay();
	return true;
}

void FGoGame::Resign(EGoColor Who)
{
	using namespace GoDetail;
	if (Outcome.IsOver())
	{
		return;
	}
	Resigned = Who;
	Outcome = FGoResult();
	Outcome.Outcome = EGoOutcome::Resignation;
	Outcome.Winner = GoOpponent(Who);
}

FGoThought FGoGame::Think(const FGoStrength& Strength) const
{
	using namespace GoDetail;
	if (Outcome.IsOver())
	{
		return FGoThought();
	}
	FGoSearch Search(Current, GameMode, KomiPoints, &Seen, Strength.Seed);
	return Search.Run(std::max(1, Strength.Playouts), Strength.Temperature);
}

/* Dead stones first: a group the other colour owns at the end of most quick games is dead.
   Then the area: stones, plus empty regions that touch one colour only - and where a border
   was never closed, the quick games say whose the gap is. */
FGoEstimate FGoGame::Estimate(int Playouts, uint32_t Seed) const
{
	using namespace GoDetail;
	const int N = Size();
	const int Count = N * N;
	// Half the quick games with each side to move first: whoever happens to be on move would
	// otherwise get the first stone into every gap.
	std::vector<float> Own;
	std::vector<float> OtherHalf;
	FGoSearch Search(Current, EGoMode::Full, KomiPoints, nullptr, Seed);
	Search.Ownership(std::max(1, Playouts / 2), Own);
	FGoBoard Flipped = Current;
	Flipped.Play(GoPass);
	FGoSearch Other(Flipped, EGoMode::Full, KomiPoints, nullptr, Seed ^ 0x5A5A5A5Au);
	Other.Ownership(std::max(1, Playouts - Playouts / 2), OtherHalf);
	for (size_t I = 0; I < Own.size(); ++I)
	{
		Own[I] = 0.5f * (Own[I] + OtherHalf[I]);
	}

	FGoEstimate E;
	E.Owner.assign(Count, EGoColor::Empty);
	E.Dead.assign(Count, false);
	FGoBoard Clean = Current;
	std::vector<bool> Done(Count, false);
	std::vector<int> Stones;
	for (int P = 0; P < Count; ++P)
	{
		const EGoColor C = Current.At(P);
		if (C == EGoColor::Empty || Done[P])
		{
			continue;
		}
		Current.Group(P, Stones);
		float Sum = 0.0f;
		for (int S : Stones)
		{
			Done[S] = true;
			Sum += Own[S];
		}
		const float Mine = (C == EGoColor::Black ? 1.0f : -1.0f) * Sum / static_cast<float>(Stones.size());
		if (Mine < 0.0f)
		{
			for (int S : Stones)
			{
				E.Dead[S] = true;
				Clean.RemoveForCount(S);
			}
		}
	}

	// Where a border was left open, a point goes to whoever owns it at the end of at least five
	// quick games in eight (+0.25). The quick games are noisy - a random stone can live
	// anywhere - so a point either side really holds still scores only a little over that.
	constexpr float UnsettledShare = 0.25f;
	std::vector<bool> Visited(Count, false);
	std::vector<int> Region;
	for (int P = 0; P < Count; ++P)
	{
		const EGoColor C = Clean.At(P);
		if (C != EGoColor::Empty)
		{
			E.Owner[P] = C;
			continue;
		}
		if (Visited[P])
		{
			continue;
		}
		Region.clear();
		Region.push_back(P);
		Visited[P] = true;
		bool bBlack = false;
		bool bWhite = false;
		for (size_t I = 0; I < Region.size(); ++I)
		{
			const int R = Region[I];
			const int X = R % N;
			const int Y = R / N;
			const int Around[4] = { X > 0 ? R - 1 : -1, X < N - 1 ? R + 1 : -1, Y > 0 ? R - N : -1, Y < N - 1 ? R + N : -1 };
			for (int Q : Around)
			{
				if (Q < 0)
				{
					continue;
				}
				const EGoColor QC = Clean.At(Q);
				if (QC == EGoColor::Empty && !Visited[Q])
				{
					Visited[Q] = true;
					Region.push_back(Q);
				}
				bBlack |= QC == EGoColor::Black;
				bWhite |= QC == EGoColor::White;
			}
		}
		for (int R : Region)
		{
			if (bBlack != bWhite)
			{
				E.Owner[R] = bBlack ? EGoColor::Black : EGoColor::White;
			}
			else if (Own[R] > UnsettledShare)
			{
				E.Owner[R] = EGoColor::Black;
			}
			else if (Own[R] < -UnsettledShare)
			{
				E.Owner[R] = EGoColor::White;
			}
		}
	}

	for (int P = 0; P < Count; ++P)
	{
		E.BlackArea += E.Owner[P] == EGoColor::Black ? 1.0f : 0.0f;
		E.WhiteArea += E.Owner[P] == EGoColor::White ? 1.0f : 0.0f;
	}
	E.WhiteArea += KomiPoints;
	E.Margin = std::fabs(E.BlackArea - E.WhiteArea);
	E.Leader = E.BlackArea > E.WhiteArea ? EGoColor::Black : E.WhiteArea > E.BlackArea ? EGoColor::White : EGoColor::Empty;
	return E;
}

std::string FGoGame::PointName(int Point) const
{
	using namespace GoDetail;
	if (Point == GoPass)
	{
		return "pass";
	}
	if (Point < 0 || Point >= Size() * Size())
	{
		return "?";
	}
	char Buffer[8];
	std::snprintf(Buffer, sizeof(Buffer), "%c%d", GoColumnLetter(Point % Size()), Point / Size() + 1);
	return Buffer;
}

std::string FGoGame::MoveListText(int LastN) const
{
	using namespace GoDetail;
	std::string Text;
	const int Total = MovesPlayed();
	const int From = LastN > 0 ? std::max(0, Total - LastN) : 0;
	const bool bBlackFirst = Initial.ToMove() == EGoColor::Black;
	for (int I = From; I < Total; ++I)
	{
		const bool bBlack = (I % 2 == 0) == bBlackFirst;
		char Buffer[32];
		std::snprintf(Buffer, sizeof(Buffer), "%s%d. %s %s", Text.empty() ? "" : "  ", I + 1, bBlack ? "B" : "W", PointName(Moves[I]).c_str());
		Text += Buffer;
	}
	return Text;
}

// =====================================================================================
// THE COACH

namespace GoDetail
{
	inline std::string Capitalized(std::string Text)
	{
		if (!Text.empty() && Text[0] >= 'a' && Text[0] <= 'z')
		{
			Text[0] = static_cast<char>(Text[0] - 'a' + 'A');
		}
		return Text;
	}

	inline std::string Stones(int Count)
	{
		return Count == 1 ? "1 stone" : std::to_string(Count) + " stones";
	}
}

FGoAdvice FGoGame::Advise(const FGoStrength& Strength, const char* OpponentName) const
{
	using namespace GoDetail;
	FGoAdvice A;
	if (Outcome.IsOver())
	{
		return A;
	}
	A.Search = Think(Strength);
	A.Move = A.Search.Move;
	A.WinRate = A.Search.WinRate;
	const std::string Opp = OpponentName ? OpponentName : "the other side";
	const std::string Name = PointName(A.Move);
	const bool bCapture = GameMode == EGoMode::FirstCapture;
	const FGoBoard& B = Current;
	const EGoColor Me = B.ToMove();
	const int N = Size();

	std::string Why;
	if (A.Move == GoPass)
	{
		Why = "Pass. Nothing left is worth a stone; if " + Opp + " passes too, the game is counted.";
	}
	else
	{
		const int X = A.Move % N;
		const int Y = A.Move / N;
		const int Around[4] = { X > 0 ? A.Move - 1 : -1, X < N - 1 ? A.Move + 1 : -1, Y > 0 ? A.Move - N : -1, Y < N - 1 ? A.Move + N : -1 };
		const int Takes = B.WouldCapture(A.Move);
		int Saves = 0;
		int Ataris = 0;
		int MyGroups = 0;
		std::vector<int> Counted;
		std::vector<int> Group;
		for (int Q : Around)
		{
			if (Q < 0 || B.At(Q) == EGoColor::Empty)
			{
				continue;
			}
			B.Group(Q, Group);
			if (std::find(Counted.begin(), Counted.end(), Group[0]) != Counted.end())
			{
				continue;
			}
			Counted.push_back(Group[0]);
			const int Libs = B.Liberties(Q, 3);
			if (B.At(Q) == Me)
			{
				++MyGroups;
				Saves += Libs == 1 ? static_cast<int>(Group.size()) : 0;
			}
			else if (Libs == 2)
			{
				Ataris += static_cast<int>(Group.size());
			}
		}
		if (Takes > 0)
		{
			Why = bCapture ? Name + " captures - and the first capture wins the game!"
				: Name + " captures " + Stones(Takes) + ".";
		}
		else if (Saves > 0 && B.LibertiesAfter(A.Move, 2) >= 2)
		{
			Why = Name + " rescues your " + Stones(Saves) + ": " + (Saves == 1 ? "it was" : "they were")
				+ " in atari, one liberty from being taken.";
		}
		else if (Ataris > 0)
		{
			Why = Name + " puts " + Stones(Ataris) + " of " + Opp + "'s in atari: one liberty left, so "
				+ (bCapture ? "unless they escape, you take them and win." : "they must run or be taken.");
		}
		else if (MyGroups >= 2)
		{
			Why = Name + " joins your stones into one group. A group is harder to kill than its parts.";
		}
		else if (N <= 9 && MovesPlayed() < 4 && X == N / 2 && Y == N / 2)
		{
			Why = Name + " is the centre point. On a small board it is the strongest first move: it reaches every side.";
		}
		else if (N <= 9 && MovesPlayed() < 6)
		{
			Why = Name + " takes open space. Early on, claim the corners and sides; they are the easiest to keep.";
		}
		else if (N > 9 && MovesPlayed() < 12 && std::min(std::min(X, Y), std::min(N - 1 - X, N - 1 - Y)) >= 2
			&& std::min(std::min(X, Y), std::min(N - 1 - X, N - 1 - Y)) <= 4
			&& (std::min(X, N - 1 - X) <= 5 && std::min(Y, N - 1 - Y) <= 5))
		{
			Why = Name + " takes a corner. On a big board, corners first: a corner needs the fewest stones to make "
				"territory, then the sides, then the centre.";
		}
		else if (N > 9 && MovesPlayed() < 30 && std::min(std::min(X, Y), std::min(N - 1 - X, N - 1 - Y)) >= 2
			&& std::min(std::min(X, Y), std::min(N - 1 - X, N - 1 - Y)) <= 3)
		{
			Why = Name + " is on the third or fourth line, where the opening is played: the third line makes "
				"territory, the fourth makes influence toward the centre.";
		}
		else
		{
			Why = Name + " is the coach's choice: of its " + std::to_string(A.Search.Playouts) + " quick games, it wins the most.";
		}
	}
	const int Percent = static_cast<int>(std::lround(A.WinRate * 100.0f));
	A.Text = Why + " From there you win about " + std::to_string(Percent) + "% of the coach's games.";
	return A;
}

FGoReview FGoGame::ReviewLastMove(const FGoStrength& Strength, const char* OpponentName, const FGoAdvice* Before) const
{
	using namespace GoDetail;
	FGoReview R;
	if (Moves.empty())
	{
		return R;
	}
	const int Mine = Moves.back();
	FGoGame Earlier = *this;
	Earlier.Undo();
	const FGoAdvice Best = Before ? *Before : Earlier.Advise(Strength, OpponentName);
	if (Best.Move == Mine)
	{
		return R;
	}
	const std::string Opp = OpponentName ? OpponentName : "the other side";

	float MyChance = 0.0f;
	int Answer = GoNoMove;
	if (Outcome.IsOver())
	{
		MyChance = Outcome.Winner == Earlier.ToMove() ? 1.0f : 0.0f;
	}
	else
	{
		const FGoThought Reply = Think(Strength);
		MyChance = 1.0f - Reply.WinRate;
		Answer = Reply.Move;
	}
	const float Drop = Best.WinRate - MyChance;
	if (Drop < 0.2f || Best.WinRate < 0.15f)
	{
		return R;
	}
	R.bMistake = true;
	// What he will see happen next, first: stones about to be taken, whether his move put them
	// in atari or left them there.
	std::string Text;
	const int Lost = (Answer >= 0 && Current.IsLegal(Answer)) ? Current.WouldCapture(Answer) : 0;
	if (Lost > 0)
	{
		Text = "That leaves your " + Stones(Lost) + " in atari: " + Opp + " takes " + (Lost == 1 ? "it" : "them") + " at "
			+ PointName(Answer) + (GameMode == EGoMode::FirstCapture ? ", and wins. " : ". ");
	}
	else if (Mine == GoPass)
	{
		Text = "Passing gives " + Opp + " a free move. ";
	}
	const int Was = static_cast<int>(std::lround(Best.WinRate * 100.0f));
	const int Now = static_cast<int>(std::lround(MyChance * 100.0f));
	Text += "That cost you: from about " + std::to_string(Was) + "% to " + std::to_string(Now) + "%. The coach liked "
		+ PointName(Best.Move) + ".";
	if (Lost == 0 && Answer >= 0)
	{
		Text += " " + Capitalized(Opp) + "'s best answer is " + PointName(Answer) + ".";
	}
	R.Text = Text;
	return R;
}
