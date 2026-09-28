// ChessTable.cpp — see the header for what this actor is for and what it must never do.

#include "ChessTable.h"

#include "CodeVisionComponent.h"
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"

#include "Async/Async.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace ChessTableDetail
{
	const TCHAR* const TypeNames[6] = { TEXT("Pawn"), TEXT("Knight"), TEXT("Bishop"), TEXT("Rook"), TEXT("Queen"), TEXT("King") };

	int32 SetIndex(EChessPieceType Type, EChessColor Color)
	{
		return (Color == EChessColor::White ? 0 : 6) + static_cast<int32>(Type) - 1;
	}

	// Square marks, in SquareMarks order, and their colours (RGB, opacity).
	enum EMark : int32 { LastFrom, LastTo, Picked, CursorMark, Check, MarkCount };
	const FLinearColor MarkColors[MarkCount] = {
		FLinearColor(1.00f, 0.82f, 0.25f, 0.35f),
		FLinearColor(1.00f, 0.82f, 0.25f, 0.45f),
		FLinearColor(0.25f, 1.00f, 0.40f, 0.50f),
		FLinearColor(0.35f, 0.65f, 1.00f, 0.40f),
		FLinearColor(1.00f, 0.12f, 0.10f, 0.55f),
	};
	const FLinearColor DotColor(0.20f, 0.95f, 0.35f, 0.65f);
	constexpr int32 DotCount = 28;       // a queen in the middle of an empty board has 27
	constexpr double ShapeSize = 100.0;  // the engine's basic shapes
	constexpr double MarkLift = 0.06;    // cm above the playing surface

	FString OutcomeText(const FChessResult& R, EChessColor Player)
	{
		switch (R.Outcome)
		{
		case EChessOutcome::Checkmate:
			return R.Winner == Player ? TEXT("Checkmate - you win!") : TEXT("Checkmate - the ghost wins.");
		case EChessOutcome::Resignation:
			return R.Winner == Player ? TEXT("The ghost resigns - you win!") : TEXT("You resigned.");
		case EChessOutcome::Stalemate:            return TEXT("Stalemate - a draw.");
		case EChessOutcome::FiftyMoves:           return TEXT("A draw - fifty moves without a capture or a pawn move.");
		case EChessOutcome::Repetition:           return TEXT("A draw - the same position three times.");
		case EChessOutcome::InsufficientMaterial: return TEXT("A draw - neither side can mate.");
		default:                                  return FString();
		}
	}

	const TCHAR* LevelName(EChessGhostLevel Level)
	{
		switch (Level)
		{
		case EChessGhostLevel::Beginner: return TEXT("a beginner");
		case EChessGhostLevel::Master:   return TEXT("a master");
		default:                         return TEXT("a park hustler");
		}
	}
}

