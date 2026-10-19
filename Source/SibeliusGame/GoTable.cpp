// GoTable.cpp — see the header for what this actor is for and what it must never do.

#include "GoTable.h"

#include "CodeVisionComponent.h"
#include "ProgressionSubsystem.h"   // his wins, for the agents to remember (ChatDeeds.h)
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
namespace GoTableDetail
{
	// The marks, in MarkMaterials order, and their colours (RGB, opacity).
	// The cursor is a see-through stone of his colour, never a disc: a blue disc for the cursor
	// beside a cyan disc for the hint was one mark to the eye, and he clicked the cursor
	// thinking it was the coach's move (Walt, 2026-10-10). The hint is the only disc, and it pulses.
	enum EMark : int32 { MarkGhostBlack, MarkIllegal, MarkLast, MarkHint, MarkAtari, MarkOwnBlack, MarkOwnWhite, MarkVisionBlack,
		MarkVisionWhite, MarkGhostWhite, MarkCount };
	const FLinearColor MarkColors[MarkCount] = {
		FLinearColor(0.02f, 0.02f, 0.03f, 0.50f),   // the cursor, as a see-through black stone
		FLinearColor(1.00f, 0.25f, 0.20f, 0.45f),   // ... red, where he may not play (suicide, the ko)
		FLinearColor(1.00f, 0.30f, 0.15f, 0.90f),   // the last stone played
		FLinearColor(0.10f, 0.95f, 1.00f, 0.60f),   // the coach's point (its opacity pulses)
		FLinearColor(1.00f, 0.10f, 0.08f, 0.85f),   // a stone in atari
		FLinearColor(0.02f, 0.02f, 0.03f, 0.92f),   // Black's at the count
		FLinearColor(0.95f, 0.95f, 0.92f, 0.92f),   // White's at the count
		FLinearColor(0.02f, 0.02f, 0.03f, 0.45f),   // Black's, by the ghost's guess
		FLinearColor(0.95f, 0.95f, 0.92f, 0.45f),   // White's, by the ghost's guess
		FLinearColor(0.95f, 0.95f, 0.92f, 0.55f),   // the cursor, as a see-through white stone
	};

	// LINEAR colours, which is what a material multiplies: the first try took sRGB-looking
	// numbers for linear ones, and the board came out the pale cream of paper.
	const FLinearColor Kaya(0.50f, 0.31f, 0.11f);    // the honey yellow of a kaya board
	const FLinearColor Ink(0.012f, 0.009f, 0.007f);
	const FLinearColor Slate(0.012f, 0.012f, 0.014f);
	const FLinearColor Shell(0.80f, 0.79f, 0.75f);

	constexpr double ShapeSize = 100.0;   // the engine's basic shapes are a metre across
	// The star points: on 19x19 the 4-4 points, the middles of the sides and the centre; on
	// 13x13 the 4-4 points and the centre; on 9x9 the 3-3 points and the centre.
	TArray<int32> StarPointsFor(int32 N)
	{
		TArray<int32> Out;
		const int32 Near = N >= 13 ? 3 : 2;
		const int32 Far = N - 1 - Near;
		const int32 Mid = N / 2;
		const TArray<int32> Along = N >= 19 ? TArray<int32>{ Near, Mid, Far } : TArray<int32>{ Near, Far };
		for (int32 Y : Along)
		{
			for (int32 X : Along)
			{
				Out.Add(X + Y * N);
			}
		}
		if (N < 19)
		{
			Out.Add(Mid + Mid * N);
		}
		return Out;
	}

	// What the coach's sentences call the other player.
	const char* const Opponent = "the ghost";

	// Broken at spaces into lines of at most Width characters: the panel does not wrap.
	FString Wrapped(const FString& Text, int32 Width)
	{
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "), true);
		FString Out;
		FString Line;
		for (const FString& Word : Words)
		{
			if (!Line.IsEmpty() && Line.Len() + 1 + Word.Len() > Width)
			{
				Out += Out.IsEmpty() ? Line : TEXT("\n") + Line;
				Line.Reset();
			}
			Line += Line.IsEmpty() ? Word : TEXT(" ") + Word;
		}
		if (!Line.IsEmpty())
		{
			Out += Out.IsEmpty() ? Line : TEXT("\n") + Line;
		}
		return Out;
	}

	const TCHAR* LevelName(EGoGhostLevel Level)
	{
		switch (Level)
		{
		case EGoGhostLevel::Beginner: return TEXT("a beginner");
		case EGoGhostLevel::Master:   return TEXT("a master");
		default:                      return TEXT("a park hustler");
		}
	}

	FString PointText(const FGoGame& Game, int32 Point)
	{
		return FString(UTF8_TO_TCHAR(Game.PointName(Point).c_str()));
	}
}

