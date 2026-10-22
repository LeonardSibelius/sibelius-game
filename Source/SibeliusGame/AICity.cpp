// AICity.cpp - see the header for what the AI city is, and why it turns.

#include "AICity.h"

#include "SibeliusHUD.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace AICityDetail
{
	constexpr double ShapeSize = 100.0;              // the engine's basic shapes are a metre across

	// The fallback lattice's three shells, inner to outer: their radius and their light.
	constexpr double ShellRadius[AAICity::ShellCount] = { 34000.0, 46000.0, 60000.0 };
	const FVector ShellAxis[AAICity::ShellCount] = {
		FVector(0.0, 0.0, 1.0), FVector(0.3, 0.0, 1.0).GetSafeNormal(), FVector(0.0, -0.4, 1.0).GetSafeNormal() };

	// LINEAR colours, which is what a material multiplies.
	const FLinearColor StarColour(1.0f, 0.86f, 0.62f);
	const FLinearColor CoronaColour(1.0f, 0.58f, 0.24f);
	const FLinearColor PadDark(0.012f, 0.014f, 0.02f);
	const FLinearColor CityLight[AAICity::ShellCount] = {
		FLinearColor(0.25f, 0.85f, 1.0f), FLinearColor(1.0f, 0.72f, 0.35f), FLinearColor(0.7f, 0.5f, 1.0f) };
	const FLinearColor Edge(0.3f, 0.85f, 1.0f);

	FTransform Bar(const FQuat& Turn, const FVector& At, const FVector& Size)
	{
		return FTransform(Turn, At, Size / ShapeSize);
	}

	FQuat Facing(const FVector& Forward, const FVector& UpHint)
	{
		return FRotationMatrix::MakeFromXZ(Forward, UpHint).ToQuat();
	}

	/* THE FALLBACK LATTICE (2026-10-18 to -22 the city itself; Walt: "glowing geodesic lattice. i
	   like it", and four days later "lines and dots"). A Matrioshka brain: each shell a web round
	   the star, struts between nodes. Drawn only where the dome's pack is not here.

	   Each shell is a geodesic sphere - an icosahedron, its triangles split LatticeLevel times
	   and pushed out to the sphere - turned at random so no two shells line up. One connection in
	   seven is left out; one node in twenty is a hub, bigger and brighter. */
	constexpr int32 LatticeLevel[AAICity::ShellCount] = { 3, 3, 3 };   // 1,920 struts each
	constexpr double StrutThick = 0.0028;    // of the shell's radius
	constexpr double NodeSize = 0.0095;      // of the shell's radius
	constexpr float MissingStrut = 0.14f;
	constexpr float HubShare = 0.05f;

	// A geodesic sphere of unit radius: its vertices, and each edge once.
	void Geodesic(int32 Level, TArray<FVector>& Verts, TArray<FIntPoint>& Edges)
	{
		const double T = (1.0 + FMath::Sqrt(5.0)) * 0.5;
		Verts = {
			FVector(-1.0, T, 0.0), FVector(1.0, T, 0.0), FVector(-1.0, -T, 0.0), FVector(1.0, -T, 0.0),
			FVector(0.0, -1.0, T), FVector(0.0, 1.0, T), FVector(0.0, -1.0, -T), FVector(0.0, 1.0, -T),
			FVector(T, 0.0, -1.0), FVector(T, 0.0, 1.0), FVector(-T, 0.0, -1.0), FVector(-T, 0.0, 1.0) };
		for (FVector& V : Verts)
		{
			V.Normalize();
		}
		TArray<FIntVector> Faces = {
			FIntVector(0, 11, 5), FIntVector(0, 5, 1), FIntVector(0, 1, 7), FIntVector(0, 7, 10), FIntVector(0, 10, 11),
			FIntVector(1, 5, 9), FIntVector(5, 11, 4), FIntVector(11, 10, 2), FIntVector(10, 7, 6), FIntVector(7, 1, 8),
			FIntVector(3, 9, 4), FIntVector(3, 4, 2), FIntVector(3, 2, 6), FIntVector(3, 6, 8), FIntVector(3, 8, 9),
			FIntVector(4, 9, 5), FIntVector(2, 4, 11), FIntVector(6, 2, 10), FIntVector(8, 6, 7), FIntVector(9, 8, 1) };
		auto Key = [](int32 A, int32 B)
		{
			return A < B ? (static_cast<uint64>(A) << 32) | static_cast<uint32>(B)
				: (static_cast<uint64>(B) << 32) | static_cast<uint32>(A);
		};
		for (int32 L = 0; L < Level; ++L)
		{
			// Split every triangle in four, sharing each edge's midpoint with its neighbour.
			TMap<uint64, int32> Middles;
			auto Middle = [&](int32 A, int32 B)
			{
				if (const int32* Found = Middles.Find(Key(A, B)))
				{
					return *Found;
				}
				const int32 Made = Verts.Add(((Verts[A] + Verts[B]) * 0.5).GetSafeNormal());
				Middles.Add(Key(A, B), Made);
				return Made;
			};
			TArray<FIntVector> Split;
			Split.Reserve(Faces.Num() * 4);
			for (const FIntVector& F : Faces)
			{
				const int32 AB = Middle(F.X, F.Y);
				const int32 BC = Middle(F.Y, F.Z);
				const int32 CA = Middle(F.Z, F.X);
				Split.Add(FIntVector(F.X, AB, CA));
				Split.Add(FIntVector(F.Y, BC, AB));
				Split.Add(FIntVector(F.Z, CA, BC));
				Split.Add(FIntVector(AB, BC, CA));
			}
			Faces = MoveTemp(Split);
		}
		TSet<uint64> Seen;
		Edges.Reset();
		for (const FIntVector& F : Faces)
		{
			const FIntPoint Sides[3] = { FIntPoint(F.X, F.Y), FIntPoint(F.Y, F.Z), FIntPoint(F.Z, F.X) };
			for (const FIntPoint& E : Sides)
			{
				bool bAlready = false;
				Seen.Add(Key(E.X, E.Y), &bAlready);
				if (!bAlready)
				{
					Edges.Add(E);
				}
			}
		}
	}

	void LayLattice(UInstancedStaticMeshComponent* Struts, UInstancedStaticMeshComponent* Nodes, int32 Shell, double Radius)
	{
		// The Blender-made rod and ball (build_genesis_meshes_blender.py), a metre each like the
		// engine's shapes; the engine's cube and sphere stand in until they are imported.
		if (UStaticMesh* Rod = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Genesis/SM_GenesisStrut.SM_GenesisStrut"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Struts->SetStaticMesh(Rod);
		}
		if (UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Genesis/SM_GenesisNode.SM_GenesisNode"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Nodes->SetStaticMesh(Ball);
		}
		Struts->ClearInstances();
		Nodes->ClearInstances();
		Struts->SetNumCustomDataFloats(4);
		Nodes->SetNumCustomDataFloats(1);

		TArray<FVector> Verts;
		TArray<FIntPoint> Edges;
		Geodesic(LatticeLevel[Shell], Verts, Edges);
		FRandomStream Rng(3000 + Shell);
		FVector Axis(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f));
		Axis = Axis.IsNearlyZero() ? FVector::UpVector : Axis.GetSafeNormal();
		const FQuat Twist(Axis, Rng.FRandRange(0.0f, PI));
		for (FVector& V : Verts)
		{
			V = Twist.RotateVector(V) * Radius;
		}

		// The struts. Custom data: 0 the distance at the rod's start, 1 what its length adds
		// (negative when it runs backward, so comets cross both ways), 2 speed, 3 brightness.
		const double Thick = Radius * StrutThick;
		TArray<FTransform> StrutList;
		TArray<float> StrutData;
		TArray<int32> Links;
		Links.Init(0, Verts.Num());
		for (const FIntPoint& E : Edges)
		{
			if (Rng.FRand() < MissingStrut)
			{
				continue;
			}
			const FVector& A = Verts[E.X];
			const FVector& B = Verts[E.Y];
			const FVector Along = B - A;
			const double Length = Along.Size();
			StrutList.Add(FTransform(FRotationMatrix::MakeFromX(Along).ToQuat(), (A + B) * 0.5, FVector(Length, Thick, Thick) / ShapeSize));
			const double Start = Rng.FRandRange(0.0f, 20000.0f);
			const bool bBackward = Rng.FRand() < 0.5f;
			StrutData.Add(static_cast<float>(bBackward ? Start + Length : Start));
			StrutData.Add(static_cast<float>(bBackward ? -Length : Length));
			StrutData.Add(Rng.FRandRange(0.6f, 1.4f));
			StrutData.Add(Rng.FRandRange(0.6f, 1.0f));
			++Links[E.X];
			++Links[E.Y];
		}

		// The nodes, wherever a strut still arrives. Custom data 0: brightness - a hub's is high.
		TArray<FTransform> NodeList;
		TArray<float> NodeData;
		for (int32 V = 0; V < Verts.Num(); ++V)
		{
			if (Links[V] == 0)
			{
				continue;
			}
			const bool bHub = Rng.FRand() < HubShare;
			NodeList.Add(FTransform(FQuat::Identity, Verts[V], FVector(Radius * NodeSize * (bHub ? 2.4 : 1.0) / ShapeSize)));
			NodeData.Add(bHub ? 1.8f : Rng.FRandRange(0.5f, 1.0f));
		}

		Struts->AddInstances(StrutList, false);
		for (int32 I = 0; I < StrutList.Num(); ++I)
		{
			Struts->SetCustomData(I, TArrayView<const float>(&StrutData[I * 4], 4), false);
		}
		Nodes->AddInstances(NodeList, false);
		for (int32 I = 0; I < NodeList.Num(); ++I)
		{
			Nodes->SetCustomData(I, TArrayView<const float>(&NodeData[I], 1), false);
		}
		Struts->MarkRenderStateDirty();
		Nodes->MarkRenderStateDirty();
	}

	/* THE SUN OF CELESTIAL FORTUNE (Walt, 2026-10-15: "could the sun here be made to look like
	   my sun in https://leonardsibelius.com/celestial-fortune/ ?"). The slot's own sun - its
	   SVG rendered to T_Sun (make_sun_texture.mjs) - alive in M_GenesisSunSoft (a slow turn, a
	   shimmer, a breath), on a flat disc that turns to face whoever is looking (FaceSun): since
	   2026-10-22 he sees it from every side. Until the material is made, the sphere and its glass
	   corona stand in. */
	UMaterialInterface* SunDiscMaterial()
	{
		if (UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Genesis/M_GenesisSunSoft.M_GenesisSunSoft"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Soft;
		}
		if (UMaterialInterface* Live = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Genesis/M_GenesisSunLive.M_GenesisSunLive"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Live;
		}
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Genesis/M_GenesisSunDisc.M_GenesisSunDisc"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	// In the SVG the disc is 67 of a 200-unit square: the rest is corona and prominences.
	constexpr double SunSquarePerRadius = 200.0 / 67.0;

	/* THE DOME'S LAYERS, inner to outer (the pack's demo map, Main, places all of them on one
	   spot): its blueprints, which build their own materials from ~28 look variables each. Every
	   mesh is a whole shell (half-extent 333 cm the inner panels, 355 the buildings), so one
	   scale fits them all. The pack's translucent Shield is left out: a glass skin over the city. */
	const TCHAR* const DomeLayerClass[AAICity::DomeLayerCount] = {
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BPC_InternalPanels.BPC_InternalPanels_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BPC_InternalStructures.BPC_InternalStructures_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/BP_Dome1.BP_Dome1_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BPC_CellStructures.BPC_CellStructures_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BPC_SurfacePanels.BPC_SurfacePanels_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BPC_SurfaceStructures.BPC_SurfaceStructures_C"),
		TEXT("/Game/Megastructure_Dome_1/Blueprints/Dome1_Modules/BP_Buildings.BP_Buildings_C"),
	};
	constexpr double DomeNativeRadius = 345.0;    // the pack's shell at scale 1 (Dome1Base: 343)

	/* THE LANDINGS, measured 2026-10-22 from the pack's meshes (their OBJ exports, rays cast out
	   from the star): the dome is a Goldberg sphere of 162 cells; at each of its 320 junctions a
	   spire stands ~170 m toward the star, and along every frame between two junctions runs a
	   ridge 60-100 m high with a flat top 25-32 m wide - the landing ground. Each is the ridge's
	   top a little short of halfway along its frame, at DomeRadius 345000: the direction out to it
	   from the star, the ridge's direction there, and the top's radius. The two are on opposite
	   sides of the sphere. The way in is through the cell beside B (EntryCell), so the flight in
	   crosses the whole sphere and lands at A. */
	struct FLanding { FVector Dir; FVector Along; double RidgeRadius; };
	const FLanding Landings[AAICity::PadCount] = {
		{ FVector(-0.048082, -0.100174, -0.993807), FVector(0.948025, -0.317896, -0.013824), 329179.0 },   // A
		{ FVector(0.048642, 0.099831, 0.993815), FVector(-0.948291, 0.317069, 0.014564), 320743.0 },       // B
	};
	const FVector EntryCell(0.08451, 0.24236, 0.96650);   // a hexagon, 7.1 degrees clear, 8.6 from B
	constexpr double MeasuredDomeRadius = 345000.0;
	constexpr double EyeHeight = 165.0;                  // the first-person camera, above the floor
	constexpr double StartDistance = 2500000.0;          // the flight in starts 25 km out
	constexpr double SkyRadius = 20000000.0;             // the far stars: 200 km, well beyond that
}

