// AICity.cpp - see the header for what the AI city is and what it is not.

#include "AICity.h"

#include "SibeliusHUD.h"
#include "UniverseForge.h"
#include "UniverseExhibit.h"
#include "ProgressionSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace AICityDetail
{
	constexpr double ShapeSize = 100.0;              // the engine's basic shapes are a metre across
	constexpr double GoldenAngle = 2.39996322972865332;

	// The city's three shells, inner to outer: their radius and their light.
	constexpr double ShellRadius[AAICity::ShellCount] = { 34000.0, 46000.0, 60000.0 };
	const FVector ShellAxis[AAICity::ShellCount] = {
		FVector(0.0, 0.0, 1.0), FVector(0.3, 0.0, 1.0).GetSafeNormal(), FVector(0.0, -0.4, 1.0).GetSafeNormal() };

	// LINEAR colours, which is what a material multiplies.
	const FLinearColor StarColour(1.0f, 0.86f, 0.62f);
	const FLinearColor CoronaColour(1.0f, 0.58f, 0.24f);
	const FLinearColor RingHull(0.04f, 0.045f, 0.055f);
	const FLinearColor FloorDark(0.012f, 0.014f, 0.02f);
	const FLinearColor CityLight[AAICity::ShellCount] = {
		FLinearColor(0.25f, 0.85f, 1.0f), FLinearColor(1.0f, 0.72f, 0.35f), FLinearColor(0.7f, 0.5f, 1.0f) };
	const FLinearColor RingLight(0.6f, 0.9f, 1.0f);
	const FLinearColor Edge(0.3f, 0.85f, 1.0f);

	// The gallery's universes: where each floats (degrees round the gallery from the star's
	// direction, height over the floor), how big, what colour, what kind of galaxy is in it.
	enum class EGalaxy : uint8 { Spiral, Elliptical, Ring };
	struct FUniverse { double Angle; double Height; double Radius; FLinearColor Hue; EGalaxy Kind; int32 Arms; };
	const FUniverse Universes[AAICity::UniverseCount] = {
		{  70.0, 300.0, 130.0, FLinearColor(0.55f, 0.75f, 1.0f),  EGalaxy::Spiral,     2 },
		{ 110.0, 420.0, 160.0, FLinearColor(1.0f, 0.75f, 0.35f),  EGalaxy::Spiral,     3 },
		{ 150.0, 260.0, 120.0, FLinearColor(1.0f, 0.45f, 0.6f),   EGalaxy::Elliptical, 0 },
		{ 180.0, 360.0, 170.0, FLinearColor(0.4f, 1.0f, 0.75f),   EGalaxy::Spiral,     4 },
		{ 210.0, 470.0, 140.0, FLinearColor(0.7f, 0.5f, 1.0f),    EGalaxy::Ring,       0 },
		// 225 and 315, not 250 and 290: the forge's console is at 270, and its camera looks out
		// over the rail there - those two floated in front of the forge's glass (2026-10-15).
		{ 225.0, 290.0, 150.0, FLinearColor(1.0f, 0.35f, 0.25f),  EGalaxy::Spiral,     2 },
		{ 315.0, 400.0, 125.0, FLinearColor(0.9f, 0.9f, 1.0f),    EGalaxy::Elliptical, 0 },
	};
	constexpr double UniverseRing = 1150.0;          // from the middle of the gallery

	FTransform Bar(const FQuat& Turn, const FVector& At, const FVector& Size)
	{
		return FTransform(Turn, At, Size / ShapeSize);
	}

	/* THE CITY AS A MIND (Walt, 2026-10-18: "you really have to use your imagination to regard
	   those cubes and lines as some kind of worlds or cities ... is there an art idea you have
	   that could be easily generated and look more attractive?" - and of the answer, "glowing
	   geodesic lattice. i like it"). A Matrioshka brain: each shell a web round the star, struts
	   between nodes, open, so the shells behind it and the star itself show through - the crates
	   were walls. Thought runs down every strut (M_GenesisStrut) and every node is a neuron
	   twinkling on its own (M_GenesisNode); neither costs C++ anything per frame.

	   Each shell is a geodesic sphere - an icosahedron, its triangles split LatticeLevel times
	   and pushed out to the sphere - turned at random so no two shells line up. One connection in
	   seven is left out, so it reads as a web that grew rather than a mesh that was drawn; one
	   node in twenty is a hub, bigger and brighter. Cells are 47, 64 and 83 m across, inner to outer:
	   the outer shell at the inner ones' 45 m (a level finer) read as a tangle in the first render. */
	constexpr int32 LatticeLevel[AAICity::ShellCount] = { 3, 3, 3 };   // 1,920 struts each
	constexpr double StrutThick = 0.0028;    // of the shell's radius: 95 cm inner, 1.7 m outer
	constexpr double NodeSize = 0.0095;      // of the shell's radius: a 3 to 6 m neuron
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
	   SVG rendered to T_Sun (make_sun_texture.mjs) - alive in M_GenesisSunDisc (a slow turn, a
	   shimmer, a breath), on a flat disc facing the gallery: the gallery is 260 m off and 30 m
	   across, so the star is only ever seen from this side. Until the material is made, the
	   sphere and its glass corona stand in. */
	UMaterialInterface* SunDiscMaterial()
	{
		// The soft-edged layered one first (2026-10-18: the plain one's glow was cut off in straight
		// lines at its square, plain against the lattice), then the layered one (Walt: "let's do
		// the medium please" - each loop rising and falling on its own, the spots drifting, the
		// flare bursting), then the one-picture one.
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
	Rings = MakeMany(TEXT("Rings"), Cube, false);
	RingLights = MakeMany(TEXT("RingLights"), Cube, false);
	SkyStars = MakeMany(TEXT("SkyStars"), Sphere, false);
	GalleryFloor = MakeMesh(TEXT("GalleryFloor"), Cylinder, true);
	GalleryRail = MakeMany(TEXT("GalleryRail"), Cube, true);
	GalleryLines = MakeMany(TEXT("GalleryLines"), Cube, false);
	for (int32 U = 0; U < UniverseCount; ++U)
	{
		UniverseGlass.Add(MakeMesh(FString::Printf(TEXT("UniverseGlass_%d"), U), Sphere, false));
		UniverseStars.Add(MakeMany(FString::Printf(TEXT("UniverseStars_%d"), U), Sphere, false));
	}

	StarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarLight"));
	StarLight->SetupAttachment(SceneRoot);
	StarLight->SetMobility(EComponentMobility::Movable);
	StarLight->SetCastShadows(false);
	StarLight->SetLightColor(StarColour);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);
	FillLight->SetLightColor(FLinearColor(0.35f, 0.45f, 0.8f));
	// Travelling +X and a little down: it lights what faces away from the star.
	FillLight->SetRelativeRotation(FRotator(-20.0, 0.0, 0.0));

	Look = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Look"));
	Look->SetupAttachment(SceneRoot);
	Look->bUnbound = true;
}

