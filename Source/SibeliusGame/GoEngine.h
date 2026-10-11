// GoEngine.h
//
// GO — the model for a second table in the Transhuman Chess Park (Walt, 2026-10-10: "what
// about Go? That was a famous win for AI against humans..."; "yes, build Go on a 9x9 board").
// In March 2016 AlphaGo beat Lee Sedol 4-1 in Seoul. The park already has chess, where Deep
// Blue beat Kasparov in 1997; with Go beside it, Leonard's city holds both of the great
// matches between people and machines.
//
// ===========================================================================
// THE HOUSE METHOD (FChessGame, FPinballSim): a pure model, standard library only,
// deterministic, tested standalone by Tools/Tests/run_go_engine_tests.ps1. AGoTable only
// presents it.
//
// THE RULES are the Chinese ones, the ones computers play: AREA scoring (your stones on the
// board plus the empty points only you surround), komi 7.5 to White for moving second, no
// suicide, and POSITIONAL SUPERKO - a move may not recreate any earlier whole-board position,
// which is the simple ko rule made complete. Two passes in a row end the game.
//
// FIRST CAPTURE WINS (EGoMode::FirstCapture) is how children are taught Go: the first player
// to take a stone wins on the spot. Every move is a threat or an answer to one, a game takes
// two minutes, and it teaches liberties - the one idea everything else in Go is built on.
//
// THE GHOST is Monte Carlo tree search, the method that made computer Go strong a decade
// before AlphaGo, and the half of AlphaGo that is not a neural network: to judge a move it
// plays thousands of quick, nearly random games from it to the end and counts how many it
// wins. RAVE ("all moves as first") shares what each game learns about a point across every
// branch that could play it, which is what makes a few thousand games enough on a 9x9 board.
// The quick games are not quite random: they capture a group the last move left in atari,
// run from atari, never fill their own eyes and never throw a big group into atari. Its
// strength is how many games it plays (FGoStrength::Playouts); the beginner also picks
// among its good moves rather than always the best (Temperature).
//
// COUNTING A FINISHED GAME needs to know which stones are dead - stones left inside the other
// player's area that could not live. People agree on that by looking; here the board's own
// quick games decide it (FGoGame::Estimate): a stone the other colour owns at the end of most
// of them is dead. People who are new to Go pass long before the borders are sealed, so a
// gap in a border is filled in the same way.
//
// THREADING. No mutable global state (the hashing keys are built once, on first use, which
// C++ makes thread-safe), so Think runs on a worker thread on a copy of the game.
//
// Points are 0..Size*Size-1: x + y * Size, x = 0 the left column (A), y = 0 the bottom row
// (1), seen from Black's side. GoPass is a pass.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class EGoColor : uint8_t
{
	Empty,
	Black,
	White,
	Edge,   // off the board, inside FGoBoard only
};

inline EGoColor GoOpponent(EGoColor Color)
{
	return Color == EGoColor::Black ? EGoColor::White : EGoColor::Black;
}

constexpr int GoMaxSize = 19;
constexpr int GoMinSize = 5;
constexpr int GoPass = -1;
constexpr int GoNoMove = -2;

enum class EGoMode : uint8_t
{
	Full,           // territory, two passes end it
	FirstCapture,   // the first stone taken wins
};

// One position: the stones, whose move it is, and the simple-ko point. Copyable (fixed arrays,
// no heap), which is what the search's thousands of quick games are played on. It keeps every
// group's stones on a ring and a count of its "pseudo-liberties" (each stone's empty
// neighbours, counted once per stone), which says exactly when a group has no liberty left,
// and exactly when its only liberty is a given point, without walking the group.
class FGoBoard
{
public:
	explicit FGoBoard(int InSize = 9);

	// A test position from rows of text, top row (the highest y) first: 'X' Black, 'O' White,
	// '.' empty; spaces are ignored. Empty on a malformed diagram (wrong row count or width).
	static FGoBoard FromDiagram(const std::vector<std::string>& Rows, EGoColor ToMove);

