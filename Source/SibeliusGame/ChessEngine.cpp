// ChessEngine.cpp — see the header for the design, and Tools/Tests/chess_engine_test.cpp for
// what it is held to.

#include "ChessEngine.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <sstream>

// NAMED, NOT ANONYMOUS: the unity-build rule (see PinballSim.cpp's PinMath).
namespace ChessDetail
{
	constexpr uint8_t BlackBit = 8;
	constexpr int Infinity = 1000000;
	constexpr int MaxPly = 96;
	constexpr int MateScore = FChessSearchResult::MateScore;

	inline uint8_t Code(EChessPieceType Type, EChessColor Color)
	{
		return static_cast<uint8_t>(static_cast<uint8_t>(Type) | (Color == EChessColor::Black ? BlackBit : 0));
	}
	inline EChessPieceType TypeOf(uint8_t C) { return static_cast<EChessPieceType>(C & 7); }
	inline EChessColor ColorOf(uint8_t C) { return (C & BlackBit) ? EChessColor::Black : EChessColor::White; }
	inline int Index(EChessColor Color) { return Color == EChessColor::White ? 0 : 1; }
	inline int FileOf(int Square) { return Square & 7; }
	inline int RankOf(int Square) { return Square >> 3; }
	inline bool OnBoard(int File, int Rank) { return File >= 0 && File < 8 && Rank >= 0 && Rank < 8; }
	inline int SquareAt(int File, int Rank) { return Rank * 8 + File; }

