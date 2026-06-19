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
class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;
class UMaterialInterface;
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

	// --- Gate read-back for the richer scatter (headless determinism / exclusion / open-path /
	// per-seed-variation checks; never inspects "the look"). ---
	// World transforms of every scattered DETAIL prop placed this build. Returns the count.
	int32 GetScatterInstanceTransforms(TArray<FTransform>& OutXforms) const;
	// The exact carve geometry the last scatter honoured (world space), so the gate asserts the
	// SAME numbers the builder used — no duplicated magic constants. False if no build has run.
	bool GetScatterExclusionZones(
		FVector& OutCurioCenter, float& OutCurioRadius,
		FVector& OutDoorCenter,  float& OutDoorRadius,
		FVector& OutSpawnCenter, float& OutSpawnRadius,
		float& OutCorridorHalfWidth, float& OutFloorZ) const;

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

	// --- Dressing pass 2: deliberate kit machinery so the hall reads as a real built space —
	// bulkhead arch-ribs the player walks through + pipe runs along the walls. Deterministic
	// (grid positions, no RNG), kit-by-path with graceful skip when the kit isn't installed
	// (so the gate stays green with zero kit bytes). Master toggle: ---
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") bool bStructuralProps = true;

	// Return-door beacon (the "this way home" affordance): a warm amber glow at the doorway.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") FLinearColor ReturnBeaconColor = FLinearColor(1.0f, 0.55f, 0.2f);
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Atmosphere") float ReturnBeaconIntensity = 6000.f;

	// --- SIB-47 PCG spike (incremental — see docs "PCG spike — resume here"): replace the
	// C++ seeded prop scatter with a REAL UPCGComponent running a PCG graph. Behind a flag
	// with the C++ scatter as fallback, so the working loop + ElsewhereSmokeTest stay green
	// (default OFF). When ON *and* ScatterGraph is assigned, the floor props come from PCG,
	// seeded from the run (deterministic). Floor/walls/ceiling/curio/return door stay C++. ---
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|PCG") bool bUsePCGScatter = false;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|PCG") TSoftObjectPtr<UPCGGraphInterface> ScatterGraph;
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder|PCG") TObjectPtr<UPCGComponent> PCGComponent;

	// --- SIB-47 richer per-seed C++ scatter (the verifiable gate path; the PCG path mirrors it
	// behind bUsePCGScatter). The per-place CONTENT (density + corridor) lives on FPlaceTypeDef;
	// these are builder-global SHAPING knobs Walt nudges by eye. ---
	// The default scatter palette — the curated SciFi BOXES A & B crate/container/barrel set
	// (bounds-verified, populated in the ctor; in lockstep with build_pcg_scatter_graph.py). Used
	// for any place that doesn't author its own FPlaceTypeDef::ScatterMeshes — which is every place
	// today (the DataTable has no ScatterMeshes column), so it drives the look regardless of whether
	// the DataTable or the code default is loaded. Per-mesh Weight/Scale/Lean are the tunables.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter") TArray<FScatterMeshDef> ScatterSet;
	// Clear-zone radii (cm), so props never block the curio, the doorway/return, or the spawn bay.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter") float CurioExclusionRadius = 320.f;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter") float DoorwayExclusionRadius = 450.f;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter") float SpawnExclusionRadius = 380.f;
	// 0 = props spread evenly; 1 = props bunch into a few lived-in clumps. Boxes read as a debris
	// PILE, so this defaults high (strong grouping). The cluster STRENGTH knob.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "0", ClampMax = "1")) float ScatterClusterBias = 0.78f;
	// 0 = clusters sit anywhere in the interior; 1 = clusters hug the perimeter walls/corners (where
	// debris naturally gathers). Lerps each cluster centre toward the nearest wall. No extra RNG draw.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "0", ClampMax = "1")) float ScatterWallBias = 0.5f;
	// Global density multiplier for the per-seed target count (BaseCount × place ScatterDensity ×
	// this). The place's PropCountMin/Max is tuned sparse (a few hero props); a debris pile wants
	// MANY more boxes, so this scales it up WITHOUT editing the DataTable (the runtime source). The
	// per-seed density wobble still rides on top. Result is clamped to 0..64.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "0", ClampMax = "12")) float ScatterCountScale = 3.0f;
	// Per-seed composition: each visit activates a random subset of the palette (size in this
	// range), so the SET of objects — not just their positions — differs run to run.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "1")) int32 ScatterSubsetMin = 4;
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "1")) int32 ScatterSubsetMax = 8;
	// Candidate positions pre-drawn per prop (the first clear one is placed); a prop drops only
	// if all reject. A constant budget -> deterministic instanced count (the gate's handle).
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Scatter", meta = (ClampMin = "1", ClampMax = "16")) int32 ScatterPlacementTries = 8;

	// --- Floor material quick win: an explicit project-side reference to the kit floor material,
	// applied to the FLOOR ISM (does NOT modify the kit — referenced by path; default points at
	// the _K floor base MI). Kit-absent -> the soft ref is null -> skipped, so the headless gate
	// is unaffected. NOTE: the residual editor "checker" is the kit's shared M_Base_MAT lacking
	// the Used-With-Instanced-Static-Meshes flag — that's the known harmless editor-prompt /
	// cook-time concern (we deliberately do NOT toggle that flag here; see the asset-policy note). ---
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder|Materials") TSoftObjectPtr<UMaterialInterface> FloorMaterialOverride;

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

	// Dressing pass 2: deterministic kit machinery (bulkhead arch-ribs + wall pipe runs).
	// Uses GetOrCreateISM, so its ISMs live in KitISMs and clear on the next build.
	void SpawnStructuralProps(const FPlaceTypeDef& Place);

	// Warm beacon seated at the return doorway — purely the visual "exit here" cue now.
	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UPointLightComponent> ReturnBeacon;

	// THE RETURN, as an OVERLAP trigger (not E-interact): an invisible box spanning the west
	// doorway threshold, seated INSIDE the edge so the player walks into it BEFORE reaching the
	// drop. On the player Pawn entering, we discard the Elsewhere and travel home — no key, no
	// aim, no interact trace to be blocked by the structural props/beacon, and no fall (you
	// leave before the edge). Seated in AssembleGeometry once the doorway position is known.
	void SeatReturnTrigger(const FVector& Origin, float GridHalfX, float DoorCY, float WallH);

	UFUNCTION()
	void OnReturnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Elsewhere Builder") TObjectPtr<UBoxComponent> ReturnTrigger;

	// Where "home" is (mirrors AReturnDoor) — used by the overlap return.
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") FName HomeLevelName = TEXT("L_Office_v02");

	// Brief arm delay so the trigger can't fire on the spawn frame (the fixed PlayerStart can
	// sit at the doorway in smaller rooms — without this it could instant-return-loop).
	UPROPERTY(EditAnywhere, Category = "Elsewhere Builder") float ReturnArmDelay = 1.0f;

	bool bReturnArmed = false;     // false until ReturnArmDelay elapses after the build
	bool bReturningHome = false;   // re-entry guard (OpenLevel is async)

	// PCG spike: assign the ScatterGraph + seed and SCHEDULE generation for next tick (the
	// actor's runtime ISM bounds aren't valid yet this frame -> PCG would abort). Returns true
	// if scheduled (graph valid); false -> caller uses the C++ scatter fallback.
	bool RunPCGScatter(const FPlaceTypeDef& Place, int32 LayoutSeed);

	// Deferred Generate() target (next-tick timer set by RunPCGScatter), once bounds are valid.
	void GeneratePCGScatterDeferred();

	// --- SIB-47 richer per-seed scatter (the C++ fallback = the gate's determinism handle). ---
	// Deterministic data-driven scatter: per-seed active subset of the curated palette + per-seed
	// density, weighted per-mesh placement, exclusion zones (curio/doorway/spawn) + an open central
	// corridor, and cluster bunching. Returns the INSTANCED prop count (a pure function of the
	// seed, the gate's handle). All entropy from the passed-in Rng, in a fixed draw order so two
	// fresh actors from the same plan match — and so kit-absent consumes the stream identically.
	int32 PlaceScatter(const FPlaceTypeDef& Place, const FVector& Origin,
		float GridHalfX, float GridHalfY, float Tile, float DoorCY,
		UStaticMesh* CylFallback, FRandomStream& Rng);

	// Effective palette: Place.ScatterMeshes if non-empty, else one entry per Place.PropMeshes
	// (Weight 10, upright, default scale). PURE — no RNG, no loads — so its SIZE is a function of
	// data only (identical kit-present/absent), which the scatter's draw count depends on.
	TArray<FScatterMeshDef> ResolveScatterPalette(const FPlaceTypeDef& Place) const;

	// Place ONE already-chosen scatter prop. Draws NO RNG (the caller spent the selection draw),
	// so kit-present/absent consume the stream identically. Falls back to CylFallback when the kit
	// mesh fails to load. Tracks the scatter ISM(s) in ScatterISMs for the gate read-back.
	void PlaceScatterPiece(const FScatterMeshDef& Def, UStaticMesh* CylFallback, float KitMeshScale,
		float PerMeshScale, const FTransform& BaseXform, bool bRestBaseOnFloor, float FloorZ);

	// True if a candidate world XY lies inside any carve zone (curio / doorway / spawn disc) or
	// the central corridor band. Pure; uses the cached zone members set by PlaceScatter.
	bool IsScatterExcluded(const FVector2D& WorldXY) const;

	// Floor material quick win: assign FloorMaterialOverride to the kit floor ISM (kit-floor only;
	// never the fallback cube — that would trip the Used-With-ISM editor prompt on a cube).
	void ApplyFloorMaterial(const FPlaceTypeDef& Place);

	// The rich-scatter ISMs (one per distinct scatter mesh actually placed). A subset of KitISMs,
	// tracked separately so the gate reads back ONLY the scatter layer. Reset each build.
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ScatterISMs;

	// Cached carve geometry from the last scatter (world space) — surfaced to the gate so it
	// asserts against the exact numbers used. bScatterZonesValid is false until a build runs.
	FVector CurioCenterWS = FVector::ZeroVector;
	FVector DoorCenterWS  = FVector::ZeroVector;
	FVector SpawnCenterWS = FVector::ZeroVector;
	float CurioCarveR = 0.f, DoorCarveR = 0.f, SpawnCarveR = 0.f, CorridorHalfWS = 0.f, ScatterFloorZ = 0.f;
	bool bScatterZonesValid = false;

	UPROPERTY() TObjectPtr<ACurio> SpawnedCurio;
	UPROPERTY() TObjectPtr<AReturnDoor> SpawnedReturnDoor;
};
