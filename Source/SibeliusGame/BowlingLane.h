// BowlingLane.h
//
// BOWLING — one lane of the Transhuman Bowling Alley (Walt, 2026-09-27), in L_Bowling, the
// alley from PixelForgeESP's Retro Bowling Alley pack, reached by the BOWLING door in the
// living room. Ten frames, scored the real way; free play.
//
// ===========================================================================
// THE MODEL DECIDES, THIS ACTOR SHOWS (the pinball's and the pool table's pattern).
// FBowlingSim owns the ball, the pins and the score (BowlingSim.h, tested standalone by
// Tools/Tests). This actor places ten pin meshes and the pack's ball from it, follows the
// ball down the lane with the camera, shows the scorecard on the HUD's table panel, plays
// the sounds, and takes the player's controls while he bowls.
//
// PLACING IT: the pack's lanes are SM_Pist meshes, and this actor is meant to sit at
// EXACTLY an SM_Pist actor's location and rotation (copy them across in Details). The
// Lane frame inside it then lands on that lane's foul line: SM_Pist runs toward its own
// -Y, 154 cm wide with the lane centred at X 77, and the foul line 30 cm in (the
// measurements are above MakeRetroBowl in BowlingSim.cpp). Tick bDrawLaneInEditor to see
// the lane edges, the foul line and the pin spots drawn over it.
//
// L_BOWLING, AS SET UP ON 2026-09-27 (the level is not in git - see .gitignore - so this is
// how to rebuild it): File > Save Current Level As on IE_RetroBowl/Levels/L_Map, to
// /Game/Bowling/L_Bowling. Then, on the lane SM_Pist4 (world 192, -115, 0, no rotation):
//   - delete the pack's ten decorative pins on that lane (SM_Stick40 is its head pin, at
//     world 269, -1976), which cannot be knocked down and stood behind ours
//   - place one ABowlingLane at 192, -115, 0; DeckLight 90 and ApproachLight 60 lumens
//   - move the PlayerStart to 269, 255, 100, yaw -90: on the approach, facing the lane
// The way home is the game's own [O]. The way in is the BOWLING door in L_Office_v02: an
// AHiddenDoor with bAlwaysVisible, TravelTargetLevel L_Bowling, the pack's SM_Door_1 at
// scale 0.63 and T_Sign_Bowling, on the wall behind the pinball.
//
// SIM TO UNREAL: the sim's +Y is the bowler's LEFT (right-handed); Unreal's +Y is right.
// So the Lane frame's +X runs down the lane and sim (X, Y) is Lane-local (X, -Y). The
// ball's spin, an axial vector, maps as (-X, Y, -Z) under that mirror.
//
// HOUSE LESSONS CARRIED OVER (PinballTable.h has where each came from): EndPlay restores
// input and the view target; components are built in the constructor so it can be seen
// and placed; every mesh is a HARD reference so it reaches the pak; interaction goes
// through IInteractable only; text goes to the HUD's table panel, not 3D text.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BowlingSim.h"
#include "BowlingLane.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API ABowlingLane : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABowlingLane();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The foul line, at the middle of the lane: the sim's origin. See the header. */
	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<USceneComponent> Lane;

	/** What the interaction trace lands on: the approach, just behind the foul line. It
	    blocks only the Visibility trace, so he can still walk through it. */
	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UBoxComponent> Approach;

	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TArray<TObjectPtr<UStaticMeshComponent>> PinMeshes;

	/** Where he will send it: a line up the lane from the ball, straight along the aim. */
	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UStaticMeshComponent> AimLine;

	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UCameraComponent> LaneCamera;

	/** The light over the pin deck, as every real alley has. The pack's lighting is baked
	    into its fixed meshes, and our pins MOVE, so they get none of it: without this they
	    stood black against the pack's glowing ones (Walt, 2026-09-27). */
	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UPointLightComponent> DeckLight;

	/** And a softer one over the approach, for the ball waiting at his feet. */
	UPROPERTY(VisibleAnywhere, Category = "Bowling")
	TObjectPtr<UPointLightComponent> ApproachLight;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Bowling")
	bool bDrawLaneInEditor = true;

	UPROPERTY(EditAnywhere, Category = "Bowling", meta = (ClampMin = "0.2"))
	float PowerChargeSeconds = 1.3f;

	/** cm per second the stance slides along the foul line with A/D held. */
	UPROPERTY(EditAnywhere, Category = "Bowling", meta = (ClampMin = "1.0"))
	float StanceSpeed = 45.0f;

	/** Degrees per second of aim with the arrow keys held; Shift for fine. */
	UPROPERTY(EditAnywhere, Category = "Bowling", meta = (ClampMin = "0.1"))
	float AimDegreesPerSecond = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Bowling", meta = (ClampMin = "0.0"))
	float MouseAimDegreesPerUnit = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Bowling", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindLaneKeys();

	void OnStanceLeftPressed();
	void OnStanceLeftReleased();
	void OnStanceRightPressed();
	void OnStanceRightReleased();
	void OnAimLeftPressed();
	void OnAimLeftReleased();
	void OnAimRightPressed();
	void OnAimRightReleased();
	void OnFinePressed();
	void OnFineReleased();
	void OnMouseX(float Value);
	void OnHookMore();
	void OnHookLess();
	void OnBowlPressed();
	void OnBowlReleased();
	void OnNewGame();
	void OnLeave();

	void UpdateControls(float DeltaSeconds);
	void SyncBall(float DeltaSeconds);
	void SyncPins();
	void SyncAimLine();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	void HandleEvents();
	void RefreshPanel();
	FString Scorecard() const;
	void DrawLane() const;
	float CurrentPower() const;
	void FitMeshes();

	static FVector SimToLocal(FBowlingVec2 P, double Height);

	ASibeliusHUD* GetHud() const;

	FBowlingSim Sim;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	double StanceY = -20.0;   // sim cm; a right-hander stands a little right of centre
	double AimAngle = 0.0;    // sim radians
	double Hook = 0.5;        // -1..1
	bool bStanceLeft = false;
	bool bStanceRight = false;
	bool bAimLeft = false;
	bool bAimRight = false;
	bool bFine = false;
	float PendingMouseAim = 0.0f;
	bool bCharging = false;
	float ChargeSeconds = 0.0f;

	FQuat BallRotation = FQuat::Identity;
	FQuat PinUpright = FQuat::Identity;   // turns the pin mesh to stand on its base
	double BallMeshScale = 1.0;
	FVector BallMeshCentre = FVector::ZeroVector;   // the ball's centre in its mesh, scaled

	/** Places the ball mesh so its CENTRE, not its pivot, is at Centre. */
	void PlaceBallMesh(const FVector& Centre);
	int32 BestScore = 0;                  // this session
};