AChessTable::AChessTable()
{
	PrimaryActorTick.bCanEverTick = true;

	using namespace ChessTableDetail;

	// Hard references, so every piece reaches the pak.
	SetMeshes.SetNum(12);
	for (int32 C = 0; C < 2; ++C)
	{
		for (int32 T = 0; T < 6; ++T)
		{
			const FString Name = FString::Printf(TEXT("SM_Chess_%s%s"), C == 0 ? TEXT("White") : TEXT("Black"), TypeNames[T]);
			ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*FString::Printf(TEXT("/Game/ChessSet/Meshes/%s.%s"), *Name, *Name));
			SetMeshes[C * 6 + T] = Finder.Succeeded() ? Finder.Object : nullptr;
		}
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoardMesh(TEXT("/Game/ChessSet/Meshes/SM_Chess_Board.SM_Chess_Board"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkMaterial(TEXT("/Game/Chess/M_ChessHighlight.M_ChessHighlight"));

	auto MakeVisualOnly = [](UStaticMeshComponent* Component)
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
	};

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(SceneRoot);
	MakeVisualOnly(Board);
	if (BoardMesh.Succeeded())
	{
		Board->SetStaticMesh(BoardMesh.Object);
	}

	Seat = CreateDefaultSubobject<UBoxComponent>(TEXT("Seat"));
	Seat->SetupAttachment(SceneRoot);
	Seat->SetMobility(EComponentMobility::Movable);
	Seat->SetBoxExtent(FVector(40.0, 40.0, 20.0));
	Seat->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Seat->SetCollisionResponseToAllChannels(ECR_Ignore);
	Seat->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Seat->SetGenerateOverlapEvents(false);

	PieceMeshes.SetNum(32);
	for (int32 I = 0; I < 32; ++I)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Piece%02d"), I));
		Piece->SetupAttachment(SceneRoot);
		MakeVisualOnly(Piece);
		PieceMeshes[I] = Piece;
	}

	SquareMarks.SetNum(MarkCount);
	for (int32 I = 0; I < MarkCount; ++I)
	{
		UStaticMeshComponent* Mark = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Mark%d"), I));
		Mark->SetupAttachment(SceneRoot);
		MakeVisualOnly(Mark);
		Mark->SetCastShadow(false);
		if (PlaneMesh.Succeeded())
		{
			Mark->SetStaticMesh(PlaneMesh.Object);
		}
		if (MarkMaterial.Succeeded())
		{
			Mark->SetMaterial(0, MarkMaterial.Object);
		}
		Mark->SetVisibility(false);
		SquareMarks[I] = Mark;
	}

	MoveDots.SetNum(DotCount);
	for (int32 I = 0; I < DotCount; ++I)
	{
		UStaticMeshComponent* Dot = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Dot%02d"), I));
		Dot->SetupAttachment(SceneRoot);
		MakeVisualOnly(Dot);
		Dot->SetCastShadow(false);
		if (CylinderMesh.Succeeded())
		{
			Dot->SetStaticMesh(CylinderMesh.Object);
		}
		if (MarkMaterial.Succeeded())
		{
			Dot->SetMaterial(0, MarkMaterial.Object);
		}
		Dot->SetVisibility(false);
		MoveDots[I] = Dot;
	}

	TableCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TableCamera"));
	TableCamera->SetupAttachment(SceneRoot);

	BoardLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoardLight"));
	BoardLight->SetupAttachment(SceneRoot);
	BoardLight->SetMobility(EComponentMobility::Movable);
	BoardLight->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	BoardLight->SetIntensityUnits(ELightUnits::Lumens);
	BoardLight->SetAttenuationRadius(140.0f);
	BoardLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.88f));
	BoardLight->SetCastShadows(true);

	FitSet();
	ResetPieces();
	SyncCamera();
}

// =====================================================================================
// THE SET

UStaticMesh* AChessTable::MeshFor(EChessPieceType Type, EChessColor Color) const
{
	if (Type == EChessPieceType::None)
	{
		return nullptr;
	}
	const int32 Index = ChessTableDetail::SetIndex(Type, Color);
	return SetMeshes.IsValidIndex(Index) ? SetMeshes[Index].Get() : nullptr;
}

/* The vendor stood the whole set up in the opening position and every mesh kept its place,
   so the set measures itself: the kings are on e1 and e8, seven ranks apart, which gives
   the direction from White to Black and the square pitch; the queen is one file toward
   the a-file from her king; and every base stands on the playing surface. From those,
   one rotation and one offset take the vendor's frame to ours. */
void AChessTable::FitSet()
{
	UStaticMesh* WhiteKing = MeshFor(EChessPieceType::King, EChessColor::White);
	UStaticMesh* BlackKing = MeshFor(EChessPieceType::King, EChessColor::Black);
	UStaticMesh* WhiteQueen = MeshFor(EChessPieceType::Queen, EChessColor::White);
	if (!WhiteKing || !BlackKing || !WhiteQueen)
	{
		return;   // not imported yet: the table stands empty until it is
	}

	const FBox WK = WhiteKing->GetBoundingBox();
	const FBox BK = BlackKing->GetBoundingBox();
	const FVector WKc = WK.GetCenter();
	const FVector BKc = BK.GetCenter();
	FVector Ranks = BKc - WKc;
	Ranks.Z = 0.0;
	FVector Files = WKc - WhiteQueen->GetBoundingBox().GetCenter();   // d-file to e-file
	Files.Z = 0.0;
	if (Ranks.SizeSquared() < 1.0 || Files.SizeSquared() < 0.01)
	{
		return;
	}
	SquarePitch = Ranks.Size() / 7.0;
	SetRotation = FQuat::FindBetweenNormals(Ranks.GetSafeNormal(), FVector::ForwardVector);
	FileSign = SetRotation.RotateVector(Files).Y >= 0.0 ? 1.0 : -1.0;

	// The e-file runs through both kings; the board's centre is half a file toward d.
	const double SurfaceZ = WK.Min.Z;
	GridCentre = (WKc + BKc) * 0.5 - Files.GetSafeNormal() * (0.5 * SquarePitch);
	GridCentre.Z = SurfaceZ;

	SetBaseCentres.SetNum(SetMeshes.Num());
	for (int32 I = 0; I < SetMeshes.Num(); ++I)
	{
		if (SetMeshes[I])
		{
			const FBox Box = SetMeshes[I]->GetBoundingBox();
			SetBaseCentres[I] = FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
		}
	}

	if (Board && Board->GetStaticMesh())
	{
		const FBox BoardBox = Board->GetStaticMesh()->GetBoundingBox();
		BoardHalf = 0.5 * FMath::Max(BoardBox.GetSize().X, BoardBox.GetSize().Y);
		BoardUnderside = BoardBox.Min.Z - SurfaceZ;
		Board->SetRelativeLocationAndRotation(-SetRotation.RotateVector(GridCentre), SetRotation);
	}
	if (Seat)
	{
		Seat->SetBoxExtent(FVector(BoardHalf + 12.0, BoardHalf + 12.0, 20.0));
	}
}