AAICity::AAICity()
{
	using namespace AICityDetail;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowFinder(TEXT("/Game/Genesis/M_GenesisGlow.M_GenesisGlow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassFinder(TEXT("/Game/Genesis/M_GenesisGlass.M_GenesisGlass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	GlowMaterial = GlowFinder.Object;
	GlassMaterial = GlassFinder.Object;
	SurfaceMaterial = SurfaceFinder.Object;
	UStaticMesh* Cube = CubeMesh.Object;
	UStaticMesh* Sphere = SphereMesh.Object;
	UStaticMesh* Cylinder = CylinderMesh.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	auto Quiet = [](UPrimitiveComponent* Part, bool bSolid)
	{
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetReceivesDecals(false);
		Part->SetCastShadow(false);
		Part->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	};
	auto MakeMesh = [this, &Quiet](const FString& Name, UStaticMesh* Mesh, bool bSolid)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Part->SetupAttachment(SceneRoot);
		Part->SetStaticMesh(Mesh);
		Quiet(Part, bSolid);
		return Part;
	};
	auto MakeMany = [this, &Quiet](const FString& Name, UStaticMesh* Mesh, bool bSolid)
	{
		UInstancedStaticMeshComponent* Part = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*Name);
		Part->SetupAttachment(SceneRoot);
		Part->SetStaticMesh(Mesh);
		Quiet(Part, bSolid);
		return Part;
	};

	Star = MakeMesh(TEXT("Star"), Sphere, false);
	Corona = MakeMesh(TEXT("Corona"), Sphere, false);
	for (int32 S = 0; S < ShellCount; ++S)
	{
		ShellStruts.Add(MakeMany(FString::Printf(TEXT("ShellStruts_%d"), S), Cube, false));
		ShellNodes.Add(MakeMany(FString::Printf(TEXT("ShellNodes_%d"), S), Sphere, false));
	}
	SkyStars = MakeMany(TEXT("SkyStars"), Sphere, false);
	for (int32 P = 0; P < PadCount; ++P)
	{
		PadFloors.Add(MakeMesh(FString::Printf(TEXT("PadFloor_%d"), P), Cylinder, true));
	}
	PadRails = MakeMany(TEXT("PadRails"), Cube, true);
	PadLines = MakeMany(TEXT("PadLines"), Cube, false);
	PadLegs = MakeMany(TEXT("PadLegs"), Cylinder, false);

	StarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarLight"));
	StarLight->SetupAttachment(SceneRoot);
	StarLight->SetMobility(EComponentMobility::Movable);
	StarLight->SetCastShadows(false);
	StarLight->SetLightColor(StarColour);
	StarLight->SetLightingChannels(true, true, false);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);
	FillLight->SetLightColor(FLinearColor(0.35f, 0.45f, 0.8f));
	FillLight->SetLightingChannels(true, true, false);
	// Travelling +X and a little down: it lights what faces away from the star.
	FillLight->SetRelativeRotation(FRotator(-20.0, 0.0, 0.0));

	Look = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Look"));
	Look->SetupAttachment(SceneRoot);
	Look->bUnbound = true;

	for (int32 L = 0; L < DomeLayerCount; ++L)
	{
		UChildActorComponent* Layer = CreateDefaultSubobject<UChildActorComponent>(*FString::Printf(TEXT("DomeLayer_%d"), L));
		Layer->SetupAttachment(SceneRoot);
		DomeLayers.Add(Layer);
	}

	FlightCam = CreateDefaultSubobject<UCameraComponent>(TEXT("FlightCam"));
	FlightCam->SetupAttachment(SceneRoot);
	FlightCam->SetFieldOfView(90.0f);
	FlightCam->bConstrainAspectRatio = false;
}

