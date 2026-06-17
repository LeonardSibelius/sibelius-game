// ElsewhereBuilder.h
//
// THE SAUCE DOOR — the runtime room assembler (SIB-47, §4). One of these lives in the
// Elsewhere generation map. On BeginPlay it reads the plan staged by the
// UElsewhereSubsystem and assembles the room from the place-type + seed: a modular
// floor, scattered props, mood lighting, the ONE curio, and a return door. Everything
// is built from the seed and discarded when the level unloads (§4).
//
// PCG SEAM: the design calls for UE5's PCG framework to assemble the geometry. That's
// a graph asset (editor-authored) the runtime can't create headlessly, so the MVP
// ships a deterministic C++ assembly here instead — same inputs (place-type + seed),
// same throwaway-room contract, fully gate-testable. AssembleGeometry() is the single
// method a PCGComponent later replaces; the rest of the loop (plan -> curio -> return
// -> discard) is unchanged when it does. See docs/sib-47-sauce-door-notes.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ElsewhereTypes.h"
#include "ElsewhereBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class ACurio;
class AReturnDoor;

UCLASS()
class SIBELIUSGAME_API AElsewhereBuilder : public AActor
{
	GENERATED_BODY()

public:
	AElsewhereBuilder();

	// Build the room from a plan + content registry. Returns the number of props
	// scattered (deterministic for a given LayoutSeed) — the gate asserts determinism
	// and that exactly one curio with Plan.CurioId was spawned. Idempotent-ish: call
	// once per level load. Returns 0 (and spawns nothing) for an invalid plan/place.
	int32 BuildFromPlan(
		const FElsewherePlan& Plan,
		const TArray<FPlaceTypeDef>& Places,
		const TArray<FCurioDef>& Curios);

	// Accessors for the gate.
	ACurio* GetSpawnedCurio() const { return SpawnedCurio; }
	AReturnDoor* GetSpawnedReturnDoor() const { return SpawnedReturnDoor; }

	// Where to drop the return door + curio relative to the builder (cm).
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") FVector CurioOffset = FVector(0.f, 0.f, 80.f);
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") FVector ReturnDoorOffset = FVector(-300.f, 0.f, 0.f);

protected:
	virtual void BeginPlay() override;

	// THE geometry step — the PCG seam (see header). Lays the modular floor + scatters
	// props from the place-type + LayoutSeed. Returns the prop count.
	int32 AssembleGeometry(const FPlaceTypeDef& Place, int32 LayoutSeed);

	// Mood pass: tints the ambient glow from the place + MoodSeed jitter.
	void ApplyMood(const FPlaceTypeDef& Place, int32 MoodSeed);

	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UInstancedStaticMeshComponent> FloorISM;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UInstancedStaticMeshComponent> PropISM;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UPointLightComponent> MoodLight;

private:
	UPROPERTY() TObjectPtr<ACurio> SpawnedCurio;
	UPROPERTY() TObjectPtr<AReturnDoor> SpawnedReturnDoor;
};
