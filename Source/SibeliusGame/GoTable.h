// GoTable.h
//
// THE GO TABLE — beside the chess table in the Transhuman Chess Park (L_Chess) (Walt,
// 2026-10-10: "what about Go? That was a famous win for AI against humans"; "yes, build Go on
// a 9x9 board"). Leonard sits down, and the park's ghost plays him at Go as well.
//
// ===========================================================================
// THE HOUSE PATTERN (AChessTable): the game is FGoGame (GoEngine.h), a pure model tested
// standalone; this actor only presents it. It takes his input and the view target while he
// plays and gives both back in one place (StopPlaying); components are built in the
// constructor so the table can be seen and placed; interaction goes through IInteractable;
// text goes to the HUD's table panel.
//
// THE BOARD IS BUILT HERE, from the engine's own basic shapes: a block of wood, the lines, the
// star points and a stone for every point, sized from GridPitch to a real board's proportions.
// No purchased asset, so nothing in it carries an "Allows usage with AI: No", and it can be
// rendered and fixed by eye. M_GoSurface (Tools/Scripts/make_go_assets.py) gives the wood its
// grain; without it the board falls back to the engine's BasicShapeMaterial.
//
// 19x19, A FULL GAME, is how it starts: the real game, counted with the Chinese rules and komi
// 7.5, the count marking every point's owner on the board. [B] switches to 13x13 or 9x9, and
// [M] to first capture wins - the way children are taught, two minutes a game.
//
// THE GHOST THINKS ON A WORKER THREAD, like the chess ghost: FGoGame::Think has no shared
// state, runs on a copy, and a take-back or new game while it thinks simply discards the
// answer (ThinkTicket). THE COACH ([H], [K]) is the chess coach's pattern: a hint ready
// before he asks, and a review after his move that speaks only when the move cost him.
//
// Frame: the actor's origin is the centre of the playing surface. Local +X points from
// Black's side to White's (up the rows, 1 to 9); +Y from column A to column J (Black's right).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Async/Future.h"
#include "Interactable.h"
#include "GoEngine.h"
#include "GoTable.generated.h"

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
enum class EGoGhostLevel : uint8
{
	Beginner,
	Hustler,
	Master,
};