void AAICity::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
	Paint();
	Light();
}

void AAICity::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	/* BEFORE ANYONE IS SPAWNED. In Play the layers are spawned afresh when their components
	   register, with the pack's own collision: one convex hull per mesh, which for a shell is a
	   solid ball 6.9 km across. Leonard was placed inside it before BeginPlay quieted it, and the
	   physics threw him out of the world (2026-10-22, "game is stuck": no pawn, the camera at
	   KillZ). This runs after the layers exist and before any player logs in. */
	QuietDome();
}

void AAICity::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Paint();
	Light();
	// The visit starts in Tick, once there is a pawn to take: the flight in, from 25 km out.
}

UMaterialInstanceDynamic* AAICity::Glow(const FLinearColor& Color, float Intensity)
{
	UMaterialInstanceDynamic* M = GlowMaterial ? UMaterialInstanceDynamic::Create(GlowMaterial, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}
	return M;
}

UMaterialInstanceDynamic* AAICity::Glass(const FLinearColor& Color, float Intensity, float RimOpacity, float BaseOpacity)
{
	UMaterialInstanceDynamic* M = GlassMaterial ? UMaterialInstanceDynamic::Create(GlassMaterial, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		M->SetScalarParameterValue(TEXT("RimOpacity"), RimOpacity);
		M->SetScalarParameterValue(TEXT("BaseOpacity"), BaseOpacity);
	}
	return M;
}

