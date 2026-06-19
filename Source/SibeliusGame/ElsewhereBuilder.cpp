// ElsewhereBuilder.cpp — see header. Deterministic C++ modular assembly (the PCG seam):
// floor + ceiling tiles, perimeter walls with a doorway, scattered props — all chosen
// from the place-type's kit palette + the run seed, with engine-shape fallback.

#include "ElsewhereBuilder.h"
#include "ElsewhereGen.h"
#include "ElsewhereSubsystem.h"
#include "Curio.h"
#include "ReturnDoor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "PCGComponent.h"      // SIB-47 PCG spike
#include "PCGGraph.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogElsewhereBuilder, Log, All);

AElsewhereBuilder::AElsewhereBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MoodLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MoodLight"));
	MoodLight->SetupAttachment(SceneRoot);
	MoodLight->SetAttenuationRadius(4000.f);
	MoodLight->CastShadows = false;   // public UPROPERTY on ULightComponentBase

	// Return-door beacon: a warm amber glow seated at the doorway so the way home reads as an
	// obvious exit ("this way to the kitchen"). Off until BuildFromPlan seats it at the door.
	ReturnBeacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("ReturnBeacon"));
	ReturnBeacon->SetupAttachment(SceneRoot);
	ReturnBeacon->SetIntensity(0.f);
	ReturnBeacon->CastShadows = false;

	// SIB-47 PCG spike: a real PCG component. We drive it manually (set graph + seed,
	// then Generate in RunPCGScatter), so it doesn't auto-generate on load.
	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGScatter"));
	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	// Point the seam at the (currently empty) Elsewhere scatter graph. Loaded only when
	// bUsePCGScatter is ON; authoring the graph's nodes + flipping the flag is next session.
	ScatterGraph = TSoftObjectPtr<UPCGGraphInterface>(
		FSoftObjectPath(TEXT("/Game/PCG/PCG_ElsewhereScatter.PCG_ElsewhereScatter")));

	// Floor material quick win: default the override at the real _K floor base MI (BY PATH —
	// we never modify the kit). Kit-absent -> the soft ref is null -> ApplyFloorMaterial skips,
	// so the headless gate is unaffected. Clear it to restore the floor mesh's own 2-slot kit
	// materials (base + border trim).
	FloorMaterialOverride = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/ModularSciFiEnv_K/Materials/Instances/MI_Floor_A_Base.MI_Floor_A_Base")));

	// --- The SciFi BOXES A & B debris palette (SIB-47 scatter pass: a believable pile of sci-fi
	// crates/containers/barrels, replacing the _K machinery set — the _K kit has no small props).
	// Bounds-verified (dump_box_bounds.py) FLOOR-STANDING, base-pivot objects: a size mix from
	// 0.3m canisters → 2.4m hero containers, with a few colour variants (the v0/v3/v4 recolours)
	// for palette variety and two toppled pieces (bUpright=false + small lean) for debris realism.
	// Skipped: Box1_Cover (flat lid), Box1_Locker (thin wall panel), SciFiBox_B_B/_H (dup / huge).
	// Builder default so it drives the look whether the DataTable or the code-default registry is
	// loaded (the DT has no ScatterMeshes column). KEPT IN LOCKSTEP with build_pcg_scatter_graph.py.
	// KitMeshScale=1 (boxes are authored at real cm), so ScaleMin/Max is the per-instance band;
	// rest-base-on-floor seats each by its bbox Min.Z (handles the few non-base pivots). ---
	const TCHAR* A = TEXT("/Game/SciFiBoxes_A/Meshes");      // pack A folder (note: "Boxes")
	const TCHAR* B = TEXT("/Game/SciFi_Box_B/Meshes");       // pack B folder (note: "Box", flat)
	auto MakeScatter = [](const FString& Path, int32 Weight, float SMin, float SMax,
		bool bUpright = true, float Lean = 0.f)
	{
		FScatterMeshDef D;
		D.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path));
		D.Weight = Weight;
		D.bUpright = bUpright;
		D.ScaleMin = SMin;
		D.ScaleMax = SMax;
		D.LeanJitterDeg = Lean;
		return D;
	};
	ScatterSet = {
		// Small / medium crates — the bulk of the pile (high weight). A few colour recolours mixed in.
		MakeScatter(FString::Printf(TEXT("%s/Box3/Box3_v0.Box3_v0"), A),                   5, 0.85f, 1.20f),          // 0.9m cube crate
		MakeScatter(FString::Printf(TEXT("%s/Box3/Box3_v3.Box3_v3"), A),                   3, 0.85f, 1.15f, false, 7.f), // colour variant, lightly toppled
		MakeScatter(FString::Printf(TEXT("%s/Box2/Box2_v0.Box2_v0"), A),                   4, 0.85f, 1.20f),          // 0.8x0.8x1.3 upright box
		MakeScatter(FString::Printf(TEXT("%s/Box2/Box2_v4.Box2_v4"), A),                   3, 0.85f, 1.15f),          // colour variant
		MakeScatter(FString::Printf(TEXT("%s/Box1/Box1_Closed_v0.Box1_Closed_v0"), A),     3, 0.80f, 1.05f),          // 1.4x2.3x1.0 closed crate
		MakeScatter(FString::Printf(TEXT("%s/Box1/Box1_Closed_v3.Box1_Closed_v3"), A),     2, 0.80f, 1.05f),          // colour variant
		MakeScatter(FString::Printf(TEXT("%s/Box1/Box1_Container.Box1_Container"), A),     4, 0.85f, 1.30f, false, 12.f), // 0.3m canister, knocked over
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_C.SM_SciFiBox_B_C"), B),        3, 0.85f, 1.10f),          // 1x2x1.1 crate
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_Barrel_A.SM_SciFiBox_B_Barrel_A"), B), 3, 0.85f, 1.15f),   // barrel
		// Mid containers (medium weight).
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_F.SM_SciFiBox_B_F"), B),        2, 0.80f, 1.05f),          // 1.6m cube
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_G.SM_SciFiBox_B_G"), B),        2, 0.80f, 1.05f),          // 1.6m cube
		MakeScatter(FString::Printf(TEXT("%s/Box1/Box1_Base_v0.Box1_Base_v0"), A),         2, 0.85f, 1.10f),          // low pallet/skid
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_E.SM_SciFiBox_B_E"), B),        2, 0.80f, 1.00f),          // 2.26x2.26x1.2 squat
		// Large hero anchors (low weight — the pile centrepieces).
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_A.SM_SciFiBox_B_A"), B),        1, 0.85f, 1.00f),          // 2m cube container
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_B_D.SM_SciFiBox_B_D"), B),        1, 0.80f, 1.00f),          // 1.5x1.3x2.2 tall container
		MakeScatter(FString::Printf(TEXT("%s/SM_SciFiBox_I.SM_SciFiBox_I"), B),            1, 0.80f, 0.95f),          // 2.2x2.4x2 big container
		MakeScatter(FString::Printf(TEXT("%s/Box1/Box1_Open_Full_v0.Box1_Open_Full_v0"), A), 1, 0.80f, 1.00f),        // 2.25m open full crate
	};
}