FVector AChessTable::SquareLocal(int32 Square) const
{
	const double File = (Square & 7) - 3.5;
	const double Rank = (Square >> 3) - 3.5;
	return FVector(Rank * SquarePitch, FileSign * File * SquarePitch, 0.0);
}

// Taken pieces line up beside the board: Black's on White's right, White's on White's left,
// as a player sets them down.
FVector AChessTable::GraveyardLocal(EChessColor Color, int32 Index) const
{
	const double Side = BoardHalf + 3.5;
	const double Step = 0.55 * SquarePitch;
	const double Row = FMath::Min(Index, 14);
	return Color == EChessColor::Black
		? FVector(-3.75 * SquarePitch + Row * Step, Side, BoardUnderside)
		: FVector(3.75 * SquarePitch - Row * Step, -Side, BoardUnderside);
}

void AChessTable::PlaceMesh(UStaticMeshComponent* Mesh, const FVector& BaseCentre) const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}
	const int32 Index = SetMeshes.IndexOfByKey(Mesh->GetStaticMesh());
	const FVector Base = SetBaseCentres.IsValidIndex(Index) ? SetBaseCentres[Index] : FVector::ZeroVector;
	Mesh->SetRelativeLocationAndRotation(BaseCentre - SetRotation.RotateVector(Base), SetRotation);
}

void AChessTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FitSet();
	ResetPieces();
	SyncCamera();
}

void AChessTable::BeginPlay()
{
	Super::BeginPlay();
	FitSet();

	MarkMaterials.Reset();
	for (int32 I = 0; I < SquareMarks.Num() + 1; ++I)
	{
		UStaticMeshComponent* Holder = I < SquareMarks.Num() ? SquareMarks[I].Get() : (MoveDots.Num() > 0 ? MoveDots[0].Get() : nullptr);
		UMaterialInstanceDynamic* Mid = Holder ? Holder->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
		const FLinearColor Color = I < SquareMarks.Num() ? ChessTableDetail::MarkColors[I] : ChessTableDetail::DotColor;
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R, Color.G, Color.B, 1.0f));
			Mid->SetScalarParameterValue(TEXT("Opacity"), Color.A);
		}
		MarkMaterials.Add(Mid);
	}
	UMaterialInstanceDynamic* DotMaterial = MarkMaterials.Num() > 0 ? MarkMaterials.Last().Get() : nullptr;
	for (UStaticMeshComponent* Dot : MoveDots)
	{
		if (Dot && DotMaterial)
		{
			Dot->SetMaterial(0, DotMaterial);
		}
	}
	if (BoardLight)
	{
		BoardLight->SetIntensity(BoardLightLumens);
		BoardLight->SetVisibility(BoardLightLumens > 0.0f);
	}
	NewGame();
}

void AChessTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();   // whatever tore this down, he gets his controls and his eyes back
	++ThinkTicket;   // and an answer still on its way is thrown away
	Super::EndPlay(EndPlayReason);
}

// =====================================================================================
// THE GAME

void AChessTable::NewGame()
{
	++ThinkTicket;
	bGhostThinking = false;
	bHaveGhostThought = false;
	Selected = -1;
	Game.Reset();
	GameSeed = static_cast<uint32>(FMath::Rand()) | 1u;
	ResetPieces();
	if (!IsPlayersTurn() && !Game.Result().IsOver())
	{
		StartGhostThinking();
	}
}

