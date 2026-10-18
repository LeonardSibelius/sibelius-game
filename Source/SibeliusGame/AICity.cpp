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

	// The city's three shells, inner to outer: their radius, their avenues, their light.
	constexpr double ShellRadius[AAICity::ShellCount] = { 34000.0, 46000.0, 60000.0 };
	constexpr double BlockPitch = 2600.0;            // one block and its share of the street
	const FVector ShellAxis[AAICity::ShellCount] = {
		FVector(0.0, 0.0, 1.0), FVector(0.3, 0.0, 1.0).GetSafeNormal(), FVector(0.0, -0.4, 1.0).GetSafeNormal() };

	// LINEAR colours, which is what a material multiplies.
	const FLinearColor StarColour(1.0f, 0.86f, 0.62f);
	const FLinearColor CoronaColour(1.0f, 0.58f, 0.24f);
	const FLinearColor Hull(0.025f, 0.028f, 0.035f);
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

	/* THE SUN OF CELESTIAL FORTUNE (Walt, 2026-10-15: "could the sun here be made to look like
	   my sun in https://leonardsibelius.com/celestial-fortune/ ?"). The slot's own sun - its
	   SVG rendered to T_Sun (make_sun_texture.mjs) - alive in M_GenesisSunDisc (a slow turn, a
	   shimmer, a breath), on a flat disc facing the gallery: the gallery is 260 m off and 30 m
	   across, so the star is only ever seen from this side. Until the material is made, the
	   sphere and its glass corona stand in. */
	UMaterialInterface* SunDiscMaterial()
	{
		// The layered one first (Walt: "let's do the medium please" - each loop rising and
		// falling on its own, the spots drifting, the flare bursting), then the one-picture one.
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
		ShellBlocks.Add(MakeMany(FString::Printf(TEXT("ShellBlocks_%d"), S), Cube, false));
		ShellLights.Add(MakeMany(FString::Printf(TEXT("ShellLights_%d"), S), Cube, false));
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

	// --- the shells: blocks between avenues, towers growing inward toward the light ---------
	for (int32 S = 0; S < ShellCount; ++S)
	{
		UInstancedStaticMeshComponent* Blocks = ShellBlocks.IsValidIndex(S) ? ShellBlocks[S].Get() : nullptr;
		UInstancedStaticMeshComponent* Lights = ShellLights.IsValidIndex(S) ? ShellLights[S].Get() : nullptr;
		if (!Blocks || !Lights)
		{
			continue;
		}
		Blocks->ClearInstances();
		Lights->ClearInstances();
		FRandomStream Rng(1000 + S);
		const double R = ShellRadius[S];
		TArray<FTransform> BlockList;
		TArray<FTransform> LightList;
		/* A STREET GRID ON A SPHERE (the first render, 2026-10-15, scattered blocks evenly and
		   turned each at random, and it read as tumbling confetti, not a city). Rows of
		   latitude, each cut into as many lots as fit round it; every block squared to its
		   own east and north; narrow alleys between lots, a wide avenue every AvenueRows rows
		   and along Meridians meridians; a lot in ten left empty, a park. Nothing within
		   ten degrees of a pole, where the rows would crowd. */
		const int32 Rows = FMath::RoundToInt(PI * R / BlockPitch);
		const int32 AvenueRows = 6 + S;
		const int32 Meridians = 18 + 6 * S;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const double Lat = -0.5 * PI + (Row + 0.5) * PI / Rows;
			if (FMath::Abs(Lat) > FMath::DegreesToRadians(80.0) || Row % AvenueRows == 0)
			{
				continue;
			}
			const double Round = 2.0 * PI * R * FMath::Cos(Lat);
			const int32 Lots = FMath::Max(1, FMath::RoundToInt(Round / BlockPitch));
			const double Along = Round / Lots;          // this row's spacing, east-west
			const double Across = PI * R / Rows;        // and north-south
			for (int32 Lot = 0; Lot < Lots; ++Lot)
			{
				const double Lon = 2.0 * PI * (Lot + 0.5) / Lots;
				const float Park = Rng.FRand();
				const float TallRoll = Rng.FRand();
				const int32 Count = Rng.RandRange(0, 3);
				// The meridian avenues: the same longitudes on every row, so they line up.
				const double M = FMath::Frac(Lon / (2.0 * PI) * Meridians + 0.37 * S);
				if (M < 0.12 || Park < 0.1f)
				{
					continue;
				}
				const FVector Dir(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
				const FVector East(-FMath::Sin(Lon), FMath::Cos(Lon), 0.0);
				const FVector North = FVector::CrossProduct(Dir, East);
				const FQuat Turn = FMatrix(East, North, Dir, FVector::ZeroVector).ToQuat();
				const double Height = 150.0 + FMath::Pow(static_cast<double>(TallRoll), 3.0) * 3500.0;
				const double Wide = Along * 0.86;
				const double Deep = Across * 0.86;
				BlockList.Add(Bar(Turn, Dir * (R - Height * 0.5), FVector(Wide, Deep, Height)));

				// Its lights: up to three bars on the face toward the star, square to the grid.
				for (int32 L = 0; L < Count; ++L)
				{
					const double Ox = Rng.FRandRange(-0.35f, 0.35f) * Wide;
					const double Oy = Rng.FRandRange(-0.35f, 0.35f) * Deep;
					const bool bAcross = Rng.FRand() < 0.5f;
					const double Long = Rng.FRandRange(300.0f, 900.0f);
					const FQuat LightTurn = bAcross ? Turn * FQuat(FVector::UpVector, PI * 0.5) : Turn;
					LightList.Add(Bar(LightTurn, Dir * (R - Height - 30.0) + East * Ox + North * Oy, FVector(Long, 60.0, 40.0)));
				}
			}
		}
		Blocks->AddInstances(BlockList, false);
		Lights->AddInstances(LightList, false);
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
	UMaterialInstanceDynamic* HullPaint = Surface(Hull, 0.35f);
	ResponseLights.Reset();
	for (int32 S = 0; S < ShellCount; ++S)
	{
		Wear(ShellBlocks.IsValidIndex(S) ? ShellBlocks[S].Get() : nullptr, HullPaint);
		// Kept, so the city's answer can breathe each shell's lights (AnswerUniverse).
		UMaterialInstanceDynamic* Response = Glow(CityLight[S], CityGlow);
		ResponseLights.Add(Response);
		Wear(ShellLights.IsValidIndex(S) ? ShellLights[S].Get() : nullptr, Response);
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
		}
		if (ResponseAge >= Duration)
		{
			ResponseAge = -1.0f;
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

	// Each shell turns on its own axis; the blocks and their lights together.
	for (int32 S = 0; S < ShellCount; ++S)
	{
		const float Spin = ShellSpin.IsValidIndex(S) ? ShellSpin[S] : 0.0f;
		const FQuat Turn(ShellAxis[S], FMath::DegreesToRadians(Age * Spin));
		if (ShellBlocks.IsValidIndex(S) && ShellBlocks[S])
		{
			ShellBlocks[S]->SetRelativeRotation(Turn);
		}
		if (ShellLights.IsValidIndex(S) && ShellLights[S])
		{
			ShellLights[S]->SetRelativeRotation(Turn);
		}
	}

	/* WHERE HE IS AND WHAT IS HERE, just above "[O] Back to Office" (Walt, 2026-10-15: "can
	   you please put some instructions on the screen above [O] Back to Office?"). While he is
	   at the forge's console it shows its own controls there instead. */
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && !AUniverseForge::IsAnyInUse(this))
	{
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ShowAwayHint(TEXT("A city that is also a mind, wrapped round a star - built by the AIs to amuse themselves\n")
				TEXT("The small glass spheres hold universes the AIs have grown.  Grow your own: [E] at the console by the great sphere."));
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