void AElsewhereBuilder::BeginPlay()
{
	Super::BeginPlay();

	// Runtime path: pull the plan + registry the Sauce Door staged before travel.
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	if (!Elsewhere)
	{
		return;
	}

	if (Elsewhere->HasStagedElsewhere())
	{
		BuildFromPlan(Elsewhere->GetStagedPlan(), Elsewhere->GetPlaceTypes(), Elsewhere->GetCurios());
		return;
	}

	// No staged plan. In the real loop the Sauce Door always stages one; this path is
	// only hit by opening L_Elsewhere directly. Build a forced preview if asked.
	if (bPreviewWhenUnstaged)
	{
		FElsewherePlan Preview;
		Preview.Seed = PreviewSeed;
		Preview.PlaceTypeId = PreviewPlaceType;
		if (const FPlaceTypeDef* P = Elsewhere->FindPlace(PreviewPlaceType))
		{
			if (P->CurioPool.Num() > 0)
			{
				Preview.CurioId = P->CurioPool[0];
			}
		}
		Preview.LayoutSeed = PreviewSeed;
		Preview.MoodSeed = PreviewSeed * 7 + 1;
		if (Preview.IsValid())
		{
			UE_LOG(LogElsewhereBuilder, Display, TEXT("[%s] preview build (unstaged): %s"),
				*GetName(), *PreviewPlaceType.ToString());
			BuildFromPlan(Preview, Elsewhere->GetPlaceTypes(), Elsewhere->GetCurios());
			return;
		}
	}

	UE_LOG(LogElsewhereBuilder, Warning,
		TEXT("[%s] no staged Elsewhere at BeginPlay — nothing to build (arrive via the Sauce Door, "
		     "or set bPreviewWhenUnstaged for a standalone preview)."), *GetName());
}

UInstancedStaticMeshComponent* AElsewhereBuilder::GetOrCreateISM(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}
	for (UInstancedStaticMeshComponent* ISM : KitISMs)
	{
		if (ISM && ISM->GetStaticMesh() == Mesh)
		{
			return ISM;
		}
	}
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetStaticMesh(Mesh);
	ISM->SetCanEverAffectNavigation(false);
	ISM->SetCollisionProfileName(TEXT("BlockAll"));   // floor to stand on, walls to stop at
	ISM->SetupAttachment(SceneRoot);
	ISM->RegisterComponent();
	ISM->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(ISM);
	KitISMs.Add(ISM);
	return ISM;
}

void AElsewhereBuilder::PlacePiece(
	const TArray<TSoftObjectPtr<UStaticMesh>>& Palette,
	UStaticMesh* Fallback,
	float KitMeshScale,
	const FTransform& KitXform,
	const FVector& FitScale,
	FRandomStream& Rng,
	bool bRestBaseOnFloor,
	float FloorZ)
{
	// Deterministic pick within the palette. RandRange is called whenever the palette
	// is non-empty — regardless of whether the kit actually loads — so the seed
	// consumes identically with or without the marketplace bytes installed.
	UStaticMesh* Chosen = nullptr;
	bool bIsKit = false;
	if (Palette.Num() > 0)
	{
		const int32 Idx = Rng.RandRange(0, Palette.Num() - 1);
		Chosen = Palette[Idx].LoadSynchronous();   // null if the kit isn't installed
		bIsKit = (Chosen != nullptr);
	}
	if (!Chosen)
	{
		Chosen = Fallback;
	}
	if (!Chosen)
	{
		return;
	}

	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Chosen);
	if (!ISM)
	{
		return;
	}

	FTransform Xform = KitXform;
	// A kit mesh is authored to its grid -> uniform KitMeshScale. A fallback engine
	// shape is stretched to fill the tile/segment.
	const FVector Scale = bIsKit ? FVector(KitMeshScale) : FitScale;
	Xform.SetScale3D(Scale);

	// Seal: drop the piece so its mesh BOTTOM rests at FloorZ — works whether the pivot
	// is at the base (kit walls: bounds Min.Z=0) or centered (fallback cube). Fixes the
	// floating-wall gap that let the void/sky show through.
	if (bRestBaseOnFloor)
	{
		const float BottomLocalZ = Chosen->GetBoundingBox().Min.Z;
		FVector Loc = Xform.GetLocation();
		Loc.Z = FloorZ - BottomLocalZ * Scale.Z;
		Xform.SetLocation(Loc);
	}

	ISM->AddInstance(Xform, /*bWorldSpace=*/true);
}