/* Every mesh to where the position says, at once: a new game, a take-back. Pieces that are
   not on the board go to the side of it - which ones is the difference between a full set
   and what is left, so a take-back past a capture puts the right piece back. */
void AChessTable::ResetPieces()
{
	const FChessPosition& P = Game.Position();
	Anims.SetNum(PieceMeshes.Num());
	int32 Next = 0;
	int32 Present[2][7] = {};
	for (int32 Square = 0; Square < 64 && Next < Anims.Num(); ++Square)
	{
		const FChessPiece Piece = P.PieceAt(Square);
		if (Piece.IsEmpty())
		{
			continue;
		}
		FPieceAnim& A = Anims[Next++];
		A.Square = Square;
		A.Type = Piece.Type;
		A.Color = Piece.Color;
		A.To = A.From = SquareLocal(Square);
		A.T = 1.0f;
		++Present[Piece.Color == EChessColor::White ? 0 : 1][static_cast<int32>(Piece.Type)];
	}

	static const int32 FullSet[7] = { 0, 8, 2, 2, 2, 1, 1 };
	Captured[0] = Captured[1] = 0;
	for (int32 C = 0; C < 2; ++C)
	{
		for (int32 T = 1; T <= 6; ++T)
		{
			for (int32 Missing = FullSet[T] - Present[C][T]; Missing > 0 && Next < Anims.Num(); --Missing)
			{
				FPieceAnim& A = Anims[Next++];
				A.Square = -1;
				A.Type = static_cast<EChessPieceType>(T);
				A.Color = C == 0 ? EChessColor::White : EChessColor::Black;
				A.To = A.From = GraveyardLocal(A.Color, Captured[C]++);
				A.T = 1.0f;
			}
		}
	}
	for (; Next < Anims.Num(); ++Next)
	{
		Anims[Next] = FPieceAnim();   // a promoted pawn's twin: nothing to show
	}

	for (int32 I = 0; I < PieceMeshes.Num(); ++I)
	{
		UStaticMeshComponent* Mesh = PieceMeshes[I];
		if (!Mesh)
		{
			continue;
		}
		const FPieceAnim& A = Anims[I];
		Mesh->SetStaticMesh(MeshFor(A.Type, A.Color));
		Mesh->SetVisibility(A.Type != EChessPieceType::None);
		PlaceMesh(Mesh, A.To);
	}
}

void AChessTable::AnimateMove(const FChessPosition& Before, const FChessMove& Move)
{
	auto FindAt = [this](int32 Square) { return Anims.IndexOfByPredicate([Square](const FPieceAnim& A) { return A.Square == Square; }); };
	auto Start = [this](int32 Index, const FVector& To, float Lift)
	{
		FPieceAnim& A = Anims[Index];
		A.From = A.T >= 1.0f ? A.To : FMath::Lerp(A.From, A.To, A.T);
		A.To = To;
		A.T = 0.0f;
		A.Lift = Lift;
	};

	// The taken piece first, before anything else claims its square.
	if (Move.IsCapture())
	{
		const int32 Taken = (Move.Flags & ChessMoveFlag::EnPassant)
			? Move.To + (Before.SideToMove == EChessColor::White ? -8 : 8) : Move.To;
		const int32 Index = FindAt(Taken);
		if (Index != INDEX_NONE)
		{
			const int32 Side = Anims[Index].Color == EChessColor::White ? 0 : 1;
			Anims[Index].Square = -1;
			Start(Index, GraveyardLocal(Anims[Index].Color, Captured[Side]++), 6.0f);
		}
	}

	const int32 Mover = FindAt(Move.From);
	if (Mover != INDEX_NONE)
	{
		FPieceAnim& A = Anims[Mover];
		A.Square = Move.To;
		Start(Mover, SquareLocal(Move.To), A.Type == EChessPieceType::Knight ? 4.5f : 1.5f);
		if (Move.Promotion != EChessPieceType::None)
		{
			A.Type = Move.Promotion;
			if (UStaticMeshComponent* Mesh = PieceMeshes[Mover])
			{
				Mesh->SetStaticMesh(MeshFor(A.Type, A.Color));
			}
		}
	}

	if (Move.Flags & ChessMoveFlag::Castle)
	{
		const bool bKingSide = Move.To > Move.From;
		const int32 RookFrom = bKingSide ? Move.From + 3 : Move.From - 4;
		const int32 RookTo = bKingSide ? Move.From + 1 : Move.From - 1;
		const int32 Rook = FindAt(RookFrom);
		if (Rook != INDEX_NONE)
		{
			Anims[Rook].Square = RookTo;
			Start(Rook, SquareLocal(RookTo), 4.0f);
		}
	}

	SoundCountdown = MoveSeconds;
	bSoundIsCapture = Move.IsCapture();
}