void AAICity::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
	Paint();
	Light();
}

void AAICity::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Paint();
	Light();

	// His three gallery slots (AUniverseExhibit), spawned here rather than saved in L_Genesis so
	// the map needs no resave: a row across the gallery, 5 m toward the star from its centre.
	for (int32 Slot = 0; Slot < UProgressionSubsystem::UniverseSlots; ++Slot)
	{
		const FVector Local = GalleryCentre() + FVector(500.0f, (Slot - 1) * 550.0f, 0.0f);
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AUniverseExhibit* Exhibit = GetWorld()->SpawnActor<AUniverseExhibit>(GetActorTransform().TransformPosition(Local), GetActorRotation(), Params))
		{
			Exhibit->SetSlot(Slot);
		}
	}
}

void AAICity::AnswerUniverse()
{
	/* A universe he grew has made machines, and they signal: the city answers, a slow breath
	   of light through each shell in turn (Tick). The first time ever it is long - 12 seconds -
	   and is recorded only once it has played to the end, so leaving halfway means the next
	   universe to get there is answered in full; after that, a short 5-second reply. */
	if (IsAnswering())
	{
		return;
	}
	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	bFirstResponse = !Progress || !Progress->GetStateForRead().bUniverseContactComplete;
	ResponseAge = 0.0f;
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

void AAICity::Build()
{
	using namespace AICityDetail;

	// --- the star -------------------------------------------------------------------------
	Star->SetRelativeScale3D(FVector(2.0 * StarRadius / ShapeSize));
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (SunDiscMaterial() && Plane)
	{
		// Celestial Fortune's sun: the Corona part becomes the disc, facing the gallery (-X),
		// its picture upright (U to the viewer's right, +Y; V down), the sphere hidden.
		Star->SetVisibility(false);
		Corona->SetStaticMesh(Plane);
		const double Width = StarRadius * SunSquarePerRadius;
		// Lifted clear of the horizon: with its middle at the gallery floor's height, the floor
		// hid its lower half and two of its loops (the first render, 2026-10-15). From his eyes
		// the floor's edge dips about six degrees; 0.75 of a radius up clears it with the loops.
		Corona->SetRelativeLocationAndRotation(FVector(0.0, 0.0, StarRadius * 0.75),
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

	// --- the shells: a lattice each, a web round the star (LayLattice) ----------------------
	for (int32 S = 0; S < ShellCount; ++S)
	{
		UInstancedStaticMeshComponent* Struts = ShellStruts.IsValidIndex(S) ? ShellStruts[S].Get() : nullptr;
		UInstancedStaticMeshComponent* Nodes = ShellNodes.IsValidIndex(S) ? ShellNodes[S].Get() : nullptr;
		if (Struts && Nodes)
		{
			LayLattice(Struts, Nodes, S, ShellRadius[S]);
		}
	}

	// --- the transit rings ------------------------------------------------------------------
	Rings->ClearInstances();
	RingLights->ClearInstances();
	{
		struct FRingSpec { double Radius; FQuat Tilt; };
		const FRingSpec Specs[] = {
			{ 30000.0, FQuat::Identity },
			{ 40000.0, FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(28.0)) },
			{ 53000.0, FQuat(FVector(0.0, 1.0, 0.0), FMath::DegreesToRadians(-38.0)) },
		};
		constexpr int32 Segments = 160;
		TArray<FTransform> Hulls;
		TArray<FTransform> Strips;
		for (const FRingSpec& Ring : Specs)
		{
			const double Length = 2.0 * PI * Ring.Radius / Segments * 0.92;
			for (int32 K = 0; K < Segments; ++K)
			{
				const double A = 2.0 * PI * K / Segments;
				const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.0);
				// X along the ring, Y out from the star, Z through the ring's plane.
				const FQuat Turn = Ring.Tilt * FQuat(FVector::UpVector, A + PI * 0.5);
				Hulls.Add(Bar(Turn, Ring.Tilt.RotateVector(Out * Ring.Radius), FVector(Length, 900.0, 250.0)));
				Strips.Add(Bar(Turn, Ring.Tilt.RotateVector(Out * (Ring.Radius - 470.0)), FVector(Length * 0.9, 40.0, 60.0)));
			}
		}
		Rings->AddInstances(Hulls, false);
		RingLights->AddInstances(Strips, false);
	}

	// --- the far sky ---------------------------------------------------------------------------
	SkyStars->ClearInstances();
	{
		FRandomStream Rng(77);
		TArray<FTransform> Sky;
		for (int32 I = 0; I < 3500; ++I)
		{
			const double Z = Rng.FRandRange(-1.0f, 1.0f);
			const double Phi = Rng.FRandRange(0.0f, 2.0f * PI);
			const double Rho = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Size = 1200.0 + FMath::Pow(static_cast<double>(Rng.FRand()), 4.0) * 5000.0;
			Sky.Add(Bar(FQuat::Identity, FVector(Rho * FMath::Cos(Phi), Rho * FMath::Sin(Phi), Z) * 1800000.0, FVector(Size)));
		}
		SkyStars->AddInstances(Sky, false);
	}

	// --- the gallery: a dark glass floor, a glass rail, a ring of light --------------------------
	const FVector G = GalleryCentre();
	GalleryFloor->SetRelativeLocation(G - FVector(0.0, 0.0, 20.0));   // its top at the star's equator
	GalleryFloor->SetRelativeScale3D(FVector(2.0 * GalleryRadius / ShapeSize, 2.0 * GalleryRadius / ShapeSize, 40.0 / ShapeSize));
	GalleryRail->ClearInstances();
	GalleryLines->ClearInstances();
	{
		constexpr int32 Panels = 96;
		TArray<FTransform> Rail;
		TArray<FTransform> Lines;
		const double RailR = GalleryRadius - 20.0;
		for (int32 K = 0; K < Panels; ++K)
		{
			const double A = 2.0 * PI * (K + 0.5) / Panels;
			const FQuat Turn(FVector::UpVector, A + PI * 0.5);
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.0);
			Rail.Add(Bar(Turn, G + Out * RailR + FVector(0.0, 0.0, 55.0), FVector(2.0 * PI * RailR / Panels * 0.96, 4.0, 110.0)));
			Lines.Add(Bar(Turn, G + Out * (GalleryRadius - 6.0) + FVector(0.0, 0.0, 1.0), FVector(2.0 * PI * GalleryRadius / Panels, 12.0, 3.0)));
		}
		constexpr int32 Inlay = 64;
		for (int32 K = 0; K < Inlay; ++K)
		{
			const double A = 2.0 * PI * (K + 0.5) / Inlay;
			Lines.Add(Bar(FQuat(FVector::UpVector, A + PI * 0.5), G + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * 700.0 + FVector(0.0, 0.0, 0.5),
				FVector(2.0 * PI * 700.0 / Inlay, 6.0, 2.0)));
		}
		GalleryRail->AddInstances(Rail, false);
		GalleryLines->AddInstances(Lines, false);
	}

	// --- the universes the AIs have grown, floating over the gallery ----------------------------
	UniverseHome.Reset();
	UniverseAxis.Reset();
	UniverseRate.Reset();
	for (int32 U = 0; U < UniverseCount; ++U)
	{
		const FUniverse& Spec = Universes[U];
		FRandomStream Rng(500 + U);
		const double A = FMath::DegreesToRadians(Spec.Angle);
		const FVector Home = G + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * UniverseRing + FVector(0.0, 0.0, Spec.Height);
		UniverseHome.Add(Home);
		UniverseAxis.Add(FVector(Rng.FRandRange(-0.5f, 0.5f), Rng.FRandRange(-0.5f, 0.5f), 1.0f).GetSafeNormal());
		UniverseRate.Add(Rng.FRandRange(12.0f, 28.0f));
		if (UStaticMeshComponent* Shell = UniverseGlass.IsValidIndex(U) ? UniverseGlass[U].Get() : nullptr)
		{
			Shell->SetRelativeLocation(Home);
			Shell->SetRelativeScale3D(FVector(2.0 * Spec.Radius / ShapeSize));
		}
		UInstancedStaticMeshComponent* Field = UniverseStars.IsValidIndex(U) ? UniverseStars[U].Get() : nullptr;
		if (!Field)
		{
			continue;
		}
		Field->ClearInstances();
		Field->SetRelativeLocation(Home);
		// The galaxy lies in its own XY plane; Tick turns the plane round its axis.
		TArray<FTransform> Points;
		const double Reach = Spec.Radius * 0.75;
		const int32 Count = Spec.Kind == EGalaxy::Elliptical ? 220 : 260;
		for (int32 I = 0; I < Count; ++I)
		{
			FVector P;
			const double T = FMath::Pow(static_cast<double>(Rng.FRand()), 0.6);
			if (Spec.Kind == EGalaxy::Elliptical)
			{
				// A ball of old stars, thickest in the middle, a little flattened.
				const FVector D = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f)).GetSafeNormal();
				P = D * Reach * FMath::Pow(static_cast<double>(Rng.FRand()), 1.6);
				P.Z *= 0.6;
			}
			else if (Spec.Kind == EGalaxy::Ring)
			{
				// A ring round a small bright core.
				const double Th = Rng.FRandRange(0.0f, 2.0f * PI);
				const double Rr = I < Count / 6 ? Reach * 0.12 * Rng.FRand() : Reach * Rng.FRandRange(0.78f, 0.98f);
				P = FVector(Rr * FMath::Cos(Th), Rr * FMath::Sin(Th), Rng.FRandRange(-0.03f, 0.03f) * Reach);
			}
			else if (I < Count / 7)
			{
				// The bulge.
				const FVector D = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-0.5f, 0.5f)).GetSafeNormal();
				P = D * Reach * 0.18 * Rng.FRand();
			}
			else
			{
				// The arms: logarithmic spirals, the stars scattered a little off them.
				const int32 Arm = I % FMath::Max(Spec.Arms, 1);
				const double Th = 2.0 * PI * Arm / FMath::Max(Spec.Arms, 1) + T * 3.6 + Rng.FRandRange(-0.25f, 0.25f);
				const double Rr = Reach * (0.12 + 0.88 * T);
				P = FVector(Rr * FMath::Cos(Th), Rr * FMath::Sin(Th), Rng.FRandRange(-0.05f, 0.05f) * Reach * (1.0 - T));
			}
			const double Size = Rng.FRandRange(2.4f, 4.5f);
			Points.Add(Bar(FQuat::Identity, P, FVector(Size)));
		}
		Field->AddInstances(Points, false);
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
	// The lattice: its struts' and nodes' materials (make_genesis_assets.py), loaded by path and
	// cooked by DefaultGame.ini's /Game/Genesis line; plain glow until they are made.
	ResponseLights.Reset();
	ThoughtLights.Reset();
	UMaterialInterface* NodeSkin = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Genesis/M_GenesisNode.M_GenesisNode"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* StrutSkin = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Genesis/M_GenesisStrut.M_GenesisStrut"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 S = 0; S < ShellCount; ++S)
	{
		// The nodes: what the city's answer breathes (Tick) - their Color and Intensity.
		UMaterialInstanceDynamic* Node = NodeSkin ? UMaterialInstanceDynamic::Create(NodeSkin, this) : Glow(CityLight[S], CityGlow);
		if (NodeSkin && Node)
		{
			Node->SetVectorParameterValue(TEXT("Color"), CityLight[S]);
			Node->SetScalarParameterValue(TEXT("Intensity"), CityGlow);
		}
		ResponseLights.Add(Node);
		Wear(ShellNodes.IsValidIndex(S) ? ShellNodes[S].Get() : nullptr, Node);

		// The struts: dim rods, bright comets (the wiring's lesson - at an even balance the
		// thought is lost in the rods), and what the answer surges.
		UMaterialInstanceDynamic* Strut = StrutSkin ? UMaterialInstanceDynamic::Create(StrutSkin, this) : Glow(CityLight[S], CityGlow * 0.3f);
		if (StrutSkin && Strut)
		{
			Strut->SetVectorParameterValue(TEXT("Color"), CityLight[S]);
			Strut->SetVectorParameterValue(TEXT("PulseColor"), FMath::Lerp(CityLight[S], FLinearColor::White, 0.55f) * 4.0f);
			// Half the city's glow, comets and all (Walt, 2026-10-18: "to give a more cosmic distance
			// sense, please dim the rods even more, by half"): halving Base alone darkened the
			// frame by a tenth - what reads as a rod is mostly the comets riding it.
			Strut->SetScalarParameterValue(TEXT("Intensity"), CityGlow * 0.5f);
			Strut->SetScalarParameterValue(TEXT("Base"), 0.03f);   // 0.15 washed every rod to white
		}
		ThoughtLights.Add(Strut);
		Wear(ShellStruts.IsValidIndex(S) ? ShellStruts[S].Get() : nullptr, Strut);
	}
	Wear(Rings, Surface(RingHull, 0.25f));
	Wear(RingLights, Glow(RingLight, CityGlow * 1.5f));
	Wear(SkyStars, Glow(FLinearColor(0.85f, 0.9f, 1.0f), 40.0f));
	Wear(GalleryFloor, Surface(FloorDark, 0.06f));
	Wear(GalleryRail, Glass(FLinearColor(0.5f, 0.75f, 1.0f), 0.8f, 0.25f, 0.03f));
	Wear(GalleryLines, Glow(Edge, 8.0f));
	for (int32 U = 0; U < UniverseCount; ++U)
	{
		const FLinearColor Hue = Universes[U].Hue;
		Wear(UniverseGlass.IsValidIndex(U) ? UniverseGlass[U].Get() : nullptr, Glass(Hue * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f), 1.2f, 0.45f, 0.02f));
		Wear(UniverseStars.IsValidIndex(U) ? UniverseStars[U].Get() : nullptr, Glow(Hue, 25.0f));
	}
}