int32 AElsewhereBuilder::AssembleGeometry(const FPlaceTypeDef& Place, int32 LayoutSeed)
{
	// Reset any prior build (BeginPlay builds once; the gate builds on fresh actors).
	for (UInstancedStaticMeshComponent* ISM : KitISMs)
	{
		if (ISM)
		{
			ISM->ClearInstances();
		}
	}
	ScatterISMs.Reset();          // re-tracked as PlaceScatter places (gate read-back)
	bScatterZonesValid = false;

	UStaticMesh* CubeFallback = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylFallback  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	const FVector Origin = GetActorLocation();
	const float Tile  = FMath::Max(50.f, Place.KitTileSize);
	const float WallH = FMath::Max(50.f, Place.KitWallHeight);
	const float Scale = Place.KitMeshScale;
	const float HalfX = FMath::Max(Tile, Place.RoomExtent.X);
	const float HalfY = FMath::Max(Tile, Place.RoomExtent.Y);
	const int32 NX = FMath::Max(1, FMath::RoundToInt((2.f * HalfX) / Tile));
	const int32 NY = FMath::Max(1, FMath::RoundToInt((2.f * HalfY) / Tile));

	// Grid origin (corner-of-first-tile center). Walls sit on the grid boundary so kit
	// pieces seam with the floor.
	const float GridHalfX = (NX * Tile) * 0.5f;
	const float GridHalfY = (NY * Tile) * 0.5f;
	const float X0 = Origin.X - GridHalfX + Tile * 0.5f;
	const float Y0 = Origin.Y - GridHalfY + Tile * 0.5f;

	FRandomStream Rng(LayoutSeed);

	const FVector TileFit(Tile / 100.f, Tile / 100.f, 0.2f);   // engine cube is 100cm

	// --- Floor + ceiling tiles. ---
	for (int32 i = 0; i < NX; ++i)
	{
		for (int32 j = 0; j < NY; ++j)
		{
			const float CX = X0 + i * Tile;
			const float CY = Y0 + j * Tile;

			const FTransform FloorXf(FRotator::ZeroRotator, FVector(CX, CY, Origin.Z - 10.f));
			PlacePiece(Place.FloorMeshes, CubeFallback, Scale, FloorXf, TileFit, Rng);

			const FTransform CeilXf(FRotator::ZeroRotator, FVector(CX, CY, Origin.Z + WallH));
			PlacePiece(Place.CeilingMeshes, CubeFallback, Scale, CeilXf, TileFit, Rng);
		}
	}

	// Floor material quick win — reskin the (kit) floor ISM with the real _K floor material
	// (project-side ref; no kit edit). No-op when the kit is absent (override won't load).
	ApplyFloorMaterial(Place);

	// --- Perimeter walls, with a doorway gap on the west (-X) edge. ---
	// WALL ORIENTATION CONVENTION: a wall PANEL's face-normal is along local +X and its
	// width runs along local +Y (thin in X, wide in Y, tall in Z) — this matches the kit
	// wall mesh SM_Wall_A_Mid_4x4m (bounds X≈[-20,5], Y=[-200,200], Z=[0,400]). So a panel
	// at yaw 0 faces ±X and spans Y — naturally an EAST/WEST wall. A NORTH/SOUTH wall needs
	// yaw 90 (face ±Y, span X). The fallback cube's WallFit is shaped to the same convention
	// (thin X, wide Y) so kit and fallback orient identically. (Earlier code used the opposite
	// convention, which left the kit walls rotated 90° — thin fins poking into the room with
	// gaps between them that let the void/sky show through.)
	const FVector WallFit(0.2f, Tile / 100.f, WallH / 100.f);   // thin along local X, wide along Y, tall Z
	const float WallZ = Origin.Z + WallH * 0.5f;
	const int32 DoorJ = NY / 2;   // the gap tile on the west edge
	const float DoorCY = Y0 + DoorJ * Tile;   // doorway centre Y (return trigger + scatter carve)

	// Walls rest their base on the floor (bRestBaseOnFloor) so they seal the wall-to-floor
	// band — the WallZ in the transform is overridden by the floor-rest.
	for (int32 i = 0; i < NX; ++i)   // north (+Y) and south (-Y) edges run along X -> face ±Y -> yaw 90
	{
		const float CX = X0 + i * Tile;
		const FTransform North(FRotator(0.f, 90.f, 0.f), FVector(CX, Origin.Y + GridHalfY, WallZ));
		PlacePiece(Place.WallMeshes, CubeFallback, Scale, North, WallFit, Rng, true, Origin.Z);
		const FTransform South(FRotator(0.f, 90.f, 0.f), FVector(CX, Origin.Y - GridHalfY, WallZ));
		PlacePiece(Place.WallMeshes, CubeFallback, Scale, South, WallFit, Rng, true, Origin.Z);
	}
	for (int32 j = 0; j < NY; ++j)   // east (+X) and west (-X) edges run along Y -> face ±X -> yaw 0
	{
		const float CY = Y0 + j * Tile;
		const FTransform East(FRotator(0.f, 0.f, 0.f), FVector(Origin.X + GridHalfX, CY, WallZ));
		PlacePiece(Place.WallMeshes, CubeFallback, Scale, East, WallFit, Rng, true, Origin.Z);
		if (j != DoorJ)   // leave the doorway open (the way home stands here)
		{
			const FTransform West(FRotator(0.f, 0.f, 0.f), FVector(Origin.X - GridHalfX, CY, WallZ));
			PlacePiece(Place.WallMeshes, CubeFallback, Scale, West, WallFit, Rng, true, Origin.Z);
		}
	}

	// --- Dark-void backdrop behind the west doorway ---
	// The doorway is intentionally open (the way home), but the level has no sky actor, so
	// the open portal otherwise looks out onto the engine's default blue backdrop ("daytime
	// sky"). Drop a tall, wide unlit slab well WEST of the doorway: it sits outside the room
	// in the unlit void, so the faint skylight leaves it dark navy — occluding the bright
	// background in the doorway's view cone. Added directly to the fallback-cube ISM (NO RNG
	// draw, NO palette pick) so the prop scatter + gate determinism are untouched. Placed far
	// past the return door, so it never blocks the way home.
	if (CubeFallback)
	{
		if (UInstancedStaticMeshComponent* Void = GetOrCreateISM(CubeFallback))
		{
			FTransform BackXf;
			BackXf.SetLocation(FVector(Origin.X - GridHalfX - 700.f, Origin.Y, Origin.Z + WallH));
			BackXf.SetScale3D(FVector(0.4f, (2.f * GridHalfY + 600.f) / 100.f, (WallH * 3.f) / 100.f));
			Void->AddInstance(BackXf, /*bWorldSpace=*/true);
		}
	}

	// --- THE RETURN, as an overlap trigger across the west doorway threshold ---
	// (Replaces the old E-interact / blocking approach.) Walking into the doorway returns the
	// player home — no key-press, no aim, no interact trace to be blocked, and no fall: the
	// trigger sits INSIDE the edge so they leave before reaching the drop. See SeatReturnTrigger.
	SeatReturnTrigger(Origin, GridHalfX, DoorCY, WallH);

	// --- Scattered DETAIL props (the small lived-in layer). PCG spike: when enabled AND a graph
	// is assigned, a REAL UPCGComponent owns the props (seeded), replacing the C++ scatter. The
	// C++ path (flag off, or no graph) is the fallback — and the gate's determinism handle. ---
	if (bUsePCGScatter && RunPCGScatter(Place, LayoutSeed))
	{
		UE_LOG(LogElsewhereBuilder, Verbose, TEXT("[%s] props via PCG (C++ scatter skipped)."), *GetName());
		return 0; // prop count is owned by PCG now
	}

	const int32 PropCount = PlaceScatter(Place, Origin, GridHalfX, GridHalfY, Tile, DoorCY, CylFallback, Rng);

	UE_LOG(LogElsewhereBuilder, Verbose, TEXT("[%s] assembled '%s': %dx%d tiles, walls+ceiling, %d scattered props (seed=%d)."),
		*GetName(), *Place.Id.ToString(), NX, NY, PropCount, LayoutSeed);
	return PropCount;
}

