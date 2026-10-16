// UniverseForge.h
//
// THE FORGE - in the AI city (L_Genesis), a great glass sphere floating past the gallery rail,
// and a console in front of it. Walt, 2026-10-15: "go ahead with growing a universe" (Seed 1).
//
// E at the console: three dials - gravity, light, time - and [Space] to begin. Then, in about
// two minutes (hold [Space] to hurry), the universe the model grew (UniverseSim) plays inside
// the glass: a flash and an expanding ball of matter; the fog gathering into threads; the first
// stars catching fire in the colour the light dial gave them; galaxies turning; a dive into one
// galaxy, to one star and one small world; the world turning blue and green; and at last a
// shell of tiny lights closing round its star - its machines, beginning a city like the one he
// is standing in. Or it stops early, as the dials doomed it to: the cold fog, the crunch, the
// burn, the dim, too young. Its name, its chronicle and its ending are on the panel.
//
// THE HOUSE PATTERN (ABasketballCourt): the model decides everything and is tested standalone;
// this actor only presents it, takes his input and the view while it runs and gives both back
// in one place (StopPlaying); the parts are built in the constructor from the engine's shapes
// and /Game/Genesis's materials, so it can be rendered and judged by eye.
//
// Frame: the actor's origin is the sphere's centre; the console stands ConsoleOffset from it
// (toward the gallery), and the camera looks back across the console at the glass.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "UniverseSim.h"
#include "UniverseForge.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

UENUM()
enum class EForgeState : uint8
{
	Idle,       // nobody at the console: a faint mote turns in the glass
	Dials,      // he is setting the three dials
	Growing,    // the universe plays
	Ended,      // its ending, until [R] or he steps back
};

UCLASS()
class SIBELIUSGAME_API AUniverseForge : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AUniverseForge();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	/** For render checks: grow these dials, stop the clock this many seconds in, and look
	    through the forge's camera. Seed 1 always, so a check is repeatable. */
	UFUNCTION(BlueprintCallable, Category = "Forge")
	void PreviewAt(float Gravity, float Light, float Time, float Seconds);

	/** Whether anyone is at a forge in this world - the AI city keeps its own lines off the
	    screen while one is in use. */
	static bool IsAnyInUse(const UObject* WorldContext);

	static constexpr int32 MatterCount = 1800;
	static constexpr int32 MachineCount = 160;
	static constexpr int32 HoleCount = 6;
	static constexpr int32 DialCount = 3;

	// --- components -----------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> Glass;

	/** Every grain of matter in the universe, from the bang to the galaxies. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UInstancedStaticMeshComponent> Matter;

	/** The bang's light. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> Flash;

	/** The one ordinary star, once the forge has dived into its galaxy. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> Sun;

	/** And its one small world. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> World;

	/** Its machines: lights closing round their star. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UInstancedStaticMeshComponent> Machines;

	/** The crunch: black holes where the galaxies would have been. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UInstancedStaticMeshComponent> HoleCores;

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UInstancedStaticMeshComponent> HoleRims;

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> ConsoleBase;

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UStaticMeshComponent> ConsoleTop;

	/** The three dials, glowing rings on the console's top: gravity, light, time. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Forge")
	TArray<TObjectPtr<UStaticMeshComponent>> DialRings;

	/** What E finds. Visibility only. */
	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UBoxComponent> ConsoleReach;

	UPROPERTY(VisibleAnywhere, Category = "Forge")
	TObjectPtr<UCameraComponent> ForgeCamera;

	// --- tuning -----------------------------------------------------------------------------

	/** The glass's radius; the universe fills most of it. */
	UPROPERTY(EditAnywhere, Category = "Forge", meta = (ClampMin = "100"))
	float UniverseRadius = 650.0f;

	/** Where the console stands, from the sphere's centre. */
	UPROPERTY(EditAnywhere, Category = "Forge")
	FVector ConsoleOffset = FVector(0.0, 1350.0, -300.0);

	/** Seconds each stage plays: the bang, first light, the first stars, galaxies, one world,
	    life, its machines. Then EndingSeconds for how it ends. */
	UPROPERTY(EditAnywhere, Category = "Forge")
	TArray<float> StageSeconds = { 8.0f, 14.0f, 16.0f, 22.0f, 20.0f, 18.0f, 24.0f };

	UPROPERTY(EditAnywhere, Category = "Forge", meta = (ClampMin = "1"))
	float EndingSeconds = 8.0f;

	/** How much faster it plays while [Space] is held. */
	UPROPERTY(EditAnywhere, Category = "Forge", meta = (ClampMin = "1"))
	float HurrySpeed = 4.0f;

	/** How far one press of a dial key turns it. */
	UPROPERTY(EditAnywhere, Category = "Forge", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float DialStep = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Forge")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category = "Forge")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	/** M_GoSurface (Color x Pattern): the console, the world, the black holes' cores. */
	UPROPERTY(EditAnywhere, Category = "Forge")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void Paint();
	void Layout();                    // this history's grains, clusters and galaxies
	void BeginGrowing();
	void Pose();                      // everything in the glass, at Clock
	void PoseIdle();
	void AimCamera(bool bInstant);
	void RefreshPanel();
	FString DialLine(int32 Index) const;

	void StartPlaying(APlayerController* PC);
	void StopPlaying();               // the one place his controls and view come back
	void BindKeys();
	void OnUp();
	void OnDown();
	void OnLeft();
	void OnRight();
	void OnSpacePressed();
	void OnSpaceReleased();
	void OnAgain();
	void OnLeave();
	ASibeliusHUD* GetHud() const;
	void Turn(int32 Index, float By);

	EForgeState State = EForgeState::Idle;
	FUniverseDials Dials;
	int32 DialIndex = 0;
	FUniverseHistory History;
	uint64 NextSeed = 1;
	float Clock = 0.0f;               // seconds into the growing, at 1x
	float Age = 0.0f;                 // seconds since play began, for idle motion
	bool bHurry = false;
	bool bFrozen = false;             // PreviewAt: the clock stands still
	bool bPlaying = false;
	bool bKeysBound = false;
	TWeakObjectPtr<APlayerController> PlayingPC;
	FVector CameraGoal = FVector::ZeroVector;
	FRotator CameraGoalRot = FRotator::ZeroRotator;

	// The grains, in units of UniverseRadius from the centre.
	TArray<FVector> Ball;             // where each was at the end of the bang
	TArray<FVector> Thread;           // gathered part way toward its cluster (first light)
	TArray<FVector> Clump;            // in its cluster (the first stars)
	TArray<FVector> Spiral;           // its place in its galaxy, in the galaxy's own plane
	TArray<int32> ClusterOf;
	TArray<FVector> ClusterCentre;
	TArray<FQuat> ClusterTilt;
	TArray<FVector> MachineDir;
	TArray<FTransform> Scratch;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MatterLight;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FlashLight;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SunLight;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WorldSkin;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MachineLight;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DialLight;
};