	struct FStep
	{
		int DF;
		int DR;
	};
	constexpr FStep KnightSteps[8] = { {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2} };
	constexpr FStep KingSteps[8] = { {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
	constexpr FStep RookSteps[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	constexpr FStep BishopSteps[4] = { {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };

	constexpr int PieceValue[7] = { 0, 100, 320, 330, 500, 900, 0 };

	inline FChessMove MakeMove(int From, int To, EChessPieceType Promotion = EChessPieceType::None, uint8_t Flags = 0)
	{
		FChessMove Move;
		Move.From = static_cast<uint8_t>(From);
		Move.To = static_cast<uint8_t>(To);
		Move.Promotion = Promotion;
		Move.Flags = Flags;
		return Move;
	}

	// Moving from or to a king's or rook's home square ends the rights that need it there.
	// Capturing a rook on its corner counts, which is the rule that is easiest to forget.
	inline uint8_t CastleMask(int Square)
	{
		switch (Square)
		{
		case 0:  return static_cast<uint8_t>(~ChessCastle::WhiteQueenSide & 15);
		case 7:  return static_cast<uint8_t>(~ChessCastle::WhiteKingSide & 15);
		case 4:  return static_cast<uint8_t>(~(ChessCastle::WhiteKingSide | ChessCastle::WhiteQueenSide) & 15);
		case 56: return static_cast<uint8_t>(~ChessCastle::BlackQueenSide & 15);
		case 63: return static_cast<uint8_t>(~ChessCastle::BlackKingSide & 15);
		case 60: return static_cast<uint8_t>(~(ChessCastle::BlackKingSide | ChessCastle::BlackQueenSide) & 15);
		default: return 15;
		}
	}

	inline char PieceLetter(EChessPieceType Type)
	{
		switch (Type)
		{
		case EChessPieceType::Pawn:   return 'P';
		case EChessPieceType::Knight: return 'N';
		case EChessPieceType::Bishop: return 'B';
		case EChessPieceType::Rook:   return 'R';
		case EChessPieceType::Queen:  return 'Q';
		case EChessPieceType::King:   return 'K';
		default:                      return '?';
		}
	}

	inline EChessPieceType TypeFromLetter(char Letter)
	{
		switch (Letter)
		{
		case 'p': case 'P': return EChessPieceType::Pawn;
		case 'n': case 'N': return EChessPieceType::Knight;
		case 'b': case 'B': return EChessPieceType::Bishop;
		case 'r': case 'R': return EChessPieceType::Rook;
		case 'q': case 'Q': return EChessPieceType::Queen;
		case 'k': case 'K': return EChessPieceType::King;
		default:            return EChessPieceType::None;
		}
	}

	inline uint64_t SplitMix(uint64_t& State)
	{
		uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	struct FZobrist
	{
		uint64_t Piece[16][64];
		uint64_t BlackToMove;
		uint64_t Castling[16];
		uint64_t EnPassantFile[8];

		FZobrist()
		{
			uint64_t State = 0x5EB1E11A5C4E55ull;
			for (auto& Row : Piece)
			{
				for (uint64_t& Key : Row)
				{
					Key = SplitMix(State);
				}
			}
			BlackToMove = SplitMix(State);
			for (uint64_t& Key : Castling)
			{
				Key = SplitMix(State);
			}
			for (uint64_t& Key : EnPassantFile)
			{
				Key = SplitMix(State);
			}
		}
	};

	// Built once, on first use: a function-local static is thread-safe to initialise.
	const FZobrist& Zobrist()
	{
		static const FZobrist Keys;
		return Keys;
	}

	inline void AddPawnMove(std::vector<FChessMove>& Out, int From, int To, uint8_t Flags, bool bPromotes)
	{
		if (bPromotes)
		{
			Out.push_back(MakeMove(From, To, EChessPieceType::Queen, Flags));
			Out.push_back(MakeMove(From, To, EChessPieceType::Rook, Flags));
			Out.push_back(MakeMove(From, To, EChessPieceType::Bishop, Flags));
			Out.push_back(MakeMove(From, To, EChessPieceType::Knight, Flags));
		}
		else
		{
			Out.push_back(MakeMove(From, To, EChessPieceType::None, Flags));
		}
	}

	// Every move the pieces can make, before asking whether it leaves the king in check.
	// Captures-only still includes quiet promotions: a pawn becoming a queen is as loud as
	// a capture, and the leaf search must not stop in front of one.
	void GeneratePseudo(const FChessPosition& P, std::vector<FChessMove>& Out, bool bCapturesOnly)
	{
		const EChessColor Us = P.SideToMove;
		const EChessColor Them = ChessOpponent(Us);

		auto Leap = [&](int From, const FStep* Steps, int Count)
		{
			const int F = FileOf(From);
			const int R = RankOf(From);
			for (int I = 0; I < Count; ++I)
			{
				const int TF = F + Steps[I].DF;
				const int TR = R + Steps[I].DR;
				if (!OnBoard(TF, TR))
				{
					continue;
				}
				const int To = SquareAt(TF, TR);
				const uint8_t Target = P.Board[To];
				if (Target == 0)
				{
					if (!bCapturesOnly)
					{
						Out.push_back(MakeMove(From, To));
					}
				}
				else if (ColorOf(Target) == Them)
				{
					Out.push_back(MakeMove(From, To, EChessPieceType::None, ChessMoveFlag::Capture));
				}
			}
		};

		auto Slide = [&](int From, const FStep* Steps, int Count)
		{
			for (int I = 0; I < Count; ++I)
			{
				int TF = FileOf(From) + Steps[I].DF;
				int TR = RankOf(From) + Steps[I].DR;
				while (OnBoard(TF, TR))
				{
					const int To = SquareAt(TF, TR);
					const uint8_t Target = P.Board[To];
					if (Target == 0)
					{
						if (!bCapturesOnly)
						{
							Out.push_back(MakeMove(From, To));
						}
					}
					else
					{
						if (ColorOf(Target) == Them)
						{
							Out.push_back(MakeMove(From, To, EChessPieceType::None, ChessMoveFlag::Capture));
						}
						break;
					}
					TF += Steps[I].DF;
					TR += Steps[I].DR;
				}
			}
		};

		for (int From = 0; From < 64; ++From)
		{
			const uint8_t C = P.Board[From];
			if (C == 0 || ColorOf(C) != Us)
			{
				continue;
			}

			switch (TypeOf(C))
			{
			case EChessPieceType::Pawn:
			{
				const int F = FileOf(From);
				const int R = RankOf(From);
				const int Dir = Us == EChessColor::White ? 1 : -1;
				const int StartRank = Us == EChessColor::White ? 1 : 6;
				const int LastRank = Us == EChessColor::White ? 7 : 0;
				const int R1 = R + Dir;         // on the board: FromFen refuses back-rank pawns
				const bool bPromotes = R1 == LastRank;

				const int One = SquareAt(F, R1);
				if (P.Board[One] == 0)
				{
					if (!bCapturesOnly || bPromotes)
					{
						AddPawnMove(Out, From, One, 0, bPromotes);
					}
					if (!bCapturesOnly && R == StartRank)
					{
						const int Two = SquareAt(F, R + 2 * Dir);
						if (P.Board[Two] == 0)
						{
							Out.push_back(MakeMove(From, Two, EChessPieceType::None, ChessMoveFlag::DoublePush));
						}
					}
				}

				for (int DF = -1; DF <= 1; DF += 2)
				{
					const int TF = F + DF;
					if (TF < 0 || TF > 7)
					{
						continue;
					}
					const int To = SquareAt(TF, R1);
					const uint8_t Target = P.Board[To];
					if (Target != 0 && ColorOf(Target) == Them)
					{
						AddPawnMove(Out, From, To, ChessMoveFlag::Capture, bPromotes);
					}
					else if (Target == 0 && To == P.EnPassant)
					{
						Out.push_back(MakeMove(From, To, EChessPieceType::None,
							static_cast<uint8_t>(ChessMoveFlag::Capture | ChessMoveFlag::EnPassant)));
					}
				}
				break;
			}
			case EChessPieceType::Knight:
				Leap(From, KnightSteps, 8);
				break;
			case EChessPieceType::Bishop:
				Slide(From, BishopSteps, 4);
				break;
			case EChessPieceType::Rook:
				Slide(From, RookSteps, 4);
				break;
			case EChessPieceType::Queen:
				Slide(From, RookSteps, 4);
				Slide(From, BishopSteps, 4);
				break;
			case EChessPieceType::King:
			{
				Leap(From, KingSteps, 8);
				if (bCapturesOnly)
				{
					break;
				}
				// Castling: the squares between empty, the rook at home, and the king not in
				// check, not passing through check, and not landing in it.
				const bool bWhite = Us == EChessColor::White;
				const int Home = bWhite ? 4 : 60;
				if (From != Home)
				{
					break;
				}
				const uint8_t KingSide = bWhite ? ChessCastle::WhiteKingSide : ChessCastle::BlackKingSide;
				const uint8_t QueenSide = bWhite ? ChessCastle::WhiteQueenSide : ChessCastle::BlackQueenSide;
				const uint8_t OurRook = Code(EChessPieceType::Rook, Us);
				if ((P.Castling & KingSide) && P.Board[Home + 1] == 0 && P.Board[Home + 2] == 0 && P.Board[Home + 3] == OurRook
					&& !P.IsAttacked(Home, Them) && !P.IsAttacked(Home + 1, Them) && !P.IsAttacked(Home + 2, Them))
				{
					Out.push_back(MakeMove(Home, Home + 2, EChessPieceType::None, ChessMoveFlag::Castle));
				}
				if ((P.Castling & QueenSide) && P.Board[Home - 1] == 0 && P.Board[Home - 2] == 0 && P.Board[Home - 3] == 0
					&& P.Board[Home - 4] == OurRook
					&& !P.IsAttacked(Home, Them) && !P.IsAttacked(Home - 1, Them) && !P.IsAttacked(Home - 2, Them))
				{
					Out.push_back(MakeMove(Home, Home - 2, EChessPieceType::None, ChessMoveFlag::Castle));
				}
				break;
			}
			default:
				break;
			}
		}
	}

	// Can the side to move capture en passant at all? Only then does the square count for
	// repetition: two positions that differ only by an en passant nobody can take are the same.
	bool EnPassantMatters(const FChessPosition& P)
	{
		if (P.EnPassant < 0)
		{
			return false;
		}
		const int F = FileOf(P.EnPassant);
		const int FromRank = P.SideToMove == EChessColor::White ? 4 : 3;
		const uint8_t OurPawn = Code(EChessPieceType::Pawn, P.SideToMove);
		return (F > 0 && P.Board[SquareAt(F - 1, FromRank)] == OurPawn)
			|| (F < 7 && P.Board[SquareAt(F + 1, FromRank)] == OurPawn);
	}

	// ---------------------------------------------------------------------------------
	// EVALUATION. Piece-square tables written as the board looks from White's side, rank 8
	// at the top: a white piece on square S reads entry S ^ 56, a black one reads entry S,
	// which is the same table seen from the other side of the board.

	constexpr int PawnTable[64] = {
		 0,  0,  0,  0,  0,  0,  0,  0,
		50, 50, 50, 50, 50, 50, 50, 50,
		10, 10, 20, 30, 30, 20, 10, 10,
		 5,  5, 10, 25, 25, 10,  5,  5,
		 0,  0,  0, 20, 20,  0,  0,  0,
		 5, -5,-10,  0,  0,-10, -5,  5,
		 5, 10, 10,-20,-20, 10, 10,  5,
		 0,  0,  0,  0,  0,  0,  0,  0,
	};
	constexpr int KnightTable[64] = {
		-50,-40,-30,-30,-30,-30,-40,-50,
		-40,-20,  0,  0,  0,  0,-20,-40,
		-30,  0, 10, 15, 15, 10,  0,-30,
		-30,  5, 15, 20, 20, 15,  5,-30,
		-30,  0, 15, 20, 20, 15,  0,-30,
		-30,  5, 10, 15, 15, 10,  5,-30,
		-40,-20,  0,  5,  5,  0,-20,-40,
		-50,-40,-30,-30,-30,-30,-40,-50,
	};
	constexpr int BishopTable[64] = {
		-20,-10,-10,-10,-10,-10,-10,-20,
		-10,  0,  0,  0,  0,  0,  0,-10,
		-10,  0,  5, 10, 10,  5,  0,-10,
		-10,  5,  5, 10, 10,  5,  5,-10,
		-10,  0, 10, 10, 10, 10,  0,-10,
		-10, 10, 10, 10, 10, 10, 10,-10,
		-10,  5,  0,  0,  0,  0,  5,-10,
		-20,-10,-10,-10,-10,-10,-10,-20,
	};
	constexpr int RookTable[64] = {
		  0,  0,  0,  0,  0,  0,  0,  0,
		  5, 10, 10, 10, 10, 10, 10,  5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		  0,  0,  0,  5,  5,  0,  0,  0,
	};
	constexpr int QueenTable[64] = {
		-20,-10,-10, -5, -5,-10,-10,-20,
		-10,  0,  0,  0,  0,  0,  0,-10,
		-10,  0,  5,  5,  5,  5,  0,-10,
		 -5,  0,  5,  5,  5,  5,  0, -5,
		  0,  0,  5,  5,  5,  5,  0, -5,
		-10,  5,  5,  5,  5,  5,  0,-10,
		-10,  0,  5,  0,  0,  0,  0,-10,
		-20,-10,-10, -5, -5,-10,-10,-20,
	};
	// The middlegame king stays behind its pawns; the endgame king is a fighting piece.
	constexpr int KingMiddleTable[64] = {
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-20,-30,-30,-40,-40,-30,-30,-20,
		-10,-20,-20,-20,-20,-20,-20,-10,
		 20, 20,  0,  0,  0,  0, 20, 20,
		 20, 30, 10,  0,  0, 10, 30, 20,
	};
	constexpr int KingEndTable[64] = {
		-50,-40,-30,-20,-20,-30,-40,-50,
		-30,-20,-10,  0,  0,-10,-20,-30,
		-30,-10, 20, 30, 30, 20,-10,-30,
		-30,-10, 30, 40, 40, 30,-10,-30,
		-30,-10, 30, 40, 40, 30,-10,-30,
		-30,-10, 20, 30, 30, 20,-10,-30,
		-30,-30,  0,  0,  0,  0,-30,-30,
		-50,-30,-30,-30,-30,-30,-30,-50,
	};

	inline int CentreDistance(int Square)
	{
		const int F = FileOf(Square);
		const int R = RankOf(Square);
		return std::max(3 - F, F - 4) + std::max(3 - R, R - 4);
	}

	// ---------------------------------------------------------------------------------
	// THE SEARCH.

	struct FSearcher
	{
		const FChessStrength& Strength;
		int64_t Nodes = 0;
		bool bStopped = false;
		bool bMayStop = false;                  // the first depth always finishes
		std::vector<uint64_t> Path;             // game history, then this line
		std::vector<FChessMove> PreviousLine;   // last depth's best line, searched first
		std::vector<FChessMove> Buffers[MaxPly];
		FChessMove Killers[MaxPly][2];
		FChessMove Pv[MaxPly][MaxPly];
		int PvLength[MaxPly] = {};

		explicit FSearcher(const FChessStrength& InStrength) : Strength(InStrength) {}

		bool OutOfNodes()
		{
			if (bMayStop && Nodes >= Strength.NodeLimit)
			{
				bStopped = true;
			}
			return bStopped;
		}

		// A position met before in the stretch since the last capture or pawn move is a draw
		// the moment it recurs: the side that can avoid it will, and the other is glad of it.
		bool IsRepetition(const FChessPosition& P) const
		{
			const int Count = static_cast<int>(Path.size());
			const uint64_t Here = Path.back();
			const int Limit = std::min(P.HalfmoveClock, Count - 1);
			for (int Back = 2; Back <= Limit; Back += 2)
			{
				if (Path[static_cast<size_t>(Count - 1 - Back)] == Here)
				{
					return true;
				}
			}
			return false;
		}

		int OrderScore(const FChessPosition& P, const FChessMove& Move, int Ply, const FChessMove& First) const
		{
			if (!First.IsNull() && Move.SameAs(First))
			{
				return 1 << 30;
			}
			int Score = 0;
			if (Move.IsCapture())
			{
				// Most valuable victim first, taken by the least valuable attacker.
				const EChessPieceType Victim = (Move.Flags & ChessMoveFlag::EnPassant)
					? EChessPieceType::Pawn : TypeOf(P.Board[Move.To]);
				Score = 100000 + PieceValue[static_cast<int>(Victim)] * 10
					- PieceValue[static_cast<int>(TypeOf(P.Board[Move.From]))];
			}
			if (Move.Promotion != EChessPieceType::None)
			{
				Score += 90000 + PieceValue[static_cast<int>(Move.Promotion)];
			}
			if (Score == 0)
			{
				if (Move.SameAs(Killers[Ply][0]))
				{
					Score = 80000;
				}
				else if (Move.SameAs(Killers[Ply][1]))
				{
					Score = 70000;
				}
			}
			return Score;
		}

		void Order(const FChessPosition& P, std::vector<FChessMove>& Moves, int Ply, const FChessMove& First) const
		{
			struct FScored
			{
				int Score;
				FChessMove Move;
			};
			FScored Scored[256];
			const size_t Count = std::min<size_t>(Moves.size(), 256);
			for (size_t I = 0; I < Count; ++I)
			{
				Scored[I] = FScored{ OrderScore(P, Moves[I], Ply, First), Moves[I] };
			}
			std::stable_sort(Scored, Scored + Count, [](const FScored& A, const FScored& B) { return A.Score > B.Score; });
			for (size_t I = 0; I < Count; ++I)
			{
				Moves[I] = Scored[I].Move;
			}
		}

		void CopyLine(int Ply, const FChessMove& Move)
		{
			Pv[Ply][Ply] = Move;
			for (int J = Ply + 1; J < PvLength[Ply + 1]; ++J)
			{
				Pv[Ply][J] = Pv[Ply + 1][J];
			}
			PvLength[Ply] = std::max(PvLength[Ply + 1], Ply + 1);
		}

		// Captures (and promotions) only, until the position is quiet enough to believe the
		// evaluation. Standing pat is the right to decline every capture on offer.
		int Quiesce(const FChessPosition& P, int Alpha, int Beta, int Ply)
		{
			PvLength[Ply] = Ply;
			if (OutOfNodes())
			{
				return 0;
			}
			++Nodes;
			if (Ply >= MaxPly - 1)
			{
				return P.Evaluate();
			}

			std::vector<FChessMove>& Moves = Buffers[Ply];
			int Best = -Infinity;
			if (P.InCheck())
			{
				// In check there is no standing pat: every evasion, or it is mate.
				P.LegalMoves(Moves);
				if (Moves.empty())
				{
					return -(MateScore - Ply);
				}
			}
			else
			{
				const int StandPat = P.Evaluate();
				if (StandPat >= Beta)
				{
					return StandPat;
				}
				Alpha = std::max(Alpha, StandPat);
				Best = StandPat;
				P.LegalMoves(Moves, true);
			}

			Order(P, Moves, Ply, FChessMove());
			for (size_t I = 0; I < Moves.size(); ++I)
			{
				const FChessMove Move = Moves[I];
				const int Score = -Quiesce(P.Played(Move), -Beta, -Alpha, Ply + 1);
				if (bStopped)
				{
					return 0;
				}
				if (Score > Best)
				{
					Best = Score;
					if (Score > Alpha)
					{
						Alpha = Score;
						CopyLine(Ply, Move);
						if (Alpha >= Beta)
						{
							break;
						}
					}
				}
			}
			return Best;
		}

		int Negamax(const FChessPosition& P, int Depth, int Alpha, int Beta, int Ply, bool bOnPreviousLine)
		{
			PvLength[Ply] = Ply;
			if (OutOfNodes())
			{
				return 0;
			}
			if (Ply > 0 && (P.HalfmoveClock >= 100 || IsRepetition(P) || P.InsufficientMaterial()))
			{
				return 0;
			}
			if (Ply >= MaxPly - 1)
			{
				return P.Evaluate();
			}

			const bool bInCheck = P.InCheck();
			if (bInCheck && Ply < MaxPly / 2)
			{
				++Depth;    // never stop searching while the king is in check
			}
			if (Depth <= 0)
			{
				return Quiesce(P, Alpha, Beta, Ply);
			}
			++Nodes;

			std::vector<FChessMove>& Moves = Buffers[Ply];
			P.LegalMoves(Moves);
			if (Moves.empty())
			{
				return bInCheck ? -(MateScore - Ply) : 0;
			}

			const FChessMove Hint = (bOnPreviousLine && static_cast<size_t>(Ply) < PreviousLine.size())
				? PreviousLine[static_cast<size_t>(Ply)] : FChessMove();
			Order(P, Moves, Ply, Hint);

			int Best = -Infinity;
			for (size_t I = 0; I < Moves.size(); ++I)
			{
				const FChessMove Move = Moves[I];
				const FChessPosition Child = P.Played(Move);
				Path.push_back(Child.Hash());
				const int Score = -Negamax(Child, Depth - 1, -Beta, -Alpha, Ply + 1,
					bOnPreviousLine && !Hint.IsNull() && Move.SameAs(Hint));
				Path.pop_back();
				if (bStopped)
				{
					return 0;
				}
				if (Score > Best)
				{
					Best = Score;
					if (Score > Alpha)
					{
						Alpha = Score;
						CopyLine(Ply, Move);
						if (Alpha >= Beta)
						{
							if (!Move.IsCapture() && Move.Promotion == EChessPieceType::None && !Move.SameAs(Killers[Ply][0]))
							{
								Killers[Ply][1] = Killers[Ply][0];
								Killers[Ply][0] = Move;
							}
							break;
						}
					}
				}
			}
			return Best;
		}
	};
}

// =====================================================================================
// MOVES AND SQUARES

std::string ChessSquareName(int Square)
{
	if (Square < 0 || Square > 63)
	{
		return "-";
	}
	std::string Name;
	Name += static_cast<char>('a' + ChessDetail::FileOf(Square));
	Name += static_cast<char>('1' + ChessDetail::RankOf(Square));
	return Name;
}

int ChessParseSquare(const std::string& Name)
{
	if (Name.size() != 2 || Name[0] < 'a' || Name[0] > 'h' || Name[1] < '1' || Name[1] > '8')
	{
		return -1;
	}
	return ChessDetail::SquareAt(Name[0] - 'a', Name[1] - '1');
}

std::string FChessMove::ToUci() const
{
	std::string Text = ChessSquareName(From) + ChessSquareName(To);
	if (Promotion != EChessPieceType::None)
	{
		Text += static_cast<char>(ChessDetail::PieceLetter(Promotion) - 'A' + 'a');
	}
	return Text;
}

FChessStrength FChessStrength::Beginner()
{
	FChessStrength S;
	S.MaxDepth = 2;
	S.NodeLimit = 30000;
	S.NoiseCentipawns = 120;
	return S;
}

FChessStrength FChessStrength::Hustler()
{
	FChessStrength S;
	S.MaxDepth = 3;
	S.NodeLimit = 150000;
	S.NoiseCentipawns = 35;
	return S;
}

FChessStrength FChessStrength::Master()
{
	FChessStrength S;
	S.MaxDepth = 5;
	S.NodeLimit = 2000000;
	S.NoiseCentipawns = 0;
	return S;
}

// =====================================================================================
// POSITIONS

FChessPosition FChessPosition::Start()
{
	FChessPosition P;
	FromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", P);
	return P;
}

bool FChessPosition::FromFen(const std::string& Fen, FChessPosition& Out)
{
	std::istringstream Stream(Fen);
	std::string Fields[6];
	int FieldCount = 0;
	while (FieldCount < 6 && Stream >> Fields[FieldCount])
	{
		++FieldCount;
	}
	if (FieldCount < 4)
	{
		return false;
	}

	FChessPosition P;
	int Rank = 7;
	int File = 0;
	for (const char Ch : Fields[0])
	{
		if (Ch == '/')
		{
			if (File != 8 || Rank == 0)
			{
				return false;
			}
			--Rank;
			File = 0;
		}
		else if (Ch >= '1' && Ch <= '8')
		{
			File += Ch - '0';
			if (File > 8)
			{
				return false;
			}
		}
		else
		{
			const EChessPieceType Type = ChessDetail::TypeFromLetter(Ch);
			if (Type == EChessPieceType::None || File >= 8)
			{
				return false;
			}
			const EChessColor Color = (Ch >= 'a' && Ch <= 'z') ? EChessColor::Black : EChessColor::White;
			P.Board[ChessDetail::SquareAt(File, Rank)] = ChessDetail::Code(Type, Color);
			++File;
		}
	}
	if (Rank != 0 || File != 8)
	{
		return false;
	}

	if (Fields[1] == "w")
	{
		P.SideToMove = EChessColor::White;
	}
	else if (Fields[1] == "b")
	{
		P.SideToMove = EChessColor::Black;
	}
	else
	{
		return false;
	}

	if (Fields[2] != "-")
	{
		for (const char Ch : Fields[2])
		{
			switch (Ch)
			{
			case 'K': P.Castling |= ChessCastle::WhiteKingSide; break;
			case 'Q': P.Castling |= ChessCastle::WhiteQueenSide; break;
			case 'k': P.Castling |= ChessCastle::BlackKingSide; break;
			case 'q': P.Castling |= ChessCastle::BlackQueenSide; break;
			default: return false;
			}
		}
	}

	if (Fields[3] != "-")
	{
		const int Square = ChessParseSquare(Fields[3]);
		if (Square < 0)
		{
			return false;
		}
		P.EnPassant = static_cast<int8_t>(Square);
	}

	if (FieldCount >= 5)
	{
		char* End = nullptr;
		const long Value = std::strtol(Fields[4].c_str(), &End, 10);
		if (*End != '\0' || Value < 0)
		{
			return false;
		}
		P.HalfmoveClock = static_cast<int>(Value);
	}
	if (FieldCount >= 6)
	{
		char* End = nullptr;
		const long Value = std::strtol(Fields[5].c_str(), &End, 10);
		if (*End != '\0' || Value < 1)
		{
			return false;
		}
		P.FullmoveNumber = static_cast<int>(Value);
	}

	// Exactly one king each, and no pawn on a back rank.
	int Kings[2] = { 0, 0 };
	for (int Square = 0; Square < 64; ++Square)
	{
		const uint8_t C = P.Board[Square];
		if (ChessDetail::TypeOf(C) == EChessPieceType::King)
		{
			++Kings[ChessDetail::Index(ChessDetail::ColorOf(C))];
			P.KingSquare[ChessDetail::Index(ChessDetail::ColorOf(C))] = static_cast<uint8_t>(Square);
		}
		if (ChessDetail::TypeOf(C) == EChessPieceType::Pawn && (ChessDetail::RankOf(Square) == 0 || ChessDetail::RankOf(Square) == 7))
		{
			return false;
		}
	}
	if (Kings[0] != 1 || Kings[1] != 1)
	{
		return false;
	}

	// A right that needs a king or rook not on its home square is dropped, not trusted.
	const uint8_t WK = ChessDetail::Code(EChessPieceType::King, EChessColor::White);
	const uint8_t WR = ChessDetail::Code(EChessPieceType::Rook, EChessColor::White);
	const uint8_t BK = ChessDetail::Code(EChessPieceType::King, EChessColor::Black);
	const uint8_t BR = ChessDetail::Code(EChessPieceType::Rook, EChessColor::Black);
	if (P.Board[4] != WK || P.Board[7] != WR) P.Castling &= static_cast<uint8_t>(~ChessCastle::WhiteKingSide);
	if (P.Board[4] != WK || P.Board[0] != WR) P.Castling &= static_cast<uint8_t>(~ChessCastle::WhiteQueenSide);
	if (P.Board[60] != BK || P.Board[63] != BR) P.Castling &= static_cast<uint8_t>(~ChessCastle::BlackKingSide);
	if (P.Board[60] != BK || P.Board[56] != BR) P.Castling &= static_cast<uint8_t>(~ChessCastle::BlackQueenSide);

	// Likewise an en passant square no pawn can just have skipped.
	if (P.EnPassant >= 0)
	{
		const bool bWhiteToMove = P.SideToMove == EChessColor::White;
		const int Skipped = P.EnPassant;
		const int Landed = Skipped + (bWhiteToMove ? -8 : 8);
		const int Origin = Skipped + (bWhiteToMove ? 8 : -8);
		const bool bRankOk = ChessDetail::RankOf(Skipped) == (bWhiteToMove ? 5 : 2);
		if (!bRankOk || P.Board[Skipped] != 0 || P.Board[Origin] != 0
			|| P.Board[Landed] != ChessDetail::Code(EChessPieceType::Pawn, ChessOpponent(P.SideToMove)))
		{
			P.EnPassant = -1;
		}
	}

	// The side that just moved cannot have left its own king in check.
	const EChessColor Moved = ChessOpponent(P.SideToMove);
	if (P.IsAttacked(P.KingSquare[ChessDetail::Index(Moved)], P.SideToMove))
	{
		return false;
	}

	Out = P;
	return true;
}

std::string FChessPosition::ToFen() const
{
	std::string Fen;
	for (int Rank = 7; Rank >= 0; --Rank)
	{
		int Empty = 0;
		for (int File = 0; File < 8; ++File)
		{
			const uint8_t C = Board[ChessDetail::SquareAt(File, Rank)];
			if (C == 0)
			{
				++Empty;
				continue;
			}
			if (Empty > 0)
			{
				Fen += static_cast<char>('0' + Empty);
				Empty = 0;
			}
			const char Letter = ChessDetail::PieceLetter(ChessDetail::TypeOf(C));
			Fen += ChessDetail::ColorOf(C) == EChessColor::White ? Letter : static_cast<char>(Letter - 'A' + 'a');
		}
		if (Empty > 0)
		{
			Fen += static_cast<char>('0' + Empty);
		}
		if (Rank > 0)
		{
			Fen += '/';
		}
	}
	Fen += SideToMove == EChessColor::White ? " w " : " b ";
	if (Castling == 0)
	{
		Fen += '-';
	}
	else
	{
		if (Castling & ChessCastle::WhiteKingSide) Fen += 'K';
		if (Castling & ChessCastle::WhiteQueenSide) Fen += 'Q';
		if (Castling & ChessCastle::BlackKingSide) Fen += 'k';
		if (Castling & ChessCastle::BlackQueenSide) Fen += 'q';
	}
	Fen += ' ';
	Fen += EnPassant >= 0 ? ChessSquareName(EnPassant) : std::string("-");
	Fen += ' ' + std::to_string(HalfmoveClock) + ' ' + std::to_string(FullmoveNumber);
	return Fen;
}

FChessPiece FChessPosition::PieceAt(int Square) const
{
	FChessPiece Piece;
	if (Square >= 0 && Square < 64 && Board[Square] != 0)
	{
		Piece.Type = ChessDetail::TypeOf(Board[Square]);
		Piece.Color = ChessDetail::ColorOf(Board[Square]);
	}
	return Piece;
}

bool FChessPosition::IsAttacked(int Square, EChessColor By) const
{
	const int F = ChessDetail::FileOf(Square);
	const int R = ChessDetail::RankOf(Square);

	// A pawn attacks forward diagonally, so an attacker sits one rank behind the square.
	const int PawnRank = R + (By == EChessColor::White ? -1 : 1);
	if (PawnRank >= 0 && PawnRank < 8)
	{
		const uint8_t TheirPawn = ChessDetail::Code(EChessPieceType::Pawn, By);
		if ((F > 0 && Board[ChessDetail::SquareAt(F - 1, PawnRank)] == TheirPawn) || (F < 7 && Board[ChessDetail::SquareAt(F + 1, PawnRank)] == TheirPawn))
		{
			return true;
		}
	}

	const uint8_t TheirKnight = ChessDetail::Code(EChessPieceType::Knight, By);
	for (const ChessDetail::FStep& Step : ChessDetail::KnightSteps)
	{
		if (ChessDetail::OnBoard(F + Step.DF, R + Step.DR) && Board[ChessDetail::SquareAt(F + Step.DF, R + Step.DR)] == TheirKnight)
		{
			return true;
		}
	}
	const uint8_t TheirKing = ChessDetail::Code(EChessPieceType::King, By);
	for (const ChessDetail::FStep& Step : ChessDetail::KingSteps)
	{
		if (ChessDetail::OnBoard(F + Step.DF, R + Step.DR) && Board[ChessDetail::SquareAt(F + Step.DF, R + Step.DR)] == TheirKing)
		{
			return true;
		}
	}

	const uint8_t TheirQueen = ChessDetail::Code(EChessPieceType::Queen, By);
	auto RayHits = [&](const ChessDetail::FStep* Steps, uint8_t Slider)
	{
		for (int I = 0; I < 4; ++I)
		{
			int TF = F + Steps[I].DF;
			int TR = R + Steps[I].DR;
			while (ChessDetail::OnBoard(TF, TR))
			{
				const uint8_t C = Board[ChessDetail::SquareAt(TF, TR)];
				if (C != 0)
				{
					if (C == Slider || C == TheirQueen)
					{
						return true;
					}
					break;
				}
				TF += Steps[I].DF;
				TR += Steps[I].DR;
			}
		}
		return false;
	};
	return RayHits(ChessDetail::RookSteps, ChessDetail::Code(EChessPieceType::Rook, By))
		|| RayHits(ChessDetail::BishopSteps, ChessDetail::Code(EChessPieceType::Bishop, By));
}

bool FChessPosition::InCheck() const
{
	return IsAttacked(KingSquare[ChessDetail::Index(SideToMove)], ChessOpponent(SideToMove));
}

void FChessPosition::LegalMoves(std::vector<FChessMove>& Out, bool bCapturesOnly) const
{
	Out.clear();
	ChessDetail::GeneratePseudo(*this, Out, bCapturesOnly);

	// Legal = does not leave our own king attacked. Playing the move and asking is slower
	// than pin detection and cannot be wrong, which is the trade this file makes everywhere.
	const EChessColor Us = SideToMove;
	const EChessColor Them = ChessOpponent(Us);
	size_t Kept = 0;
	for (size_t I = 0; I < Out.size(); ++I)
	{
		const FChessPosition Next = Played(Out[I]);
		if (!Next.IsAttacked(Next.KingSquare[ChessDetail::Index(Us)], Them))
		{
			Out[Kept++] = Out[I];
		}
	}
	Out.resize(Kept);
}

std::vector<FChessMove> FChessPosition::LegalMoves() const
{
	std::vector<FChessMove> Moves;
	LegalMoves(Moves);
	return Moves;
}

FChessPosition FChessPosition::Played(const FChessMove& Move) const
{
	FChessPosition N = *this;
	const uint8_t C = Board[Move.From];
	const EChessColor Us = SideToMove;
	const EChessPieceType Type = ChessDetail::TypeOf(C);

	N.Board[Move.To] = Move.Promotion != EChessPieceType::None ? ChessDetail::Code(Move.Promotion, Us) : C;
	N.Board[Move.From] = 0;

	if (Move.Flags & ChessMoveFlag::EnPassant)
	{
		N.Board[Move.To + (Us == EChessColor::White ? -8 : 8)] = 0;
	}
	if (Move.Flags & ChessMoveFlag::Castle)
	{
		// The rook jumps to the square the king crossed.
		const int RookFrom = Move.To > Move.From ? Move.From + 3 : Move.From - 4;
		const int RookTo = Move.To > Move.From ? Move.From + 1 : Move.From - 1;
		N.Board[RookTo] = N.Board[RookFrom];
		N.Board[RookFrom] = 0;
	}
	if (Type == EChessPieceType::King)
	{
		N.KingSquare[ChessDetail::Index(Us)] = Move.To;
	}

	N.Castling = static_cast<uint8_t>(N.Castling & ChessDetail::CastleMask(Move.From) & ChessDetail::CastleMask(Move.To));
	N.EnPassant = (Move.Flags & ChessMoveFlag::DoublePush) ? static_cast<int8_t>((Move.From + Move.To) / 2) : static_cast<int8_t>(-1);
	N.HalfmoveClock = (Type == EChessPieceType::Pawn || Board[Move.To] != 0) ? 0 : HalfmoveClock + 1;
	if (Us == EChessColor::Black)
	{
		++N.FullmoveNumber;
	}
	N.SideToMove = ChessOpponent(Us);
	return N;
}

bool FChessPosition::FindMove(int From, int To, EChessPieceType Promotion, FChessMove& Out) const
{
	const FChessMove Wanted = ChessDetail::MakeMove(From, To, Promotion);
	for (const FChessMove& Move : LegalMoves())
	{
		if (Move.SameAs(Wanted))
		{
			Out = Move;
			return true;
		}
	}
	return false;
}

bool FChessPosition::FindMove(const std::string& Uci, FChessMove& Out) const
{
	if (Uci.size() != 4 && Uci.size() != 5)
	{
		return false;
	}
	const int From = ChessParseSquare(Uci.substr(0, 2));
	const int To = ChessParseSquare(Uci.substr(2, 2));
	if (From < 0 || To < 0)
	{
		return false;
	}
	EChessPieceType Promotion = EChessPieceType::None;
	if (Uci.size() == 5)
	{
		Promotion = ChessDetail::TypeFromLetter(Uci[4]);
		if (Promotion == EChessPieceType::None || Promotion == EChessPieceType::Pawn || Promotion == EChessPieceType::King)
		{
			return false;
		}
	}
	return FindMove(From, To, Promotion, Out);
}

bool FChessPosition::InsufficientMaterial() const
{
	int Minors = 0;
	int Knights = 0;
	int BishopSquareColours[2] = { 0, 0 };
	for (int Square = 0; Square < 64; ++Square)
	{
		switch (ChessDetail::TypeOf(Board[Square]))
		{
		case EChessPieceType::Pawn:
		case EChessPieceType::Rook:
		case EChessPieceType::Queen:
			return false;
		case EChessPieceType::Knight:
			++Minors;
			++Knights;
			break;
		case EChessPieceType::Bishop:
			++Minors;
			++BishopSquareColours[(ChessDetail::FileOf(Square) + ChessDetail::RankOf(Square)) & 1];
			break;
		default:
			break;
		}
	}
	// Bare kings, one minor piece, or bishops that all stand on one colour of square.
	return Minors <= 1 || (Knights == 0 && (BishopSquareColours[0] == 0 || BishopSquareColours[1] == 0));
}

std::string FChessPosition::ToSan(const FChessMove& Move) const
{
	std::string San;
	const EChessPieceType Type = ChessDetail::TypeOf(Board[Move.From]);
	if (Move.Flags & ChessMoveFlag::Castle)
	{
		San = Move.To > Move.From ? "O-O" : "O-O-O";
	}
	else if (Type == EChessPieceType::Pawn)
	{
		if (Move.IsCapture())
		{
			San += static_cast<char>('a' + ChessDetail::FileOf(Move.From));
			San += 'x';
		}
		San += ChessSquareName(Move.To);
		if (Move.Promotion != EChessPieceType::None)
		{
			San += '=';
			San += ChessDetail::PieceLetter(Move.Promotion);
		}
	}
	else
	{
		San += ChessDetail::PieceLetter(Type);
		// Name the file if that tells the pieces apart, else the rank, else both.
		bool bAmbiguous = false;
		bool bSameFile = false;
		bool bSameRank = false;
		for (const FChessMove& Other : LegalMoves())
		{
			if (Other.To == Move.To && Other.From != Move.From && ChessDetail::TypeOf(Board[Other.From]) == Type)
			{
				bAmbiguous = true;
				bSameFile = bSameFile || ChessDetail::FileOf(Other.From) == ChessDetail::FileOf(Move.From);
				bSameRank = bSameRank || ChessDetail::RankOf(Other.From) == ChessDetail::RankOf(Move.From);
			}
		}
		if (bAmbiguous)
		{
			if (!bSameFile)
			{
				San += static_cast<char>('a' + ChessDetail::FileOf(Move.From));
			}
			else if (!bSameRank)
			{
				San += static_cast<char>('1' + ChessDetail::RankOf(Move.From));
			}
			else
			{
				San += ChessSquareName(Move.From);
			}
		}
		if (Move.IsCapture())
		{
			San += 'x';
		}
		San += ChessSquareName(Move.To);
	}

	const FChessPosition After = Played(Move);
	if (After.InCheck())
	{
		San += After.LegalMoves().empty() ? '#' : '+';
	}
	return San;
}

uint64_t FChessPosition::Hash() const
{
	const ChessDetail::FZobrist& Keys = ChessDetail::Zobrist();
	uint64_t H = 0;
	for (int Square = 0; Square < 64; ++Square)
	{
		if (Board[Square] != 0)
		{
			H ^= Keys.Piece[Board[Square]][Square];
		}
	}
	if (SideToMove == EChessColor::Black)
	{
		H ^= Keys.BlackToMove;
	}
	H ^= Keys.Castling[Castling & 15];
	if (ChessDetail::EnPassantMatters(*this))
	{
		H ^= Keys.EnPassantFile[ChessDetail::FileOf(EnPassant)];
	}
	return H;
}

int FChessPosition::Evaluate() const
{
	int Score = 0;          // White's point of view until the last line
	int Phase = 0;          // 24 with every piece on the board, 0 with none
	int Material[2] = { 0, 0 };
	int Pawns[2] = { 0, 0 };
	int Bishops[2] = { 0, 0 };
	int KingMiddle[2] = { 0, 0 };
	int KingEnd[2] = { 0, 0 };

	for (int Square = 0; Square < 64; ++Square)
	{
		const uint8_t C = Board[Square];
		if (C == 0)
		{
			continue;
		}
		const EChessColor Color = ChessDetail::ColorOf(C);
		const int Side = ChessDetail::Index(Color);
		const int Entry = Color == EChessColor::White ? (Square ^ 56) : Square;
		const int Sign = Color == EChessColor::White ? 1 : -1;
		int Placement = 0;
		switch (ChessDetail::TypeOf(C))
		{
		case EChessPieceType::Pawn:   Placement = ChessDetail::PawnTable[Entry]; ++Pawns[Side]; break;
		case EChessPieceType::Knight: Placement = ChessDetail::KnightTable[Entry]; Phase += 1; break;
		case EChessPieceType::Bishop: Placement = ChessDetail::BishopTable[Entry]; Phase += 1; ++Bishops[Side]; break;
		case EChessPieceType::Rook:   Placement = ChessDetail::RookTable[Entry]; Phase += 2; break;
		case EChessPieceType::Queen:  Placement = ChessDetail::QueenTable[Entry]; Phase += 4; break;
		case EChessPieceType::King:
			KingMiddle[Side] = ChessDetail::KingMiddleTable[Entry];
			KingEnd[Side] = ChessDetail::KingEndTable[Entry];
			break;
		default:
			break;
		}
		Material[Side] += ChessDetail::PieceValue[static_cast<int>(ChessDetail::TypeOf(C))];
		Score += Sign * (ChessDetail::PieceValue[static_cast<int>(ChessDetail::TypeOf(C))] + Placement);
	}

	Phase = std::min(Phase, 24);
	Score += ((KingMiddle[0] - KingMiddle[1]) * Phase + (KingEnd[0] - KingEnd[1]) * (24 - Phase)) / 24;
	Score += (Bishops[0] >= 2 ? 30 : 0) - (Bishops[1] >= 2 ? 30 : 0);

	// MOP-UP. Up a rook or more against a side with no pawns, the winner's job is to push
	// the lone king to the edge and walk its own king up: without this, a search a few plies
	// deep shuffles a won ending until the fifty-move rule takes it away.
	const int Lead = Material[0] - Material[1];
	if (Phase <= 12 && (Lead >= 400 || Lead <= -400))
	{
		const int Winner = Lead > 0 ? 0 : 1;
		const int Loser = 1 - Winner;
		if (Pawns[Loser] == 0)
		{
			const int WinnerKing = KingSquare[Winner];
			const int LoserKing = KingSquare[Loser];
			const int KingGap = std::abs(ChessDetail::FileOf(WinnerKing) - ChessDetail::FileOf(LoserKing)) + std::abs(ChessDetail::RankOf(WinnerKing) - ChessDetail::RankOf(LoserKing));
			const int MopUp = 10 * ChessDetail::CentreDistance(LoserKing) + 4 * (14 - KingGap);
			Score += Winner == 0 ? MopUp : -MopUp;
		}
	}

	return SideToMove == EChessColor::White ? Score : -Score;
}

FChessSearchResult FChessPosition::Search(const FChessStrength& Strength, const std::vector<uint64_t>& History) const
{
	FChessSearchResult Result;
	std::vector<FChessMove> RootMoves = LegalMoves();
	if (RootMoves.empty())
	{
		Result.Score = InCheck() ? -ChessDetail::MateScore : 0;
		return Result;
	}

	// On the heap: the line tables are 36 KB, and this may run on a worker thread's stack.
	std::unique_ptr<ChessDetail::FSearcher> S = std::make_unique<ChessDetail::FSearcher>(Strength);
	S->Path = History;
	const uint64_t Here = Hash();
	if (S->Path.empty() || S->Path.back() != Here)
	{
		S->Path.push_back(Here);
	}

	const int MaxDepth = std::max(1, std::min(Strength.MaxDepth, ChessDetail::MaxPly / 3));
	const int Noise = std::max(0, Strength.NoiseCentipawns);
	S->Order(*this, RootMoves, 0, FChessMove());

	// What the last FINISHED depth thought of each root move. An unfinished depth is thrown
	// away whole: half a depth knows about some moves and not others.
	std::vector<FChessMove> DoneMoves;
	std::vector<int> DoneScores;
	std::vector<std::vector<FChessMove>> DoneLines;    // best first

	for (int Depth = 1; Depth <= MaxDepth; ++Depth)
	{
		S->bMayStop = Depth > 1;
		S->PreviousLine = DoneLines.empty() ? std::vector<FChessMove>() : DoneLines[0];

		std::vector<int> Scores(RootMoves.size(), -ChessDetail::Infinity);
		std::vector<std::vector<FChessMove>> Lines(RootMoves.size());
		int BestScore = -ChessDetail::Infinity;
		int BestIndex = 0;

		for (size_t I = 0; I < RootMoves.size(); ++I)
		{
			// Without noise, plain alpha-beta: only a better move needs an exact score. With
			// noise, every move within Noise of the best needs one, so the window stays open
			// that far below it. The window's top is always open, so a score above the
			// floor is exact.
			const int Floor = BestScore == -ChessDetail::Infinity ? -ChessDetail::Infinity : BestScore - (Noise > 0 ? Noise + 1 : 0);
			const FChessMove Move = RootMoves[I];
			const FChessPosition Child = Played(Move);
			S->Path.push_back(Child.Hash());
			const bool bOnLine = !S->PreviousLine.empty() && Move.SameAs(S->PreviousLine[0]);
			const int Score = -S->Negamax(Child, Depth - 1, -ChessDetail::Infinity, -Floor, 1, bOnLine);
			S->Path.pop_back();
			if (S->bStopped)
			{
				break;
			}
			if (Score > Floor)
			{
				Scores[I] = Score;
				Lines[I].push_back(Move);
				for (int J = 1; J < S->PvLength[1]; ++J)
				{
					Lines[I].push_back(S->Pv[1][J]);
				}
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				BestIndex = static_cast<int>(I);
			}
		}
		if (S->bStopped)
		{
			break;
		}

		// Finished: keep it, and search the next depth best-first.
		std::vector<size_t> Rank(RootMoves.size());
		for (size_t I = 0; I < Rank.size(); ++I)
		{
			Rank[I] = I;
		}
		std::stable_sort(Rank.begin(), Rank.end(), [&](size_t A, size_t B)
		{
			if (A == static_cast<size_t>(BestIndex) || B == static_cast<size_t>(BestIndex))
			{
				return A == static_cast<size_t>(BestIndex) && B != static_cast<size_t>(BestIndex);
			}
			return Scores[A] > Scores[B];
		});
		DoneMoves.clear();
		DoneScores.clear();
		DoneLines.clear();
		for (const size_t I : Rank)
		{
			DoneMoves.push_back(RootMoves[I]);
			DoneScores.push_back(Scores[I]);
			DoneLines.push_back(Lines[I]);
		}
		RootMoves = DoneMoves;
		Result.DepthReached = Depth;

		// A mate found at this depth is the shortest there is; deeper cannot improve it.
		if (BestScore > ChessDetail::MateScore - 1000 && ChessDetail::MateScore - BestScore <= Depth)
		{
			break;
		}
	}

	size_t Pick = 0;
	const int Best = DoneScores[0];
	const bool bMateInSight = Best > ChessDetail::MateScore - 1000 || Best < -(ChessDetail::MateScore - 1000);
	if (Noise > 0 && !bMateInSight)
	{
		// The park player's weakness: it sees the good moves and does not always choose the
		// best. Moves nearer the best are likelier. Same seed, same position, same choice.
		int64_t Total = 0;
		for (size_t I = 0; I < DoneScores.size(); ++I)
		{
			if (DoneScores[I] != -ChessDetail::Infinity && DoneScores[I] >= Best - Noise)
			{
				Total += DoneScores[I] - (Best - Noise) + 1;
			}
		}
		uint64_t State = (static_cast<uint64_t>(Strength.Seed) << 32) ^ Here;
		int64_t Roll = static_cast<int64_t>(ChessDetail::SplitMix(State) % static_cast<uint64_t>(Total));
		for (size_t I = 0; I < DoneScores.size(); ++I)
		{
			if (DoneScores[I] != -ChessDetail::Infinity && DoneScores[I] >= Best - Noise)
			{
				Roll -= DoneScores[I] - (Best - Noise) + 1;
				if (Roll < 0)
				{
					Pick = I;
					break;
				}
			}
		}
	}

	Result.Best = DoneMoves[Pick];
	Result.Score = DoneScores[Pick];
	Result.Line = DoneLines[Pick];
	Result.Nodes = S->Nodes;
	return Result;
}

uint64_t ChessPerft(const FChessPosition& Position, int Depth)
{
	if (Depth <= 0)
	{
		return 1;
	}
	std::vector<FChessMove> Moves;
	Position.LegalMoves(Moves);
	if (Depth == 1)
	{
		return Moves.size();
	}
	uint64_t Count = 0;
	for (const FChessMove& Move : Moves)
	{
		Count += ChessPerft(Position.Played(Move), Depth - 1);
	}
	return Count;
}

// =====================================================================================
// THE GAME

FChessGame::FChessGame()
{
	Reset();
}

void FChessGame::Reset()
{
	Positions.assign(1, FChessPosition::Start());
	Hashes.assign(1, Positions[0].Hash());
	PlayedMoves.clear();
	San.clear();
	bResigned = false;
	Refresh();
}

bool FChessGame::LoadFen(const std::string& Fen)
{
	FChessPosition P;
	if (!FChessPosition::FromFen(Fen, P))
	{
		return false;
	}
	Positions.assign(1, P);
	Hashes.assign(1, P.Hash());
	PlayedMoves.clear();
	San.clear();
	bResigned = false;
	Refresh();
	return true;
}

bool FChessGame::Play(const FChessMove& Move)
{
	if (CurrentResult.IsOver())
	{
		return false;
	}
	for (const FChessMove& LegalMove : Legal)
	{
		if (LegalMove.SameAs(Move))
		{
			San.push_back(Position().ToSan(LegalMove));
			const FChessPosition Next = Position().Played(LegalMove);
			Positions.push_back(Next);
			Hashes.push_back(Next.Hash());
			PlayedMoves.push_back(LegalMove);
			Refresh();
			return true;
		}
	}
	return false;
}

bool FChessGame::PlayUci(const std::string& Uci)
{
	FChessMove Move;
	return Position().FindMove(Uci, Move) && Play(Move);
}

bool FChessGame::Undo()
{
	if (PlayedMoves.empty())
	{
		return false;
	}
	Positions.pop_back();
	Hashes.pop_back();
	PlayedMoves.pop_back();
	San.pop_back();
	bResigned = false;
	Refresh();
	return true;
}

void FChessGame::Resign(EChessColor Who)
{
	if (CurrentResult.IsOver())
	{
		return;
	}
	bResigned = true;
	Resigner = Who;
	Refresh();
}

std::string FChessGame::MoveListText(int MaxMoves) const
{
	std::vector<std::string> Groups;
	int Number = Positions[0].FullmoveNumber;
	bool bWhite = Positions[0].SideToMove == EChessColor::White;
	for (const std::string& Move : San)
	{
		if (bWhite)
		{
			Groups.push_back(std::to_string(Number) + ". " + Move);
		}
		else if (Groups.empty())
		{
			Groups.push_back(std::to_string(Number) + "... " + Move);
		}
		else
		{
			Groups.back() += " " + Move;
		}
		if (!bWhite)
		{
			++Number;
		}
		bWhite = !bWhite;
	}

	const size_t First = (MaxMoves > 0 && Groups.size() > static_cast<size_t>(MaxMoves)) ? Groups.size() - static_cast<size_t>(MaxMoves) : 0;
	std::string Text;
	for (size_t I = First; I < Groups.size(); ++I)
	{
		if (!Text.empty())
		{
			Text += ' ';
		}
		Text += Groups[I];
	}
	return Text;
}

FChessSearchResult FChessGame::Think(const FChessStrength& Strength) const
{
	return Position().Search(Strength, Hashes);
}

void FChessGame::Refresh()
{
	const FChessPosition& P = Position();
	P.LegalMoves(Legal);
	CurrentResult = FChessResult();

	if (bResigned)
	{
		CurrentResult.Outcome = EChessOutcome::Resignation;
		CurrentResult.Winner = ChessOpponent(Resigner);
		return;
	}
	// Mate first: a mate on the fiftieth move still counts.
	if (Legal.empty())
	{
		if (P.InCheck())
		{
			CurrentResult.Outcome = EChessOutcome::Checkmate;
			CurrentResult.Winner = ChessOpponent(P.SideToMove);
		}
		else
		{
			CurrentResult.Outcome = EChessOutcome::Stalemate;
		}
		return;
	}
	if (P.InsufficientMaterial())
	{
		CurrentResult.Outcome = EChessOutcome::InsufficientMaterial;
		return;
	}
	if (P.HalfmoveClock >= 100)
	{
		CurrentResult.Outcome = EChessOutcome::FiftyMoves;
		return;
	}
	// Three times, counted only back to the last capture or pawn move: nothing earlier can
	// come round again.
	const int Count = static_cast<int>(Hashes.size());
	const int Window = std::min(P.HalfmoveClock, Count - 1);
	int Seen = 1;
	for (int Back = 2; Back <= Window; Back += 2)
	{
		Seen += Hashes[static_cast<size_t>(Count - 1 - Back)] == Hashes.back() ? 1 : 0;
	}
	if (Seen >= 3)
	{
		CurrentResult.Outcome = EChessOutcome::Repetition;
	}
}
