// ChessTable.h
//
// THE CHESS TABLE — in the Transhuman Chess Park (L_Chess), reached by the CHESS door in the
// living room, beside the bowling door (Walt, 2026-09-28: "five games in the living room
// without having to do the ai agent routine"). Leonard sits down; a ghost plays him.
//
// ===========================================================================
// THE HOUSE PATTERN (ABowlingLane, ABilliardsTable): the game is FChessGame (ChessEngine.h),
// a pure model tested standalone; this actor only presents it. It takes his input and the
// view target while he plays and gives both back in one place (StopPlaying); components are
// built in the constructor so the table can be seen and placed; every mesh is a HARD
// reference so it reaches the pak; interaction goes through IInteractable; text goes to the
// HUD's table panel.
//
// THE PIECES are Tomas Dvorak's free Chess Set (Content/ChessSet, gitignored; imported by
// Tools/Scripts/import_chess_set.py). Its vertices are baked where the vendor stood them, so
// each mesh's bounds say where it came from: FitSet reads the board's orientation, the
// square pitch and the playing surface off the meshes themselves, and each piece is placed
// by the middle of its base. Nothing here is a number measured by hand.
//
// THE GHOST THINKS ON A WORKER THREAD. FChessPosition::Search has no shared state, so it
// runs on a copy of the game, and a take-back or new game while it thinks simply discards
// the answer (ThinkTicket). It waits at least a moment before moving: an instant reply from
// across a park table feels like a machine, which the ghost would rather you forgot.
//
// THE COACH (Walt, 2026-10-09: "so the player can actually learn the game"). [H] lights up
// the move the coach would play and says why, in a sentence ChessAdvise writes from its own
// search: what it wins, what it saves, which threat it stops. After each of his moves the
// coach reviews it (ChessReviewMove) and speaks only when it was a real mistake, naming the
// answer that punishes it and the better move; [U] takes it back, [K] silences the coach.
//
// Frame: the actor's origin is the centre of the board's playing surface. Local +X points
// from White's side to Black's; +Y from the a-file to the h-file (White's right).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Async/Future.h"
#include "Interactable.h"
#include "ChessEngine.h"
#include "ChessTable.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EChessGhostLevel : uint8
{
	Beginner,
	Hustler,
	Master,
};