void AChessTable::UpdatePieces(float DeltaSeconds)
{
	const float Rate = MoveSeconds > 0.0f ? DeltaSeconds / MoveSeconds : 1.0f;
	for (int32 I = 0; I < Anims.Num() && I < PieceMeshes.Num(); ++I)
	{
		FPieceAnim& A = Anims[I];
		if (A.T >= 1.0f)
		{
			continue;
		}
		A.T = FMath::Min(1.0f, A.T + Rate);
		const float Ease = A.T * A.T * (3.0f - 2.0f * A.T);
		FVector Where = FMath::Lerp(A.From, A.To, Ease);
		Where.Z += A.Lift * 4.0f * A.T * (1.0f - A.T);   // up and over, not through
		PlaceMesh(PieceMeshes[I], Where);
	}

	if (SoundCountdown >= 0.0f)
	{
		SoundCountdown -= DeltaSeconds;
		if (SoundCountdown < 0.0f)
		{
			TableSounds::Play(this, bSoundIsCapture ? ETableSound::ChessCapture : ETableSound::ChessPlace,
				0.8f, FMath::FRandRange(0.94f, 1.06f));
		}
	}
}

bool AChessTable::IsPlayersTurn() const
{
	return Game.SideToMove() == PlayerColor && !Game.Result().IsOver();
}

FChessStrength AChessTable::GhostStrength() const
{
	FChessStrength S;
	switch (GhostLevel)
	{
	case EChessGhostLevel::Beginner: S = FChessStrength::Beginner(); break;
	case EChessGhostLevel::Master:   S = FChessStrength::Master(); break;
	default:                         S = FChessStrength::Hustler(); break;
	}
	S.Seed = GameSeed + static_cast<uint32>(Game.PliesPlayed()) * 2654435761u;
	return S;
}

void AChessTable::TryPlayerMove(int32 From, int32 To)
{
	if (!IsPlayersTurn())
	{
		return;
	}
	// A pawn reaching the last rank becomes a queen: the choice almost everyone makes, and
	// one key fewer to learn.
	const FChessPosition Before = Game.Position();
	FChessMove Move;
	if (!Before.FindMove(From, To, EChessPieceType::None, Move) && !Before.FindMove(From, To, EChessPieceType::Queen, Move))
	{
		return;
	}
	if (Game.Play(Move))
	{
		AnimateMove(Before, Move);
		AfterMove();
	}
}

void AChessTable::AfterMove()
{
	Selected = -1;
	const FChessResult R = Game.Result();
	if (R.IsOver())
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Chess] %s  %s"), *ChessTableDetail::OutcomeText(R, PlayerColor),
			UTF8_TO_TCHAR(Game.MoveListText().c_str()));
		return;
	}
	if (!IsPlayersTurn())
	{
		StartGhostThinking();
	}
}

void AChessTable::StartGhostThinking()
{
	const int32 Ticket = ++ThinkTicket;
	TicketInFlight = Ticket;
	bGhostThinking = true;
	GhostThinkElapsed = 0.0f;
	GhostThoughtFrom = Game.Position();

	// A copy of the game goes to the worker, so nothing it touches is ever touched here.
	const FChessGame Copy = Game;
	const FChessStrength Strength = GhostStrength();
	GhostFuture = Async(EAsyncExecution::ThreadPool, [Copy, Strength]()
	{
		return Copy.Think(Strength);
	});
}

void AChessTable::PollGhost(float DeltaSeconds)
{
	if (!bGhostThinking)
	{
		return;
	}
	GhostThinkElapsed += DeltaSeconds;
	if (!GhostFuture.IsValid() || !GhostFuture.IsReady() || GhostThinkElapsed < GhostMinThinkSeconds)
	{
		return;
	}
	const FChessSearchResult Result = GhostFuture.Get();
	GhostFuture = TFuture<FChessSearchResult>();
	bGhostThinking = false;
	if (TicketInFlight != ThinkTicket || Result.Best.IsNull())
	{
		return;   // a take-back or a new game happened while it thought
	}

	// It never resigns, even seeing mate coming: the mate is his to deliver.

	const FChessPosition Before = Game.Position();
	if (Game.Play(Result.Best))
	{
		LastGhostThought = Result;
		bHaveGhostThought = true;
		AnimateMove(Before, Result.Best);
		AfterMove();
	}
}