int32 AElsewhereBuilder::BuildFromPlan(
	const FElsewherePlan& Plan,
	const TArray<FPlaceTypeDef>& Places,
	const TArray<FCurioDef>& Curios)
{
	if (!Plan.IsValid())
	{
		return 0;
	}
	const FPlaceTypeDef* Place = FElsewhereGen::FindPlace(Places, Plan.PlaceTypeId);
	const FCurioDef* CurioDef = FElsewhereGen::FindCurio(Curios, Plan.CurioId);
	if (!Place || !CurioDef)
	{
		UE_LOG(LogElsewhereBuilder, Error, TEXT("[%s] plan references missing place/curio — aborting build."), *GetName());
		return 0;
	}

	const int32 PropCount = AssembleGeometry(*Place, Plan.LayoutSeed);
	ApplyMood(*Place, Plan.MoodSeed);
	if (bGodRayShafts)
	{
		SpawnGodRayShafts(*Place); // deterministic; visual-only (doesn't touch PropCount)
	}
	if (bStructuralProps)
	{
		SpawnStructuralProps(*Place); // deterministic kit machinery; doesn't touch PropCount/seed
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return PropCount;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Origin = GetActorLocation();

	// The ONE curio (§6): a glowing orb floating in the centre of the hall (the single
	// focal object). CurioOffset is the floating height above the builder origin.
	const FTransform CurioXf(FRotator::ZeroRotator, Origin + CurioOffset);
	SpawnedCurio = World->SpawnActor<ACurio>(ACurio::StaticClass(), CurioXf, SpawnParams);
	if (SpawnedCurio)
	{
		SpawnedCurio->Configure(Plan.CurioId, Plan.PlaceTypeId, Place->CurioGlowColor);
		// Real curio art from the data (else the default glowing-orb sphere).
		if (!CurioDef->Mesh.IsNull())
		{
			SpawnedCurio->SetDisplayMesh(CurioDef->Mesh.LoadSynchronous());
		}
	}

	// The way home (§3 step 6) — seated IN the west wall doorway gap (X = -RoomExtent.X,
	// Y = 0 where AssembleGeometry leaves the gap). Facing is the tunable ReturnDoorRotation
	// (default yaw 0 = aligned with the west wall panels; the old hardcoded yaw 90 read
	// sideways). It sits BEHIND the player's spawn, so it never blocks the view of the curio.
	const FVector ReturnLoc = Origin + FVector(-Place->RoomExtent.X + 50.f, 0.f, 0.f) + ReturnDoorOffset;
	const FTransform ReturnXf(ReturnDoorRotation, ReturnLoc);
	SpawnedReturnDoor = World->SpawnActor<AReturnDoor>(AReturnDoor::StaticClass(), ReturnXf, SpawnParams);

	// Light the way home: warm amber beacon just inside the doorway, raised to head height, so
	// the exit is unmistakable against the cool/dark hall.
	if (ReturnBeacon)
	{
		ReturnBeacon->SetWorldLocation(ReturnLoc + FVector(120.f, 0.f, 200.f));
		ReturnBeacon->SetLightColor(ReturnBeaconColor);
		ReturnBeacon->SetIntensity(ReturnBeaconIntensity);
		ReturnBeacon->SetAttenuationRadius(1000.f);
	}

	UE_LOG(LogElsewhereBuilder, Display,
		TEXT("[%s] built '%s' (seed=%d): %d props, curio='%s', return door=%s"),
		*GetName(), *Place->Id.ToString(), Plan.Seed, PropCount, *Plan.CurioId.ToString(),
		SpawnedReturnDoor ? TEXT("ok") : TEXT("FAILED"));

	return PropCount;
}

void AElsewhereBuilder::SpawnGodRayShafts(const FPlaceTypeDef& Place)
{
	// Clear any prior build's shafts (the gate builds on fresh actors; BeginPlay builds once).
	for (USpotLightComponent* S : ShaftLights)
	{
		if (S) { S->DestroyComponent(); }
	}
	ShaftLights.Reset();

	if (ShaftsPerWall <= 0)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const float HalfX = FMath::Max(FMath::Max(50.f, Place.KitTileSize), Place.RoomExtent.X);
	const float HalfY = FMath::Max(FMath::Max(50.f, Place.KitTileSize), Place.RoomExtent.Y);
	const float WallH = FMath::Max(50.f, Place.KitWallHeight);
	const float ShaftZ = Origin.Z + WallH * 0.85f;     // high on the wall, raking down

	// One row of shafts down each long wall (+Y north, -Y south), angled into the hall and
	// down — reads as light raking through the upper wall. Positions are pure functions of
	// the room grid (no RNG), so they always line up across visits.
	auto SpawnRow = [&](float WallY, float Yaw)
	{
		for (int32 i = 0; i < ShaftsPerWall; ++i)
		{
			const float T = (i + 1.0f) / (ShaftsPerWall + 1.0f);   // even, avoid corners
			const float X = Origin.X - HalfX + T * (2.0f * HalfX);
			USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
			Spot->SetupAttachment(SceneRoot);
			Spot->RegisterComponent();
			Spot->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepWorldTransform);
			Spot->SetWorldLocation(FVector(X, Origin.Y + WallY, ShaftZ));
			Spot->SetWorldRotation(FRotator(-55.0f, Yaw, 0.0f));   // pitch down, into the room
			Spot->SetLightColor(ShaftColor);
			Spot->SetIntensity(ShaftIntensity);
			Spot->SetInnerConeAngle(0.0f);
			Spot->SetOuterConeAngle(ShaftOuterConeDeg);
			Spot->SetAttenuationRadius(WallH * 4.0f);
			Spot->VolumetricScatteringIntensity = ShaftVolumetricScattering; // the visible beam
			Spot->CastShadows = false;                                       // god-rays, cheap
			AddInstanceComponent(Spot);
			ShaftLights.Add(Spot);
		}
	};

	SpawnRow(+HalfY, -90.0f); // north wall, point toward -Y (into the room)
	SpawnRow(-HalfY, 90.0f);  // south wall, point toward +Y

	UE_LOG(LogElsewhereBuilder, Verbose, TEXT("[%s] %d god-ray shafts."), *GetName(), ShaftLights.Num());
}

