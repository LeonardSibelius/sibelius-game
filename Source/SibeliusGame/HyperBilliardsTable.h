// HyperBilliardsTable.h
//
// 4D BILLIARDS — the table in Aisling's snowfield (Walt, 2026-10-25: "let's do the 4D billiards
// in Aisling's snowfield"). FHyperBilliardsSim is the game, one dimension up from the living
// room's: the cloth is a space with a hidden third direction, W, and the balls are 4D. This
// actor shows the player one 3D SLICE of it, at his W, and lets him move the slice.
//
// ===========================================================================
// WHAT HE SEES
//   - THE TABLE: an ordinary-looking pool table - green cloth, dark wood, thin cyan light along
//     the rails - because what is strange about it is the balls, not the furniture. No pack has
//     a 4D table to buy, so it is built here of the engine's shapes.
//   - THE BALLS, AS THEIR SLICES: each is drawn as its true cross-section at his W - the living
//     room's own ball meshes, scaled. A ball in his slice is whole; one further off in W is
//     smaller, and its centre still sits a full radius above the cloth, because that is where
//     a 4D ball's centre is: a thin slice hangs in the air above the cloth, as the cross-sections
//     in 4D Toys do. One more than a radius away is not there at all.
//   - THE POCKETS, where they are: at the W ends of the box. A pocket mouth opens on the cloth as
//     his slice nears the end of W it belongs to; in the middle of W the cloth has no holes.
//   - THE W-BOARD, standing behind the far rail: the hidden dimension turned upright. Its width
//     is the table's length, lined up with it, so each ball's dot hangs straight above the ball;
//     its height is W. A bright line across it is his slice, the dots are every ball wherever it
//     is in W, the rings at its corners are the pockets, and while he aims a bar shows the shot.
//
// WHAT HE DOES - the living room's controls, and three for the fourth dimension:
//   A D / mouse aim, SHIFT fine; W S spin; hold SPACE and let go to shoot; R rerack; V view down
//   the cue; ESC walk away - as at the living-room table. And: UP / DOWN tilt the shot into W;
//   the mouse wheel (or Q / C) moves his slice through W; F brings his slice back to the cue ball;
//   T aims at the next ball, straight at its centre in all three directions.
//   When a stroke ends, his slice glides back to the cue ball, so every shot starts with it seen.
//
// THE HOUSE LESSONS (BilliardsTable.h, PinballTable.h): the model decides and this actor shows;
// EndPlay restores input and the view target; components are built in the constructor; every
// mesh and material is a HARD reference, so it reaches the pak.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "HyperBilliardsSim.h"
#include "HyperBilliardsTable.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AHyperBilliardsTable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHyperBilliardsTable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;

	bool IsPlaying() const { return bPlaying; }

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The marble dais under it all: its top is the actor's origin, and it reaches well down so
	 *  it meets a sloping snowfield. It keeps the grass, which the landscape grows at runtime,
	 *  from growing through the table. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> Dais;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> Felt;

	/** Long rails at -Y and +Y, short rails at -X and +X. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> Rails;

	/** The cyan light along the inner edge of each rail. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> RailGlows;

	/** A pocket mouth over each of the six places a pocket can open: four corners, two sides. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> PocketMouths;

	/** What the player bumps into and what the interaction trace lands on. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UBoxComponent> Body;

	/** The plane of the cloth, centred on it: the sim's (X, Y). Balls hang off this. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<USceneComponent> Cloth;

	/** Index 0 is the cue ball, 1..10 the numbered balls, as in the sim. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> BallMeshes;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> Cue;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> AimLine;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> ObjectLine;

	/** The W-board's frame: its X is the sim's X, its Z is the sim's W. */
	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<USceneComponent> Board;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> BoardPanel;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> BoardSlice;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UStaticMeshComponent> BoardAim;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> BoardDots;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> BoardPockets;

	UPROPERTY(VisibleAnywhere, Category = "4D Billiards")
	TObjectPtr<UCameraComponent> TableCamera;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "0.2"))
	float PowerChargeSeconds = 1.4f;

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "1.0"))
	float AimDegreesPerSecond = 50.0f;

	/** UP / DOWN tilt the shot into W this fast. */
	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "1.0"))
	float TiltDegreesPerSecond = 30.0f;

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FineAimFraction = 0.12f;

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "0.0"))
	float MouseAimDegreesPerUnit = 0.12f;

	/** One notch of the mouse wheel moves his slice this far through W, cm. */
	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "0.5"))
	float SliceStepPerNotch = 2.5f;

	/** Q / C held move his slice this fast, cm/s. */
	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "1.0"))
	float SliceSpeed = 30.0f;

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	/** The overhead view, from the -Y side: far enough to take in the W-board behind. */
	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "50.0"))
	float OverheadDistance = 330.0f;

	UPROPERTY(EditAnywhere, Category = "4D Billiards", meta = (ClampMin = "10.0", ClampMax = "90.0"))
	float OverheadPitchDegrees = 40.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindTableKeys();

	void OnAimLeftPressed();
	void OnAimLeftReleased();
	void OnAimRightPressed();
	void OnAimRightReleased();
	void OnTiltUpPressed();
	void OnTiltUpReleased();
	void OnTiltDownPressed();
	void OnTiltDownReleased();
	void OnSliceUpPressed();
	void OnSliceUpReleased();
	void OnSliceDownPressed();
	void OnSliceDownReleased();
	void OnWheelUp();
	void OnWheelDown();
	void OnSliceToCue();
	void OnNextBall();
	void OnFinePressed();
	void OnFineReleased();
	void OnMouseX(float Value);
	void OnSpinUp();
	void OnSpinDown();
	void OnShotPressed();
	void OnShotReleased();
	void OnRack();
	void OnToggleView();
	void OnLeave();

	void UpdateAim(float DeltaSeconds);
	void UpdateSlice(float DeltaSeconds);
	void SyncBalls(float DeltaSeconds);
	void SyncPockets();
	void SyncCueAndGuides();
	void SyncBoard();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	void HandleEvents();
	void RefreshPanel();
	float CurrentPower() const;
	void AimAtBall(int32 Ball);

	/** Sim (X, Y) on the cloth, at Height above it, in Cloth space. */
	static FVector SimToCloth(FHyperVec3 P, double Height);
	/** Sim (X, W) on the W-board, just in front of its face, in Board space. */
	static FVector SimToBoard(FHyperVec3 P);

	ASibeliusHUD* GetHud() const;

	FHyperBilliardsSim Sim;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	double Heading = 0.0;    // radians, in the cloth's X-Y
	double Tilt = 0.0;       // radians, into W
	double Spin = 0.0;       // -1 draw .. +1 follow
	double SliceW = 0.0;     // his slice, cm
	double SliceTarget = 0.0;
	bool bAimLeftHeld = false;
	bool bAimRightHeld = false;
	bool bTiltUpHeld = false;
	bool bTiltDownHeld = false;
	bool bSliceUpHeld = false;
	bool bSliceDownHeld = false;
	bool bFineHeld = false;
	float PendingMouseAim = 0.0f;
	bool bCharging = false;
	float ChargeSeconds = 0.0f;
	bool bBehindBallView = false;
	int32 NextBallPick = 0;   // which ball T aims at next, by distance

	int32 BestStrokes = 0;   // this session
	int32 LastClearedStrokes = 0;
	TArray<FQuat> BallRotations;

	/** Colours each part: a dynamic instance of the material it was built with. On
	 *  construction (so the editor shows it) and on BeginPlay. */
	void ApplyPaint();
};
