// ChessEngine.h
//
// CHESS — the model for a park table in Transhuman City, where one of the ghosts has
// finally stopped dancing long enough to play Leonard (Walt, 2026-09-28: "let's write
// our own chess engine").
//
// ===========================================================================
// THE HOUSE METHOD (FPinballSim, FBilliardsSim, FBowlingSim): a pure model, standard
// library only, deterministic, tested standalone by Tools/Tests/run_chess_engine_tests.ps1.
// A table actor will only present it.
//
// WHY OUR OWN AND NOT STOCKFISH. Stockfish is GPL-3: shipping it means shipping its source
// beside the Steam build, and linking it would make the game GPL. It also beats world
// champions, and a park hustler has to be beatable. This one is small, ours, and tunable.
//
// WHY IT CAN BE TRUSTED. Move generation is the part that is easy to get subtly wrong
// (castling through check, en passant that exposes the king, promotion under a pin), and
// it is the one part of chess with an exact oracle: PERFT, the published count of every
// legal move sequence to a given depth from a given position. From the opening position
// there are 20 moves, 400 after two plies, and 4,865,609 after five. The tests hold the
// generator to six positions' published counts, which between them exercise every rule.
//
// THE SEARCH is the textbook one: negamax alpha-beta with iterative deepening, a capture
// search at the leaves so it never stops counting in the middle of a trade, a check
// extension, killer moves and captures-first ordering. The evaluation is material plus
// piece-square tables, a king that hides in the middlegame and walks out in the endgame,
// and a mop-up term so a won ending actually gets won. It plays like a decent club player
// at depth 4, and it is weakened for the park the way a person is weak: it sees the good
// moves and does not always pick the best one (FChessStrength::NoiseCentipawns).
//
// THREADING. There is no mutable global state (the hashing keys are built once, on first
// use, which C++ makes thread-safe), so FChessPosition::Search can run on a worker thread
// while the game thread draws the board. NodeLimit bounds how long it thinks.
//
// Squares are 0..63: a1 = 0, h1 = 7, a8 = 56, h8 = 63. File = Square & 7, rank = Square >> 3.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class EChessColor : uint8_t
{
	White,
	Black,
};

inline EChessColor ChessOpponent(EChessColor Color)
{
	return Color == EChessColor::White ? EChessColor::Black : EChessColor::White;
}

enum class EChessPieceType : uint8_t
{
	None,
	Pawn,
	Knight,
	Bishop,
	Rook,
	Queen,
	King,
};

struct FChessPiece
{
	EChessPieceType Type = EChessPieceType::None;
	EChessColor Color = EChessColor::White;

	bool IsEmpty() const { return Type == EChessPieceType::None; }
};

// Castling rights, as a bit set.
namespace ChessCastle
{
	constexpr uint8_t WhiteKingSide = 1;
	constexpr uint8_t WhiteQueenSide = 2;
	constexpr uint8_t BlackKingSide = 4;
	constexpr uint8_t BlackQueenSide = 8;
}

// What kind of move it is. Filled in by the generator; a move typed in by a player only
// needs From, To and Promotion, and FChessPosition::FindMove supplies the rest.
namespace ChessMoveFlag
{
	constexpr uint8_t Capture = 1;
	constexpr uint8_t DoublePush = 2;
	constexpr uint8_t EnPassant = 4;
	constexpr uint8_t Castle = 8;
}

struct FChessMove
{
	uint8_t From = 0;
	uint8_t To = 0;
	EChessPieceType Promotion = EChessPieceType::None;
	uint8_t Flags = 0;

	bool IsNull() const { return From == To; }
	bool IsCapture() const { return (Flags & ChessMoveFlag::Capture) != 0; }

	// Same move, whatever the flags say.
	bool SameAs(const FChessMove& Other) const
	{
		return From == Other.From && To == Other.To && Promotion == Other.Promotion;
	}

	// "e2e4", "e7e8q": the long algebraic form engines speak.
	std::string ToUci() const;
};

std::string ChessSquareName(int Square);      // 0 -> "a1"
int ChessParseSquare(const std::string& Name); // "a1" -> 0, or -1

// How hard the other player thinks. The presets are starting points for the table actor.
struct FChessStrength
{
	int MaxDepth = 4;               // plies of full search, before the capture search
	int64_t NodeLimit = 300000;     // stops thinking here, keeping the last finished depth
	int NoiseCentipawns = 0;        // picks among moves this close to the best (0 = the best)
	uint32_t Seed = 1;              // with the position, decides which of those it picks

	static FChessStrength Beginner();   // depth 2, careless
	static FChessStrength Hustler();    // depth 3, a little careless: the park table
	static FChessStrength Master();     // depth 5, always the best move it finds
};

struct FChessSearchResult
{
	FChessMove Best;                        // null only when there is no legal move
	int Score = 0;                          // centipawns, for the side to move
	int DepthReached = 0;
	int64_t Nodes = 0;
	std::vector<FChessMove> Line;           // the line it expects, starting with Best