void AElsewhereBuilder::SpawnStructuralProps(const FPlaceTypeDef& Place)
{
	// Deliberate, deterministic kit machinery so the hall reads as a REAL built space rather
	// than an empty box: rows of bulkhead arch-ribs the player walks through down the hall,
	// plus pipe runs along the base of both long walls. Kit-by-path (the kit is gitignored);
	// if a mesh isn't installed we simply skip it — structure is optional dressing, so the
	// headless gate stays green with zero kit bytes. Positions are pure functions of the room
	// grid (no RNG), so they line up every visit and never perturb the prop-scatter seed.
	// The arch/pipe ISMs are tracked in KitISMs (via GetOrCreateISM) so the next build clears them.
	const FVector Origin = GetActorLocation();
	const float Tile  = FMath::Max(50.f, Place.KitTileSize);
	const float Scale = Place.KitMeshScale;
	const float HalfX = FMath::Max(Tile, Place.RoomExtent.X);
	const float HalfY = FMath::Max(Tile, Place.RoomExtent.Y);
	const int32 NX = FMath::Max(1, FMath::RoundToInt((2.f * HalfX) / Tile));
	const int32 NY = FMath::Max(1, FMath::RoundToInt((2.f * HalfY) / Tile));
	const float GridHalfX = (NX * Tile) * 0.5f;
	const float GridHalfY = (NY * Tile) * 0.5f;
	const float X0 = Origin.X - GridHalfX + Tile * 0.5f;
	const float Y0 = Origin.Y - GridHalfY + Tile * 0.5f;

	// --- Arch-ribs: rows of bulkhead gates spanning the room width (yaw 90 -> the gate's wide
	// axis runs along Y), at the 1/4 and 3/4 marks along X so they frame — not block — the
	// central curio. The player walks through the arch openings down the hall. ---
	if (UStaticMesh* Gate = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ModularSciFiEnv_K/Meshes/Bulkheads/SM_Bulkhead_A_Gate_A.SM_Bulkhead_A_Gate_A")))
	{
		if (UInstancedStaticMeshComponent* GateISM = GetOrCreateISM(Gate))
		{
			const int32 RowsX[] = { FMath::Clamp(NX / 4, 0, NX - 1), FMath::Clamp((3 * NX) / 4, 0, NX - 1) };
			for (int32 RowI : RowsX)
			{
				const float RX = X0 + RowI * Tile;
				for (int32 j = 0; j < NY; ++j)
				{
					FTransform Xf(FRotator(0.f, 90.f, 0.f), FVector(RX, Y0 + j * Tile, Origin.Z));
					Xf.SetScale3D(FVector(Scale));
					GateISM->AddInstance(Xf, /*bWorldSpace=*/true);
				}
			}
		}
	}

	// --- Pipe runs along the base of both long walls (machinery texture). The 4m pipe section
	// runs along Y by default; yaw 90 turns it to run along X (the wall direction). Inset just
	// inside the wall. ---
	if (UStaticMesh* Pipe = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ModularSciFiEnv_K/Meshes/Pipes/SM_Pipes_A_4m.SM_Pipes_A_4m")))
	{
		if (UInstancedStaticMeshComponent* PipeISM = GetOrCreateISM(Pipe))
		{
			const float Inset = Tile * 0.4f;
			for (int32 i = 0; i < NX; ++i)
			{
				const float CX = X0 + i * Tile;
				FTransform North(FRotator(0.f, 90.f, 0.f), FVector(CX, Origin.Y + GridHalfY - Inset, Origin.Z + 10.f));
				North.SetScale3D(FVector(Scale));
				PipeISM->AddInstance(North, /*bWorldSpace=*/true);
				FTransform South(FRotator(0.f, 90.f, 0.f), FVector(CX, Origin.Y - GridHalfY + Inset, Origin.Z + 10.f));
				South.SetScale3D(FVector(Scale));
				PipeISM->AddInstance(South, /*bWorldSpace=*/true);
			}
		}
	}

	UE_LOG(LogElsewhereBuilder, Verbose, TEXT("[%s] structural props placed (arch-ribs + pipe runs)."), *GetName());
}

void AElsewhereBuilder::SeatReturnTrigger(const FVector& Origin, float GridHalfX, float DoorCY, float WallH)
{
	// Create ONCE, born with the correct extent BEFORE registration (runtime-resized boxes are
	// unreliable — that bit the old blocker), then only reposition on rebuilds. It's an OVERLAP
	// volume (QueryOnly, ignore everything except a Pawn overlap), so it never blocks movement
	// or any trace — it just detects the player entering the doorway.
	if (!ReturnTrigger)
	{
		ReturnTrigger = NewObject<UBoxComponent>(this, TEXT("ReturnTrigger"));
		ReturnTrigger->SetBoxExtent(FVector(70.f, 240.f, FMath::Max(60.f, WallH * 0.5f + 50.f)));
		ReturnTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ReturnTrigger->SetCollisionObjectType(ECC_WorldStatic);
		ReturnTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		ReturnTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		ReturnTrigger->SetGenerateOverlapEvents(true);
		ReturnTrigger->SetCanEverAffectNavigation(false);
		ReturnTrigger->SetHiddenInGame(true);
		ReturnTrigger->SetVisibility(false);
		ReturnTrigger->OnComponentBeginOverlap.AddDynamic(this, &AElsewhereBuilder::OnReturnTriggerBeginOverlap);
		ReturnTrigger->SetupAttachment(SceneRoot);
		ReturnTrigger->RegisterComponent();
	}
	// Seat at the doorway threshold, ~30cm INSIDE the west edge: east of the drop (so the player
	// returns before falling) and east of the PlayerStart (so it never fires on spawn). Spans
	// the gap width and floor-to-ceiling so any approach to the doorway trips it.
	ReturnTrigger->SetWorldLocation(FVector(Origin.X - GridHalfX + 30.f, DoorCY, Origin.Z + WallH * 0.5f));

	// Arm after a short delay so a spawn-frame overlap (the fixed PlayerStart can land in the
	// doorway in smaller rooms) can't instant-return.
	bReturnArmed = false;
	if (UWorld* World = GetWorld())
	{
		FTimerHandle ArmTimer;
		World->GetTimerManager().SetTimer(ArmTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { bReturnArmed = true; }),
			FMath::Max(0.05f, ReturnArmDelay), /*bLoop=*/false);
	}
}

void AElsewhereBuilder::OnReturnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (bReturningHome || !bReturnArmed)
	{
		return;   // already returning (OpenLevel is async), or still in the spawn-frame arm delay
	}
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;   // only the player returns home
	}
	bReturningHome = true;

	// Same contract as AReturnDoor::Interact: the room is throwaway (§4), so drop the staged
	// plan, then travel home.
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (UElsewhereSubsystem* Elsewhere = GI->GetSubsystem<UElsewhereSubsystem>())
		{
			Elsewhere->DiscardStagedElsewhere();
		}
	}
	UE_LOG(LogElsewhereBuilder, Display, TEXT("[%s] doorway return trigger entered -> home to %s; Elsewhere discarded."),
		*GetName(), *HomeLevelName.ToString());
	UGameplayStatics::OpenLevel(this, HomeLevelName);
}