UMaterialInstanceDynamic* AAICity::Surface(const FLinearColor& Color, float Roughness)
{
	UMaterialInstanceDynamic* M = SurfaceMaterial ? UMaterialInstanceDynamic::Create(SurfaceMaterial, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
	}
	return M;
}

FTransform AAICity::PadFrame(int32 Pad) const
{
	using namespace AICityDetail;
	const FLanding& Spot = Landings[FMath::Clamp(Pad, 0, PadCount - 1)];
	const FVector Out = Spot.Dir.GetSafeNormal();
	const FVector Up = -Out;
	const double Radius = Spot.RidgeRadius * (DomeRadius / MeasuredDomeRadius) - PadLift;
	return FTransform(FRotationMatrix::MakeFromXZ(Spot.Along, Up).ToQuat(), Out * Radius);
}

FQuat AAICity::PadBelow(int32 Pad) const
{
	// The pad's own frame, undone: its ridge to world X, its up (toward the star) to world Z.
	return PadFrame(Pad).GetRotation().Inverse();
}

void AAICity::Build()
{
	using namespace AICityDetail;

	// --- the star -------------------------------------------------------------------------
	Star->SetRelativeScale3D(FVector(2.0 * StarRadius / ShapeSize));
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	bSunDisc = SunDiscMaterial() && Plane;
	if (bSunDisc)
	{
		// Celestial Fortune's sun: the Corona part becomes the disc, at the star's centre, turned
		// to face the eye every frame (FaceSun); until then toward -X, its picture upright.
		Star->SetVisibility(false);
		Corona->SetStaticMesh(Plane);
		const double Width = StarRadius * SunSquarePerRadius;
		Corona->SetRelativeLocationAndRotation(FVector::ZeroVector,
			FMatrix(FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, -1.0), FVector(-1.0, 0.0, 0.0), FVector::ZeroVector).ToQuat());
		Corona->SetRelativeScale3D(FVector(Width / ShapeSize, Width / ShapeSize, 1.0));
	}
	else
	{
		Star->SetVisibility(true);
		Corona->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Corona->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
		Corona->SetRelativeScale3D(FVector(2.24 * StarRadius / ShapeSize));
	}

	// --- the city: the dome where the pack is here, else a lattice each shell (LayLattice) ---
	bDomeUp = RaiseDome();
	for (int32 S = 0; S < ShellCount; ++S)
	{
		UInstancedStaticMeshComponent* Struts = ShellStruts.IsValidIndex(S) ? ShellStruts[S].Get() : nullptr;
		UInstancedStaticMeshComponent* Nodes = ShellNodes.IsValidIndex(S) ? ShellNodes[S].Get() : nullptr;
		if (!Struts || !Nodes)
		{
			continue;
		}
		if (bDomeUp)
		{
			Struts->ClearInstances();
			Nodes->ClearInstances();
		}
		else
		{
			LayLattice(Struts, Nodes, S, ShellRadius[S]);
		}
		Struts->SetVisibility(!bDomeUp);
		Nodes->SetVisibility(!bDomeUp);
	}

	// --- the far sky: 200 km out, past where the flight in begins ---------------------------
	SkyStars->ClearInstances();
	{
		FRandomStream Rng(77);
		TArray<FTransform> Sky;
		const double Grow = SkyRadius / 1800000.0;      // sized as they were at 18 km
		for (int32 I = 0; I < 3500; ++I)
		{
			const double Z = Rng.FRandRange(-1.0f, 1.0f);
			const double Phi = Rng.FRandRange(0.0f, 2.0f * PI);
			const double Rho = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Size = (1200.0 + FMath::Pow(static_cast<double>(Rng.FRand()), 4.0) * 5000.0) * Grow;
			Sky.Add(Bar(FQuat::Identity, FVector(Rho * FMath::Cos(Phi), Rho * FMath::Sin(Phi), Z) * SkyRadius, FVector(Size)));
		}
		SkyStars->AddInstances(Sky, false);
	}

	// --- the landing pads: a dark floor on three legs, a glass rail, a ring of light ----------
	PadRails->ClearInstances();
	PadLines->ClearInstances();
	PadLegs->ClearInstances();
	{
		TArray<FTransform> Rail;
		TArray<FTransform> Lines;
		TArray<FTransform> Legs;
		for (int32 P = 0; P < PadCount; ++P)
		{
			const FTransform Frame = PadFrame(P);
			auto Place = [&Frame](const FTransform& Local) { return Local * Frame; };
			if (UStaticMeshComponent* Floor = PadFloors.IsValidIndex(P) ? PadFloors[P].Get() : nullptr)
			{
				// Its top at the frame's origin.
				Floor->SetRelativeTransform(Place(Bar(FQuat::Identity, FVector(0.0, 0.0, -20.0), FVector(2.0 * PadRadius, 2.0 * PadRadius, 40.0))));
			}
			constexpr int32 Panels = 72;
			const double RailR = PadRadius - 20.0;
			for (int32 K = 0; K < Panels; ++K)
			{
				const double A = 2.0 * PI * (K + 0.5) / Panels;
				const FQuat Turn(FVector::UpVector, A + PI * 0.5);
				const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.0);
				Rail.Add(Place(Bar(Turn, Out * RailR + FVector(0.0, 0.0, 55.0), FVector(2.0 * PI * RailR / Panels * 0.96, 4.0, 110.0))));
				Lines.Add(Place(Bar(Turn, Out * (PadRadius - 6.0) + FVector(0.0, 0.0, 1.0), FVector(2.0 * PI * PadRadius / Panels, 12.0, 3.0))));
			}
			constexpr int32 Inlay = 48;
			const double InlayR = PadRadius * 0.55;
			for (int32 K = 0; K < Inlay; ++K)
			{
				const double A = 2.0 * PI * (K + 0.5) / Inlay;
				Lines.Add(Place(Bar(FQuat(FVector::UpVector, A + PI * 0.5), FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * InlayR + FVector(0.0, 0.0, 0.5),
					FVector(2.0 * PI * InlayR / Inlay, 6.0, 2.0))));
			}
			// Three legs down through the relief into the ridge.
			const double LegLength = PadLift + 400.0;
			for (int32 K = 0; K < 3; ++K)
			{
				const double A = 2.0 * PI * K / 3.0;
				Legs.Add(Place(Bar(FQuat::Identity, FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * PadRadius * 0.6 + FVector(0.0, 0.0, -40.0 - LegLength * 0.5),
					FVector(80.0, 80.0, LegLength))));
			}
		}
		PadRails->AddInstances(Rail, false);
		PadLines->AddInstances(Lines, false);
		PadLegs->AddInstances(Legs, false);
	}
}