AGoTable::AGoTable()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMaterial(TEXT("/Game/Chess/M_ChessHighlight.M_ChessHighlight"));
	FallbackMaterial = BasicMaterial.Succeeded() ? BasicMaterial.Object : nullptr;
	MarkMaterial = HighlightMaterial.Succeeded() ? HighlightMaterial.Object : nullptr;

	auto MakePart = [this](const TCHAR* PartName, UStaticMesh* Mesh, bool bShadow) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(PartName);
		Part->SetupAttachment(SceneRoot);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(bShadow);
		Part->SetStaticMesh(Mesh);
		return Part;
	};

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	UStaticMesh* Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	UStaticMesh* Plane = PlaneMesh.Succeeded() ? PlaneMesh.Object : nullptr;

	BoardBlock = MakePart(TEXT("BoardBlock"), Cube, true);

	GridLines.SetNum(2 * MaxLines);
	for (int32 I = 0; I < GridLines.Num(); ++I)
	{
		GridLines[I] = MakePart(*FString::Printf(TEXT("Line%02d"), I), Cube, false);
	}
	StarPoints.SetNum(9);
	for (int32 I = 0; I < StarPoints.Num(); ++I)
	{
		StarPoints[I] = MakePart(*FString::Printf(TEXT("Star%d"), I), Cylinder, false);
	}
	Stones.SetNum(MaxPoints);
	for (int32 I = 0; I < MaxPoints; ++I)
	{
		Stones[I] = MakePart(*FString::Printf(TEXT("Stone%02d"), I), Sphere, true);
		Stones[I]->SetVisibility(false);
	}

	CursorMark = MakePart(TEXT("CursorMark"), Cylinder, false);
	LastMark = MakePart(TEXT("LastMark"), Cylinder, false);
	HintMark = MakePart(TEXT("HintMark"), Cylinder, false);
	PointMarks.SetNum(MaxPoints);
	for (int32 I = 0; I < MaxPoints; ++I)
	{
		PointMarks[I] = MakePart(*FString::Printf(TEXT("PointMark%02d"), I), Plane, false);
	}
	for (UStaticMeshComponent* Mark : { CursorMark.Get(), LastMark.Get(), HintMark.Get() })
	{
		Mark->SetMaterial(0, MarkMaterial);
		Mark->SetVisibility(false);
	}
	for (UStaticMeshComponent* Mark : PointMarks)
	{
		Mark->SetMaterial(0, MarkMaterial);
		Mark->SetVisibility(false);
	}

	Seat = CreateDefaultSubobject<UBoxComponent>(TEXT("Seat"));
	Seat->SetupAttachment(SceneRoot);
	Seat->SetMobility(EComponentMobility::Movable);
	Seat->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Seat->SetCollisionResponseToAllChannels(ECR_Ignore);
	Seat->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Seat->SetGenerateOverlapEvents(false);

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

	Views.SetNum(MaxPoints);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoSurface(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	SurfaceMaterial = GoSurface.Succeeded() ? GoSurface.Object : nullptr;
	Game.Reset(BoardSize, bFirstCaptureWins ? EGoMode::FirstCapture : EGoMode::Full, Komi);
	LayOutBoard();
	SyncCamera();
}

// =====================================================================================
// THE BOARD

FVector AGoTable::PointLocal(int32 Point) const
{
	const int32 N = Lines();
	const double Mid = (N - 1) * 0.5;
	return FVector((Point / N - Mid) * PitchAlong(), (Point % N - Mid) * PitchAcross(), 0.0);
}

void AGoTable::LayOutBoard()
{
	using namespace GoTableDetail;
	const int32 N = Lines();
	const double Across = PitchAcross();
	const double Along = PitchAlong();
	const double SpanAcross = (N - 1) * Across;    // first column's line to the last
	const double SpanAlong = (N - 1) * Along;      // first row's to the last
	const double LineWidth = FMath::Max(0.06, 0.045 * Across);
	const double LineHeight = 0.03;
	const double Mid = (N - 1) * 0.5;

	if (BoardBlock)
	{
		BoardBlock->SetRelativeLocation(FVector(0.0, 0.0, -0.5 * BoardThickness));
		BoardBlock->SetRelativeScale3D(FVector(BoardDepth() / ShapeSize, BoardWidth() / ShapeSize, BoardThickness / ShapeSize));
	}
	for (int32 I = 0; I < GridLines.Num(); ++I)
	{
		UStaticMeshComponent* Line = GridLines[I];
		if (!Line)
		{
			continue;
		}
		const bool bColumn = I < MaxLines;   // a column's line runs along X
		const int32 Index = bColumn ? I : I - MaxLines;
		Line->SetVisibility(Index < N);
		if (Index >= N)
		{
			continue;
		}
		Line->SetRelativeLocation(bColumn ? FVector(0.0, (Index - Mid) * Across, 0.5 * LineHeight)
			: FVector((Index - Mid) * Along, 0.0, 0.5 * LineHeight));
		Line->SetRelativeScale3D(bColumn
			? FVector((SpanAlong + LineWidth) / ShapeSize, LineWidth / ShapeSize, LineHeight / ShapeSize)
			: FVector(LineWidth / ShapeSize, (SpanAcross + LineWidth) / ShapeSize, LineHeight / ShapeSize));
	}
	const TArray<int32> Stars = StarPointsFor(N);
	const double Star = 0.18 * Across;
	for (int32 I = 0; I < StarPoints.Num(); ++I)
	{
		if (!StarPoints[I])
		{
			continue;
		}
		StarPoints[I]->SetVisibility(Stars.IsValidIndex(I));
		if (Stars.IsValidIndex(I))
		{
			StarPoints[I]->SetRelativeLocation(PointLocal(Stars[I]) + FVector(0.0, 0.0, 0.5 * LineHeight + 0.002));
			StarPoints[I]->SetRelativeScale3D(FVector(Star / ShapeSize, Star / ShapeSize, (LineHeight + 0.004) / ShapeSize));
		}
	}
	if (Seat)
	{
		Seat->SetRelativeLocation(FVector::ZeroVector);
		Seat->SetBoxExtent(FVector(0.5 * BoardDepth() + 10.0, 0.5 * BoardWidth() + 10.0, 20.0));
	}
	SyncStones(true);
}

void AGoTable::MakeMaterials()
{
	using namespace GoTableDetail;
	UMaterialInterface* Base = SurfaceMaterial ? SurfaceMaterial.Get() : FallbackMaterial.Get();
	auto Surface = [this, Base](const FLinearColor& Color, float Roughness, float Pattern) -> UMaterialInstanceDynamic*
	{
		UMaterialInstanceDynamic* M = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			M->SetScalarParameterValue(TEXT("PatternAmount"), Pattern);
		}
		return M;
	};
	WoodMaterial = Surface(Kaya, 0.55f, 1.0f);
	LineMaterial = Surface(Ink, 0.8f, 0.0f);
	BlackStoneMaterial = Surface(Slate, 0.62f, 0.0f);   // slate is matte: shiny, it reads as grey
	WhiteStoneMaterial = Surface(Shell, 0.30f, 0.0f);

	MarkMaterials.Reset();
	for (int32 I = 0; I < MarkCount; ++I)
	{
		UMaterialInstanceDynamic* M = MarkMaterial ? UMaterialInstanceDynamic::Create(MarkMaterial, this) : nullptr;
		if (M)
		{
			const FLinearColor& C = MarkColors[I];
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(C.R, C.G, C.B, 1.0f));
			M->SetScalarParameterValue(TEXT("Opacity"), C.A);
		}
		MarkMaterials.Add(M);
	}

	if (BoardBlock)
	{
		BoardBlock->SetMaterial(0, WoodMaterial);
	}
	for (UStaticMeshComponent* Line : GridLines)
	{
		if (Line)
		{
			Line->SetMaterial(0, LineMaterial);
		}
	}
	for (UStaticMeshComponent* Star : StarPoints)
	{
		if (Star)
		{
			Star->SetMaterial(0, LineMaterial);
		}
	}
	if (CursorMark && Stones.Num() > 0 && Stones[0])
	{
		CursorMark->SetStaticMesh(Stones[0]->GetStaticMesh());   // a stone's shape, not a disc
		CursorMark->SetMaterial(0, MarkMaterials[MarkGhostBlack]);
	}
	if (LastMark)
	{
		LastMark->SetMaterial(0, MarkMaterials[MarkLast]);
	}
	if (HintMark)
	{
		HintMark->SetMaterial(0, MarkMaterials[MarkHint]);
	}
}