bool AElsewhereBuilder::RunPCGScatter(const FPlaceTypeDef& Place, int32 LayoutSeed)
{
	if (!PCGComponent)
	{
		return false;
	}
	UPCGGraphInterface* Graph = ScatterGraph.LoadSynchronous();
	if (!Graph)
	{
		// Flag on but no graph wired yet — fall back to the C++ scatter (caller handles it).
		UE_LOG(LogElsewhereBuilder, Warning,
			TEXT("[%s] bUsePCGScatter is on but no ScatterGraph is assigned — using C++ scatter fallback."),
			*GetName());
		return false;
	}

	// Real UE5 PCG: assign the graph + seed now (deterministic: same seed -> same scatter).
	PCGComponent->SetGraph(Graph);
	PCGComponent->Seed = LayoutSeed;

	// DEFER Generate() one tick. We're mid-AssembleGeometry inside BeginPlay: the floor/wall
	// ISMs were just built but their world bounds haven't been recomputed yet, so the actor's
	// aggregate bounds are still invalid. UPCGComponent::CreateGenerateTask aborts on an
	// invalid GetGridBounds() ("Component has invalid bounds, not registered nor updated") and
	// the graph never runs -> 0 props. One tick later the ISM bounds are valid, the component
	// registers, and the graph executes. (The graph itself no longer raycasts the floor — see
	// build_pcg_scatter_graph.py — so this is purely about valid bounds, not live collision.)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AElsewhereBuilder::GeneratePCGScatterDeferred));
	}
	else
	{
		PCGComponent->Generate(/*bForce=*/true);   // no world (shouldn't happen in PIE) — best effort
	}

	UE_LOG(LogElsewhereBuilder, Display, TEXT("[%s] PCG scatter scheduled (deferred 1 tick) via '%s' (seed=%d) for place '%s'."),
		*GetName(), *Graph->GetName(), LayoutSeed, *Place.Id.ToString());
	return true;
}

void AElsewhereBuilder::GeneratePCGScatterDeferred()
{
	// Fires one tick after RunPCGScatter scheduled it (see why there). By now the runtime ISM
	// bounds are valid, so GetGridBounds() is valid and generation won't abort.
	if (PCGComponent && PCGComponent->GetGraph())
	{
		PCGComponent->Generate(/*bForce=*/true);
		UE_LOG(LogElsewhereBuilder, Verbose, TEXT("[%s] deferred PCG Generate fired."), *GetName());
	}
}

void AElsewhereBuilder::ApplyMood(const FPlaceTypeDef& Place, int32 MoodSeed)
{
	// Tint the ambient glow from the place, with a small seeded jitter so two visits to
	// the same place-type still feel a touch different (§4 variation lever).
	FRandomStream Rng(MoodSeed);
	const float Jitter = Rng.FRandRange(0.85f, 1.15f);
	MoodLight->SetLightColor(Place.AmbientColor);
	MoodLight->SetIntensity(Place.LightIntensity * 2000.f * Jitter);
	MoodLight->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, Place.RoomExtent.Z * 0.5f));
}

TArray<FScatterMeshDef> AElsewhereBuilder::ResolveScatterPalette(const FPlaceTypeDef& Place) const
{
	// Resolution order: the place's own ScatterMeshes (per-place override, authored in DT/code) ->
	// the builder's curated ScatterSet default (drives the look for every undressed place) -> a
	// basic palette synthesised from the legacy PropMeshes (last resort). PURE — no RNG, no loads —
	// so the palette SIZE is a function of DATA only (identical kit-present/absent), which the
	// scatter's fixed draw budget depends on for determinism.
	if (Place.ScatterMeshes.Num() > 0)
	{
		return Place.ScatterMeshes;
	}
	if (ScatterSet.Num() > 0)
	{
		return ScatterSet;
	}
	TArray<FScatterMeshDef> Out;
	Out.Reserve(Place.PropMeshes.Num());
	for (const TSoftObjectPtr<UStaticMesh>& M : Place.PropMeshes)
	{
		FScatterMeshDef D;
		D.Mesh = M;
		D.Weight = 10;
		D.bUpright = true;
		D.ScaleMin = 0.9f;
		D.ScaleMax = 1.3f;
		D.LeanJitterDeg = 0.f;
		Out.Add(D);
	}
	return Out;
}

bool AElsewhereBuilder::IsScatterExcluded(const FVector2D& WorldXY) const
{
	const FVector P(WorldXY.X, WorldXY.Y, ScatterFloorZ);
	// Carve discs: keep the focal curio clear, the doorway/return/beacon clear, and the arrival
	// bay clear (for a big hall the door+spawn discs overlap and clear the whole west bay).
	if (FVector::Dist2D(P, CurioCenterWS) < CurioCarveR) { return true; }
	if (FVector::Dist2D(P, DoorCenterWS)  < DoorCarveR)  { return true; }
	if (FVector::Dist2D(P, SpawnCenterWS) < SpawnCarveR) { return true; }
	// Open central corridor: a clear walk-lane from the west doorway to the curio (props may
	// still fill BEHIND the curio, framing it). Band around the doorway's Y, on the approach side.
	if (CorridorHalfWS > 0.f && FMath::Abs(P.Y - DoorCenterWS.Y) < CorridorHalfWS && P.X < CurioCenterWS.X)
	{
		return true;
	}
	return false;
}