UCLASS()
class SIBELIUSGAME_API AChessTable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AChessTable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> Board;

	/** What the interaction trace lands on. Blocks only Visibility: he can still walk up. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TObjectPtr<UBoxComponent> Seat;

	/** All 32, always: a captured piece leaves the board, it does not stop existing. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TArray<TObjectPtr<UStaticMeshComponent>> PieceMeshes;

	/** Square markers: the last move (two), the picked-up piece, the cursor, a king in check. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TArray<TObjectPtr<UStaticMeshComponent>> SquareMarks;

	/** Where the picked-up piece may go. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TArray<TObjectPtr<UStaticMeshComponent>> MoveDots;

	/** The coach's suggestion: the piece to move, and where. Their own array, not two more
	    SquareMarks, because the placed table has SquareMarks saved in the level. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TArray<TObjectPtr<UStaticMeshComponent>> HintMarks;

	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TObjectPtr<UCameraComponent> TableCamera;

	/** A lamp over the board. The park's light is baked into its fixed meshes and the
	    pieces MOVE, so, like the bowling pins, they would get none of it. */
	UPROPERTY(VisibleAnywhere, Category = "Chess")
	TObjectPtr<UPointLightComponent> BoardLight;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Chess")
	EChessGhostLevel GhostLevel = EChessGhostLevel::Hustler;

	/** The ghost never answers sooner than this, however fast it found the move. */
	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "0.0"))
	float GhostMinThinkSeconds = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "0.05"))
	float MoveSeconds = 0.35f;

	/** The seated view: how far back from the board centre, how high, and the lens. */
	UPROPERTY(EditAnywhere, Category = "Chess")
	float CameraBack = 52.0f;

	UPROPERTY(EditAnywhere, Category = "Chess")
	float CameraHeight = 58.0f;

	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "20.0", ClampMax = "100.0"))
	float CameraFov = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "0.0"))
	float BoardLightLumens = 180.0f;

	/** The coach speaks up after a bad move. [K] turns it off and on; [H] asks for a hint either way. */
	UPROPERTY(EditAnywhere, Category = "Chess")
	bool bCoachWarnings = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// --- the set ------------------------------------------------------------------------

	/** Reads the board's orientation, pitch and surface off the vendor meshes. */
	void FitSet();
	UStaticMesh* MeshFor(EChessPieceType Type, EChessColor Color) const;
	FVector SquareLocal(int32 Square) const;
	FVector GraveyardLocal(EChessColor Color, int32 Index) const;
	void PlaceMesh(UStaticMeshComponent* Mesh, const FVector& BaseCentre) const;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> SetMeshes;   // [Color * 6 + Type - 1], filled in the constructor

	FQuat SetRotation = FQuat::Identity;   // turns the vendor's frame into ours
	FVector GridCentre = FVector::ZeroVector;   // the board's playing-surface centre, vendor frame
	double SquarePitch = 5.7;
	double FileSign = 1.0;                 // -1 if the import mirrored the set
	double BoardHalf = 26.5;               // the board's half width, frame included
	double BoardUnderside = -2.4;          // the board's bottom, below the playing surface
	TArray<FVector> SetBaseCentres;        // each set mesh's base centre, in its own space

	// --- the game -----------------------------------------------------------------------

	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindTableKeys();

	void NewGame();
	void ResetPieces();                      // puts every mesh where the position says
	void AnimateMove(const FChessPosition& Before, const FChessMove& Move);
	void TryPlayerMove(int32 From, int32 To);
	void AfterMove();
	void StartGhostThinking();
	void PollGhost(float DeltaSeconds);
	bool IsPlayersTurn() const;
	FChessStrength GhostStrength() const;

	void OnClick();
	void OnCursorLeft();
	void OnCursorRight();
	void OnCursorUp();
	void OnCursorDown();
	void OnSelectKey();
	void OnTakeBack();
	void OnNewGameKey();
	void OnSwapSides();
	void OnLevel1();
	void OnLevel2();
	void OnLevel3();
	void OnLeave();
	void OnHint();
	void OnCoachToggle();
	void MoveCursor(int32 DFile, int32 DRank);
	void SelectOrMove(int32 Square);

	int32 SquareUnderMouse() const;
	void UpdatePieces(float DeltaSeconds);
	void UpdateMarks();
	void SyncCamera();
	void RefreshPanel();
	FString LineInSan(const FChessSearchResult& Result) const;
	bool IsCodeVisionOn() const;
	ASibeliusHUD* GetHud() const;

	FChessGame Game;
	EChessColor PlayerColor = EChessColor::White;
	int32 Selected = -1;
	int32 Cursor = 12;          // e2
	bool bMouseOwnsCursor = true;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	// The ghost.
	TFuture<FChessSearchResult> GhostFuture;
	bool bGhostThinking = false;
	int32 ThinkTicket = 0;
	float GhostThinkElapsed = 0.0f;
	int32 TicketInFlight = 0;
	uint32 GameSeed = 1;                   // new each game, so no two games go the same way
	FChessSearchResult LastGhostThought;   // shown under Code Vision
	FChessPosition GhostThoughtFrom;       // the position it was thinking about
	bool bHaveGhostThought = false;

	// THE COACH (2026-10-09). Two searches of its own, beside the ghost's: the hint for his
	// move, started the moment it is his turn so it is ready when he asks, and the review of
	// the move he just made, which reuses that hint's search when it finished in time. Both
	// run on copies, like the ghost; CoachEpoch moves on at a take-back or a new game, and
	// anything the coach was still working out for the old position is thrown away.
	void StartHint();
	void StartReview();
	void PollCoach();
	void ForgetCoach();
	bool HintIsCurrent() const;
	FString CoachNote() const;

	TFuture<FChessAdvice> HintFuture;
	bool bHintThinking = false;
	int32 HintEpoch = 0;
	int32 HintPly = -1;                    // the ply the hint is for
	FChessAdvice Hint;
	bool bHaveHint = false;
	bool bShowHint = false;                // he pressed [H] on this move

	TFuture<FChessReview> ReviewFuture;
	bool bReviewThinking = false;
	int32 ReviewEpoch = 0;
	FChessReview Review;
	bool bHaveReview = false;

	int32 CoachEpoch = 0;
	float CoachToggleShown = 0.0f;         // seconds left to show "coach warnings on/off"

	float SoundCountdown = -1.0f;          // the piece lands when its move ends
	bool bSoundIsCapture = false;
	FVector2D LastMouse = FVector2D(-1.0, -1.0);

	// Where each mesh is and where it is going.
	struct FPieceAnim
	{
		int32 Square = -1;                  // -1 = off the board
		EChessPieceType Type = EChessPieceType::None;
		EChessColor Color = EChessColor::White;
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		float T = 1.0f;                     // 0..1 along the current move
		float Lift = 0.0f;                  // how high it arcs
	};
	TArray<FPieceAnim> Anims;
	int32 Captured[2] = { 0, 0 };           // pieces of each colour already off the board

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkMaterials;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HintMaterial;
};