void AGoTable::PreviewStones()
{
	// A typical opening for each size - the corners first on the big boards, the centre on
	// 9x9 - so the preview has both colours, groups touching, and a stone in atari.
	static const char* const Big[] = { "Q16", "D4", "Q4", "D16", "C14", "F17", "R6", "O3", "K16", "C6", "E3", "Q10",
		"P17", "R17", "Q17", "R16", "R15", "S15", "F4" };
	static const char* const Middle[] = { "K10", "D4", "K4", "D10", "F3", "C6", "J7", "G10", "L7" };
	static const char* const Small[] = { "E5", "C4", "G4", "F6", "F5", "G6", "D6", "C6", "D7", "H5", "H4", "C7", "C8", "B8",
		"D8", "G5", "H6", "H7", "J6", "J5", "G7" };
	Game.Reset(BoardSize, EGoMode::Full, Komi);
	const char* const* Moves = Lines() >= 19 ? Big : Lines() >= 13 ? Middle : Small;
	const int32 Count = static_cast<int32>(Lines() >= 19 ? UE_ARRAY_COUNT(Big) : Lines() >= 13 ? UE_ARRAY_COUNT(Middle) : UE_ARRAY_COUNT(Small));
	for (int32 I = 0; I < Count; ++I)
	{
		const char* Text = Moves[I];
		const int32 X = Text[0] - 'A' - (Text[0] > 'I' ? 1 : 0);
		const int32 Y = FCStringAnsi::Atoi(Text + 1) - 1;
		Game.Play(X + Y * Lines());
	}
	LayOutBoard();
	MakeMaterials();
	SyncStones(true);
	SyncCamera();
}

void AGoTable::ClearPreview()
{
	Game.Reset(BoardSize, bFirstCaptureWins ? EGoMode::FirstCapture : EGoMode::Full, Komi);
	LayOutBoard();
	SyncCamera();
}

void AGoTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BoardSize = BoardSize >= 19 ? 19 : BoardSize >= 13 ? 13 : 9;
	if (Game.Size() != BoardSize)
	{
		Game.Reset(BoardSize, bFirstCaptureWins ? EGoMode::FirstCapture : EGoMode::Full, Komi);
	}
	LayOutBoard();
	MakeMaterials();
	SyncStones(true);
	SyncCamera();
}

void AGoTable::BeginPlay()
{
	Super::BeginPlay();
	LayOutBoard();
	MakeMaterials();
	if (BoardLight)
	{
		BoardLight->SetIntensity(BoardLightLumens);
		BoardLight->SetVisibility(BoardLightLumens > 0.0f);
	}
	NewGame();
}

void AGoTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();   // whatever tore this down, he gets his controls and his eyes back
	++ThinkTicket;   // and an answer still on its way is thrown away
	ForgetCoach();
	Super::EndPlay(EndPlayReason);
}

// =====================================================================================
// THE GAME

void AGoTable::NewGame()
{
	++ThinkTicket;
	bGhostThinking = false;
	bHaveGhostThought = false;
	bGhostPassed = false;
	BoardSize = BoardSize >= 19 ? 19 : BoardSize >= 13 ? 13 : 9;
	Game.Reset(BoardSize, bFirstCaptureWins ? EGoMode::FirstCapture : EGoMode::Full, Komi);
	ForgetCoach();
	GameSeed = static_cast<uint32>(FMath::Rand()) | 1u;
	Cursor = Points() / 2;
	LayOutBoard();   // the board may have changed size
	SyncCamera();
	if (!IsPlayersTurn() && !Game.Result().IsOver())
	{
		StartGhostThinking();
	}
	StartHint();
}

bool AGoTable::IsPlayersTurn() const
{
	return Game.ToMove() == PlayerColor && !Game.Result().IsOver();
}

int32 AGoTable::TotalPrisoners() const
{
	return Game.Board().Prisoners(EGoColor::Black) + Game.Board().Prisoners(EGoColor::White);
}

FGoStrength AGoTable::GhostStrength() const
{
	FGoStrength S;
	switch (GhostLevel)
	{
	case EGoGhostLevel::Beginner: S = FGoStrength::Beginner(); break;
	case EGoGhostLevel::Master:   S = FGoStrength::Master(); break;
	default:                      S = FGoStrength::Hustler(); break;
	}
	S.Seed = GameSeed + static_cast<uint32>(Game.MovesPlayed()) * 2654435761u;
	return S;
}

void AGoTable::TryPlayerMove(int32 Point)
{
	if (!IsPlayersTurn() || !Game.IsLegal(Point))
	{
		return;
	}
	const int32 Before = TotalPrisoners();
	if (Game.Play(Point))
	{
		bGhostPassed = false;
		StartReview();   // even a last move gets one: a game thrown away is worth a word
		AfterMove(Before);
	}
}

void AGoTable::AfterMove(int32 CapturedBefore)
{
	SyncStones(false);
	if (Game.LastMove() != GoPass)
	{
		PlaceSoundIn = MoveSeconds;
		CaptureSoundIn = TotalPrisoners() > CapturedBefore ? MoveSeconds + 0.07f : -1.0f;
	}
	const FGoResult& R = Game.Result();
	if (R.IsOver())
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Go] over: winner %s by %.1f (outcome %d)  %s"),
			R.Winner == EGoColor::Black ? TEXT("Black") : R.Winner == EGoColor::White ? TEXT("White") : TEXT("nobody"),
			R.Margin, static_cast<int32>(R.Outcome), UTF8_TO_TCHAR(Game.MoveListText().c_str()));
		// A win is kept, so the agents can still remember it (ChatDeeds.h).
		if (R.Winner == PlayerColor)
		{
			if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
			{
				Progression->BumpStat(SibeliusStats::GoWins);
			}
		}
		return;
	}
	if (IsPlayersTurn())
	{
		StartHint();
	}
	else
	{
		StartGhostThinking();
	}
}