// =====================================================================================
// SITTING DOWN AND GETTING UP

void AChessTable::Interact_Implementation(AActor* Interactor)
{
	if (bPlaying)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (PC)
	{
		StartPlaying(PC);
	}
}

FText AChessTable::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Chess", "Prompt", "[E] sit down and play chess");
}

void AChessTable::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	Selected = -1;
	LastMouse = FVector2D(-1.0, -1.0);

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	SyncCamera();
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	// A pointer, for picking up pieces. Game-and-UI so a click in the view is still a key
	// press this actor hears.
	PC->SetShowMouseCursor(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);

	EnableInput(PC);
	BindTableKeys();

	if (!IsPlayersTurn() && !Game.Result().IsOver() && !bGhostThinking)
	{
		StartGhostThinking();
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Chess] sat down (%s)."), *GetActorNameOrLabel());
}

void AChessTable::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	Selected = -1;
	UpdateMarks();
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Chess] got up from the table."));
}

void AChessTable::BindTableKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (AChessTable::*)();
	auto Bind = [this](std::initializer_list<FKey> Keys, FHandler Handler)
	{
		for (const FKey& Key : Keys)
		{
			InputComponent->BindKey(Key, IE_Pressed, this, Handler);
		}
	};
	Bind({ EKeys::LeftMouseButton }, &AChessTable::OnClick);
	Bind({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }, &AChessTable::OnCursorLeft);
	Bind({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }, &AChessTable::OnCursorRight);
	Bind({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }, &AChessTable::OnCursorUp);
	Bind({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }, &AChessTable::OnCursorDown);
	Bind({ EKeys::SpaceBar, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom }, &AChessTable::OnSelectKey);
	Bind({ EKeys::U, EKeys::BackSpace, EKeys::Gamepad_FaceButton_Left }, &AChessTable::OnTakeBack);
	Bind({ EKeys::R, EKeys::Gamepad_Special_Right }, &AChessTable::OnNewGameKey);
	Bind({ EKeys::C, EKeys::Gamepad_FaceButton_Top }, &AChessTable::OnSwapSides);
	Bind({ EKeys::One }, &AChessTable::OnLevel1);
	Bind({ EKeys::Two }, &AChessTable::OnLevel2);
	Bind({ EKeys::Three }, &AChessTable::OnLevel3);
	Bind({ EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right }, &AChessTable::OnLeave);
}

void AChessTable::OnClick()
{
	const int32 Square = SquareUnderMouse();
	if (Square >= 0)
	{
		bMouseOwnsCursor = true;
		Cursor = Square;
		SelectOrMove(Square);
	}
	else
	{
		Selected = -1;   // a click off the board puts the piece back down
	}
}

void AChessTable::OnSelectKey()   { SelectOrMove(Cursor); }
void AChessTable::OnLeave()       { StopPlaying(); }
void AChessTable::OnNewGameKey()  { NewGame(); }
void AChessTable::OnLevel1()      { GhostLevel = EChessGhostLevel::Beginner; }
void AChessTable::OnLevel2()      { GhostLevel = EChessGhostLevel::Hustler; }
void AChessTable::OnLevel3()      { GhostLevel = EChessGhostLevel::Master; }

// The arrows move the cursor as he sees the board, whichever side he sits on.
void AChessTable::OnCursorLeft()  { MoveCursor(PlayerColor == EChessColor::White ? -1 : 1, 0); }
void AChessTable::OnCursorRight() { MoveCursor(PlayerColor == EChessColor::White ? 1 : -1, 0); }
void AChessTable::OnCursorUp()    { MoveCursor(0, PlayerColor == EChessColor::White ? 1 : -1); }
void AChessTable::OnCursorDown()  { MoveCursor(0, PlayerColor == EChessColor::White ? -1 : 1); }

void AChessTable::MoveCursor(int32 DFile, int32 DRank)
{
	bMouseOwnsCursor = false;
	const int32 File = FMath::Clamp((Cursor & 7) + DFile, 0, 7);
	const int32 Rank = FMath::Clamp((Cursor >> 3) + DRank, 0, 7);
	Cursor = Rank * 8 + File;
}