bool AAICity::RaiseDome()
{
	using namespace AICityDetail;
	/* Each layer is the pack's own blueprint, made the child of a component here and scaled
	   round the star. Its class is set only when it changes: setting it again would throw the
	   layer away and build it anew, 1.6 million triangles of buildings and all, on every Build. */
	const double Scale = DomeRadius / DomeNativeRadius;
	int32 Raised = 0;
	for (int32 L = 0; L < DomeLayers.Num() && L < DomeLayerCount; ++L)
	{
		UChildActorComponent* Layer = DomeLayers[L];
		if (!Layer)
		{
			continue;
		}
		UClass* Wanted = bUseDome ? LoadClass<AActor>(nullptr, DomeLayerClass[L], nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		if (Layer->GetChildActorClass() != Wanted)
		{
			Layer->SetChildActorClass(Wanted);
		}
		Layer->SetRelativeScale3D(FVector(Scale));
		Raised += Layer->GetChildActor() ? 1 : 0;
	}
	QuietDome();
	return Raised > 0;
}

void AAICity::QuietDome()
{
	// No collision 3 km away (and none at all to a hull that fills the dome), no shadows, lit
	// only by the star (channel 1), and movable like the rest of the city - which turns.
	for (UChildActorComponent* Layer : DomeLayers)
	{
		AActor* Child = Layer ? Layer->GetChildActor() : nullptr;
		if (!Child)
		{
			continue;
		}
		Child->SetActorEnableCollision(false);
		TInlineComponentArray<USceneComponent*> Parts(Child);
		for (USceneComponent* Part : Parts)
		{
			Part->SetMobility(EComponentMobility::Movable);
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Part))
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prim->SetCanEverAffectNavigation(false);
				Prim->SetCastShadow(false);
				Prim->SetLightingChannels(false, true, false);
			}
		}
	}
}

