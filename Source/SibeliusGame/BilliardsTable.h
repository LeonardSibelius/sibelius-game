// BilliardsTable.h
//
// BILLIARDS — the pool table in the living room (Walt, 2026-09-27). Solo "clear the
// table": fifteen balls, and the score is the strokes it takes to pot them all.
//
// PijayArt's Classic Billiards Table (Fab, "Generated with AI: No"), from the listing's
// UE project at Content/Billiard_Table/ - NOT the copy Fab's in-editor button made under
// Content/Fab/, which fused the balls into the table. Here every ball is its own mesh.
//
// ===========================================================================
// THE MODEL DECIDES, THIS ACTOR SHOWS. FBilliardsSim owns the balls, the collisions, the
// pockets and the strokes (BilliardsSim.h, tested standalone by Tools/Tests). This actor:
//   - places the sixteen real ball meshes from the sim each frame, and turns them by the
//     sim's own spin, so a drawn ball is seen spinning backwards while it slides forwards
//   - lays the cue behind the cue ball, pulled back as the power charges
//   - draws the aim guides: a line to where the cue ball would touch, a disc where it
//     would be, and the object ball's path. Real meshes, not debug lines: debug drawing is
//     compiled out of Shipping builds, and the aim line must be there in every build
//   - takes the player's controls while he plays, and gives them back however it ends
//   - shows the strokes and the controls on the HUD's table panel
//
// THE TABLE MESH needs no alignment frame, unlike the pinball: its cloth is flat at
// 89.01 cm above its own pivot, centred on it, long axis along X (measured from the mesh;
// see MakeDefault in BilliardsSim.cpp). Tick bDrawPhysicsLayoutInEditor to see the
// cushions and pockets drawn over it in the viewport.
//
// HOUSE LESSONS CARRIED OVER (see PinballTable.h for where each came from): EndPlay
// restores input and the view target; components are built in the constructor so it can
// be seen and placed in the editor; every mesh is a HARD reference so it reaches the pak;
// interaction goes through IInteractable only.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BilliardsSim.h"
#include "BilliardsTable.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API ABilliardsTable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABilliardsTable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	bool IsPlaying() const { return bPlaying; }

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The table. Visual only; Body does the collision. */
	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> Table;

	/** What the player bumps into and what the interaction trace lands on. */
	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UBoxComponent> Body;

	/** The plane of the cloth, centred on it: the sim's (0, 0). Balls hang off this. */
	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<USceneComponent> Cloth;

	/** Index 0 is the cue ball, 1..15 the numbered balls, as in the sim. */
	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TArray<TObjectPtr<UStaticMeshComponent>> BallMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> Cue;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> AimLine;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> GhostDisc;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> ObjectLine;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UStaticMeshComponent> DeflectLine;

	UPROPERTY(VisibleAnywhere, Category = "Billiards")
	TObjectPtr<UCameraComponent> TableCamera;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Billiards")
	bool bDrawPhysicsLayoutInEditor = true;

	/** Holding the shot key this long is full power; hold on and it falls back again. */
	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "0.2"))
	float PowerChargeSeconds = 1.4f;

	/** Degrees per second with the aim keys held; Shift (or Ctrl) is the fine-aim fraction. */
	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "1.0"))
	float AimDegreesPerSecond = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FineAimFraction = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "0.0"))
	float MouseAimDegreesPerUnit = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	/** The overhead view: from the table's -Y side, this far from the centre of the cloth. */
	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "50.0"))
	float OverheadDistance = 200.0f;   // below a living-room ceiling: 265 put it in the attic

	UPROPERTY(EditAnywhere, Category = "Billiards", meta = (ClampMin = "10.0", ClampMax = "90.0"))
	float OverheadPitchDegrees = 50.0f;

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
	void OnFinePressed();
	void OnFineReleased();
	void OnMouseX(float Value);
	void OnStickX(float Value);
	void OnSpinUp();
	void OnSpinDown();
	void OnShotPressed();
	void OnShotReleased();
	void OnRack();
	void OnToggleView();
	void OnLeave();

	void UpdateAim(float DeltaSeconds);
	void SyncBalls(float DeltaSeconds);
	void SyncCueAndGuides();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	void HandleEvents();
	void RefreshPanel();
	void DrawPhysicsLayout() const;
	float CurrentPower() const;
	void AimAtNearestBall();

	/** Sim (X, Y) on the cloth, at Height above it, in Cloth space. */
	static FVector SimToLocal(FBilliardsVec2 P, double Height);

	ASibeliusHUD* GetHud() const;

	FBilliardsSim Sim;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	double AimAngle = 0.0;   // radians, sim space
	double Spin = 0.0;       // -1 draw .. +1 follow
	bool bAimLeftHeld = false;
	bool bAimRightHeld = false;
	bool bFineHeld = false;
	float PendingMouseAim = 0.0f;
	float StickAim = 0.0f;
	bool bCharging = false;
	float ChargeSeconds = 0.0f;
	bool bBehindBallView = false;

	int32 BestStrokes = 0;   // this session; 0 = none yet. A saved best is a later job.
	int32 LastClearedStrokes = 0;
	TArray<FQuat> BallRotations;
};