void AChessTable::SelectOrMove(int32 Square)
{
	if (!IsPlayersTurn())
	{
		return;
	}
	const FChessPiece Piece = Game.Position().PieceAt(Square);
	if (!Piece.IsEmpty() && Piece.Color == PlayerColor)
	{
		Selected = Selected == Square ? -1 : Square;   // pick it up, or put it back
		return;
	}
	if (Selected >= 0)
	{
		TryPlayerMove(Selected, Square);
	}
}

void AChessTable::OnTakeBack()
{
	// Back to his last move: the ghost's answer goes too, or its thinking stops.
	if (bGhostThinking)
	{
		++ThinkTicket;
		bGhostThinking = false;
		Game.Undo();
	}
	else if (Game.PliesPlayed() > 0)
	{
		Game.Undo();
		if (Game.SideToMove() != PlayerColor && Game.PliesPlayed() > 0)
		{
			Game.Undo();
		}
	}
	bHaveGhostThought = false;
	Selected = -1;
	ResetPieces();
	if (!IsPlayersTurn() && !Game.Result().IsOver())
	{
		StartGhostThinking();   // he played Black and took back the whole game
	}
}

void AChessTable::OnSwapSides()
{
	PlayerColor = ChessOpponent(PlayerColor);
	Cursor = PlayerColor == EChessColor::White ? 12 : 52;   // e2 or e7
	SyncCamera();
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);
	}
	NewGame();
}

// =====================================================================================
// EVERY FRAME

void AChessTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	PollGhost(DeltaSeconds);
	UpdatePieces(DeltaSeconds);

	if (bPlaying)
	{
		if (APlayerController* PC = PlayingPC.Get())
		{
			float X = 0.0f;
			float Y = 0.0f;
			if (PC->GetMousePosition(X, Y) && FVector2D(X, Y) != LastMouse)
			{
				LastMouse = FVector2D(X, Y);
				const int32 Square = SquareUnderMouse();
				if (Square >= 0)
				{
					bMouseOwnsCursor = true;
					Cursor = Square;
				}
			}
		}
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
		UpdateMarks();
		RefreshPanel();
	}
}

int32 AChessTable::SquareUnderMouse() const
{
	const APlayerController* PC = PlayingPC.Get();
	FVector Origin;
	FVector Direction;
	if (!PC || !PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return -1;
	}
	const FTransform& Frame = GetActorTransform();
	const FVector O = Frame.InverseTransformPosition(Origin);
	const FVector D = Frame.InverseTransformVector(Direction);
	if (FMath::Abs(D.Z) < 1e-6)
	{
		return -1;
	}
	const double T = -O.Z / D.Z;
	if (T < 0.0)
	{
		return -1;
	}
	const FVector P = O + D * T;
	const int32 Rank = FMath::FloorToInt32(P.X / SquarePitch + 4.0);
	const int32 File = FMath::FloorToInt32(FileSign * P.Y / SquarePitch + 4.0);
	return (Rank >= 0 && Rank < 8 && File >= 0 && File < 8) ? Rank * 8 + File : -1;
}

void AChessTable::UpdateMarks()
{
	using namespace ChessTableDetail;
	const double Size = SquarePitch * 0.97 / ShapeSize;
	auto Show = [this, Size](int32 Mark, int32 Square)
	{
		UStaticMeshComponent* M = SquareMarks.IsValidIndex(Mark) ? SquareMarks[Mark].Get() : nullptr;
		if (!M)
		{
			return;
		}
		const bool bShow = bPlaying && Square >= 0;
		M->SetVisibility(bShow);
		if (bShow)
		{
			// Stacked a hair apart so two marks on one square do not flicker through each other.
			M->SetRelativeLocationAndRotation(SquareLocal(Square) + FVector(0.0, 0.0, MarkLift + 0.01 * Mark), FRotator::ZeroRotator);
			M->SetRelativeScale3D(FVector(Size, Size, 1.0));
		}
	};

	const FChessMove* Last = Game.LastMove();
	Show(LastFrom, Last ? Last->From : -1);
	Show(LastTo, Last ? Last->To : -1);
	Show(Picked, Selected);
	Show(CursorMark, IsPlayersTurn() ? Cursor : -1);
	const FChessPosition& P = Game.Position();
	Show(Check, P.InCheck() ? P.KingSquare[P.SideToMove == EChessColor::White ? 0 : 1] : -1);

	int32 Used = 0;
	if (bPlaying && Selected >= 0)
	{
		for (const FChessMove& Move : Game.LegalMoves())
		{
			if (Move.From != Selected || (Move.Promotion != EChessPieceType::None && Move.Promotion != EChessPieceType::Queen))
			{
				continue;
			}
			if (UStaticMeshComponent* Dot = MoveDots.IsValidIndex(Used) ? MoveDots[Used].Get() : nullptr)
			{
				// A ring for a capture would be nicer; a bigger dot says the same.
				const double D = SquarePitch * (Move.IsCapture() ? 0.55 : 0.3) / ShapeSize;
				Dot->SetRelativeLocation(SquareLocal(Move.To) + FVector(0.0, 0.0, MarkLift + 0.2));
				Dot->SetRelativeScale3D(FVector(D, D, 0.004));
				Dot->SetVisibility(true);
				++Used;
			}
		}
	}
	for (int32 I = Used; I < MoveDots.Num(); ++I)
	{
		if (MoveDots[I])
		{
			MoveDots[I]->SetVisibility(false);
		}
	}
}