void AAICity::Paint()
{
	using namespace AICityDetail;
	auto Wear = [](UStaticMeshComponent* Part, UMaterialInstanceDynamic* M)
	{
		if (Part && M)
		{
			Part->SetMaterial(0, M);
		}
	};
	// The star: M_GenesisStar if it has been made (hot middle, red limb, mottled - the flat
	// glow read as a lamp), loaded by path and cooked by DefaultGame.ini's /Game/Genesis line.
	if (UMaterialInterface* StarSkin = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Genesis/M_GenesisStar.M_GenesisStar"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(StarSkin, this);
		M->SetScalarParameterValue(TEXT("Intensity"), StarGlow);
		Wear(Star, M);
	}
	else
	{
		Wear(Star, Glow(StarColour, StarGlow));
	}
	if (UMaterialInterface* SunDisc = SunDiscMaterial())
	{
		Wear(Corona, UMaterialInstanceDynamic::Create(SunDisc, this));
	}
	else
	{
		Wear(Corona, Glass(CoronaColour, 3.0f, 0.8f, 0.0f));
	}
	// The fallback lattice: its struts' and nodes' materials (make_genesis_assets.py), loaded by
	// path and cooked by DefaultGame.ini's /Game/Genesis line; plain glow until they are made.
	UMaterialInterface* NodeSkin = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Genesis/M_GenesisNode.M_GenesisNode"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* StrutSkin = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Genesis/M_GenesisStrut.M_GenesisStrut"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 S = 0; S < ShellCount && !bDomeUp; ++S)
	{
		UMaterialInstanceDynamic* Node = NodeSkin ? UMaterialInstanceDynamic::Create(NodeSkin, this) : Glow(CityLight[S], CityGlow);
		if (NodeSkin && Node)
		{
			Node->SetVectorParameterValue(TEXT("Color"), CityLight[S]);
			Node->SetScalarParameterValue(TEXT("Intensity"), CityGlow);
		}
		Wear(ShellNodes.IsValidIndex(S) ? ShellNodes[S].Get() : nullptr, Node);
		UMaterialInstanceDynamic* Strut = StrutSkin ? UMaterialInstanceDynamic::Create(StrutSkin, this) : Glow(CityLight[S], CityGlow * 0.3f);
		if (StrutSkin && Strut)
		{
			Strut->SetVectorParameterValue(TEXT("Color"), CityLight[S]);
			Strut->SetVectorParameterValue(TEXT("PulseColor"), FMath::Lerp(CityLight[S], FLinearColor::White, 0.55f) * 4.0f);
			Strut->SetScalarParameterValue(TEXT("Intensity"), CityGlow * 0.5f);
			Strut->SetScalarParameterValue(TEXT("Base"), 0.03f);
		}
		Wear(ShellStruts.IsValidIndex(S) ? ShellStruts[S].Get() : nullptr, Strut);
	}
	Wear(SkyStars, Glow(FLinearColor(0.85f, 0.9f, 1.0f), 40.0f));
	for (UStaticMeshComponent* Floor : PadFloors)
	{
		Wear(Floor, Surface(PadDark, 0.2f));
	}
	Wear(PadRails, Glass(FLinearColor(0.5f, 0.75f, 1.0f), 0.8f, 0.25f, 0.03f));
	Wear(PadLines, Glow(Edge, 8.0f));
	Wear(PadLegs, Surface(PadDark, 0.35f));
}

void AAICity::Light()
{
	// The star's light by the inverse square, DomeLux on the dome's inner face - and, on channel
	// 0, on him and the pads, which stand a little nearer.
	const double Metres = DomeRadius / 100.0;
	StarLight->SetIntensityUnits(ELightUnits::Candelas);
	StarLight->SetIntensity(static_cast<float>(DomeLux * Metres * Metres));
	StarLight->SetAttenuationRadius(static_cast<float>(DomeRadius * 1.4));
	FillLight->SetIntensity(0.25f);

	FPostProcessSettings& S = Look->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = 0;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 1.2f;
}