	// A score this far from zero is a forced mate: ChessMateScore - plies to mate.
	static constexpr int MateScore = 30000;
	bool IsMate() const { return Score > MateScore - 1000 || Score < -(MateScore - 1000); }
	int MateInPlies() const { return Score > 0 ? MateScore - Score : MateScore + Score; }
};

struct FChessPosition
{
	// Each square holds 0 for empty, else the piece type plus 8 for black.
	uint8_t Board[64] = {};
	EChessColor SideToMove = EChessColor::White;
	uint8_t Castling = 0;
	int8_t EnPassant = -1;          // the square a pawn just skipped, or -1
	int HalfmoveClock = 0;          // plies since the last capture or pawn move
	int FullmoveNumber = 1;
	uint8_t KingSquare[2] = {};     // indexed by EChessColor

	static FChessPosition Start();

	// Rejects anything that is not a playable position: no king, two kings, pawns on the
	// back rank, or the side NOT to move already in check.
	static bool FromFen(const std::string& Fen, FChessPosition& Out);
	std::string ToFen() const;

	FChessPiece PieceAt(int Square) const;

	void LegalMoves(std::vector<FChessMove>& Out, bool bCapturesOnly = false) const;
	std::vector<FChessMove> LegalMoves() const;

	// Plays a move the generator produced. Does not check it is legal: FindMove does that.
	FChessPosition Played(const FChessMove& Move) const;

	// Matches a typed move ("e2e4", "e7e8q") against the legal moves. A pawn reaching the
	// last rank matches only with its Promotion named: the table actor asks, or picks queen.
	bool FindMove(const std::string& Uci, FChessMove& Out) const;
	bool FindMove(int From, int To, EChessPieceType Promotion, FChessMove& Out) const;

	bool InCheck() const;
	bool IsAttacked(int Square, EChessColor By) const;
	bool InsufficientMaterial() const;

	// Standard algebraic notation, for the move list on the panel: "Nbd2", "exd6", "O-O",
	// "e8=Q+", "Qxf7#". The move must be legal in this position.
	std::string ToSan(const FChessMove& Move) const;

	// Zobrist hash: equal for positions that are the same for the repetition rule.
	uint64_t Hash() const;

	// Material and placement in centipawns, from the side to move's point of view.
	int Evaluate() const;

	// Thinks. History is the hashes of the positions already played in this game, oldest
	// first, so the search knows which lines repeat into a draw.
	FChessSearchResult Search(const FChessStrength& Strength, const std::vector<uint64_t>& History = {}) const;
};

// The count of legal move sequences of exactly Depth plies: the move generator's oracle.
uint64_t ChessPerft(const FChessPosition& Position, int Depth);

enum class EChessOutcome : uint8_t
{
	InProgress,
	Checkmate,
	Stalemate,
	FiftyMoves,             // 50 moves each without a capture or a pawn move
	Repetition,             // the same position three times
	InsufficientMaterial,   // neither side can mate
	Resignation,
};

struct FChessResult
{
	EChessOutcome Outcome = EChessOutcome::InProgress;
	EChessColor Winner = EChessColor::White;    // only for Checkmate and Resignation

	bool IsOver() const { return Outcome != EChessOutcome::InProgress; }
	bool IsDraw() const { return IsOver() && Outcome != EChessOutcome::Checkmate && Outcome != EChessOutcome::Resignation; }
};

// A game: the position, its history for take-backs and the repetition rule, the move list
// in notation, and the result. The rules of chess end every game on their own - by mate,
// stalemate, the fifty-move rule, repetition or bare kings - so a game always ends.
class FChessGame
{
public:
	FChessGame();

	void Reset();
	bool LoadFen(const std::string& Fen);

	const FChessPosition& Position() const { return Positions.back(); }
	const std::vector<FChessMove>& LegalMoves() const { return Legal; }
	FChessResult Result() const { return CurrentResult; }
	EChessColor SideToMove() const { return Position().SideToMove; }

	// Refused (false, nothing changes) if the game is over or the move is not legal.
	bool Play(const FChessMove& Move);
	bool PlayUci(const std::string& Uci);
	bool Undo();
	void Resign(EChessColor Who);

	int PliesPlayed() const { return static_cast<int>(PlayedMoves.size()); }
	const std::vector<FChessMove>& Moves() const { return PlayedMoves; }
	const std::vector<std::string>& SanMoves() const { return San; }
	const FChessMove* LastMove() const { return PlayedMoves.empty() ? nullptr : &PlayedMoves.back(); }

	// "1. e4 e5 2. Nf3 Nc6", the last MaxMoves full moves of it.
	std::string MoveListText(int MaxMoves = 0) const;

	FChessSearchResult Think(const FChessStrength& Strength) const;

private:
	std::vector<FChessPosition> Positions;  // [0] is where the game began; back() is now
	std::vector<uint64_t> Hashes;           // one per position
	std::vector<FChessMove> PlayedMoves;
	std::vector<std::string> San;
	std::vector<FChessMove> Legal;
	FChessResult CurrentResult;
	bool bResigned = false;
	EChessColor Resigner = EChessColor::White;

	void Refresh();
};