void AChessTable::SyncCamera()
{
	if (!TableCamera)
	{
		return;
	}
	// Seated behind his own pieces, looking down the board.
	const double Side = PlayerColor == EChessColor::White ? -1.0 : 1.0;
	const FVector Eye(Side * CameraBack, 0.0, CameraHeight);
	const FVector Look(Side * SquarePitch * 0.6, 0.0, 0.0);
	TableCamera->SetRelativeLocationAndRotation(Eye, (Look - Eye).Rotation());
	TableCamera->SetFieldOfView(CameraFov);
}

bool AChessTable::IsCodeVisionOn() const
{
	const APlayerController* PC = PlayingPC.Get();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UCodeVisionComponent* Vision = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;
	return Vision && Vision->IsCodeVisionActive();
}

FString AChessTable::LineInSan(const FChessSearchResult& Result) const
{
	FString Text;
	FChessPosition P = GhostThoughtFrom;
	for (const FChessMove& Move : Result.Line)
	{
		FChessMove Legal;
		if (!P.FindMove(Move.From, Move.To, Move.Promotion, Legal))
		{
			break;
		}
		Text += (Text.IsEmpty() ? TEXT("") : TEXT(" ")) + FString(UTF8_TO_TCHAR(P.ToSan(Legal).c_str()));
		P = P.Played(Legal);
	}
	return Text;
}

void AChessTable::RefreshPanel()
{
	ASibeliusHUD* Hud = GetHud();
	if (!Hud)
	{
		return;
	}
	using namespace ChessTableDetail;

	const FChessResult R = Game.Result();
	FString Headline;
	if (R.IsOver())
	{
		Headline = OutcomeText(R, PlayerColor) + TEXT("   [R] again");
	}
	else if (bGhostThinking)
	{
		Headline = TEXT("The ghost is thinking...");
	}
	else
	{
		Headline = Game.Position().InCheck() ? TEXT("Check! Your move.") : TEXT("Your move.");
	}

	FString Status = Headline + FString::Printf(TEXT("\nYou play %s. The ghost plays like %s."),
		PlayerColor == EChessColor::White ? TEXT("White") : TEXT("Black"), LevelName(GhostLevel));
	const std::string Moves = Game.MoveListText(5);
	if (!Moves.empty())
	{
		Status += TEXT("\n") + FString(UTF8_TO_TCHAR(Moves.c_str()));
	}

	// CODE VISION: the ghost's thinking, made legible - the line it expects, and how it
	// rates its chances.
	if (IsCodeVisionOn() && bHaveGhostThought)
	{
		const FChessSearchResult& G = LastGhostThought;
		FString Rating;
		if (G.IsMate())
		{
			Rating = G.Score > 0 ? FString::Printf(TEXT("it sees mate in %d"), (G.MateInPlies() + 1) / 2)
				: FString::Printf(TEXT("it sees itself mated in %d"), (G.MateInPlies() + 1) / 2);
		}
		else
		{
			Rating = FString::Printf(TEXT("it rates itself %+.1f pawns"), G.Score / 100.0);
		}
		Status += FString::Printf(TEXT("\nCODE VISION: the ghost played %s, %s."), *LineInSan(G), *Rating);
	}

	const FString Controls = TEXT("Click a piece, then its square   (or arrows and Enter)\n")
		TEXT("[U] take back   [R] new game   [C] swap sides   [1] [2] [3] the ghost's strength   hold [V] to read its mind\n")
		TEXT("[ESC] walk away   [O] back to the office");
	Hud->ShowTablePanel(Status, Controls);
}

ASibeliusHUD* AChessTable::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