void AGoTable::StartGhostThinking()
{
	const int32 Ticket = ++ThinkTicket;
	TicketInFlight = Ticket;
	bGhostThinking = true;
	GhostThinkElapsed = 0.0f;

	// A copy of the game goes to the worker, so nothing it touches is ever touched here.
	const FGoGame Copy = Game;
	const FGoStrength Strength = GhostStrength();
	GhostFuture = Async(EAsyncExecution::ThreadPool, [Copy, Strength]()
	{
		return Copy.Think(Strength);
	});
}

void AGoTable::PollGhost(float DeltaSeconds)
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
	const FGoThought Result = GhostFuture.Get();
	GhostFuture = TFuture<FGoThought>();
	bGhostThinking = false;
	if (TicketInFlight != ThinkTicket)
	{
		return;   // a take-back or a new game happened while it thought
	}
	// It never resigns: like the chess ghost, it leaves the win for him to finish.
	const int32 Before = TotalPrisoners();
	const int32 Move = Game.IsLegal(Result.Move) ? Result.Move : GoPass;
	if (Game.Play(Move))
	{
		LastGhostThought = Result;
		bHaveGhostThought = true;
		bGhostPassed = Move == GoPass;
		AfterMove(Before);
	}
}

// =====================================================================================
// THE COACH

bool AGoTable::HintIsCurrent() const
{
	return HintEpoch == CoachEpoch && HintMove == Game.MovesPlayed();
}

void AGoTable::StartHint()
{
	// Only for a seated player, on his own move, and only once a move.
	if (!bPlaying || !IsPlayersTurn() || (HintIsCurrent() && (bHintThinking || bHaveHint)))
	{
		return;
	}
	bHintThinking = true;
	bHaveHint = false;
	bShowHint = false;
	HintEpoch = CoachEpoch;
	HintMove = Game.MovesPlayed();
	const FGoGame Copy = Game;
	FGoStrength Coach = FGoStrength::Coach();
	Coach.Seed = GameSeed ^ (static_cast<uint32>(HintMove) * 40503u);
	HintFuture = Async(EAsyncExecution::ThreadPool, [Copy, Coach]()
	{
		return Copy.Advise(Coach, GoTableDetail::Opponent);
	});
}

void AGoTable::StartReview()
{
	bHaveReview = false;
	bReviewThinking = false;
	if (!bCoachWarnings || Game.MovesPlayed() == 0)
	{
		return;
	}
	// The hint for the position he moved from is half the review, if it finished in time.
	const bool bHintFits = bHaveHint && HintEpoch == CoachEpoch && HintMove == Game.MovesPlayed() - 1;
	const FGoAdvice Best = bHintFits ? Hint : FGoAdvice();
	const FGoGame Copy = Game;
	FGoStrength Coach = FGoStrength::Coach();
	Coach.Seed = GameSeed ^ (static_cast<uint32>(Game.MovesPlayed()) * 69069u);
	bReviewThinking = true;
	ReviewEpoch = CoachEpoch;
	ReviewFuture = Async(EAsyncExecution::ThreadPool, [Copy, Best, bHintFits, Coach]()
	{
		return Copy.ReviewLastMove(Coach, GoTableDetail::Opponent, bHintFits ? &Best : nullptr);
	});
}

void AGoTable::PollCoach()
{
	if (bHintThinking && HintFuture.IsValid() && HintFuture.IsReady())
	{
		const FGoAdvice Result = HintFuture.Get();
		HintFuture = TFuture<FGoAdvice>();
		bHintThinking = false;
		if (HintIsCurrent())
		{
			Hint = Result;
			bHaveHint = true;
			if (bShowHint)
			{
				UE_LOG(LogSibeliusGame, Display, TEXT("[Go] hint: %s"), UTF8_TO_TCHAR(Hint.Text.c_str()));
			}
		}
	}
	if (bReviewThinking && ReviewFuture.IsValid() && ReviewFuture.IsReady())
	{
		const FGoReview Result = ReviewFuture.Get();
		ReviewFuture = TFuture<FGoReview>();
		bReviewThinking = false;
		if (ReviewEpoch == CoachEpoch && bCoachWarnings)
		{
			Review = Result;
			bHaveReview = Review.bMistake;
			if (bHaveReview)
			{
				UE_LOG(LogSibeliusGame, Display, TEXT("[Go] coach: %s"), UTF8_TO_TCHAR(Review.Text.c_str()));
			}
		}
	}
}

// A take-back or a new game: nothing the coach was saying or working out still applies. The
// searches already running finish on their own and are never read.
void AGoTable::ForgetCoach()
{
	++CoachEpoch;
	bHintThinking = false;
	bHaveHint = false;
	bShowHint = false;
	bReviewThinking = false;
	bHaveReview = false;
	HintFuture = TFuture<FGoAdvice>();
	ReviewFuture = TFuture<FGoReview>();
}

void AGoTable::OnHint()
{
	if (!IsPlayersTurn())
	{
		return;
	}
	StartHint();   // already thinking or thought: this does nothing
	bShowHint = true;
	if (bHaveHint && HintIsCurrent())
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Go] hint: %s"), UTF8_TO_TCHAR(Hint.Text.c_str()));
	}
}

void AGoTable::OnCoachToggle()
{
	bCoachWarnings = !bCoachWarnings;
	if (!bCoachWarnings)
	{
		bHaveReview = false;
		bReviewThinking = false;
	}
	CoachToggleShown = 2.5f;
}