void AAICity::Light()
{
	using namespace AICityDetail;
	// The star's light by the inverse square, set so it is GalleryLux where he stands.
	const double Metres = GalleryDistance / 100.0;
	StarLight->SetIntensityUnits(ELightUnits::Candelas);
	StarLight->SetIntensity(static_cast<float>(GalleryLux * Metres * Metres));
	StarLight->SetAttenuationRadius(2000000.0f);
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

void AAICity::Tick(float DeltaSeconds)
{
	using namespace AICityDetail;
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (IsAnswering())
	{
		ResponseAge += DeltaSeconds;
		const float Duration = bFirstResponse ? 12.0f : 5.0f;
		const float Stagger = bFirstResponse ? 2.0f : 0.7f;     // each shell starts this much after the last
		const float Breath = bFirstResponse ? 6.0f : 2.5f;      // and takes this long to swell and fade
		for (int32 S = 0; S < ResponseLights.Num(); ++S)
		{
			if (!ResponseLights[S])
			{
				continue;
			}
			const float Local = FMath::Clamp((ResponseAge - S * Stagger) / Breath, 0.0f, 1.0f);
			// One slow breath per shell - brighter, toward sea-green, and back. No flashing, no
			// camera move.
			const float Wave = FMath::Square(FMath::Sin(PI * Local));
			ResponseLights[S]->SetScalarParameterValue(TEXT("Intensity"), CityGlow * (1.0f + Wave * 0.65f));
			ResponseLights[S]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(CityLight[S], FLinearColor(0.5f, 1.0f, 0.8f), Wave * 0.6f));
			// ...and its thought surges: every comet on the shell brighter while the breath passes.
			if (ThoughtLights.IsValidIndex(S) && ThoughtLights[S])
			{
				ThoughtLights[S]->SetScalarParameterValue(TEXT("Surge"), Wave);
			}
		}
		if (ResponseAge >= Duration)
		{
			ResponseAge = -1.0f;
			for (UMaterialInstanceDynamic* M : ThoughtLights)
			{
				if (M)
				{
					M->SetScalarParameterValue(TEXT("Surge"), 0.0f);
				}
			}
			// Recorded only now, at the end: a first contact cut short still counts as unseen.
			if (bFirstResponse)
			{
				if (UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this))
				{
					Progress->CompleteUniverseContact();
				}
			}
		}
	}

	// Each shell turns on its own axis, its struts and nodes together.
	for (int32 S = 0; S < ShellCount; ++S)
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

	/* WHERE HE IS AND WHAT IS HERE, just above "[O] Back to Office" (Walt, 2026-10-15: "can
	   you please put some instructions on the screen above [O] Back to Office?"). While he is
	   at the forge's console it shows its own controls there instead. The third line (2026-10-17)
	   tells him the empty plinths are his: nothing else says a universe can be kept. */
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && !AUniverseForge::IsAnyInUse(this))
	{
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ShowAwayHint(TEXT("A city that is also a mind, wrapped round a star - built by the AIs to amuse themselves\n")
				TEXT("The small glass spheres hold universes the AIs have grown.  Grow your own: [E] at the console by the great sphere.\n")
				TEXT("Keep one with [K] when it ends - the three plinths facing the sun are your gallery."));
		}
	}

	// The universes bob, and the galaxy in each turns.
	for (int32 U = 0; U < UniverseCount && U < UniverseHome.Num(); ++U)
	{
		const FVector At = UniverseHome[U] + FVector(0.0, 0.0, 12.0 * FMath::Sin(Age * 0.6f + U));
		if (UniverseGlass.IsValidIndex(U) && UniverseGlass[U])
		{
			UniverseGlass[U]->SetRelativeLocation(At);
		}
		if (UniverseStars.IsValidIndex(U) && UniverseStars[U])
		{
			const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, UniverseAxis[U]);
			const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Age * UniverseRate[U]));
			UniverseStars[U]->SetRelativeLocationAndRotation(At, Tilt * Spin);
		}
	}
}
