// chess_engine_test.cpp — standalone tests for Source/SibeliusGame/ChessEngine.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_chess_engine_tests.ps1.
//
// Three kinds of claim. PERFT: the move generator agrees, to the move, with the published
// counts for six positions chosen by the chess programming community to exercise every
// rule. RULES: castling, en passant, promotion, mate, stalemate, the draws, notation, each
// shown on a position built for it. PLAY: it takes what is free, does not give away what
// is not, finds a forced mate and proves it, and every game - random or engine against
// engine - ends.

#include "ChessEngine.h"

#include <chrono>
#include <cstdint>
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

	struct FLcg
	{
		uint32_t State;
		uint32_t Next()
		{
			State = State * 1664525u + 1013904223u;
			return State >> 8;
		}
	};

	FChessPosition Fen(const char* Text)
	{
		FChessPosition P;
		if (!FChessPosition::FromFen(Text, P))
		{
			std::printf("BAD TEST FEN: %s\n", Text);
		}
		return P;
	}

	bool HasMove(const FChessPosition& P, const char* Uci)
	{
		FChessMove Move;
		return P.FindMove(Uci, Move);
	}

	std::string LineText(const std::vector<FChessMove>& Line)
	{
		std::string Text;
		for (const FChessMove& Move : Line)
		{
			Text += (Text.empty() ? "" : " ") + Move.ToUci();
		}
		return Text;
	}

	double Seconds(std::chrono::steady_clock::time_point Since)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Since).count();
	}

	// Does the side to move have a mate in one?
	bool HasMateInOne(const FChessPosition& P)
	{
		for (const FChessMove& Move : P.LegalMoves())
		{
			const FChessPosition After = P.Played(Move);
			if (After.InCheck() && After.LegalMoves().empty())
			{
				return true;
			}
		}
		return false;
	}

	// -------------------------------------------------------------------------------------
	// PERFT. Counts from the Chess Programming Wiki's "Perft Results" page. Between them:
	// the opening; "Kiwipete", which is crowded with castling, pins, en passant and
	// promotion; a rook ending full of discovered checks and en passant pins; a position
	// with promotions and castling out of check; and one known for catching bugs in
	// promotions with check.

	struct FPerftCase
	{
		const char* Name;
		const char* Fen;
		int Depth;
		uint64_t Expected;
	};

	void TestPerft()
	{
		const FPerftCase Cases[] = {
			{ "start d1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20ull },
			{ "start d2", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400ull },
			{ "start d3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ull },
			{ "start d4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ull },
			{ "start d5", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ull },
			{ "kiwipete d1", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48ull },
			{ "kiwipete d2", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039ull },
			{ "kiwipete d3", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ull },
			{ "kiwipete d4", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ull },
			{ "position 3 d5", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ull },
			{ "position 4 d4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333ull },
			{ "position 4 mirrored d4", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422333ull },
			{ "position 5 d3", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ull },
			{ "position 5 d4", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ull },
		};
		const auto Start = std::chrono::steady_clock::now();
		for (const FPerftCase& Case : Cases)
		{
			const uint64_t Got = ChessPerft(Fen(Case.Fen), Case.Depth);
			Check(Got == Case.Expected, (std::string("perft ") + Case.Name).c_str(),
				Fmt("%llu, expected %llu", static_cast<unsigned long long>(Got), static_cast<unsigned long long>(Case.Expected)));
		}
		std::printf("      (perft suite: %.2f s)\n", Seconds(Start));
	}

	// -------------------------------------------------------------------------------------
	// RULES

	void TestFen()
	{
		const char* Fens[] = {
			"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
			"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
			"rnbqkbnr/pp1ppppp/8/2pP4/8/8/PPP1PPPP/RNBQKBNR w KQkq c6 0 3",
			"8/8/8/8/8/8/8/K1k5 b - - 42 97",
		};
		for (const char* Text : Fens)
		{
			Check(Fen(Text).ToFen() == Text, "FEN round trip", Text);
		}
		FChessPosition P;
		Check(!FChessPosition::FromFen("8/8/8/8/8/8/8/8 w - - 0 1", P), "FEN refuses a board with no kings");
		Check(!FChessPosition::FromFen("4k3/8/8/8/8/8/8/4K2P w - - 0 1", P), "FEN refuses a pawn on the back rank");
		Check(!FChessPosition::FromFen("4k3/8/8/8/8/8/8/4R1K1 w - - 0 1", P),
			"FEN refuses a position where the side that just moved is in check");
		Check(!FChessPosition::FromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", P), "FEN refuses a bad side to move");
		Check(Fen("4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1").Castling == 0, "FEN drops castling rights with no rooks at home");
		Check(Fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e3 0 1").EnPassant == -1,
			"FEN drops an en passant square no pawn skipped");
	}

	void TestCastling()
	{
		// A black rook on f2 watches f1, so the king cannot cross it; the queen side is clear.
		const FChessPosition P = Fen("r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1");
		Check(!HasMove(P, "e1g1"), "cannot castle through an attacked square");
		Check(HasMove(P, "e1c1"), "can castle on the other side");

		const FChessPosition InCheck = Fen("r3k2r/8/8/8/8/8/4r3/R3K2R w KQkq - 0 1");
		Check(!HasMove(InCheck, "e1g1") && !HasMove(InCheck, "e1c1"), "cannot castle out of check");

		const FChessPosition Blocked = Fen("r3k2r/8/8/8/8/8/8/RN2K2R w KQkq - 0 1");
		Check(!HasMove(Blocked, "e1c1") && HasMove(Blocked, "e1g1"), "cannot castle past a piece in the way");

		// Only b1 attacked: the rook crosses it, the king does not. Castling is legal.
		const FChessPosition B1 = Fen("1r2k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
		Check(HasMove(B1, "e1c1"), "may castle queen side with b1 attacked (only the rook crosses it)");

		FChessMove Move;
		Fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1").FindMove("e1g1", Move);
		const FChessPosition Castled = Fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1").Played(Move);
		Check(Castled.ToFen() == "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1", "castling moves the rook and ends both white rights", Castled.ToFen());

		FChessGame Game;
		Game.LoadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
		Game.PlayUci("a1a8");
		Check(Game.Position().ToFen().find(" Kk ") != std::string::npos,
			"capturing a rook in its corner ends that side's right too", Game.Position().ToFen());
	}

	void TestEnPassant()
	{
		const FChessPosition P = Fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
		FChessMove Move;
		Check(P.FindMove("e5d6", Move) && (Move.Flags & ChessMoveFlag::EnPassant), "en passant is offered right after the double step");
		const FChessPosition After = P.Played(Move);
		Check(After.PieceAt(ChessParseSquare("d5")).IsEmpty() && After.PieceAt(ChessParseSquare("d6")).Type == EChessPieceType::Pawn,
			"en passant removes the pawn that stepped past");

		// Taking en passant would lift both pawns off the fifth rank and open it to the queen.
		const FChessPosition Pinned = Fen("8/8/8/K2pP2q/8/8/8/7k w - d6 0 1");
		Check(!HasMove(Pinned, "e5d6"), "en passant is refused when it would expose the king along the rank");

		FChessGame Game;
		Game.PlayUci("e2e4");
		Game.PlayUci("a7a6");
		Game.PlayUci("e4e5");
		Game.PlayUci("d7d5");
		Game.PlayUci("h2h3");
		Game.PlayUci("h7h6");
		Check(!Game.PlayUci("e5d6"), "en passant is gone once the moment has passed");
	}

	void TestPromotion()
	{
		const FChessPosition P = Fen("8/P6k/8/8/8/8/8/K7 w - - 0 1");
		int Promotions = 0;
		for (const FChessMove& Move : P.LegalMoves())
		{
			Promotions += Move.Promotion != EChessPieceType::None ? 1 : 0;
		}
		Check(Promotions == 4, "a pawn on the seventh has four promotions", Fmt("%d", Promotions));
		Check(!HasMove(P, "a7a8"), "a promotion must name its piece");
		FChessMove Move;
		P.FindMove("a7a8n", Move);
		Check(P.Played(Move).PieceAt(ChessParseSquare("a8")).Type == EChessPieceType::Knight, "underpromotion to a knight");
	}

	void TestMateAndStalemate()
	{
		FChessGame Scholar;
		for (const char* Uci : { "e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7" })
		{
			Scholar.PlayUci(Uci);
		}
		Check(Scholar.Result().Outcome == EChessOutcome::Checkmate && Scholar.Result().Winner == EChessColor::White,
			"scholar's mate is checkmate, white wins");
		Check(Scholar.SanMoves().back() == "Qxf7#", "the mate is written Qxf7#", Scholar.SanMoves().back());
		Check(!Scholar.PlayUci("e8e7"), "no moves after the game is over");

		FChessGame Fool;
		for (const char* Uci : { "f2f3", "e7e5", "g2g4", "d8h4" })
		{
			Fool.PlayUci(Uci);
		}
		Check(Fool.Result().Outcome == EChessOutcome::Checkmate && Fool.Result().Winner == EChessColor::Black,
			"fool's mate is checkmate, black wins");

		FChessGame Stale;
		Stale.LoadFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
		Check(Stale.Result().Outcome == EChessOutcome::Stalemate, "no move and not in check is stalemate");
	}

	void TestDraws()
	{
		FChessGame Repeat;
		for (const char* Uci : { "g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1" })
		{
			Repeat.PlayUci(Uci);
		}
		Check(!Repeat.Result().IsOver(), "the opening position twice is not yet a draw");
		Repeat.PlayUci("f6g8");
		Check(Repeat.Result().Outcome == EChessOutcome::Repetition, "the opening position three times is a draw");

		FChessGame Fifty;
		Fifty.LoadFen("4k3/8/8/8/8/8/8/R3K3 w - - 99 60");
		Check(!Fifty.Result().IsOver(), "ninety-nine quiet plies is not yet a draw");
		Fifty.PlayUci("a1a2");
		Check(Fifty.Result().Outcome == EChessOutcome::FiftyMoves, "the hundredth quiet ply is a draw");

		FChessGame Kings;
		Kings.LoadFen("8/8/8/4k3/8/8/8/4K3 w - - 0 1");
		Check(Kings.Result().Outcome == EChessOutcome::InsufficientMaterial, "king against king is a draw");
		Check(Fen("8/8/8/4k3/8/8/8/3BK3 w - - 0 1").InsufficientMaterial(), "king and bishop cannot mate");
		Check(Fen("8/8/2b5/4k3/8/8/8/3BK3 w - - 0 1").InsufficientMaterial(), "bishops on the same colour cannot mate");
		Check(!Fen("8/8/3b4/4k3/8/8/8/3BK3 w - - 0 1").InsufficientMaterial(), "bishops on opposite colours can, in principle");
		Check(!Fen("8/8/8/4k3/8/8/8/3RK3 w - - 0 1").InsufficientMaterial(), "king and rook can mate");
	}

	void TestNotation()
	{
		const FChessPosition Knights = Fen("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1");
		FChessMove B;
		FChessMove F;
		Knights.FindMove("b1d2", B);
		Knights.FindMove("f1d2", F);
		Check(Knights.ToSan(B) == "Nbd2" && Knights.ToSan(F) == "Nfd2", "two knights to one square are told apart by file",
			Knights.ToSan(B) + " " + Knights.ToSan(F));

		const FChessPosition Rooks = Fen("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1");
		FChessMove Low;
		FChessMove High;
		Rooks.FindMove("a1a3", Low);
		Rooks.FindMove("a5a3", High);
		Check(Rooks.ToSan(Low) == "R1a3" && Rooks.ToSan(High) == "R5a3", "two rooks on one file are told apart by rank",
			Rooks.ToSan(Low) + " " + Rooks.ToSan(High));

		FChessGame Game;
		for (const char* Uci : { "e2e4", "d7d5", "e4d5", "g8f6", "f1b5", "c7c6", "d5c6", "d8d7", "c6b7", "d7b5", "b7a8q" })
		{
			Game.PlayUci(Uci);
		}
		Check(Game.MoveListText() == "1. e4 d5 2. exd5 Nf6 3. Bb5+ c6 4. dxc6 Qd7 5. cxb7 Qxb5 6. bxa8=Q",
			"the move list reads the way a scoresheet does", Game.MoveListText());
		Check(Game.MoveListText(2) == "5. cxb7 Qxb5 6. bxa8=Q", "the move list can show only its last moves", Game.MoveListText(2));

		FChessGame Castle;
		Castle.LoadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
		Castle.PlayUci("e1g1");
		Castle.PlayUci("e8c8");
		Check(Castle.MoveListText() == "1. O-O O-O-O", "castling is written O-O and O-O-O", Castle.MoveListText());
	}

	void TestGameBookkeeping()
	{
		FChessGame Game;
		const std::string Start = Game.Position().ToFen();
		Check(!Game.PlayUci("e2e5"), "an illegal move is refused");
		Check(Game.Position().ToFen() == Start && Game.PliesPlayed() == 0, "and changes nothing");
		Game.PlayUci("e2e4");
		Game.PlayUci("c7c5");
		Game.Undo();
		Game.Undo();
		Check(Game.Position().ToFen() == Start && Game.PliesPlayed() == 0 && !Game.Undo(), "take-backs return to the start");

		Game.Resign(EChessColor::White);
		Check(Game.Result().Outcome == EChessOutcome::Resignation && Game.Result().Winner == EChessColor::Black, "resigning loses");

		FChessGame A;
		FChessGame BGame;
		for (const char* Uci : { "g1f3", "g8f6", "b1c3" })
		{
			A.PlayUci(Uci);
		}
		for (const char* Uci : { "b1c3", "g8f6", "g1f3" })
		{
			BGame.PlayUci(Uci);
		}
		Check(A.Position().Hash() == BGame.Position().Hash(), "a transposition hashes the same");
		FChessPosition WhiteToMove = A.Position();
		WhiteToMove.SideToMove = EChessColor::White;
		Check(WhiteToMove.Hash() != A.Position().Hash(), "the side to move changes the hash");
	}

	// -------------------------------------------------------------------------------------
	// PLAY

	void TestTactics()
	{
		const FChessStrength Strengths[] = { FChessStrength::Hustler(), FChessStrength::Master() };
		const char* Names[] = { "hustler", "master" };
		for (int I = 0; I < 2; ++I)
		{
			const FChessStrength& S = Strengths[I];

			const FChessSearchResult BackRank = Fen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1").Search(S);
			Check(BackRank.Best.ToUci() == "a1a8" && BackRank.IsMate() && BackRank.MateInPlies() == 1,
				Fmt("%s mates on the back rank", Names[I]).c_str(), BackRank.Best.ToUci());

			const FChessSearchResult Scholar = Fen("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1").Search(S);
			Check(Scholar.Best.ToUci() == "f3f7", Fmt("%s finds scholar's mate", Names[I]).c_str(), Scholar.Best.ToUci());

			const FChessSearchResult Free = Fen("4k3/8/8/3q4/8/8/8/3QK3 w - - 0 1").Search(S);
			Check(Free.Best.ToUci() == "d1d5", Fmt("%s takes a free queen", Names[I]).c_str(), Free.Best.ToUci());

			const FChessSearchResult Poisoned = Fen("4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1").Search(S);
			Check(Poisoned.Best.ToUci() != "d1d5", Fmt("%s does not take a defended pawn with its queen", Names[I]).c_str(),
				Poisoned.Best.ToUci());

			// The black king is one step from the pawn, so promoting later is losing it.
			const FChessSearchResult Promote = Fen("8/P1k5/8/8/8/8/8/K7 w - - 0 1").Search(S);
			Check(Promote.Best.ToUci() == "a7a8q", Fmt("%s promotes to a queen", Names[I]).c_str(), Promote.Best.ToUci());

			const FChessSearchResult Escape = Fen("4k3/8/8/8/8/8/3q4/4K3 w - - 0 1").Search(S);
			Check(Escape.Best.ToUci() == "e1d2", Fmt("%s takes the queen that checks it", Names[I]).c_str(), Escape.Best.ToUci());
		}

		// Mate in two with two rooks: no mate in one (Ra8+ Kh7 runs), but 1.Rb7 and any
		// reply, 2.Ra8#. The claim is PROVED, not trusted: after its first move, every
		// reply must leave a mate in one.
		const FChessPosition Ladder = Fen("7k/8/8/8/8/8/1R6/R5K1 w - - 0 1");
		const FChessSearchResult Two = Ladder.Search(FChessStrength::Master());
		Check(!HasMateInOne(Ladder), "the ladder position has no mate in one");
		Check(Two.IsMate() && Two.MateInPlies() == 3, "master sees mate in two", Fmt("score %d, line %s", Two.Score, LineText(Two.Line).c_str()));
		bool bForced = !Two.Best.IsNull();
		const FChessPosition AfterFirst = Ladder.Played(Two.Best);
		for (const FChessMove& Reply : AfterFirst.LegalMoves())
		{
			bForced = bForced && HasMateInOne(AfterFirst.Played(Reply));
		}
		Check(bForced, "and its first move forces mate against every reply", Two.Best.ToUci());

		// The line it reports is a real line.
		bool bLegalLine = !Two.Line.empty() && Two.Line[0].SameAs(Two.Best);
		FChessPosition Walk = Ladder;
		for (const FChessMove& Move : Two.Line)
		{
			FChessMove Found;
			bLegalLine = bLegalLine && Walk.FindMove(Move.From, Move.To, Move.Promotion, Found);
			if (bLegalLine)
			{
				Walk = Walk.Played(Found);
			}
		}
		Check(bLegalLine && Walk.InCheck() && Walk.LegalMoves().empty(), "the line it expects is legal and ends in mate",
			LineText(Two.Line));
	}

	void TestStrength()
	{
		const FChessPosition Start = FChessPosition::Start();

		const FChessSearchResult A = Start.Search(FChessStrength::Hustler());
		const FChessSearchResult B = Start.Search(FChessStrength::Hustler());
		Check(A.Best.SameAs(B.Best) && A.Nodes == B.Nodes, "the same seed thinks the same thoughts");

		std::vector<std::string> Seen;
		for (uint32_t Seed = 1; Seed <= 24; ++Seed)
		{
			FChessStrength S = FChessStrength::Hustler();
			S.Seed = Seed;
			const std::string Move = Start.Search(S).Best.ToUci();
			bool bNew = true;
			for (const std::string& Old : Seen)
			{
				bNew = bNew && Old != Move;
			}
			if (bNew)
			{
				Seen.push_back(Move);
			}
		}
		std::string SeenText;
		for (const std::string& Move : Seen)
		{
			SeenText += Move + " ";
		}
		Check(Seen.size() >= 3, "the hustler varies its opening with the seed", SeenText);

		std::vector<std::string> MasterSeen;
		for (uint32_t Seed = 1; Seed <= 6; ++Seed)
		{
			FChessStrength S = FChessStrength::Master();
			S.Seed = Seed;
			S.MaxDepth = 4;
			MasterSeen.push_back(Start.Search(S).Best.ToUci());
		}
		bool bSteady = true;
		for (const std::string& Move : MasterSeen)
		{
			bSteady = bSteady && Move == MasterSeen[0];
		}
		Check(bSteady, "with no noise the seed changes nothing", MasterSeen[0]);

		FChessStrength Tight = FChessStrength::Master();
		Tight.MaxDepth = 12;
		Tight.NodeLimit = 20000;
		const FChessPosition Kiwi = Fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
		const FChessSearchResult Limited = Kiwi.Search(Tight);
		Check(!Limited.Best.IsNull() && Limited.DepthReached < 12 && Limited.Nodes <= Tight.NodeLimit + 1,
			"the node limit stops it, and it still answers", Fmt("depth %d, %lld nodes", Limited.DepthReached, static_cast<long long>(Limited.Nodes)));

		const char* Middlegames[] = {
			"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
			"r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1BP2/PPPQ2PP/R3KB1R w KQ - 0 9",
		};
		for (const char* Text : Middlegames)
		{
			const auto Start2 = std::chrono::steady_clock::now();
			const FChessSearchResult Hustler = Fen(Text).Search(FChessStrength::Hustler());
			const double HustlerSeconds = Seconds(Start2);
			const auto Start3 = std::chrono::steady_clock::now();
			const FChessSearchResult Master = Fen(Text).Search(FChessStrength::Master());
			const double MasterSeconds = Seconds(Start3);
			std::printf("      hustler %s d%d %lld nodes %.2f s | master %s d%d %lld nodes %.2f s\n",
				Hustler.Best.ToUci().c_str(), Hustler.DepthReached, static_cast<long long>(Hustler.Nodes), HustlerSeconds,
				Master.Best.ToUci().c_str(), Master.DepthReached, static_cast<long long>(Master.Nodes), MasterSeconds);
		}
	}

	// -------------------------------------------------------------------------------------
	// WHOLE GAMES MUST END - the lesson the pinball's wedge taught.

	bool Sane(const FChessGame& Game, std::string& Why)
	{
		const FChessPosition& P = Game.Position();
		FChessPosition Back;
		if (!FChessPosition::FromFen(P.ToFen(), Back) || Back.ToFen() != P.ToFen() || Back.Hash() != P.Hash())
		{
			Why = "FEN round trip " + P.ToFen();
			return false;
		}
		const EChessColor Moved = ChessOpponent(P.SideToMove);
		if (P.IsAttacked(P.KingSquare[Moved == EChessColor::White ? 0 : 1], P.SideToMove))
		{
			Why = "the side that moved is in check " + P.ToFen();
			return false;
		}
		return true;
	}

	void TestRandomGames()
	{
		const auto Start = std::chrono::steady_clock::now();
		FLcg Rng{ 20260928u };
		int Ended = 0;
		int Broken = 0;
		int Outcomes[7] = {};
		int LongestPlies = 0;
		std::string FirstProblem;
		const int Games = 400;
		for (int G = 0; G < Games; ++G)
		{
			FChessGame Game;
			int Plies = 0;
			while (!Game.Result().IsOver() && Plies < 3000)
			{
				const std::vector<FChessMove>& Legal = Game.LegalMoves();
				const FChessMove Move = Legal[Rng.Next() % Legal.size()];
				const std::string Before = Game.Position().ToFen();
				Game.Play(Move);
				++Plies;
				std::string Why;
				if (!Sane(Game, Why))
				{
					++Broken;
					if (FirstProblem.empty())
					{
						FirstProblem = Why;
					}
					break;
				}
				// Every tenth game also checks that a take-back is exact.
				if (G % 10 == 0)
				{
					Game.Undo();
					if (Game.Position().ToFen() != Before)
					{
						++Broken;
						if (FirstProblem.empty())
						{
							FirstProblem = "undo " + Before;
						}
						break;
					}
					Game.Play(Move);
				}
			}
			Ended += Game.Result().IsOver() ? 1 : 0;
			++Outcomes[static_cast<int>(Game.Result().Outcome)];
			LongestPlies = Plies > LongestPlies ? Plies : LongestPlies;
		}
		Check(Broken == 0, "random games never reach an impossible position", FirstProblem);
		Check(Ended == Games, "every random game ends by the rules",
			Fmt("%d of %d; mate %d stalemate %d fifty %d repetition %d material %d; longest %d plies, %.1f s",
				Ended, Games, Outcomes[1], Outcomes[2], Outcomes[3], Outcomes[4], Outcomes[5], LongestPlies, Seconds(Start)));
	}

	void TestEngineGames()
	{
		// Engine against engine, at the strengths the table will use. Each game must end,
		// and neither side should ever be offered nothing to play.
		const auto Start = std::chrono::steady_clock::now();
		int Ended = 0;
		std::string Summary;
		const int Games = 4;
		for (int G = 0; G < Games; ++G)
		{
			FChessGame Game;
			FChessStrength White = G % 2 == 0 ? FChessStrength::Hustler() : FChessStrength::Beginner();
			FChessStrength Black = G % 2 == 0 ? FChessStrength::Beginner() : FChessStrength::Hustler();
			White.Seed = static_cast<uint32_t>(100 + G);
			Black.Seed = static_cast<uint32_t>(200 + G);
			bool bNullMove = false;
			while (!Game.Result().IsOver() && Game.PliesPlayed() < 1200)
			{
				const FChessSearchResult Think = Game.Think(Game.SideToMove() == EChessColor::White ? White : Black);
				if (Think.Best.IsNull() || !Game.Play(Think.Best))
				{
					bNullMove = true;
					break;
				}
			}
			Ended += Game.Result().IsOver() && !bNullMove ? 1 : 0;
			const FChessResult R = Game.Result();
			Summary += Fmt("[%s %s %d plies] ",
				G % 2 == 0 ? "hustler-beginner" : "beginner-hustler",
				R.Outcome == EChessOutcome::Checkmate ? (R.Winner == EChessColor::White ? "1-0" : "0-1") : "draw",
				Game.PliesPlayed());
		}
		Check(Ended == Games, "every engine game ends", Summary + Fmt("%.1f s", Seconds(Start)));
	}
}

int main()
{
	TestPerft();
	TestFen();
	TestCastling();
	TestEnPassant();
	TestPromotion();
	TestMateAndStalemate();
	TestDraws();
	TestNotation();
	TestGameBookkeeping();
	TestTactics();
	TestStrength();
	TestRandomGames();
	TestEngineGames();

	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
