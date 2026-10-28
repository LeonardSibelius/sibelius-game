// OrbitGolfCourse.h
//
// ORBITAL GOLF — the course over Elise's grassland (Walt, 2026-10-25: "let's do the orbital
// golf"). FOrbitGolfSim is the game: a ball under the world's gravity and the moons'. This actor
// is what he sees and plays: a marble tee platform on the grass, the hole's moons hanging in the
// sky with rings like the planets over Elise's world, a flag on the cup wherever it sits on its
// moon - on the crown, round the far side, underneath - the ball with a glowing trail, and while
// he aims a dotted guide through bounces to the predicted resting point.
//
// Controls, the pool tables' where they can be: A D / mouse aim, W S raise and lower the shot,
// SHIFT fine, [ ] set power; tap SPACE to swing, V the whole course, R start the round again,
// ESC walk away. When a hole is holed, SPACE goes on to the next.
//
// The house lessons (HyperBilliardsTable.h): the model decides, this actor shows; his body steps
// aside while he plays and comes back; every mesh and material is a hard reference.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "OrbitGolfSim.h"
#include "OrbitGolfCourse.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AOrbitGolfCourse : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AOrbitGolfCourse();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The tee: a marble disc, its top the actor's origin, deep enough to meet the slope. */
	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> TeeMat;

	/** A bag of clubs standing at the back of the platform: what E is pressed at (Walt,
	    2026-10-25: "instead of that obelisk, could there be a bag of golf clubs standing there?"). */
	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<USceneComponent> Bag;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TArray<TObjectPtr<UStaticMeshComponent>> BagParts;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TArray<TObjectPtr<UStaticMeshComponent>> Moons;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TArray<TObjectPtr<UStaticMeshComponent>> MoonRings;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<USceneComponent> Cup;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> CupRing;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> FlagPole;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> Flag;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UInstancedStaticMeshComponent> Trail;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UInstancedStaticMeshComponent> Guide;

	UPROPERTY(VisibleAnywhere, Category = "Orbital Golf")
	TObjectPtr<UCameraComponent> CourseCamera;

	UPROPERTY(EditAnywhere, Category = "Orbital Golf", meta = (ClampMin = "0.2"))
	float PowerChargeSeconds = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Orbital Golf", meta = (ClampMin = "1.0"))
	float AimDegreesPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Orbital Golf", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FineAimFraction = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Orbital Golf", meta = (ClampMin = "0.0"))
	float MouseAimDegreesPerUnit = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Orbital Golf", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void StartPlaying(APlayerController* PC);
	void StopPlaying();
	void BindKeys();

	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnRightReleased();
	void OnUpPressed();
	void OnUpReleased();
	void OnDownPressed();
	void OnDownReleased();
	void OnFinePressed();
	void OnFineReleased();
	void OnMouseX(float Value);
	void OnSwingPressed();
	void OnSwingReleased();
	void OnRestart();
	void OnToggleView();
	void OnLeave();
	void OnPowerUp();
	void OnPowerDown();
	void OnUndo();
	void OnCancel();
	void OnPractice();
	void OnToggleClub();
	void OnToggleHelp();
	void ResolveShot(double& WorldYaw, double& WorldPitch, double& Power) const;

	void UpdateAim(float DeltaSeconds);
	void AimAtCup();
	void ShowHole();          // the moons and the cup of the current hole
	void SyncBall(float DeltaSeconds);
	void SyncGuide();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	void HandleEvents();
	void RefreshPanel();
	float CurrentPower() const;
	void ApplyPaint();

	static FVector ToLocal(const FSpaceVec& P) { return FVector(P.X, P.Y, P.Z); }
	ASibeliusHUD* GetHud() const;

	FOrbitGolfSim Sim;
	FOrbitGolfSim BeforeShot;
	bool bCanUndo = false;
	bool bPractice = false;
	bool bPutt = false;
	bool bSavedPutt = false;
	bool bShowHelp = false;
	FVector FlightForward = FVector::ForwardVector;
	FVector FlightUp = FVector::UpVector;
	bool bUsedUndo = false;
	float SelectedPower = 0.5f;
	float SavedPower = 0.5f;
	double SavedYaw = 0.0, SavedPitch = 0.0;
	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;
	int32 ShownHole = -1;

	double Yaw = 0.0;
	double Pitch = 0.6;
	bool bLeftHeld = false;
	bool bRightHeld = false;
	bool bUpHeld = false;
	bool bDownHeld = false;
	bool bFineHeld = false;
	float PendingMouse = 0.0f;
	bool bCharging = false;
	float ChargeSeconds = 0.0f;
	bool bOverview = true;
	double PreviewYaw = 999.0, PreviewPitch = 999.0;
	float PreviewPower = -1.0f;
	FVector PreviewOrigin = FVector::ZeroVector;
	FString PreviewNote;
	float TrailClock = 0.0f;
	TArray<FVector> TrailPoints;   // course space, oldest first
	int32 BestRound = 0;           // this session, strokes over the three holes
};