void AAICity::StartFlight(EFlight Kind, int32 ToPad)
{
	using namespace AICityDetail;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !FlightCam)
	{
		return;
	}
	const double R = DomeRadius;
	const FTransform To = PadFrame(ToPad);
	const FVector ToUp = To.GetRotation().GetAxisZ();
	const FVector ToAlong = To.GetRotation().GetAxisX();
	const FVector Eye = To.TransformPosition(FVector(0.0, 0.0, EyeHeight));
	const FVector Above = To.GetLocation() + ToUp * (R * 0.09);          // ~310 m over the pad
	const FQuat Landed = Facing(ToAlong, ToUp);
	const FQuat Descending = Facing(ToAlong * 0.45 - ToUp, ToAlong);     // down at the pad, a little ahead

	PathPlace.Reset();
	PathTurn.Reset();
	TArray<FVector> Heading;     // per key: the way it is passed through, or zero to take the neighbours'
	auto Key = [this, &Heading](float Seconds, const FVector& At, FQuat Turn, const FVector& Through = FVector::ZeroVector)
	{
		// Each turn on the same side as the last, so the curve never swings the long way round.
		if (PathTurn.Points.Num() > 0 && (PathTurn.Points.Last().OutVal | Turn) < 0.0)
		{
			Turn = -Turn;
		}
		PathPlace.Points[PathPlace.AddPoint(Seconds, At)].InterpMode = CIM_CurveUser;
		PathTurn.Points[PathTurn.AddPoint(Seconds, Turn)].InterpMode = CIM_CurveAuto;
		Heading.Add(Through.GetSafeNormal());
	};

	if (Kind == EFlight::Intro)
	{
		/* THE WAY IN. From 25 km out the dome is a dark ball among the stars, the star blazing
		   through its cells; he drifts, then closes on the cell beside B, the star dead ahead
		   through it; through it, past the star a little over a kilometre off, and down onto the
		   pad at A on the far side, turning to look along its ridge. The city does not turn: it
		   is already as it will be when he lands. */
		const FVector In = EntryCell.GetSafeNormal();
		const FVector Side = (ToAlong - In * FVector::DotProduct(ToAlong, In)).GetSafeNormal();
		const FQuat AtStar = Facing(-In, Side);
		Key(0.0f, In * StartDistance, AtStar);
		Key(4.0f, In * (StartDistance * 0.92), AtStar);
		Key(19.0f, In * (R * 1.22), AtStar);         // just outside the cell
		Key(23.0f, In * (R * 0.86), AtStar);         // through it
		const FVector Past = Side * (R * 0.35);
		Key(33.0f, Past, Facing(Above - Past, ToAlong));
		Key(40.0f, Above, Descending, -ToUp);        // and straight down onto the pad
		Key(46.0f, Eye, Landed);
		TurnFrom = TurnTo = PadBelow(ToPad);
		TurnStart = TurnEnd = 0.0f;
	}
	else
	{
		/* ACROSS. Up off the pad, over toward the star and past it, and down onto the other pad.
		   The pads face each other through the star, so the path bows out along the ridge's line
		   to pass it; meanwhile the city turns, so that when he lands the far pad is below him. */
		const FTransform From = PadFrame(OnPad);
		const FVector FromUp = From.GetRotation().GetAxisZ();
		const FVector FromAlong = From.GetRotation().GetAxisX();
		/* FROM WHERE HIS EYES ARE, looking where he looks: the flight took over at the middle of
		   the pad facing along the ridge, and the view jerked there from wherever he stood and
		   looked (Walt, 2026-10-22: "when you lift off ... there is a brief jerking of the view"). */
		FVector Start = From.TransformPosition(FVector(0.0, 0.0, EyeHeight));
		FQuat StartTurn = Facing(FromAlong, FromUp);
		if (PC->PlayerCameraManager)
		{
			const FTransform City = GetActorTransform();
			Start = City.InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
			StartTurn = City.GetRotation().Inverse() * PC->PlayerCameraManager->GetCameraRotation().Quaternion();
		}
		Key(0.0f, Start, StartTurn);
		Key(4.0f, From.GetLocation() + FromUp * (R * 0.09), Facing(FromAlong + FromUp * 1.2, FromUp));
		const FVector Past = FromAlong * (R * 0.35);
		Key(16.0f, Past, Facing(Above - Past, -FromAlong));
		Key(27.0f, Above, Descending, -ToUp);        // and straight down onto the pad
		Key(33.0f, Eye, Landed);
		TurnFrom = PadBelow(OnPad);
		TurnTo = PadBelow(ToPad);
		TurnStart = 4.0f;
		TurnEnd = 27.0f;
	}
	/* WHERE: smooth through every key, still at the ends, and never past one. The engine's auto
	   tangents carried the cross-sphere speed into the last 300 m, and the first landing sank
	   through the ridge below the pad and rose back up to it (Walt, 2026-10-22). So each key is
	   passed at the slower of its two stretches' average speeds - a Hermite segment whose end
	   speeds are no more than its average cannot overshoot - heading along its neighbours'
	   chord, or the way the key itself says (straight down onto a pad). */
	TArray<FInterpCurvePoint<FVector>>& Places = PathPlace.Points;
	for (int32 I = 0; I < Places.Num(); ++I)
	{
		FVector Speed = FVector::ZeroVector;
		if (I > 0 && I + 1 < Places.Num())
		{
			const FVector Chord = (Places[I + 1].OutVal - Places[I - 1].OutVal).GetSafeNormal();
			const double In = (Places[I].OutVal - Places[I - 1].OutVal).Size() / FMath::Max(Places[I].InVal - Places[I - 1].InVal, 0.01f);
			const double Out = (Places[I + 1].OutVal - Places[I].OutVal).Size() / FMath::Max(Places[I + 1].InVal - Places[I].InVal, 0.01f);
			Speed = (Heading[I].IsNearlyZero() ? Chord : Heading[I]) * FMath::Min(In, Out);
		}
		Places[I].ArriveTangent = Places[I].LeaveTangent = Speed;
	}
	/* WHICH WAY: the turns between keys, smoothed - but still at the ends by holding each end's
	   control point on the key itself. The engine's "stationary end" is a zero quaternion, which
	   is no rotation at all: the flight in began by swinging the view away from the city and
	   back (Walt, the same day). */
	PathTurn.AutoSetTangents(0.0f, false);
	for (FInterpCurvePoint<FQuat>* End : { &PathTurn.Points[0], &PathTurn.Points.Last() })
	{
		End->InterpMode = CIM_CurveUser;
		End->ArriveTangent = End->LeaveTangent = End->OutVal;
	}

	Flight = Kind;
	FlightAge = 0.0f;
	FlightLength = PathPlace.Points.Last().InVal;
	FlightTo = ToPad;
	OnPad = -1;
	FlightCam->SetRelativeLocationAndRotation(PathPlace.Eval(0.0f, FVector::ZeroVector), PathTurn.Eval(0.0f, FQuat::Identity).GetNormalized());
	HoldPawn(UGameplayStatics::GetPlayerPawn(this, 0), true);
	PC->SetViewTargetWithBlend(this, 0.0f);
	UE_LOG(LogTemp, Display, TEXT("[AICity] %s to pad %d, %.0f s."), Kind == EFlight::Intro ? TEXT("flying in") : TEXT("flying across"), ToPad, FlightLength);
}