	int Size() const { return N; }
	int NumPoints() const { return N * N; }
	EGoColor At(int Point) const { return static_cast<EGoColor>(Color[Pad(Point)]); }
	EGoColor ToMove() const { return Mover; }
	int KoPoint() const { return Ko >= 0 ? Unpad(Ko) : GoNoMove; }
	int LastMove() const { return Last; }      // GoNoMove at the start, GoPass after a pass
	int ConsecutivePasses() const { return Passes; }
	int Prisoners(EGoColor Taker) const { return Taken[Taker == EGoColor::Black ? 0 : 1]; }
	uint64_t Hash() const { return StoneHash; }   // the stones only: what superko compares

	// Simple ko only: superko needs the game's history and is FGoGame's to check.
	bool IsLegal(int Point) const;

	// Plays for the side to move. Returns the stones it captured, or -1 if the move is
	// illegal (nothing changes). A pass returns 0.
	int Play(int Point);

	// The exact liberties of the group standing on Point, counted up to Cap.
	int Liberties(int Point, int Cap = 1000) const;
	// The group standing on Point, and its liberties.
	void Group(int Point, std::vector<int>& Stones, std::vector<int>* Libs = nullptr) const;
	// What playing Point would leave the mover's new group with (up to Cap), and how big it
	// would be; captures count as freeing a liberty. Point must be legal.
	int LibertiesAfter(int Point, int Cap, int* OutSize = nullptr) const;
	// Stones the move would capture (0 if none). Point must be legal.
	int WouldCapture(int Point) const;
	// A point every neighbour of which is Color's (or the edge), and not a false eye.
	bool IsEye(int Point, EGoColor Owner) const;

	// Area by Tromp-Taylor: stones, plus empty regions that touch only one colour.
	// Returns Black's area minus White's, before komi.
	int AreaDifference() const;

	// Rebuilds the groups from the stones alone. The tests compare it with the incremental
	// bookkeeping after every move of hundreds of random games.
	void Rebuild();
	bool SameBookkeeping(const FGoBoard& Other) const;

	// Removes a stone with no other bookkeeping than the rebuild: dead stones at the count.
	void RemoveForCount(int Point);

private:
	friend class FGoSearch;

	int Pad(int Point) const { return (Point / N + 1) * W + (Point % N + 1); }
	int Unpad(int P) const { return (P / W - 1) * N + (P % W - 1); }

	bool LegalAt(int P, EGoColor C) const;
	int PlayAt(int P);                    // -1 if illegal
	void PlaceStone(int P, EGoColor C);
	int RemoveGroup(int Head);
	void MergeGroups(int A, int B);
	void AddEmpty(int P);
	void RemoveEmpty(int P);
	int CountAdjacentOf(int P, int Head) const;
	bool IsEyeAt(int P, EGoColor C) const;
	int LibertiesAt(int P, int Cap) const;
	int LibertiesAfterAt(int P, EGoColor C, int Cap, int* OutSize) const;

	static constexpr int MaxPadded = (GoMaxSize + 2) * (GoMaxSize + 2);

	int N = 9;
	int W = 11;                            // padded width
	uint8_t Color[MaxPadded] = {};
	int Head[MaxPadded] = {};              // the group's representative stone
	int NextStone[MaxPadded] = {};         // the ring of the group's stones
	int GroupSize[MaxPadded] = {};         // by head
	int PseudoLibs[MaxPadded] = {};        // by head
	int Empties[GoMaxSize * GoMaxSize] = {};
	int EmptySlot[MaxPadded] = {};
	int EmptyCount = 0;
	int Ko = -1;                           // padded, or -1
	int Last = GoNoMove;                   // unpadded
	int Passes = 0;
	int Taken[2] = {};
	EGoColor Mover = EGoColor::Black;
	uint64_t StoneHash = 0;
};

struct FGoStrength
{
	int Playouts = 3000;            // quick games per move
	float Temperature = 0.0f;       // 0 = the most-played move; above, sometimes a lesser one
	uint32_t Seed = 1;