FString AGoTable::CoachNote() const
{
	using namespace GoTableDetail;
	constexpr int32 Width = 70;
	FString Note;
	auto Add = [&Note](const FString& Text)
	{
		Note += Note.IsEmpty() ? Text : TEXT("\n") + Text;
	};

	if (CoachToggleShown > 0.0f)
	{
		Add(bCoachWarnings ? TEXT("COACH: warnings on. I'll speak up after a costly move.")
			: TEXT("COACH: warnings off. [H] still asks me for a hint."));
	}
	if (bHaveReview && bCoachWarnings)
	{
		Add(Wrapped(TEXT("COACH: ") + FString(UTF8_TO_TCHAR(Review.Text.c_str())) + TEXT("  [U] takes it back."), Width));
	}
	if (bShowHint && IsPlayersTurn())
	{
		if (bHaveHint && HintIsCurrent())
		{
			Add(Wrapped(TEXT("HINT: ") + FString(UTF8_TO_TCHAR(Hint.Text.c_str())), Width));
		}
		else
		{
			Add(TEXT("HINT: the coach is thinking..."));
		}
	}
	return Note;
}

// =====================================================================================
// SITTING DOWN AND GETTING UP

void AGoTable::Interact_Implementation(AActor* Interactor)
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

FText AGoTable::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Go", "Prompt", "[E] sit down and play Go");
}

void AGoTable::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	LastMouse = FVector2D(-1.0, -1.0);

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	SyncCamera();
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	// A pointer, for placing stones. Game-and-UI so a click in the view is still a key press
	// this actor hears.
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
	StartHint();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Go] sat down (%s)."), *GetActorNameOrLabel());
}

void AGoTable::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
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
	UE_LOG(LogSibeliusGame, Display, TEXT("[Go] got up from the table."));
}

void AGoTable::BindTableKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (AGoTable::*)();
	auto Bind = [this](std::initializer_list<FKey> Keys, FHandler Handler)
	{
		for (const FKey& Key : Keys)
		{
			InputComponent->BindKey(Key, IE_Pressed, this, Handler);
		}
	};
	Bind({ EKeys::LeftMouseButton }, &AGoTable::OnClick);
	Bind({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }, &AGoTable::OnCursorLeft);
	Bind({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }, &AGoTable::OnCursorRight);
	Bind({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }, &AGoTable::OnCursorUp);
	Bind({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }, &AGoTable::OnCursorDown);
	Bind({ EKeys::SpaceBar, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom }, &AGoTable::OnPlaceKey);
	Bind({ EKeys::P, EKeys::Gamepad_FaceButton_Top }, &AGoTable::OnPass);
	Bind({ EKeys::U, EKeys::BackSpace, EKeys::Gamepad_FaceButton_Left }, &AGoTable::OnTakeBack);
	Bind({ EKeys::R, EKeys::Gamepad_Special_Right }, &AGoTable::OnNewGameKey);
	Bind({ EKeys::C }, &AGoTable::OnSwapSides);
	Bind({ EKeys::M }, &AGoTable::OnModeToggle);
	Bind({ EKeys::B }, &AGoTable::OnBoardSizeKey);
	Bind({ EKeys::One }, &AGoTable::OnLevel1);
	Bind({ EKeys::Two }, &AGoTable::OnLevel2);
	Bind({ EKeys::Three }, &AGoTable::OnLevel3);
	Bind({ EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right }, &AGoTable::OnLeave);
	// H is the dev overlay on foot; seated, the table hears it first and it is the hint.
	Bind({ EKeys::H, EKeys::Gamepad_LeftShoulder }, &AGoTable::OnHint);
	Bind({ EKeys::K }, &AGoTable::OnCoachToggle);
}

void AGoTable::OnClick()
{
	const int32 Point = PointUnderMouse();
	if (Point >= 0)
	{
		Cursor = Point;
		TryPlayerMove(Point);
	}
}

void AGoTable::OnPlaceKey()   { TryPlayerMove(Cursor); }
void AGoTable::OnLeave()      { StopPlaying(); }
void AGoTable::OnNewGameKey() { NewGame(); }
void AGoTable::OnLevel1()     { GhostLevel = EGoGhostLevel::Beginner; }
void AGoTable::OnLevel2()     { GhostLevel = EGoGhostLevel::Hustler; }
void AGoTable::OnLevel3()     { GhostLevel = EGoGhostLevel::Master; }

void AGoTable::OnPass()
{
	if (!IsPlayersTurn())
	{
		return;
	}
	const int32 Before = TotalPrisoners();
	if (Game.Play(GoPass))
	{
		StartReview();
		AfterMove(Before);
	}
}

void AGoTable::OnModeToggle()
{
	bFirstCaptureWins = !bFirstCaptureWins;
	NewGame();
}

// 19x19, then 13x13, then 9x9, and round again: a new game on the new board.
void AGoTable::OnBoardSizeKey()
{
	BoardSize = BoardSize >= 19 ? 13 : BoardSize >= 13 ? 9 : 19;
	NewGame();
}

// The arrows move the cursor as he sees the board, from whichever side he sits.
void AGoTable::OnCursorLeft()  { MoveCursor(PlayerColor == EGoColor::Black ? -1 : 1, 0); }
void AGoTable::OnCursorRight() { MoveCursor(PlayerColor == EGoColor::Black ? 1 : -1, 0); }
void AGoTable::OnCursorUp()    { MoveCursor(0, PlayerColor == EGoColor::Black ? 1 : -1); }
void AGoTable::OnCursorDown()  { MoveCursor(0, PlayerColor == EGoColor::Black ? -1 : 1); }

void AGoTable::MoveCursor(int32 DX, int32 DY)
{
	// The arrow keys, for a keyboard or a gamepad: they start from the centre if the mouse had
	// taken the cursor off the board.
	if (Cursor < 0 || Cursor >= Points())
	{
		Cursor = Points() / 2;
		return;
	}
	const int32 X = FMath::Clamp(Cursor % Lines() + DX, 0, Lines() - 1);
	const int32 Y = FMath::Clamp(Cursor / Lines() + DY, 0, Lines() - 1);
	Cursor = Y * Lines() + X;
}

void AGoTable::OnTakeBack()
{
	// Back to his last move: the ghost's answer goes too, or its thinking stops.
	if (bGhostThinking)
	{
		++ThinkTicket;
		bGhostThinking = false;
		Game.Undo();
	}
	else if (Game.MovesPlayed() > 0)
	{
		Game.Undo();
		if (Game.ToMove() != PlayerColor && Game.MovesPlayed() > 0)
		{
			Game.Undo();
		}
	}
	bHaveGhostThought = false;
	bGhostPassed = false;
	ForgetCoach();
	SyncStones(true);
	if (!IsPlayersTurn() && !Game.Result().IsOver())
	{
		StartGhostThinking();   // he played White and took back the whole game
	}
	StartHint();
}