void AAICity::EndFlight()
{
	SetActorRotation(TurnTo);
	Flight = EFlight::None;
	OnPad = FlightTo;
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	HoldPawn(Pawn, false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (Pawn)
		{
			PC->SetViewTargetWithBlend(Pawn, 0.0f);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[AICity] landed on pad %d."), OnPad);
}

void AAICity::HoldPawn(APawn* Pawn, bool bHold)
{
	if (!Pawn)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	ACharacter* Body = Cast<ACharacter>(Pawn);
	UCharacterMovementComponent* Moves = Body ? Body->GetCharacterMovement() : nullptr;
	if (bHold)
	{
		// Put away while the city's camera has him: unseen, untouchable, not falling.
		if (Moves)
		{
			Moves->StopMovementImmediately();
			Moves->DisableMovement();
		}
		Pawn->SetActorHiddenInGame(true);
		Pawn->SetActorEnableCollision(false);
		if (PC)
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
		return;
	}
	// Set down on the pad he flew to, facing along its ridge.
	const FTransform Pad = PadFrame(FlightTo) * GetActorTransform();
	const float Half = Body && Body->GetCapsuleComponent() ? Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
	const FRotator Ahead(0.0, Pad.Rotator().Yaw, 0.0);
	Pawn->SetActorLocationAndRotation(Pad.GetLocation() + Pad.GetRotation().GetAxisZ() * (Half + 5.0), Ahead, false, nullptr, ETeleportType::TeleportPhysics);
	Pawn->SetActorHiddenInGame(false);
	Pawn->SetActorEnableCollision(true);
	if (Moves)
	{
		Moves->SetMovementMode(MOVE_Walking);
	}
	if (PC)
	{
		PC->SetControlRotation(Ahead);
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
	}
}

void AAICity::FaceSun()
{
	if (!bSunDisc || !Corona)
	{
		return;
	}
	FVector Eye;
	FVector EyeUp;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (IsFlying() && FlightCam)
	{
		Eye = FlightCam->GetComponentLocation();
		EyeUp = FlightCam->GetUpVector();
	}
	else if (PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
		EyeUp = PC->PlayerCameraManager->GetCameraRotation().Quaternion().GetUpVector();
	}
	else
	{
		return;
	}
	// The disc's Z toward the eye, its picture's down (Y) the eye's down, its right (X) the eye's
	// right: the frame Build gave it at the gallery, now wherever he is.
	const FVector Z = (Eye - GetActorLocation()).GetSafeNormal();
	FVector Y = -(EyeUp - Z * FVector::DotProduct(EyeUp, Z)).GetSafeNormal();
	if (Z.IsNearlyZero() || Y.IsNearlyZero())
	{
		return;
	}
	const FVector X = FVector::CrossProduct(Y, Z);
	Corona->SetWorldRotation(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat());
}

void AAICity::Tick(float DeltaSeconds)
{
	using namespace AICityDetail;
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// The visit begins as soon as there is someone to take: the city turned for the landing at
	// A, and the flight in.
	if (!bVisitStarted && PC && Pawn)
	{
		bVisitStarted = true;
		SetActorRotation(PadBelow(0));
		StartFlight(EFlight::Intro, 0);
	}

	if (IsFlying())
	{
		FlightAge += DeltaSeconds;
		if (PC && PC->WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			FlightAge = FlightLength;       // [Space]: straight to the landing
		}
		const float T = FMath::Min(FlightAge, FlightLength);
		if (TurnEnd > TurnStart)
		{
			const float Share = FMath::SmoothStep(0.0f, 1.0f, (T - TurnStart) / (TurnEnd - TurnStart));
			SetActorRotation(FQuat::Slerp(TurnFrom, TurnTo, Share));
		}
		FlightCam->SetRelativeLocationAndRotation(PathPlace.Eval(T, FVector::ZeroVector), PathTurn.Eval(T, FQuat::Identity).GetNormalized());
		if (FlightAge >= FlightLength)
		{
			EndFlight();
		}
	}
	else if (OnPad >= 0)
	{
		if (PC && PC->WasInputKeyJustPressed(EKeys::F))
		{
			StartFlight(EFlight::Across, 1 - OnPad);
		}
		else if (Pawn)
		{
			// The rail should keep him on the pad; if anything ever puts him off it, he is set
			// back down rather than falling through the dome into space.
			const FTransform Pad = PadFrame(OnPad) * GetActorTransform();
			const FVector Local = Pad.InverseTransformPosition(Pawn->GetActorLocation());
			if (Local.Z < -1000.0 || FVector2D(Local.X, Local.Y).Size() > PadRadius * 1.5)
			{
				FlightTo = OnPad;
				HoldPawn(Pawn, false);
			}
		}
	}

	FaceSun();

	// What this is and what he can do, just above "[O] Back to Office".
	if (PC)
	{
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			// No size given (Walt, 2026-10-22: "please delete the '6.9 km across' - I would hope
			// this city is much bigger than that").
			const FString What = TEXT("The AI city: a sphere the AIs built round a star");
			if (Flight == EFlight::Intro)
			{
				Hud->ShowAwayHint(What + TEXT("\n[Space] Skip the flight in"));
			}
			else if (Flight == EFlight::Across)
			{
				Hud->ShowAwayHint(TEXT("Across the sphere, past its star\n[Space] Skip to the landing"));
			}
			else if (OnPad >= 0)
			{
				Hud->ShowAwayHint(What + TEXT("\n[F] Fly across the sphere to the landing on the far side"));
			}
		}
	}

	// The fallback lattice turns, each shell on its own axis (only where the dome is not up).
	for (int32 S = 0; S < ShellCount && !bDomeUp; ++S)
	{
		const float Spin = ShellSpin.IsValidIndex(S) ? ShellSpin[S] : 0.0f;
		const FQuat Turn(ShellAxis[S], FMath::DegreesToRadians(Age * Spin));
		if (ShellStruts.IsValidIndex(S) && ShellStruts[S])
		{
			ShellStruts[S]->SetRelativeRotation(Turn);
		}
		if (ShellNodes.IsValidIndex(S) && ShellNodes[S])
		{
			ShellNodes[S]->SetRelativeRotation(Turn);
		}
	}
}