UCLASS()
class SIBELIUSGAME_API AGoTable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGoTable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;

	/** In the editor: lay out the stones of a short sample game, to judge the look of the board
	    with stones on it (and render it). Play throws it away: every game starts empty. */
	UFUNCTION(CallInEditor, Category = "Go")
	void PreviewStones();

	UFUNCTION(CallInEditor, Category = "Go")
	void ClearPreview();

	/** The biggest board: every board's parts are made for this one, and a smaller board hides
	    what it does not use. */
	static constexpr int32 MaxLines = 19;
	static constexpr int32 MaxPoints = MaxLines * MaxLines;

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The board: a block of wood, its top the playing surface at the actor's origin. */
	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UStaticMeshComponent> BoardBlock;

	// THE PART LISTS ARE TRANSIENT: the constructor builds them, and a placed table's saved copy
	// must never replace them. Saved, they did: the table placed when boards were 9x9 loaded back
	// its 81-stone list over the 361-stone one, and only the bottom five rows showed stones
	// (2026-10-10). The parts themselves are still the table's subobjects, and still saved.

	/** The lines: [0..18] run along X (one per column), [19..37] along Y (one per row). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Go")
	TArray<TObjectPtr<UStaticMeshComponent>> GridLines;

	/** The star points: nine on 19x19 (the 4-4 points, the sides' middles, the centre), five on 13x13 and 9x9. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Go")
	TArray<TObjectPtr<UStaticMeshComponent>> StarPoints;

	/** One per point, always: a stone appears, drops, and shrinks away when it is taken. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Go")
	TArray<TObjectPtr<UStaticMeshComponent>> Stones;

	/** What the interaction trace lands on. Blocks only Visibility: he can still walk up. */
	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UBoxComponent> Seat;

	/** The point under the cursor, the last stone played, and the coach's suggestion. */
	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UStaticMeshComponent> CursorMark;

	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UStaticMeshComponent> LastMark;

	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UStaticMeshComponent> HintMark;

	/** One small square per point: who owns it at the count, the ghost's estimate under Code
	    Vision, and a red warning on a stone in atari. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Go")
	TArray<TObjectPtr<UStaticMeshComponent>> PointMarks;

	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UCameraComponent> TableCamera;

	/** A lamp over the board, for a level whose light is baked: moving stones would get none of it.
	    The chess park's light reaches them, so it is off there (BoardLightLumens). */
	UPROPERTY(VisibleAnywhere, Category = "Go")
	TObjectPtr<UPointLightComponent> BoardLight;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Go")
	EGoGhostLevel GhostLevel = EGoGhostLevel::Hustler;

	/** 19, 13 or 9 lines. 19x19 is the real game (Walt, 2026-10-10: "I feel like the game should be
	    regulation sized... I want it to feel like the real deal"); [B] switches between them. */
	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "9", ClampMax = "19"))
	int32 BoardSize = 19;

	/** First capture wins (the beginners' game), or a full game counted at the end. */
	UPROPERTY(EditAnywhere, Category = "Go")
	bool bFirstCaptureWins = false;

	/** Points White gets for moving second, in a full game. */
	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.0"))
	float Komi = 7.5f;

	/** Centimetres between lines, across: a real board's 22 mm. Front to back they are 23.7 mm -
	    a real board is not square, so that seen from the player's seat it looks square - and the
	    smaller boards keep the same spacing, so they are smaller boards, as real ones are. */
	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "1.0"))
	float GridPitch = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.5"))
	float BoardThickness = 4.5f;

	/** The ghost never answers sooner than this, however fast it found the move. */
	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.0"))
	float GhostMinThinkSeconds = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.05"))
	float MoveSeconds = 0.22f;

	/** The seated view for a board 40 cm deep: how far back from its centre, how high, and the
	    lens; scaled for the board in play. High enough that the far rows of a 19x19 board can be
	    read and clicked, and far enough that the first and last rows clear the panel's strips. */
	UPROPERTY(EditAnywhere, Category = "Go")
	float CameraBack = 46.0f;

	UPROPERTY(EditAnywhere, Category = "Go")
	float CameraHeight = 96.0f;

	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "20.0", ClampMax = "100.0"))
	float CameraFov = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	/** Off by default: the park's own light falls on the board, and the chess table's 180 lumens,
	    tried first, washed the kaya out to paper. There for a darker place. */
	UPROPERTY(EditAnywhere, Category = "Go", meta = (ClampMin = "0.0"))
	float BoardLightLumens = 0.0f;

	/** The coach speaks up after a costly move. [K] turns it off and on; [H] asks for a hint either way. */
	UPROPERTY(EditAnywhere, Category = "Go")
	bool bCoachWarnings = true;

	/** The board's wood and the stones: M_GoSurface (wood grain; Tools/Scripts/make_go_assets.py),
	    hard-referenced by the class so it cooks. Cleared, it is the engine's BasicShapeMaterial.
	    Either way the colours come from the Color parameter. */
	UPROPERTY(EditAnywhere, Category = "Go")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// --- the board ----------------------------------------------------------------------

	void LayOutBoard();                      // sizes and places every part from GridPitch
	void MakeMaterials();
	FVector PointLocal(int32 Point) const;
	int32 PointUnderMouse() const;
	int32 Lines() const { return Game.Size(); }            // the board in play
	int32 Points() const { return Lines() * Lines(); }
	double PitchAcross() const { return GridPitch; }
	double PitchAlong() const { return GridPitch * (23.7 / 22.0); }
	double BoardDepth() const { return (Lines() - 1) * PitchAlong() + 1.5 * PitchAlong(); }
	double BoardWidth() const { return (Lines() - 1) * PitchAcross() + 1.5 * PitchAlong(); }
	// Black stones are made a little bigger than white ones, as real ones are: white looks bigger.
	double StoneDiameter(EGoColor Color) const { return GridPitch * (Color == EGoColor::White ? 0.975 : 0.99); }
	double StoneHeight() const { return GridPitch * 0.43; }

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;   // the engine's BasicShapeMaterial

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MarkMaterial;       // M_ChessHighlight: translucent, Color and Opacity

	// --- the game -----------------------------------------------------------------------

	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindTableKeys();

	void NewGame();
	void TryPlayerMove(int32 Point);
	void AfterMove(int32 CapturedBefore);
	void StartGhostThinking();
	void PollGhost(float DeltaSeconds);
	bool IsPlayersTurn() const;
	FGoStrength GhostStrength() const;
	int32 TotalPrisoners() const;

	void OnClick();
	void OnCursorLeft();
	void OnCursorRight();
	void OnCursorUp();
	void OnCursorDown();
	void OnPlaceKey();
	void OnPass();
	void OnTakeBack();
	void OnNewGameKey();
	void OnSwapSides();
	void OnModeToggle();
	void OnBoardSizeKey();
	void OnLevel1();
	void OnLevel2();
	void OnLevel3();
	void OnLeave();
	void OnHint();
	void OnCoachToggle();
	void MoveCursor(int32 DX, int32 DY);

	void SyncStones(bool bInstant);
	void UpdateStones(float DeltaSeconds);
	void UpdateMarks();
	void SyncCamera();
	void RefreshPanel();
	FString AtariLine() const;
	bool IsCodeVisionOn() const;
	ASibeliusHUD* GetHud() const;

	FGoGame Game;
	EGoColor PlayerColor = EGoColor::Black;
	int32 Cursor = 180;         // K10, the centre of 19x19; each new game puts it in the middle
	bool bGhostPassed = false;  // its last move was a pass: the panel says so

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	// The ghost.
	TFuture<FGoThought> GhostFuture;
	bool bGhostThinking = false;
	int32 ThinkTicket = 0;
	int32 TicketInFlight = 0;
	float GhostThinkElapsed = 0.0f;
	uint32 GameSeed = 1;                // new each game, so no two games go the same way
	FGoThought LastGhostThought;        // shown under Code Vision
	bool bHaveGhostThought = false;

	// THE COACH: the hint for his move, started as soon as it is his turn so it is ready when
	// he asks, and the review of the move he just made, which reuses that hint when it is for
	// the position he moved from. CoachEpoch moves on at a take-back or a new game.
	void StartHint();
	void StartReview();
	void PollCoach();
	void ForgetCoach();
	bool HintIsCurrent() const;
	FString CoachNote() const;

	TFuture<FGoAdvice> HintFuture;
	bool bHintThinking = false;
	int32 HintEpoch = 0;
	int32 HintMove = -1;                // the move number the hint is for
	FGoAdvice Hint;
	bool bHaveHint = false;
	bool bShowHint = false;             // he pressed [H] on this move

	TFuture<FGoReview> ReviewFuture;
	bool bReviewThinking = false;
	int32 ReviewEpoch = 0;
	FGoReview Review;
	bool bHaveReview = false;

	int32 CoachEpoch = 0;
	float CoachToggleShown = 0.0f;      // seconds left to show "coach warnings on/off"

	// What each point shows, and where it is in its animation.
	struct FStoneView
	{
		EGoColor Color = EGoColor::Empty;   // what the mesh shows, even while it shrinks away
		bool bOnBoard = false;              // false: it was taken and is shrinking
		float Drop = 1.0f;                  // 0..1, landing
		float Grow = 0.0f;                  // 0..1, its size
	};
	TArray<FStoneView> Views;
	TArray<int8> ShownMark;                // what each point mark shows now, so materials change only when it does
	float PlaceSoundIn = -1.0f;            // the stone lands when its drop ends
	float CaptureSoundIn = -1.0f;
	FVector2D LastMouse = FVector2D(-1.0, -1.0);

	// Shared materials: the stones, the wood, the lines, and one per kind of mark
	// (GoTableDetail::EMark says which is which).
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BlackStoneMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WhiteStoneMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LineMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkMaterials;
};