	static FGoStrength Beginner();  // a few hundred games, and careless about which it picks
	static FGoStrength Hustler();   // a few thousand: the park table
	static FGoStrength Master();    // tens of thousands, always its best
	static FGoStrength Coach();     // for hints and reviews: as strong as the master
};

struct FGoThought
{
	int Move = GoPass;
	float WinRate = 0.5f;               // for the side that moves, by the search's own count
	int Playouts = 0;
	std::vector<int> Line;              // the line it expects, starting with Move
	std::vector<float> Ownership;       // per point, +1 Black's at the end of every game .. -1 White's
};

// Which stones are dead and who owns each point, for the count and for showing it.
struct FGoEstimate
{
	std::vector<EGoColor> Owner;        // per point: Black, White or Empty (nobody's)
	std::vector<bool> Dead;             // per point: a stone counted as captured
	float BlackArea = 0.0f;
	float WhiteArea = 0.0f;             // with komi
	EGoColor Leader = EGoColor::Empty;
	float Margin = 0.0f;
};

enum class EGoOutcome : uint8_t
{
	None,
	Counted,        // two passes, then the count
	FirstCapture,
	Resignation,
};

struct FGoResult
{
	EGoOutcome Outcome = EGoOutcome::None;
	EGoColor Winner = EGoColor::Empty;
	float Margin = 0.0f;                // points, for a count
	FGoEstimate Count;                  // the board as counted, for a count

	bool IsOver() const { return Outcome != EGoOutcome::None; }
};

// The coach: the move it would play and a sentence saying why.
struct FGoAdvice
{
	int Move = GoPass;
	float WinRate = 0.5f;               // the mover's, after the coach's move
	std::string Text;
	FGoThought Search;
};

struct FGoReview
{
	bool bMistake = false;
	std::string Text;
};

class FGoGame
{
public:
	explicit FGoGame(int InSize = 9, EGoMode InMode = EGoMode::Full, float InKomi = 7.5f);
	static FGoGame FromBoard(const FGoBoard& Start, EGoMode InMode = EGoMode::Full, float InKomi = 7.5f);

	void Reset();
	void Reset(int InSize, EGoMode InMode, float InKomi);

	const FGoBoard& Board() const { return Current; }
	int Size() const { return Current.Size(); }
	EGoMode Mode() const { return GameMode; }
	float Komi() const { return KomiPoints; }
	EGoColor ToMove() const { return Current.ToMove(); }
	int MovesPlayed() const { return static_cast<int>(Moves.size()); }
	int LastMove() const { return Moves.empty() ? GoNoMove : Moves.back(); }
	const std::vector<int>& MoveList() const { return Moves; }

	// Superko included. GoPass is always legal while the game is on.
	bool IsLegal(int Point) const;
	bool Play(int Point);           // false if illegal or the game is over
	bool Undo();                    // one move; a resignation is undone first
	void Resign(EGoColor Who);
	const FGoResult& Result() const { return Outcome; }

	FGoThought Think(const FGoStrength& Strength) const;
	FGoAdvice Advise(const FGoStrength& Strength, const char* OpponentName) const;
	// After a move, how much it cost. Before is the advice for the position it was played
	// from, if the caller has it (the hint he did not ask to see is usually ready).
	FGoReview ReviewLastMove(const FGoStrength& Strength, const char* OpponentName, const FGoAdvice* Before) const;

	// Who owns what, by quick games from here. Deterministic for a given seed.
	FGoEstimate Estimate(int Playouts, uint32_t Seed) const;

	std::string PointName(int Point) const;     // "E5"; "pass"
	std::string MoveListText(int LastN = 0) const;

private:
	void Replay();
	void Finish(EGoOutcome How, EGoColor Winner);

	FGoBoard Initial;
	FGoBoard Current;
	EGoMode GameMode = EGoMode::Full;
	float KomiPoints = 7.5f;
	std::vector<int> Moves;
	std::vector<uint64_t> Seen;     // every position's stone hash so far, for superko
	FGoResult Outcome;
	EGoColor Resigned = EGoColor::Empty;
};

// Column letters skip I, as on every Go board (it looks too much like J and 1).
char GoColumnLetter(int X);