void AElsewhereBuilder::PlaceScatterPiece(const FScatterMeshDef& Def, UStaticMesh* CylFallback,
	float KitMeshScale, float PerMeshScale, const FTransform& BaseXform, bool bRestBaseOnFloor, float FloorZ)
{
	// NO RNG drawn here: the caller already spent the selection/transform draws, so kit-present
	// and kit-absent consume the stream identically — only WHICH mesh is added differs.
	UStaticMesh* KitMesh = Def.Mesh.LoadSynchronous();   // null when the kit isn't installed
	const bool bIsKit = (KitMesh != nullptr);
	UStaticMesh* Use = bIsKit ? KitMesh : CylFallback;
	if (!Use)
	{
		return;
	}
	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Use);
	if (!ISM)
	{
		return;
	}
	ScatterISMs.AddUnique(ISM);   // track the scatter layer for the gate read-back

	FTransform Xform = BaseXform;
	// Kit meshes are authored to ~unit scale (× KitMeshScale × the per-instance band). The
	// fallback cylinder gets the band directly, stretched a little taller so it reads upright.
	const FVector Scale = bIsKit
		? FVector(KitMeshScale * PerMeshScale)
		: FVector(PerMeshScale, PerMeshScale, PerMeshScale * 1.4f);
	Xform.SetScale3D(Scale);

	// Rest the mesh bottom on the floor (handles base-pivot AND centered/odd pivots via Min.Z).
	if (bRestBaseOnFloor)
	{
		const float BottomLocalZ = Use->GetBoundingBox().Min.Z;
		FVector Loc = Xform.GetLocation();
		Loc.Z = FloorZ - BottomLocalZ * Scale.Z;
		Xform.SetLocation(Loc);
	}

	ISM->AddInstance(Xform, /*bWorldSpace=*/true);
}

int32 AElsewhereBuilder::PlaceScatter(const FPlaceTypeDef& Place, const FVector& Origin,
	float GridHalfX, float GridHalfY, float Tile, float DoorCY,
	UStaticMesh* CylFallback, FRandomStream& Rng)
{
	// Deterministic, data-driven, per-seed-varying scatter — the gate's determinism handle.
	// Returns the INSTANCED prop count (a pure function of the seed). Every randomness source is
	// the single passed-in Rng, drawn in the FIXED order documented below so two fresh actors from
	// the same plan produce identical transforms AND count, and so kit-absent consumes the stream
	// byte-identically to kit-present (the only load-dependent call, PlaceScatterPiece, draws nothing).

	const TArray<FScatterMeshDef> Full = ResolveScatterPalette(Place);   // pure; size = data
	const int32 P = Full.Num();

	// Cache the carve geometry (no draws) so the gate asserts against the exact numbers used.
	CurioCenterWS  = Origin + CurioOffset;                                    // the focal curio
	DoorCenterWS   = FVector(Origin.X - GridHalfX, DoorCY, Origin.Z);         // west doorway / return / beacon
	SpawnCenterWS  = Origin + FVector(-800.f, 0.f, 0.f);                      // PlayerStart (west bay, builder-relative)
	CurioCarveR    = FMath::Max(0.f, CurioExclusionRadius);
	DoorCarveR     = FMath::Max(0.f, DoorwayExclusionRadius);
	SpawnCarveR    = FMath::Max(0.f, SpawnExclusionRadius);
	CorridorHalfWS = FMath::Max(0.f, Place.CorridorHalfWidth);
	ScatterFloorZ  = Origin.Z;
	bScatterZonesValid = true;

	// --- Phase 1: per-seed composition (subset) + density. ---
	// 1.1 subset size (1 draw).
	const int32 SubLo = FMath::Max(1, ScatterSubsetMin);
	const int32 SubHi = FMath::Max(SubLo, ScatterSubsetMax);
	int32 SubsetSize = Rng.RandRange(SubLo, SubHi);
	SubsetSize = FMath::Clamp(SubsetSize, 1, FMath::Max(1, P));
	// 1.2 Fisher–Yates shuffle of [0..P-1] (P-1 draws; 0 when P<=1). P is data -> count is parity-safe.
	TArray<int32> Idx;
	Idx.Reserve(P);
	for (int32 i = 0; i < P; ++i) { Idx.Add(i); }
	for (int32 i = P - 1; i >= 1; --i)
	{
		const int32 j = Rng.RandRange(0, i);
		Idx.Swap(i, j);
	}
	// Active subset + parallel weights.
	TArray<int32> Active;
	TArray<int32> ActiveW;
	Active.Reserve(SubsetSize);
	ActiveW.Reserve(SubsetSize);
	for (int32 k = 0; k < SubsetSize && k < P; ++k)
	{
		Active.Add(Idx[k]);
		ActiveW.Add(FMath::Max(0, Full[Idx[k]].Weight));
	}
	int32 ActiveTotal = 0;
	for (int32 W : ActiveW) { ActiveTotal += W; }
	// 1.3 base count (1 draw) -> per-seed density. The place's PropCount is tuned sparse (a few
	// hero props); ScatterCountScale (builder-global, DT-independent) scales it up to a debris-PILE
	// count without editing the DataTable. The per-seed Density wobble still rides on top.
	const int32 BaseCount = Rng.RandRange(Place.PropCountMin, Place.PropCountMax);
	const float Density   = FMath::Max(0.1f, Place.ScatterDensity);
	const float CountScale = FMath::Max(0.f, ScatterCountScale);
	const int32 TargetCount = FMath::Clamp(FMath::RoundToInt(BaseCount * Density * CountScale), 0, 64);

	// --- Phase 2: cluster centres (the lived-in bunching), in the interior (2 draws each). ---
	const float InnerX = FMath::Max(Tile, GridHalfX - Tile);
	const float InnerY = FMath::Max(Tile, GridHalfY - Tile);
	const int32 NumClusters = FMath::Clamp(1 + TargetCount / 4, 1, 5);
	const float WallBias = FMath::Clamp(ScatterWallBias, 0.f, 1.f);
	TArray<FVector2D> Clusters;
	Clusters.Reserve(NumClusters);
	for (int32 c = 0; c < NumClusters; ++c)
	{
		// Two draws each (parity-safe). Then lerp the centre toward the nearest wall/corner so the
		// pile gathers at the perimeter where debris settles — pure math on the draws, no extra RNG.
		float CX = Rng.FRandRange(-InnerX, InnerX);
		float CY = Rng.FRandRange(-InnerY, InnerY);
		CX = FMath::Lerp(CX, FMath::Sign(CX) * InnerX, WallBias);
		CY = FMath::Lerp(CY, FMath::Sign(CY) * InnerY, WallBias);
		Clusters.Add(FVector2D(CX, CY));
	}

	// --- Phase 3: per prop — a HARD-CONSTANT budget of (3K + 5) draws each, independent of
	// exclusion hits and mesh-load state (the determinism + kit-absent-parity guarantee). ---
	const int32 K = FMath::Clamp(ScatterPlacementTries, 1, 16);
	const float ClusterBias = FMath::Clamp(ScatterClusterBias, 0.f, 1.f);
	const FScatterMeshDef DefaultDef;   // weight 10, upright, 0.9–1.2 — used if the subset is empty
	int32 Instanced = 0;

	for (int32 p = 0; p < TargetCount; ++p)
	{
		// 3a. Pre-draw K candidate positions (3K draws ALWAYS); take the first clear one.
		bool bFound = false;
		FVector2D Cand(0.f, 0.f);
		for (int32 k = 0; k < K; ++k)
		{
			const float Sel = Rng.FRand();              // draw
			const float OX  = Rng.FRandRange(-1.f, 1.f); // draw
			const float OY  = Rng.FRandRange(-1.f, 1.f); // draw
			if (bFound)
			{
				continue;   // keep drawing all K (budget constant), but stop testing
			}
			FVector2D C;
			if (Clusters.Num() > 0 && Sel < ClusterBias)
			{
				// Fold the cluster index out of Sel (no extra draw); jitter by ±Tile.
				const int32 Ci = FMath::Clamp(
					int32((Sel / FMath::Max(KINDA_SMALL_NUMBER, ClusterBias)) * Clusters.Num()),
					0, Clusters.Num() - 1);
				C = Clusters[Ci] + FVector2D(OX, OY) * Tile;
			}
			else
			{
				C = FVector2D(OX * InnerX, OY * InnerY);
			}
			const FVector2D WorldXY(Origin.X + C.X, Origin.Y + C.Y);
			if (!IsScatterExcluded(WorldXY))
			{
				Cand = C;
				bFound = true;
			}
		}

		// 3b. Per-prop mesh + transform (5 draws ALWAYS, even if the prop dropped).
		int32 Sel = INDEX_NONE;
		if (ActiveTotal > 0)
		{
			Sel = FElsewhereGen::WeightedPick(Rng, ActiveW);   // 1 draw
		}
		else
		{
			Rng.RandRange(0, 0);                                // hold the slot's 1 draw
		}
		const FScatterMeshDef& Def = (Sel != INDEX_NONE && Sel < Active.Num()) ? Full[Active[Sel]] : DefaultDef;
		const float S     = Rng.FRandRange(Def.ScaleMin, Def.ScaleMax);          // draw
		const float Yaw   = Rng.FRandRange(0.f, 360.f);                          // draw
		const float Pitch = Rng.FRandRange(-1.f, 1.f) * Def.LeanJitterDeg;       // draw (×0 if upright)
		const float Roll  = Rng.FRandRange(-1.f, 1.f) * Def.LeanJitterDeg;       // draw (×0 if upright)

		// 3c. Place (only when a clear candidate was found; no RNG here).
		if (bFound)
		{
			const FRotator Rot = Def.bUpright ? FRotator(0.f, Yaw, 0.f) : FRotator(Pitch, Yaw, Roll);
			const FTransform BaseXf(Rot, FVector(Origin.X + Cand.X, Origin.Y + Cand.Y, Origin.Z));
			PlaceScatterPiece(Def, CylFallback, Place.KitMeshScale, S, BaseXf, Def.bUpright, Origin.Z);
			++Instanced;
		}
	}

	return Instanced;
}

