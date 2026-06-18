// ElsewhereBuilder.h
//
// THE SAUCE DOOR — the runtime room assembler (SIB-47, §4). One of these lives in the
// Elsewhere generation map. On BeginPlay it reads the plan staged by the
// UElsewhereSubsystem and assembles the room from the place-type + seed: a modular
// floor, scattered props, mood lighting, the ONE curio, and a return door. Everything
// is built from the seed and discarded when the level unloads (§4).
//
// AssembleGeometry() lays a real modular room from the place-type's KIT PALETTE:
// a tiled floor + ceiling, perimeter walls with a doorway, and scattered props — all
// chosen deterministically from the run seed (same seed -> same room). Kit meshes come
// from the data (DT_ElsewherePlaces); when a kit isn't installed each slot falls back
// to an engine shape, so the room STRUCTURE always renders and the headless gate stays
// green with zero marketplace bytes.
//
// PCG SEAM: AssembleGeometry() is the single method a UPCGComponent->Generate() can
// later replace for richer scatter; the rest of the loop (plan -> curio -> return ->
// discard) is unchanged when it does. See docs/sib-47-sauce-door-notes.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ElsewhereTypes.h"
#include "ElsewhereBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UPCGComponent;
class UPCGGraphInterface;
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

	// The curio's floating height above the builder origin (it hangs in the hall centre
	// as the single focal object).
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") FVector CurioOffset = FVector(0.f, 0.f, 150.f);

	// Optional nudge added to the computed west-doorway position of the return door.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") FVector ReturnDoorOffset = FVector(0.f, 0.f, 0.f);

	// Dev/preview: when no Elsewhere is staged (opening L_Elsewhere directly instead of
	// arriving via the Sauce Door), build this place-type anyway so the map renders a
	// real room on its own — for dressing review. A real arrival always has a staged
	// plan, so this branch never fires in the actual loop.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Preview") bool bPreviewWhenUnstaged = false;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Preview") FName PreviewPlaceType = TEXT("ServerCathedral");
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Preview") int32 PreviewSeed = 4242;

	// --- Atmosphere (dressing pass 1): god-ray light shafts down the long walls, so the
	// hall reads as a sacred temple. Visual only; deterministic positions from the room
	// grid (no RNG), so they always line up. TUNE THESE on the builder instance:
	//   bGodRayShafts        — master on/off
	//   ShaftsPerWall        — how many shafts per long (north/south) wall
	//   ShaftIntensity       — brightness of each shaft
	//   ShaftColor           — cool teal/blue
	//   ShaftVolumetricScattering — how solid the visible beam is (needs the map's
	//                          volumetric height fog enabled to show)
	//   ShaftOuterConeDeg    — beam width
	// Needs the map's ExponentialHeightFog volumetric scattering ON to render as beams. ---
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") bool bGodRayShafts = true;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere", meta = (ClampMin = "0", ClampMax = "12")) int32 ShaftsPerWall = 3;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") float ShaftIntensity = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") FLinearColor ShaftColor = FLinearColor(1.0f, 0.78f, 0.42f); // warm gold shafts vs the cool/dark ambient
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") float ShaftVolumetricScattering = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere", meta = (ClampMin = "1", ClampMax = "80")) float ShaftOuterConeDeg = 22.f;

	// --- SIB-47 PCG spike (incremental — see docs "PCG spike — resume here"): replace the
	// C++ seeded prop scatter with a REAL UPCGComponent running a PCG graph. Behind a flag
	// with the C++ scatter as fallback, so the working loop + ElsewhereSmokeTest stay green
	// (default OFF). When ON *and* ScatterGraph is assigned, the floor props come from PCG,
	// seeded from the run (deterministic). Floor/walls/ceiling/curio/return door stay C++. ---
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|PCG") bool bUsePCGScatter = false;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|PCG") TSoftObjectPtr<UPCGGraphInterface> ScatterGraph;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder|PCG") TObjectPtr<UPCGComponent> PCGComponent;

protected:
	virtual void BeginPlay() override;

	// THE geometry step — the PCG seam (see header). Lays floor + ceiling tiles,
	// perimeter walls with a doorway, and scatters props from the place-type kit +
	// LayoutSeed. Returns the scattered-prop count (the gate's determinism handle).
	int32 AssembleGeometry(const FPlaceTypeDef& Place, int32 LayoutSeed);

	// Mood pass: tints the ambient glow from the place + MoodSeed jitter.
	void ApplyMood(const FPlaceTypeDef& Place, int32 MoodSeed);

	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UPointLightComponent> MoodLight;

private:
	// One ISM per distinct mesh used this build (a kit piece or a fallback shape).
	// Created on demand; cleared at the start of each AssembleGeometry.
	UInstancedStaticMeshComponent* GetOrCreateISM(UStaticMesh* Mesh);

	// Place one tile/segment: resolves a mesh from the palette (deterministic pick),
	// or the fallback engine shape. A kit mesh is placed at the place's KitMeshScale;
	// a fallback shape is stretched to FitScale so it fills the tile/segment.
	void PlacePiece(
		const TArray<TSoftObjectPtr<UStaticMesh>>& Palette,
		UStaticMesh* Fallback,
		float KitMeshScale,
		const FTransform& KitXform,
		const FVector& FitScale,
		FRandomStream& Rng,
		bool bRestBaseOnFloor = false,   // seal: drop the piece so its mesh bottom sits at FloorZ
		float FloorZ = 0.f);

	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> KitISMs;

	// Atmosphere: spawn volumetric spot-light shafts down the long walls (deterministic).
	void SpawnGodRayShafts(const FPlaceTypeDef& Place);
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> ShaftLights;

	// PCG spike: run the ScatterGraph (seeded) for the floor props. Returns true if it ran
	// (graph valid); false -> caller uses the C++ scatter fallback.
	bool RunPCGScatter(const FPlaceTypeDef& Place, int32 LayoutSeed);

	UPROPERTY() TObjectPtr<ACurio> SpawnedCurio;
	UPROPERTY() TObjectPtr<AReturnDoor> SpawnedReturnDoor;
};