void AGoTable::OnSwapSides()
{
	PlayerColor = GoOpponent(PlayerColor);
	Cursor = (Lines() * Lines()) / 2;
	SyncCamera();
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);
	}
	NewGame();
}

// =====================================================================================
// EVERY FRAME

void AGoTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	PollGhost(DeltaSeconds);
	PollCoach();
	CoachToggleShown = FMath::Max(0.0f, CoachToggleShown - DeltaSeconds);
	UpdateStones(DeltaSeconds);

	auto Countdown = [this, DeltaSeconds](float& Seconds, ETableSound Sound, float Volume)
	{
		if (Seconds >= 0.0f)
		{
			Seconds -= DeltaSeconds;
			if (Seconds < 0.0f)
			{
				TableSounds::Play(this, Sound, Volume, FMath::FRandRange(0.95f, 1.05f));
			}
		}
	};
	Countdown(PlaceSoundIn, ETableSound::GoPlace, 0.8f);
	Countdown(CaptureSoundIn, ETableSound::GoCapture, 0.7f);

	if (bPlaying)
	{
		if (APlayerController* PC = PlayingPC.Get())
		{
			float X = 0.0f;
			float Y = 0.0f;
			if (PC->GetMousePosition(X, Y) && FVector2D(X, Y) != LastMouse)
			{
				// The see-through stone follows the mouse, and goes when the mouse leaves the board
				// (Walt, 2026-10-10: a mark left sitting on the board read as a hint).
				LastMouse = FVector2D(X, Y);
				Cursor = PointUnderMouse();
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

int32 AGoTable::PointUnderMouse() const
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
	const double Mid = (Lines() - 1) * 0.5;
	const int32 X = FMath::RoundToInt32(P.Y / PitchAcross() + Mid);
	const int32 Y = FMath::RoundToInt32(P.X / PitchAlong() + Mid);
	return (X >= 0 && X < Lines() && Y >= 0 && Y < Lines()) ? Y * Lines() + X : -1;
}

/* What each point's stone should show, from the board. A stone that is new drops in (or is
   simply there, after a new game or a take-back); a stone that has gone shrinks away. */
void AGoTable::SyncStones(bool bInstant)
{
	if (Views.Num() != MaxPoints)
	{
		Views.SetNum(MaxPoints);
	}
	const FGoBoard& B = Game.Board();
	const int32 InPlay = Points();
	for (int32 P = 0; P < MaxPoints; ++P)
	{
		FStoneView& V = Views[P];
		const EGoColor C = P < InPlay ? B.At(P) : EGoColor::Empty;
		UStaticMeshComponent* Mesh = Stones.IsValidIndex(P) ? Stones[P].Get() : nullptr;
		if (C != EGoColor::Empty)
		{
			if (!V.bOnBoard || V.Color != C)
			{
				V.Color = C;
				V.bOnBoard = true;
				V.Drop = bInstant ? 1.0f : 0.0f;
				if (Mesh)
				{
					Mesh->SetMaterial(0, C == EGoColor::Black ? BlackStoneMaterial.Get() : WhiteStoneMaterial.Get());
				}
			}
			V.Grow = 1.0f;
		}
		else if (V.bOnBoard)
		{
			V.bOnBoard = false;
		}
		if (bInstant && !V.bOnBoard)
		{
			V.Grow = 0.0f;
		}
		if (bInstant && Mesh)
		{
			Mesh->SetVisibility(V.Grow > 0.0f);
			if (P < InPlay)
			{
				const double D = StoneDiameter(V.Color);
				Mesh->SetRelativeLocation(PointLocal(P) + FVector(0.0, 0.0, 0.5 * StoneHeight()));
				Mesh->SetRelativeScale3D(FVector(D, D, StoneHeight()) / GoTableDetail::ShapeSize);
			}
		}
	}
}

void AGoTable::UpdateStones(float DeltaSeconds)
{
	const float DropRate = MoveSeconds > 0.0f ? DeltaSeconds / MoveSeconds : 1.0f;
	const float ShrinkRate = DeltaSeconds / 0.35f;
	const int32 InPlay = Points();
	for (int32 P = 0; P < InPlay && P < Views.Num() && P < Stones.Num(); ++P)
	{
		FStoneView& V = Views[P];
		UStaticMeshComponent* Mesh = Stones[P];
		const bool bLanding = V.bOnBoard && V.Drop < 1.0f;
		const bool bShrinking = !V.bOnBoard && V.Grow > 0.0f;
		if (!Mesh || (!bLanding && !bShrinking))
		{
			continue;
		}
		const double D = StoneDiameter(V.Color);
		const FVector Size = FVector(D, D, StoneHeight()) / GoTableDetail::ShapeSize;
		if (bLanding)
		{
			V.Drop = FMath::Min(1.0f, V.Drop + DropRate);
			const float Ease = 1.0f - (1.0f - V.Drop) * (1.0f - V.Drop);
			Mesh->SetVisibility(true);
			Mesh->SetRelativeScale3D(Size);
			Mesh->SetRelativeLocation(PointLocal(P) + FVector(0.0, 0.0, 0.5 * StoneHeight() + 2.5 * StoneHeight() * (1.0f - Ease)));
		}
		else
		{
			V.Grow = FMath::Max(0.0f, V.Grow - ShrinkRate);
			Mesh->SetRelativeScale3D(Size * V.Grow);
			Mesh->SetVisibility(V.Grow > 0.0f);
		}
	}
}

void AGoTable::UpdateMarks()
{
	using namespace GoTableDetail;
	const FGoBoard& B = Game.Board();
	const double Pitch = PitchAcross();
	const double Top = StoneHeight();
	auto Disc = [](UStaticMeshComponent* Mark, bool bShow, const FVector& Where, double Diameter)
	{
		if (!Mark)
		{
			return;
		}
		Mark->SetVisibility(bShow);
		if (bShow)
		{
			Mark->SetRelativeLocation(Where);
			Mark->SetRelativeScale3D(FVector(Diameter / ShapeSize, Diameter / ShapeSize, 0.02 / ShapeSize));
		}
	};
	auto Lift = [&B, Top](int32 Point, double OnBoard) { return B.At(Point) == EGoColor::Empty ? OnBoard : Top + OnBoard; };

	// The cursor: a see-through stone of his colour where his stone would go, red where he may
	// not play (suicide, the ko), and nothing over a stone already there.
	const bool bCursor = bPlaying && IsPlayersTurn() && Cursor >= 0 && Cursor < Points() && B.At(Cursor) == EGoColor::Empty;
	if (CursorMark)
	{
		CursorMark->SetVisibility(bCursor);
		if (bCursor && MarkMaterials.Num() == MarkCount)
		{
			const int32 Kind = !Game.IsLegal(Cursor) ? MarkIllegal : PlayerColor == EGoColor::Black ? MarkGhostBlack : MarkGhostWhite;
			CursorMark->SetMaterial(0, MarkMaterials[Kind]);
			const double D = StoneDiameter(PlayerColor);
			CursorMark->SetRelativeLocation(PointLocal(Cursor) + FVector(0.0, 0.0, 0.5 * StoneHeight()));
			CursorMark->SetRelativeScale3D(FVector(D, D, StoneHeight()) / ShapeSize);
		}
	}

	const int32 LastMove = Game.LastMove();
	const bool bLast = bPlaying && LastMove >= 0 && B.At(LastMove) != EGoColor::Empty;
	Disc(LastMark, bLast, bLast ? PointLocal(LastMove) + FVector(0.0, 0.0, Top + 0.02) : FVector::ZeroVector, Pitch * 0.28);

	const bool bHint = bPlaying && bShowHint && bHaveHint && HintIsCurrent() && IsPlayersTurn() && Hint.Move >= 0;
	Disc(HintMark, bHint, bHint ? PointLocal(Hint.Move) + FVector(0.0, 0.0, 0.07) : FVector::ZeroVector, Pitch * 0.95);
	if (bHint && MarkMaterials.Num() == MarkCount && MarkMaterials[MarkHint] && GetWorld())
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(5.0f * static_cast<float>(GetWorld()->GetTimeSeconds()));
		MarkMaterials[MarkHint]->SetScalarParameterValue(TEXT("Opacity"), 0.30f + 0.45f * Pulse);
	}

	// One square per point: the count when it is over, the ghost's guess under Code Vision,
	// and otherwise a red square on every stone in atari.
	const FGoResult& R = Game.Result();
	const bool bCounted = R.Outcome == EGoOutcome::Counted && R.Count.Owner.size() == static_cast<size_t>(Points());
	const bool bVision = !bCounted && IsCodeVisionOn() && bHaveGhostThought && Game.Mode() == EGoMode::Full
		&& LastGhostThought.Ownership.size() == static_cast<size_t>(Points());
	if (ShownMark.Num() != PointMarks.Num())
	{
		ShownMark.Init(-2, PointMarks.Num());
	}
	const int32 InPlay = Points();
	for (int32 P = 0; P < PointMarks.Num(); ++P)
	{
		UStaticMeshComponent* Mark = PointMarks[P];
		if (!Mark || MarkMaterials.Num() != MarkCount)
		{
			continue;
		}
		int32 Kind = -1;
		double Size = 0.42;
		if (P >= InPlay)
		{
			if (ShownMark[P] != -1)
			{
				Mark->SetVisibility(false);
				ShownMark[P] = -1;
			}
			continue;
		}
		const EGoColor Stone = B.At(P);
		if (bCounted)
		{
			const EGoColor Holder = R.Count.Owner[P];
			if (Holder != EGoColor::Empty && (Stone == EGoColor::Empty || R.Count.Dead[P]))
			{
				Kind = Holder == EGoColor::Black ? MarkOwnBlack : MarkOwnWhite;
			}
		}
		else if (bVision)
		{
			const float Own = LastGhostThought.Ownership[P];
			const EGoColor Guess = Own > 0.3f ? EGoColor::Black : Own < -0.3f ? EGoColor::White : EGoColor::Empty;
			if (Guess != EGoColor::Empty && Guess != Stone)
			{
				Kind = Guess == EGoColor::Black ? MarkVisionBlack : MarkVisionWhite;
			}
		}
		else if (Stone != EGoColor::Empty && B.Liberties(P, 2) == 1)
		{
			Kind = MarkAtari;
			Size = 0.3;
		}
		// 361 of these on a big board: touch a mark only when what it shows changes.
		const int32 Shown = bPlaying ? Kind : -1;
		if (Shown == ShownMark[P] && !(Shown >= 0 && Stone != EGoColor::Empty))
		{
			continue;
		}
		ShownMark[P] = static_cast<int8>(Shown);
		Mark->SetVisibility(Shown >= 0);
		if (Shown >= 0)
		{
			Mark->SetMaterial(0, MarkMaterials[Kind]);
			Mark->SetRelativeLocation(PointLocal(P) + FVector(0.0, 0.0, Lift(P, 0.06)));
			Mark->SetRelativeScale3D(FVector(Pitch * Size / ShapeSize, Pitch * Size / ShapeSize, 1.0));
		}
	}
}

void AGoTable::SyncCamera()
{
	if (!TableCamera)
	{
		return;
	}
	// Seated on his own side, looking across the board.
	// CameraBack and CameraHeight are for a board 40 cm deep; a bigger or smaller board moves
	// the seat with it, so every board fills the same part of the screen.
	const double Scale = BoardDepth() / 40.0;
	const double Side = PlayerColor == EGoColor::Black ? -1.0 : 1.0;
	const FVector Eye(Side * CameraBack * Scale, 0.0, CameraHeight * Scale);
	// A little past the board's centre, which sits the board low on the screen: on 19x19 the
	// panel's top strip then has room to grow (a hint, a coach's warning, an atari) before it
	// reaches the far rows.
	const FVector Look(-Side * 1.5 * Scale, 0.0, 0.0);
	TableCamera->SetRelativeLocationAndRotation(Eye, (Look - Eye).Rotation());
	TableCamera->SetFieldOfView(CameraFov);
}

bool AGoTable::IsCodeVisionOn() const
{
	const APlayerController* PC = PlayingPC.Get();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UCodeVisionComponent* Vision = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;
	return Vision && Vision->IsCodeVisionActive();
}

// "Atari!" for his stones first (he can still save them), then the ghost's (he can take them).
FString AGoTable::AtariLine() const
{
	using namespace GoTableDetail;
	const FGoBoard& B = Game.Board();
	FString Mine;
	FString Theirs;
	TArray<bool> Seen;
	Seen.Init(false, Points());
	std::vector<int> Group;
	for (int32 P = 0; P < Points(); ++P)
	{
		const EGoColor C = B.At(P);
		if (C == EGoColor::Empty || Seen[P] || B.Liberties(P, 2) != 1)
		{
			continue;
		}
		B.Group(P, Group);
		for (int S : Group)
		{
			Seen[S] = true;
		}
		// "stone on D5", "stones on D5 and E5", "stones on D5, E5, F5 and 2 more".
		FString Where;
		const int32 Count = static_cast<int32>(Group.size());
		for (int32 I = 0; I < Count && I < 3; ++I)
		{
			const bool bLastNamed = I == Count - 1 || I == 2;
			Where += (I == 0 ? TEXT("") : (bLastNamed && Count <= 3 ? TEXT(" and ") : TEXT(", "))) + PointText(Game, Group[I]);
		}
		if (Count > 3)
		{
			Where += FString::Printf(TEXT(" and %d more"), Count - 3);
		}
		const FString What = FString::Printf(TEXT("%s on %s"), Count == 1 ? TEXT("stone") : TEXT("stones"), *Where);
		if (C == PlayerColor && Mine.IsEmpty())
		{
			Mine = FString::Printf(TEXT("ATARI! Your %s %s one liberty left."), *What, Group.size() == 1 ? TEXT("has") : TEXT("have"));
		}
		else if (C != PlayerColor && Theirs.IsEmpty())
		{
			Theirs = FString::Printf(TEXT("The ghost's %s %s in atari."), *What, Group.size() == 1 ? TEXT("is") : TEXT("are"));
		}
	}
	return Mine.IsEmpty() ? Theirs : (Theirs.IsEmpty() ? Mine : Mine + TEXT("  ") + Theirs);
}

void AGoTable::RefreshPanel()
{
	ASibeliusHUD* Hud = GetHud();
	if (!Hud)
	{
		return;
	}
	using namespace GoTableDetail;

	const bool bCapture = Game.Mode() == EGoMode::FirstCapture;
	const FGoResult& R = Game.Result();
	const bool bWon = R.Winner == PlayerColor;
	FString Headline;
	if (R.Outcome == EGoOutcome::Counted)
	{
		Headline = (R.Winner == EGoColor::Empty ? FString(TEXT("A draw."))
			: bWon ? FString::Printf(TEXT("You win by %.1f!"), R.Margin) : FString::Printf(TEXT("The ghost wins by %.1f."), R.Margin))
			+ FString::Printf(TEXT("  Black %.0f, White %.1f with komi.   [R] again"), R.Count.BlackArea, R.Count.WhiteArea);
	}
	else if (R.Outcome == EGoOutcome::FirstCapture)
	{
		Headline = bWon ? TEXT("You captured first - you win!   [R] again") : TEXT("The ghost captured first - it wins.   [R] again");
	}
	else if (R.Outcome == EGoOutcome::Resignation)
	{
		Headline = bWon ? TEXT("The ghost resigns - you win!   [R] again") : TEXT("You resigned.   [R] again");
	}
	else if (bGhostThinking)
	{
		Headline = TEXT("The ghost is thinking...");
	}
	else if (bGhostPassed)
	{
		Headline = TEXT("The ghost passed. [P] pass too and the board is counted - or play on.");
	}
	else
	{
		Headline = bCapture ? TEXT("Your move. Take one stone and you win.") : TEXT("Your move.");
	}

	// One line for who plays what: on 19x19 every line up here is a row of the board it covers.
	FString Status = Headline + FString::Printf(TEXT("     You: %s on %dx%d, against %s. %s"),
		PlayerColor == EGoColor::Black ? TEXT("Black") : TEXT("White"), Lines(), Lines(), LevelName(GhostLevel),
		bCapture ? TEXT("First capture wins.")
			: *FString::Printf(TEXT("Stones plus surrounded points; komi %.1f to White."), Komi));
	const std::string Moves = Game.MoveListText(5);
	if (!Moves.empty())
	{
		Status += TEXT("\n") + FString(UTF8_TO_TCHAR(Moves.c_str()));
	}
	if (!R.IsOver())
	{
		const FString Atari = AtariLine();
		if (!Atari.IsEmpty())
		{
			Status += TEXT("\n") + Atari;
		}
	}

	// CODE VISION: the ghost's thinking, made legible - its odds, the line it expects, and in
	// a full game the board shaded by who it thinks will own each point.
	if (IsCodeVisionOn() && bHaveGhostThought)
	{
		const FGoThought& G = LastGhostThought;
		FString Line;
		for (size_t I = 0; I < G.Line.size() && I < 6; ++I)
		{
			Line += (Line.IsEmpty() ? TEXT("") : TEXT(" ")) + PointText(Game, G.Line[I]);
		}
		Status += FString::Printf(TEXT("\nCODE VISION: from %d quick games it gave itself %d%% and expected %s.%s"),
			G.Playouts, FMath::RoundToInt32(G.WinRate * 100.0f), *Line,
			bCapture ? TEXT("") : TEXT(" Shaded: who it thinks will own each point."));
	}

	const int32 NextSize = BoardSize >= 19 ? 13 : BoardSize >= 13 ? 9 : 19;   // what [B] switches to
	// Two lines, not four: the strip at the bottom covers the near rows the same way.
	const FString Controls = FString::Printf(TEXT("Click a point to place a stone   [P] pass   [H] hint   [K] coach warnings: %s   [U] take back   [R] new game\n"),
			bCoachWarnings ? TEXT("on") : TEXT("off"))
		+ FString::Printf(TEXT("[B] %dx%d board   [C] swap colours   [M] %s   [1] [2] [3] the ghost's strength   hold [V] read its mind   [ESC] walk away   [O] office"),
			NextSize, NextSize, bCapture ? TEXT("full game") : TEXT("first capture wins"));
	Hud->ShowTablePanel(Status, Controls, 0.25f, CoachNote());
}

ASibeliusHUD* AGoTable::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