void AElsewhereBuilder::ApplyFloorMaterial(const FPlaceTypeDef& Place)
{
	// Reskin the FLOOR ISM with the real _K floor material (explicit project-side reference; we
	// never modify the kit). Applied ONLY to the kit floor ISM (the override is null when the kit
	// is absent -> the fallback-cube floor is never touched, so the headless gate is unaffected
	// AND we never trip the Used-With-Instanced-Static-Meshes editor prompt on a cube).
	UMaterialInterface* FloorMat = FloorMaterialOverride.LoadSynchronous();
	if (!FloorMat)
	{
		return;   // override unset, or kit absent
	}
	for (const TSoftObjectPtr<UStaticMesh>& FloorRef : Place.FloorMeshes)
	{
		UStaticMesh* FloorMesh = FloorRef.LoadSynchronous();
		if (!FloorMesh)
		{
			continue;   // this floor slot is a fallback cube (kit absent) — leave it
		}
		if (UInstancedStaticMeshComponent* FloorISM = GetOrCreateISM(FloorMesh))   // returns the existing floor ISM
		{
			const int32 NumMats = FloorISM->GetNumMaterials();
			for (int32 s = 0; s < NumMats; ++s)
			{
				FloorISM->SetMaterial(s, FloorMat);
			}
		}
	}
}

int32 AElsewhereBuilder::GetScatterInstanceTransforms(TArray<FTransform>& OutXforms) const
{
	OutXforms.Reset();
	for (const TObjectPtr<UInstancedStaticMeshComponent>& ISM : ScatterISMs)
	{
		if (!ISM)
		{
			continue;
		}
		const int32 N = ISM->GetInstanceCount();
		for (int32 i = 0; i < N; ++i)
		{
			FTransform T;
			if (ISM->GetInstanceTransform(i, T, /*bWorldSpace=*/true))
			{
				OutXforms.Add(T);
			}
		}
	}
	return OutXforms.Num();
}

bool AElsewhereBuilder::GetScatterExclusionZones(
	FVector& OutCurioCenter, float& OutCurioRadius,
	FVector& OutDoorCenter,  float& OutDoorRadius,
	FVector& OutSpawnCenter, float& OutSpawnRadius,
	float& OutCorridorHalfWidth, float& OutFloorZ) const
{
	if (!bScatterZonesValid)
	{
		return false;
	}
	OutCurioCenter = CurioCenterWS; OutCurioRadius = CurioCarveR;
	OutDoorCenter  = DoorCenterWS;  OutDoorRadius  = DoorCarveR;
	OutSpawnCenter = SpawnCenterWS; OutSpawnRadius = SpawnCarveR;
	OutCorridorHalfWidth = CorridorHalfWS;
	OutFloorZ = ScatterFloorZ;
	return true;
}
