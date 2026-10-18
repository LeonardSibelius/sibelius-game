// UniverseForge.cpp - see the header for what the forge shows and what decides it.

#include "UniverseForge.h"

#include "SibeliusGame.h"
#include "SibeliusHUD.h"
#include "AICity.h"
#include "UniverseExhibit.h"
#include "ProgressionSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/DateTime.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace UniverseForgeDetail
{
	constexpr double ShapeSize = 100.0;          // the engine's basic shapes are a metre across
	constexpr double GrainSize = 6.0;            // cm across, one grain of matter
	// The camera, from the sphere's centre. At first well back and at chest height, so Leonard
	// stands whole at the console under the glass (Walt, 2026-10-15: "field of view could be
	// bigger? maybe show Leonard's legs?" - from 19 m back and 80 cm up it framed only the top
	// of his head); rising as it dives, so it passes over him, not through him.
	constexpr double CameraBack = 2500.0;
	constexpr double CameraNear = 750.0;         // once it has dived into a galaxy
	constexpr double CameraUpFar = -120.0;       // the sphere's centre is 3 m over the floor
	constexpr double CameraUpNear = 60.0;
	constexpr float CameraFov = 90.0f;

	FVector CameraAt(const FVector& Toward, double Back)
	{
		const double A = FMath::Clamp((CameraBack - Back) / (CameraBack - CameraNear), 0.0, 1.0);
		return Toward * Back + FVector(0.0, 0.0, FMath::Lerp(CameraUpFar, CameraUpNear, A));
	}
	// The one star, its world and its machines, in the universe's radius. The first render had
	// them a third this size: a speck and a dot from the console (2026-10-15).
	constexpr double SunSize = 0.09;
	constexpr double WorldSize = 0.035;
	constexpr double WorldOrbit = 0.33;
	constexpr double MachineShell = 0.16;
	constexpr double DiveScale = 4.0;            // how much the dived-into galaxy grows: to fill the glass, not past it

	double Smooth(double A)
	{
		const double C = FMath::Clamp(A, 0.0, 1.0);
		return C * C * (3.0 - 2.0 * C);
	}

	// Its stars' colour, red to blue, as the light dial set it (LINEAR).
	FLinearColor HeatColour(double Heat)
	{
		static const FLinearColor Stops[] = {
			FLinearColor(1.0f, 0.25f, 0.1f), FLinearColor(1.0f, 0.55f, 0.2f), FLinearColor(1.0f, 0.85f, 0.45f),
			FLinearColor(0.95f, 0.95f, 1.0f), FLinearColor(0.55f, 0.7f, 1.0f) };
		const double H = FMath::Clamp(Heat, 0.0, 1.0) * 4.0;
		const int32 I = FMath::Min(3, static_cast<int32>(H));
		return FMath::Lerp(Stops[I], Stops[I + 1], static_cast<float>(H - I));
	}

	const FLinearColor Hot(1.0f, 1.0f, 1.0f);
	const FLinearColor Cooling(1.0f, 0.6f, 0.3f);
	const FLinearColor Fog(0.45f, 0.55f, 1.0f);
	const FLinearColor Mote(0.6f, 0.7f, 1.0f);
	const FLinearColor Rock(0.22f, 0.18f, 0.15f);
	const FLinearColor Seas(0.08f, 0.32f, 0.45f);
	const FLinearColor Ice(0.8f, 0.85f, 0.9f);
	const FLinearColor MachineCyan(0.3f, 0.9f, 1.0f);
	const FLinearColor Slate(0.03f, 0.032f, 0.04f);

	const TCHAR* const DialNames[AUniverseForge::DialCount] = { TEXT("GRAVITY"), TEXT("LIGHT  "), TEXT("TIME   ") };
	const TCHAR* const DialMeaning[AUniverseForge::DialCount] = {
		TEXT("how hard its matter pulls together"), TEXT("how fiercely its stars burn"), TEXT("how long it lasts") };
}

AUniverseForge::AUniverseForge()
{
	using namespace UniverseForgeDetail;
	PrimaryActorTick.bCanEverTick = true;

	// The first universe starts with time at 0.75, not 0.5: at 0.5 the machines' edge is a coin
	// toss with the seed, and a first try should reach them. The edges are still there to find.
	Dials.Time = 0.75;

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

	auto Quiet = [](UPrimitiveComponent* Part)
	{
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetReceivesDecals(false);
		Part->SetCastShadow(false);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	};
	auto MakeMesh = [this, &Quiet](const FString& Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Part->SetupAttachment(SceneRoot);
		Part->SetStaticMesh(Mesh);
		Quiet(Part);
		return Part;
	};
	auto MakeMany = [this, &Quiet](const FString& Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Part = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*Name);
		Part->SetupAttachment(SceneRoot);
		Part->SetStaticMesh(Mesh);
		Quiet(Part);
		return Part;
	};

	Glass = MakeMesh(TEXT("Glass"), Sphere);
	Matter = MakeMany(TEXT("Matter"), Sphere);
	Flash = MakeMesh(TEXT("Flash"), Sphere);
	Sun = MakeMesh(TEXT("Sun"), Sphere);
	World = MakeMesh(TEXT("World"), Sphere);
	Machines = MakeMany(TEXT("Machines"), Cube);
	HoleCores = MakeMany(TEXT("HoleCores"), Sphere);
	HoleRims = MakeMany(TEXT("HoleRims"), Sphere);
	ConsoleBase = MakeMesh(TEXT("ConsoleBase"), Cube);
	ConsoleBase->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ConsoleBase->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	ConsoleBase->SetCastShadow(true);
	ConsoleTop = MakeMesh(TEXT("ConsoleTop"), Cylinder);
	for (int32 D = 0; D < DialCount; ++D)
	{
		DialRings.Add(MakeMesh(FString::Printf(TEXT("Dial_%d"), D), Cylinder));
	}

	ConsoleReach = CreateDefaultSubobject<UBoxComponent>(TEXT("ConsoleReach"));
	ConsoleReach->SetupAttachment(SceneRoot);
	ConsoleReach->SetBoxExtent(FVector(70.0, 50.0, 80.0));
	ConsoleReach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ConsoleReach->SetCollisionResponseToAllChannels(ECR_Ignore);
	ConsoleReach->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ConsoleReach->SetGenerateOverlapEvents(false);

	ForgeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ForgeCamera"));
	ForgeCamera->SetupAttachment(SceneRoot);
	ForgeCamera->SetFieldOfView(UniverseForgeDetail::CameraFov);
}

void AUniverseForge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace UniverseForgeDetail;

	// The parts that follow the tuning: the glass's size, the console's place.
	Glass->SetRelativeScale3D(FVector(2.0 * UniverseRadius / ShapeSize));
	const FVector C = ConsoleOffset;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(-C.Y, -C.X));   // the console faces the glass
	const FQuat Facing = FRotator(0.0, Yaw, 0.0).Quaternion();
	ConsoleBase->SetRelativeLocationAndRotation(C + FVector(0.0, 0.0, 47.5), Facing);
	ConsoleBase->SetRelativeScale3D(FVector(60.0, 110.0, 95.0) / ShapeSize);
	ConsoleTop->SetRelativeLocationAndRotation(C + FVector(0.0, 0.0, 98.0), Facing);
	ConsoleTop->SetRelativeScale3D(FVector(80.0, 130.0, 6.0) / ShapeSize);
	for (int32 D = 0; D < DialCount; ++D)
	{
		DialRings[D]->SetRelativeLocation(C + FVector(0.0, 0.0, 102.0) + Facing.RotateVector(FVector(0.0, (D - 1) * 38.0, 0.0)));
		DialRings[D]->SetRelativeScale3D(FVector(24.0, 24.0, 3.0) / ShapeSize);
	}
	ConsoleReach->SetRelativeLocation(C + FVector(0.0, 0.0, 70.0));
	ConsoleReach->SetRelativeRotation(Facing);

	Paint();
	Layout();
	PoseIdle();
	AimCamera(true);
}

/* The backdrop (BeginPlay) shows only while someone is at the console: from the gallery, idle,
   the black disc read as a hole cut in the city's lattice behind the glass (Walt, 2026-10-18:
   "the lattice doesn't fit around the edge of that universe building globe thing"). */
static void ShowForgeBackdrop(AActor* Forge, bool bShow)
{
	for (UActorComponent* Part : Forge->GetComponents())
	{
		if (USceneComponent* Scene = Part && Part->GetFName() == TEXT("Backdrop") ? Cast<USceneComponent>(Part) : nullptr)
		{
			Scene->SetVisibility(bShow);
		}
	}
}

void AUniverseForge::BeginPlay()
{
	Super::BeginPlay();

	/* A BLACK BACKDROP behind the glass, seen from the console: without it the city's lit
	   blocks and its transit ring showed through and fought every stage (the first renders,
	   2026-10-15). A disc a little wider than the sphere, just past it, unlit black. Made at
	   play, so the editor shows the glass as it is - and hidden until someone steps up. */
	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMeshComponent* Backdrop = Disc ? NewObject<UStaticMeshComponent>(this, TEXT("Backdrop")) : nullptr;
	if (Backdrop)
	{
		using namespace UniverseForgeDetail;
		const FVector Toward = ConsoleOffset.GetSafeNormal2D();
		Backdrop->SetupAttachment(SceneRoot);
		Backdrop->SetStaticMesh(Disc);
		Backdrop->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Backdrop->SetCastShadow(false);
		Backdrop->SetRelativeLocationAndRotation(-Toward * UniverseRadius * 1.08, FRotationMatrix::MakeFromZ(Toward).ToQuat());
		Backdrop->SetRelativeScale3D(FVector(2.6 * UniverseRadius / ShapeSize, 2.6 * UniverseRadius / ShapeSize, 1.0 / ShapeSize));
		if (GlowMaterial)
		{
			UMaterialInstanceDynamic* Black = UMaterialInstanceDynamic::Create(GlowMaterial, this);
			Black->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
			Black->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
			Backdrop->SetMaterial(0, Black);
		}
		Backdrop->SetVisibility(false);
		Backdrop->RegisterComponent();
		AddInstanceComponent(Backdrop);
	}

	/* THE CONSOLE MUST BE FOUND (Walt, 2026-10-15: "it took me a minute to find the console -
	   it is very black"). Slate on a black glass floor under a black sky: so a pool of light
	   at its foot, a beam from it up to the glass - it feeds the sphere, and it points the
	   way from anywhere on the gallery - and a soft light over it, for him and the dials.
	   (Its top glows too: Paint.) Made at play, like the backdrop. */
	{
		using namespace UniverseForgeDetail;
		auto AddGlow = [this](const TCHAR* Name, UStaticMesh* Mesh, const FTransform& Where, const FLinearColor& Color, float Intensity)
		{
			UStaticMeshComponent* Part = Mesh ? NewObject<UStaticMeshComponent>(this, Name) : nullptr;
			if (!Part)
			{
				return;
			}
			Part->SetupAttachment(SceneRoot);
			Part->SetStaticMesh(Mesh);
			Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Part->SetCastShadow(false);
			Part->SetRelativeTransform(Where);
			if (GlowMaterial)
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(GlowMaterial, this);
				M->SetVectorParameterValue(TEXT("Color"), Color);
				M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
				Part->SetMaterial(0, M);
			}
			Part->RegisterComponent();
			AddInstanceComponent(Part);
		};
		const FLinearColor Cyan(0.3f, 0.85f, 1.0f);
		const FVector Foot = ConsoleOffset + FVector(0.0, 0.0, 0.6);
		AddGlow(TEXT("ConsolePool"), Disc, FTransform(FQuat::Identity, Foot, FVector(300.0, 300.0, 1.0) / ShapeSize), Cyan, 1.2f);
		// The beam: from the console's top to the nearest point of the glass.
		const FVector From = ConsoleOffset + FVector(0.0, 0.0, 104.0);
		const FVector To = From.GetSafeNormal() * UniverseRadius;
		const FVector Span = To - From;
		AddGlow(TEXT("ConsoleBeam"), Disc, FTransform(FRotationMatrix::MakeFromZ(Span.GetSafeNormal()).ToQuat(), (From + To) * 0.5,
			FVector(5.0, 5.0, Span.Size()) / ShapeSize), Cyan, 18.0f);

		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this, TEXT("ConsoleLamp"));
		Lamp->SetupAttachment(SceneRoot);
		Lamp->SetRelativeLocation(ConsoleOffset + FVector(0.0, 0.0, 260.0));
		Lamp->SetIntensityUnits(ELightUnits::Candelas);
		Lamp->SetIntensity(60.0f);
		Lamp->SetAttenuationRadius(700.0f);
		Lamp->SetLightColor(FLinearColor(0.75f, 0.9f, 1.0f));
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
		AddInstanceComponent(Lamp);
	}

	ForgeCamera->SetFieldOfView(UniverseForgeDetail::CameraFov);   // at play too: a placed forge keeps its saved one
	Paint();
	Layout();
	PoseIdle();
	AimCamera(true);
}

void AUniverseForge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

bool AUniverseForge::IsAnyInUse(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W)
	{
		return false;
	}
	for (TActorIterator<AUniverseForge> It(W); It; ++It)
	{
		if (It->bPlaying)
		{
			return true;
		}
	}
	return false;
}

void AUniverseForge::Paint()
{
	using namespace UniverseForgeDetail;
	auto Glow = [this](const FLinearColor& Color, float Intensity)
	{
		UMaterialInstanceDynamic* M = GlowMaterial ? UMaterialInstanceDynamic::Create(GlowMaterial, this) : nullptr;
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		}
		return M;
	};
	auto Surface = [this](const FLinearColor& Color, float Roughness)
	{
		UMaterialInstanceDynamic* M = SurfaceMaterial ? UMaterialInstanceDynamic::Create(SurfaceMaterial, this) : nullptr;
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			M->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
		}
		return M;
	};
	auto GlassOf = [this](const FLinearColor& Color, float Intensity, float Rim, float Base)
	{
		UMaterialInstanceDynamic* M = GlassMaterial ? UMaterialInstanceDynamic::Create(GlassMaterial, this) : nullptr;
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
			M->SetScalarParameterValue(TEXT("RimOpacity"), Rim);
			M->SetScalarParameterValue(TEXT("BaseOpacity"), Base);
		}
		return M;
	};
	auto Wear = [](UPrimitiveComponent* Part, UMaterialInterface* M)
	{
		if (Part && M)
		{
			Part->SetMaterial(0, M);
		}
	};

	Wear(Glass, GlassOf(FLinearColor(0.55f, 0.75f, 1.0f), 1.0f, 0.35f, 0.01f));
	MatterLight = Glow(Mote, 2.5f);
	Wear(Matter, MatterLight);
	FlashLight = Glow(Hot, 0.0f);
	Wear(Flash, FlashLight);
	SunLight = Glow(HeatColour(0.5), 30.0f);
	Wear(Sun, SunLight);
	WorldSkin = Surface(Rock, 0.6f);
	Wear(World, WorldSkin);
	MachineLight = Glow(MachineCyan, 15.0f);
	Wear(Machines, MachineLight);
	Wear(HoleCores, Surface(FLinearColor(0.0f, 0.0f, 0.0f), 1.0f));
	Wear(HoleRims, GlassOf(FLinearColor(1.0f, 0.55f, 0.2f), 3.0f, 0.9f, 0.0f));
	Wear(ConsoleBase, Surface(Slate, 0.3f));
	// Its top a ring of light under the dials, so the console is found in the dark.
	Wear(ConsoleTop, Glow(FLinearColor(0.3f, 0.85f, 1.0f), 4.0f));
	DialLight.Reset();
	for (int32 D = 0; D < DialCount; ++D)
	{
		UMaterialInstanceDynamic* M = Glow(HeatColour(0.25 * (D + 1)), 8.0f);
		DialLight.Add(M);
		Wear(DialRings[D], M);
	}
}

void AUniverseForge::Layout()
{
	// The grains of this universe: where the bang leaves each, the thread and the clump it
	// gathers into, and its place in a galaxy. From the history's seed, so the same universe
	// always looks the same.
	FRandomStream Rng(static_cast<int32>(History.Seed & 0x7fffffff) + 11);
	const int32 Clusters = FMath::Max(4, History.Galaxies);
	ClusterCentre.Reset();
	ClusterTilt.Reset();
	for (int32 K = 0; K < Clusters; ++K)
	{
		FVector C;
		for (int32 Try = 0; Try < 30; ++Try)
		{
			C = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-0.6f, 0.6f)) * 0.55;
			bool bApart = C.Size() <= 0.6;
			for (const FVector& Other : ClusterCentre)
			{
				bApart = bApart && FVector::Dist(C, Other) > 0.28;
			}
			if (bApart)
			{
				break;
			}
		}
		// The first cluster is the galaxy the forge dives into: keep it off the very middle,
		// so the dive moves.
		ClusterCentre.Add(K == 0 ? C.GetSafeNormal() * 0.35 : C);
		// The dived-into galaxy turns its face to the console, tipped a little, so the dive
		// opens into a spiral and not an edge-on smear (the first render, 2026-10-15).
		const FVector Normal = K == 0
			? (ConsoleOffset.GetSafeNormal2D() * 0.85 + FVector::UpVector * 0.5).GetSafeNormal()
			: FVector(Rng.FRandRange(-0.6f, 0.6f), Rng.FRandRange(-0.6f, 0.6f), 1.0f).GetSafeNormal();
		ClusterTilt.Add(FQuat::FindBetweenNormals(FVector::UpVector, Normal));
	}
	Ball.SetNum(MatterCount);
	Thread.SetNum(MatterCount);
	Clump.SetNum(MatterCount);
	Spiral.SetNum(MatterCount);
	ClusterOf.SetNum(MatterCount);
	for (int32 I = 0; I < MatterCount; ++I)
	{
		// Evenly through the ball.
		FVector P;
		do
		{
			P = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f));
		} while (P.SizeSquared() > 1.0);
		Ball[I] = P * 0.92;
		const int32 K = I % Clusters;
		ClusterOf[I] = K;
		Thread[I] = ClusterCentre[K] + (Ball[I] - ClusterCentre[K]) * 0.45;
		Clump[I] = ClusterCentre[K] + FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f)) * 0.05;
		// Its galaxy: a bulge and two-to-four arms, in the galaxy's own plane.
		const int32 Arms = 2 + K % 3;
		const double T = FMath::Pow(static_cast<double>(Rng.FRand()), 0.6);
		if (Rng.FRand() < 0.15f)
		{
			Spiral[I] = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-0.4f, 0.4f)) * 0.03;
		}
		else
		{
			const double Th = 2.0 * PI * (I % Arms) / Arms + T * 3.6 + Rng.FRandRange(-0.25f, 0.25f);
			const double R = 0.17 * (0.12 + 0.88 * T);
			Spiral[I] = FVector(R * FMath::Cos(Th), R * FMath::Sin(Th), Rng.FRandRange(-0.008f, 0.008f));
		}
	}
	MachineDir.SetNum(MachineCount);
	for (int32 J = 0; J < MachineCount; ++J)
	{
		// Evenly round their star (a Fibonacci lattice), in the order they are built: from one
		// pole down, so the shell visibly closes.
		const double Z = 1.0 - 2.0 * (J + 0.5) / MachineCount;
		const double Rho = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		const double Phi = J * 2.39996322972865332;
		MachineDir[J] = FVector(Rho * FMath::Cos(Phi), Rho * FMath::Sin(Phi), Z);
	}

	// The instance lists, sized once; Pose moves them.
	auto Fill = [this](UInstancedStaticMeshComponent* Many, int32 Count)
	{
		if (Many && Many->GetInstanceCount() != Count)
		{
			Many->ClearInstances();
			Scratch.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Count);
			Many->AddInstances(Scratch, false);
		}
	};
	Fill(Matter, MatterCount);
	Fill(Machines, MachineCount);
	Fill(HoleCores, HoleCount);
	Fill(HoleRims, HoleCount);
}

void AUniverseForge::PoseIdle()
{
	using namespace UniverseForgeDetail;
	// Nobody growing: a faint mote turns in the middle of the glass - the seed of one.
	const double RU = UniverseRadius;
	const FQuat Spin(FVector::UpVector, Age * 0.15);
	Scratch.SetNum(MatterCount);
	for (int32 I = 0; I < MatterCount; ++I)
	{
		Scratch[I] = FTransform(FQuat::Identity, Spin.RotateVector(Ball[I] * 0.22) * RU, FVector(GrainSize * 0.6 / ShapeSize));
	}
	Matter->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
	if (MatterLight)
	{
		MatterLight->SetVectorParameterValue(TEXT("Color"), Mote);
		MatterLight->SetScalarParameterValue(TEXT("Intensity"), 2.5f + 1.0f * FMath::Sin(Age * 1.3f));
	}
	for (UStaticMeshComponent* Part : { Flash.Get(), Sun.Get(), World.Get() })
	{
		Part->SetVisibility(false);
	}
	Scratch.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MachineCount);
	Machines->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
	Scratch.SetNum(HoleCount);
	HoleCores->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
	HoleRims->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
}

void AUniverseForge::Pose()
{
	using namespace UniverseForgeDetail;
	const double RU = UniverseRadius;
	const int32 Last = static_cast<int32>(History.LastStage);

	// Where the clock is: a stage and how far into it, or the ending and how far into that.
	int32 Stage = 0;
	double U = 0.0;
	double E = -1.0;
	{
		double At = 0.0;
		bool bFound = false;
		for (int32 S = 0; S <= Last; ++S)
		{
			const double D = StageSeconds.IsValidIndex(S) ? FMath::Max(StageSeconds[S], 0.5f) : 10.0;
			if (Clock < At + D)
			{
				Stage = S;
				U = (Clock - At) / D;
				bFound = true;
				break;
			}
			At += D;
		}
		if (!bFound)
		{
			Stage = Last;
			U = 1.0;
			E = FMath::Clamp((Clock - At) / FMath::Max(EndingSeconds, 0.5f), 0.0, 1.0);
		}
	}
	const EUniverseEnding Ending = History.Ending;
	const bool bEnding = E >= 0.0;
	const FLinearColor StarHue = HeatColour(History.StarHeat);

	// The galaxies turn on the clock, so a frozen preview is a still.
	auto InGalaxy = [this](int32 I, double Clk)
	{
		const int32 K = ClusterOf[I];
		const FQuat Spin(FVector::UpVector, Clk * 0.12 * ((K % 2) ? 1.0 : -1.0));
		return ClusterCentre[K] + ClusterTilt[K].RotateVector(Spin.RotateVector(Spiral[I]));
	};

	// The dive into the first galaxy: 0 before the world stage, to 1 once in.
	double Zoom = 0.0;
	if (Stage > static_cast<int32>(EUniverseStage::World))
	{
		Zoom = 1.0;
	}
	else if (Stage == static_cast<int32>(EUniverseStage::World))
	{
		Zoom = Smooth(U / 0.6);
	}

	FLinearColor Colour = StarHue;
	double Bright = 18.0;
	double Size = GrainSize;
	Scratch.SetNum(MatterCount);
	for (int32 I = 0; I < MatterCount; ++I)
	{
		FVector P;
		double S = Size;
		switch (static_cast<EUniverseStage>(Stage))
		{
		case EUniverseStage::Bang:
			P = Ball[I] * FMath::Pow(Smooth(U), 0.6);
			break;
		case EUniverseStage::FirstLight:
			// The fog thins outward, but stays inside the glass.
			P = Ending == EUniverseEnding::ColdFog ? Ball[I] * (1.0 + 0.04 * U) : FMath::Lerp(Ball[I], Thread[I], Smooth(U));
			if (bEnding && Ending == EUniverseEnding::ColdFog)
			{
				P = Ball[I] * (1.04 + 0.04 * E);
			}
			break;
		case EUniverseStage::FirstStars:
			P = FMath::Lerp(Thread[I], Clump[I], Smooth(U));
			if (bEnding && Ending == EUniverseEnding::Crunch)
			{
				// Everything falls into the middle of its clump, and goes out.
				P = FMath::Lerp(Clump[I], ClusterCentre[ClusterOf[I]], Smooth(E));
				S = FMath::Lerp(Size, 0.0, Smooth(E));
			}
			break;
		case EUniverseStage::Galaxies:
			P = FMath::Lerp(Clump[I], InGalaxy(I, Clock), Smooth(U * 2.5));
			break;
		default:
			P = InGalaxy(I, Clock);
			break;
		}
		if (Zoom > 0.0)
		{
			// Into the first galaxy: everything else flies out past the glass and is gone.
			P = (P - ClusterCentre[0] * Zoom) * (1.0 + DiveScale * Zoom);
			S = Size * FMath::Lerp(1.0, 0.7, Zoom);
			if (P.SizeSquared() > 0.97)
			{
				S = 0.0;
			}
		}
		Scratch[I] = FTransform(FQuat::Identity, P * RU, FVector(S / ShapeSize));
	}
	Matter->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);

	// The grains' light, by stage.
	switch (static_cast<EUniverseStage>(Stage))
	{
	case EUniverseStage::Bang:
		Colour = FMath::Lerp(Hot, Cooling, static_cast<float>(U));
		Bright = FMath::Lerp(60.0, 10.0, U);
		break;
	case EUniverseStage::FirstLight:
		Colour = Fog;
		Bright = FMath::Lerp(10.0, 3.0, U);
		if (bEnding && Ending == EUniverseEnding::ColdFog)
		{
			Bright = FMath::Lerp(3.0, 0.4, E);
		}
		break;
	case EUniverseStage::FirstStars:
		Colour = FMath::Lerp(Fog, StarHue, static_cast<float>(Smooth(U)));
		Bright = FMath::Lerp(3.0, 18.0, U);
		break;
	case EUniverseStage::Galaxies:
		Colour = StarHue;
		Bright = 18.0;
		if (bEnding && Ending == EUniverseEnding::Burn)
		{
			// They blaze, blue-white, and are gone.
			Colour = E < 0.25 ? FMath::Lerp(StarHue, FLinearColor(0.7f, 0.8f, 1.0f), static_cast<float>(E / 0.25))
				: FMath::Lerp(FLinearColor(0.7f, 0.8f, 1.0f), FLinearColor(1.0f, 0.15f, 0.05f), static_cast<float>((E - 0.25) / 0.75));
			Bright = E < 0.25 ? FMath::Lerp(18.0, 70.0, E / 0.25) : FMath::Lerp(70.0, 0.0, (E - 0.25) / 0.75);
		}
		break;
	default:
		Colour = StarHue;
		Bright = FMath::Lerp(18.0, 7.0, Zoom);
		break;
	}
	if (bEnding && Ending == EUniverseEnding::TooYoung)
	{
		Bright *= 1.0 - E;
	}
	if (MatterLight)
	{
		MatterLight->SetVectorParameterValue(TEXT("Color"), Colour);
		MatterLight->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(Bright));
	}

	// The bang's light: a flash that opens with the ball and fades.
	Flash->SetVisibility(Stage == static_cast<int32>(EUniverseStage::Bang));
	if (Stage == static_cast<int32>(EUniverseStage::Bang))
	{
		Flash->SetRelativeScale3D(FVector(2.0 * RU * FMath::Lerp(0.05, 1.1, Smooth(U * 2.0)) / ShapeSize));
		if (FlashLight)
		{
			FlashLight->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(80.0 * FMath::Pow(1.0 - Smooth(U), 2.0)));
		}
	}

	// The one star, its world, and its machines, once the forge is inside the galaxy.
	const bool bWorlds = Stage >= static_cast<int32>(EUniverseStage::World);
	const double SunIn = Stage > static_cast<int32>(EUniverseStage::World) ? 1.0 : Smooth((U - 0.3) / 0.4);
	const double WorldIn = Stage > static_cast<int32>(EUniverseStage::World) ? 1.0 : Smooth((U - 0.6) / 0.3);
	Sun->SetVisibility(bWorlds && SunIn > 0.0);
	World->SetVisibility(bWorlds && WorldIn > 0.0);
	if (bWorlds)
	{
		const bool bDim = Ending == EUniverseEnding::Dim;
		double SunBright = bDim ? 6.0 : 30.0;
		if (bEnding && Ending == EUniverseEnding::TooYoung)
		{
			SunBright *= 1.0 - E;
		}
		Sun->SetRelativeScale3D(FVector(2.0 * RU * SunSize * SunIn / ShapeSize));
		if (SunLight)
		{
			SunLight->SetVectorParameterValue(TEXT("Color"), bDim ? HeatColour(0.0) : StarHue);
			SunLight->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(SunBright));
		}
		// Its orbit faces the console (the camera looks along the console's line), tipped a
		// little, so the world swings round the star and not in front of it.
		const double Orbit = Clock * 0.5;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, ConsoleOffset.GetSafeNormal2D());
		World->SetRelativeLocation((Side * FMath::Cos(Orbit) + FVector::UpVector * FMath::Sin(Orbit)
			+ ConsoleOffset.GetSafeNormal2D() * 0.3 * FMath::Sin(Orbit)) * RU * WorldOrbit);
		World->SetRelativeScale3D(FVector(2.0 * RU * WorldSize * WorldIn / ShapeSize));
		if (WorldSkin)
		{
			FLinearColor Skin = Rock;
			if (bDim)
			{
				Skin = Ice;
			}
			else if (Stage >= static_cast<int32>(EUniverseStage::Life))
			{
				Skin = FMath::Lerp(Rock, Seas, static_cast<float>(Stage > static_cast<int32>(EUniverseStage::Life) ? 1.0 : Smooth(U)));
			}
			WorldSkin->SetVectorParameterValue(TEXT("Color"), Skin);
		}
	}

	// Its machines: one by one round their star, from a pole down, until the shell closes.
	Scratch.SetNum(MachineCount);
	const bool bBuilding = Stage == static_cast<int32>(EUniverseStage::Machines);
	const double Built = bBuilding ? (bEnding ? 1.0 : U) : 0.0;
	for (int32 J = 0; J < MachineCount; ++J)
	{
		const double Grow = FMath::Clamp(Built * MachineCount - J, 0.0, 1.0);
		const FVector Dir = MachineDir[J];
		Scratch[J] = FTransform(FRotationMatrix::MakeFromZ(Dir).ToQuat(), Dir * RU * MachineShell,
			FVector(17.0, 17.0, 3.5) * Grow / ShapeSize);
	}
	Machines->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
	if (MachineLight && bBuilding)
	{
		MachineLight->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(bEnding ? FMath::Lerp(15.0, 35.0, E) : 15.0));
	}

	// The crunch's black holes, where the clumps fell in.
	Scratch.SetNum(HoleCount);
	const double Hole = (bEnding && Ending == EUniverseEnding::Crunch) ? Smooth(E) : 0.0;
	for (int32 H = 0; H < HoleCount; ++H)
	{
		const FVector At = ClusterCentre.IsValidIndex(H) ? ClusterCentre[H] * RU : FVector::ZeroVector;
		Scratch[H] = FTransform(FQuat::Identity, At, FVector(2.0 * RU * 0.04 * Hole / ShapeSize));
	}
	HoleCores->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
	for (FTransform& X : Scratch)
	{
		X.SetScale3D(X.GetScale3D() * 1.35);
	}
	HoleRims->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);

	// The camera dives with the zoom, and stands back a little to watch it end.
	double Back = FMath::Lerp(CameraBack, CameraNear, Zoom);
	if (bEnding && Ending == EUniverseEnding::LookedBack)
	{
		Back = FMath::Lerp(CameraNear, 1500.0, Smooth(E));
	}
	const FVector Toward = ConsoleOffset.GetSafeNormal2D();
	CameraGoal = CameraAt(Toward, Back);
	CameraGoalRot = FRotationMatrix::MakeFromX(-CameraGoal).Rotator();
}

void AUniverseForge::AimCamera(bool bInstant)
{
	using namespace UniverseForgeDetail;
	if (State != EForgeState::Growing && State != EForgeState::Ended)
	{
		const FVector Toward = ConsoleOffset.GetSafeNormal2D();
		CameraGoal = CameraAt(Toward, CameraBack);
		CameraGoalRot = FRotationMatrix::MakeFromX(-CameraGoal).Rotator();
	}
	if (bInstant || bFrozen)
	{
		ForgeCamera->SetRelativeLocationAndRotation(CameraGoal, CameraGoalRot);
	}
}

void AUniverseForge::BeginGrowing()
{
	// A fresh seed each time - two universes from the same dials are never quite the same -
	// unless R asked for this one again, so one dial can be changed and the two compared.
	const uint64 Seed = bRetrySeed ? History.Seed : static_cast<uint64>(FDateTime::UtcNow().GetTicks()) ^ (NextSeed++ * 0x9E3779B97F4A7C15ull);
	History = GrowUniverse(Dials, Seed);
	RunId = FGuid::NewGuid();             // a new identity per run: keeping it twice is caught by this
	bRetrySeed = false;
	bInspecting = false;
	bContactSent = false;
	bChoosingSlot = false;
	PendingReplaceSlot = INDEX_NONE;
	GalleryMessage.Empty();
	// The stages he had already watched when this run began: F may skip only those.
	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	SeenBeforeRun = Progress ? Progress->GetStateForRead().UniverseSeenStages : 0;
	Layout();
	Clock = 0.0f;
	bHurry = false;
	bFrozen = false;
	State = EForgeState::Growing;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Forge] %s: gravity %.2f light %.2f time %.2f -> %s at stage %d."),
		UTF8_TO_TCHAR(History.Name.c_str()), Dials.Gravity, Dials.Light, Dials.Time,
		UTF8_TO_TCHAR(History.EndingTitle.c_str()), static_cast<int32>(History.LastStage));
}

void AUniverseForge::PreviewAt(float Gravity, float Light, float Time, float Seconds)
{
	Dials.Gravity = Gravity;
	Dials.Light = Light;
	Dials.Time = Time;
	History = GrowUniverse(Dials, 1);
	Layout();
	State = EForgeState::Growing;
	Clock = Seconds;
	bFrozen = true;
	Pose();
	AimCamera(true);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTargetWithBlend(this, 0.0f);
	}
}

void AUniverseForge::Tick(float DeltaSeconds)
{
	using namespace UniverseForgeDetail;
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (State == EForgeState::Growing || State == EForgeState::Ended)
	{
		if (State == EForgeState::Growing && !bFrozen)
		{
			Clock += DeltaSeconds * (bHurry ? HurrySpeed : 1.0f);
			double Total = EndingSeconds;
			for (int32 S = 0; S <= static_cast<int32>(History.LastStage); ++S)
			{
				Total += StageSeconds.IsValidIndex(S) ? StageSeconds[S] : 10.0f;
			}
			if (Clock >= Total)
			{
				Clock = static_cast<float>(Total);
				State = EForgeState::Ended;
				bHurry = false;
			}
		}
		Pose();
		if (bPlaying && !bFrozen && !bInspecting)
		{
			CheckMilestones();
		}
	}
	else
	{
		PoseIdle();
	}

	// The camera eases after its goal.
	if (!bFrozen)
	{
		const FVector Now = ForgeCamera->GetRelativeLocation();
		ForgeCamera->SetRelativeLocationAndRotation(FMath::VInterpTo(Now, CameraGoal, DeltaSeconds, 1.5f),
			FMath::RInterpTo(ForgeCamera->GetRelativeRotation(), CameraGoalRot, DeltaSeconds, 2.0f));
	}

	// The dials glow by how far they are turned; the one he is setting breathes.
	for (int32 D = 0; D < DialCount && D < DialLight.Num(); ++D)
	{
		const double Value = D == 0 ? Dials.Gravity : (D == 1 ? Dials.Light : Dials.Time);
		const bool bSelected = State == EForgeState::Dials && D == DialIndex;
		const double Breathe = bSelected ? 0.6 + 0.4 * FMath::Sin(Age * 6.0) : 1.0;
		if (DialLight[D])
		{
			DialLight[D]->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>((2.0 + 24.0 * Value) * Breathe));
		}
	}

	if (bPlaying)
	{
		RefreshPanel();
	}
}

// --- playing ----------------------------------------------------------------------------------

void AUniverseForge::Interact_Implementation(AActor* Interactor)
{
	if (bPlaying)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (PC)
	{
		StartPlaying(PC);
	}
}

FText AUniverseForge::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Forge", "Prompt", "[E] grow a universe");
}

void AUniverseForge::StartPlaying(APlayerController* PC)
{
	// A fresh visit to the console: no half-made keep or inspection carries over from the last.
	bInspecting = false;
	bChoosingSlot = false;
	PendingReplaceSlot = INDEX_NONE;
	GalleryMessage.Empty();
	PlayingPC = PC;
	bPlaying = true;
	bFrozen = false;
	State = EForgeState::Dials;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetInputMode(FInputModeGameOnly());
	EnableInput(PC);
	BindKeys();
	ShowForgeBackdrop(this, true);
	AimCamera(false);
	PC->SetViewTargetWithBlend(this, 0.8f, VTBlend_Cubic);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Forge] stepped up to the console (%s)."), *GetActorNameOrLabel());
}

void AUniverseForge::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	ShowForgeBackdrop(this, false);
	bPlaying = false;
	bHurry = false;
	State = EForgeState::Idle;
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Forge] stepped back from the console."));
}

void AUniverseForge::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;
	for (const FKey& Key : { EKeys::W, EKeys::Up })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnUp);
	}
	for (const FKey& Key : { EKeys::S, EKeys::Down })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnDown);
	}
	for (const FKey& Key : { EKeys::A, EKeys::Left })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnLeft);
		InputComponent->BindKey(Key, IE_Repeat, this, &AUniverseForge::OnLeft);
	}
	for (const FKey& Key : { EKeys::D, EKeys::Right })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnRight);
		InputComponent->BindKey(Key, IE_Repeat, this, &AUniverseForge::OnRight);
	}
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::Enter, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnSpacePressed);
		InputComponent->BindKey(Key, IE_Released, this, &AUniverseForge::OnSpaceReleased);
	}
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AUniverseForge::OnAgain);
	// N a new seed; F skip a stage already watched; K keep it; 1-3 the gallery slot to keep it in.
	// F, NOT Tab, which this first used: Slate takes Tab for focus and Play-in-Editor eats it
	// before a raw BindKey sees it - the game menu learned that the hard way ("i press tab and
	// nothing happens", SibeliusGameCharacter.cpp).
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AUniverseForge::OnNewUniverse);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AUniverseForge::OnSkip);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AUniverseForge::OnKeep);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AUniverseForge::OnSlotOne);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AUniverseForge::OnSlotTwo);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AUniverseForge::OnSlotThree);
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &AUniverseForge::OnLeave);
	}
}

void AUniverseForge::Turn(int32 Index, float By)
{
	double* Dial = Index == 0 ? &Dials.Gravity : (Index == 1 ? &Dials.Light : &Dials.Time);
	*Dial = FMath::Clamp(FMath::RoundToDouble((*Dial + By) / DialStep) * DialStep, 0.0, 1.0);
}

void AUniverseForge::OnUp()
{
	if (State == EForgeState::Dials)
	{
		DialIndex = (DialIndex + DialCount - 1) % DialCount;
	}
}

void AUniverseForge::OnDown()
{
	if (State == EForgeState::Dials)
	{
		DialIndex = (DialIndex + 1) % DialCount;
	}
}

void AUniverseForge::OnLeft()
{
	if (State == EForgeState::Dials)
	{
		Turn(DialIndex, -DialStep);
	}
}

void AUniverseForge::OnRight()
{
	if (State == EForgeState::Dials)
	{
		Turn(DialIndex, DialStep);
	}
}

void AUniverseForge::OnSpacePressed()
{
	// Space means nothing while he chooses a slot or looks at a kept universe.
	if (bInspecting || bChoosingSlot)
	{
		return;
	}
	switch (State)
	{
	case EForgeState::Dials:
		BeginGrowing();
		break;
	case EForgeState::Growing:
		bHurry = true;
		break;
	case EForgeState::Ended:
		OnNewUniverse();
		break;
	default:
		break;
	}
}

void AUniverseForge::OnSpaceReleased()
{
	bHurry = false;
}

void AUniverseForge::OnAgain()
{
	if (State == EForgeState::Ended && !bInspecting && !bChoosingSlot)
	{
		bRetrySeed = true;   // R keeps the seed; N, or Space, draws a new one
		// The same dials, ready to turn: a near miss is one press away from another try.
		State = EForgeState::Dials;
	}
}

void AUniverseForge::OnLeave()
{
	// Esc while choosing a slot cancels the keep, not the visit: the universe stays on screen.
	if (bChoosingSlot)
	{
		bChoosingSlot = false;
		PendingReplaceSlot = INDEX_NONE;
		GalleryMessage = TEXT("Gallery unchanged.");
		return;
	}
	StopPlaying();
}

void AUniverseForge::OnNewUniverse()
{
	// N (or Space at the end): back to the dials with a fresh seed coming. Not while he is
	// choosing a slot or looking at a kept universe - those have their own way out.
	if ((State == EForgeState::Ended || State == EForgeState::Dials) && !bInspecting && !bChoosingSlot)
	{
		bRetrySeed = false;
		State = EForgeState::Dials;
		GalleryMessage.Empty();
	}
}

float AUniverseForge::StageEnd(int32 Stage) const
{
	// The clock time at which the given stage finishes playing, at normal speed.
	float End = 0.0f;
	for (int32 S = 0; S <= Stage; ++S)
	{
		End += StageSeconds.IsValidIndex(S) ? FMath::Max(StageSeconds[S], 0.5f) : 10.0f;
	}
	return End;
}

void AUniverseForge::CheckMilestones()
{
	// Remember every stage he has now watched to its end - F may skip those next time.
	if (UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this))
	{
		int32 Seen = 0;
		for (int32 S = 0; S <= static_cast<int32>(History.LastStage); ++S)
		{
			if (Clock >= StageEnd(S))
			{
				Seen |= 1 << S;
			}
		}
		Progress->RememberUniverseStages(Seen);
	}

	/* FIRST CONTACT. A universe that grew machines signals, and the city answers. It waits until
	   the machines are on the screen - two seconds into their stage - never the moment
	   GrowUniverse decided the ending, which is before the bang has even played. Once per run. */
	if (!bContactSent && History.Ending == EUniverseEnding::LookedBack && Clock >= StageEnd(5) + 2.0f)
	{
		bContactSent = true;
		for (TActorIterator<AAICity> It(GetWorld()); It; ++It)
		{
			It->AnswerUniverse();
		}
	}
}

void AUniverseForge::OnSkip()
{
	// F jumps to the end of the stage playing now - but only a stage he has watched before (as of
	// this run's start), so nothing new is ever skipped unseen. Milestones still fire on the way.
	if (State != EForgeState::Growing || bInspecting)
	{
		return;
	}
	for (int32 S = 0; S <= static_cast<int32>(History.LastStage); ++S)
	{
		if (Clock < StageEnd(S))
		{
			if ((SeenBeforeRun & (1 << S)) != 0)
			{
				Clock = StageEnd(S);
				Pose();
				CheckMilestones();
			}
			return;
		}
	}
}

void AUniverseForge::OnKeep()
{
	// K at the end opens the slot chooser - unless this very run is already in the gallery.
	if (State != EForgeState::Ended || bInspecting)
	{
		return;
	}
	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	if (!Progress)
	{
		GalleryMessage = TEXT("Gallery unavailable. Your universe is still here.");
		return;
	}
	for (int32 Slot = 0; Slot < UProgressionSubsystem::UniverseSlots; ++Slot)
	{
		const FUniverseRecord* Kept = Progress->UniverseInSlot(Slot);
		if (Kept && Kept->Id == RunId)
		{
			GalleryMessage = TEXT("This universe is already in your gallery.");
			return;
		}
	}
	bChoosingSlot = true;
	PendingReplaceSlot = INDEX_NONE;
}

void AUniverseForge::OnSlotOne() { SelectSlot(0); }
void AUniverseForge::OnSlotTwo() { SelectSlot(1); }
void AUniverseForge::OnSlotThree() { SelectSlot(2); }

void AUniverseForge::SelectSlot(int32 Slot)
{
	if (!bChoosingSlot || State != EForgeState::Ended || bInspecting)
	{
		return;
	}
	UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	if (!Progress)
	{
		return;
	}
	// An occupied slot takes a second press of the same number: nothing he kept is ever lost to
	// one stray key.
	if (Progress->UniverseInSlot(Slot) && PendingReplaceSlot != Slot)
	{
		PendingReplaceSlot = Slot;
		return;
	}
	// KeepUniverse saves at once, and puts the gallery back as it was if the save fails.
	if (!Progress->KeepUniverse(Slot, FUniverseRecord::Capture(History, RunId)))
	{
		GalleryMessage = TEXT("Could not save. Your existing gallery is unchanged. Try again.");
		return;
	}
	bChoosingSlot = false;
	PendingReplaceSlot = INDEX_NONE;
	GalleryMessage = FString::Printf(TEXT("%s is now in your gallery. Step back to see it."), UTF8_TO_TCHAR(History.Name.c_str()));
	for (TActorIterator<AUniverseExhibit> It(GetWorld()); It; ++It)
	{
		It->Refresh();
	}
}

void AUniverseForge::InspectSaved(APlayerController* PC, const FUniverseRecord& Record)
{
	/* E at an exhibit: the forge shows a kept universe as it ended - the record restored, not the
	   simulation run again - with only a way back. bInspecting keeps K, R, N and the slot keys
	   from turning a look into a new run or a replacement. */
	if (!PC || !Record.IsValid() || bPlaying)
	{
		return;
	}
	StartPlaying(PC);
	bInspecting = true;
	History = Record.Restore();
	Layout();
	Clock = StageEnd(static_cast<int32>(History.LastStage)) + EndingSeconds;
	State = EForgeState::Ended;
	Pose();
	GalleryMessage = FString::Printf(TEXT("YOUR GALLERY - gravity %.2f / light %.2f / time %.2f"), Record.Gravity, Record.Light, Record.Time);
}

ASibeliusHUD* AUniverseForge::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

FString AUniverseForge::DialLine(int32 Index) const
{
	using namespace UniverseForgeDetail;
	const double Value = Index == 0 ? Dials.Gravity : (Index == 1 ? Dials.Light : Dials.Time);
	const int32 Filled = FMath::RoundToInt(Value * 20.0);
	FString Bar;
	for (int32 I = 0; I < 20; ++I)
	{
		Bar += I < Filled ? TEXT("#") : TEXT("-");
	}
	return FString::Printf(TEXT("%s %s  [%s]  %s"), Index == DialIndex ? TEXT(">") : TEXT(" "), DialNames[Index], *Bar, DialMeaning[Index]);
}

void AUniverseForge::RefreshPanel()
{
	ASibeliusHUD* Hud = GetHud();
	if (!Hud)
	{
		return;
	}
	FString Status;
	FString Controls;
	const FString Name = UTF8_TO_TCHAR(History.Name.c_str());
	switch (State)
	{
	case EForgeState::Dials:
		Status = FString(TEXT("GROW A UNIVERSE\n\n")) + DialLine(0) + TEXT("\n") + DialLine(1) + TEXT("\n") + DialLine(2);
		Status += bRetrySeed ? TEXT("\n\nSAME SEED - change a dial to compare the result. [N] for a new seed.") : TEXT("\n\nNEW UNIVERSE - a fresh seed when you begin.");
		Controls = TEXT("[W] [S] choose a dial     [A] [D] turn it     [Space] begin     [Esc] step back");
		break;
	case EForgeState::Growing:
	{
		// The chronicle so far: a line for every stage it has reached.
		int32 Stage = 0;
		double At = 0.0;
		for (int32 S = 0; S <= static_cast<int32>(History.LastStage); ++S)
		{
			At += StageSeconds.IsValidIndex(S) ? StageSeconds[S] : 10.0f;
			Stage = S;
			if (Clock < At)
			{
				break;
			}
		}
		const double Total = At;
		Status = FString::Printf(TEXT("%s\n%s  -  %.1f billion years\n"), *Name.ToUpper(),
			UTF8_TO_TCHAR(UniverseStageName(static_cast<EUniverseStage>(Stage))),
			History.AgeBillions * FMath::Clamp(Clock / FMath::Max(Total, 1.0), 0.0, 1.0));
		for (int32 S = 0; S <= Stage && S < static_cast<int32>(History.Chronicle.size()); ++S)
		{
			Status += TEXT("\n") + FString(UTF8_TO_TCHAR(History.Chronicle[S].c_str()));
		}
		Controls = TEXT("hold [Space] to hurry     [F] skip a stage you have seen     [Esc] step back");
		break;
	}
	case EForgeState::Ended:
	{
		Status = FString::Printf(TEXT("%s\n%s, %.1f billion years\n"), UTF8_TO_TCHAR(History.EndingTitle.c_str()), *Name, History.AgeBillions);
		for (const std::string& Line : History.Chronicle)
		{
			Status += TEXT("\n") + FString(UTF8_TO_TCHAR(Line.c_str()));
		}
		// What to turn next, from the edge it fell over (UniverseSim's thresholds) - a fact, not a
		// guess. Not when looking at a kept one: that is finished.
		if (!bInspecting)
		{
			const TCHAR* Advice = TEXT("Your universe grew machines. Try a different gravity or light to change its character.");
			switch (History.Ending)
			{
			case EUniverseEnding::ColdFog: Advice = TEXT("Try more GRAVITY so matter can gather."); break;
			case EUniverseEnding::Crunch: Advice = TEXT("Try less GRAVITY so the stars do not collapse together."); break;
			case EUniverseEnding::Burn: Advice = TEXT("Try less LIGHT so stars can last long enough for life."); break;
			case EUniverseEnding::Dim: Advice = TEXT("Try more LIGHT to warm the world."); break;
			case EUniverseEnding::TooYoung: Advice = TEXT("Try more TIME so life can develop further."); break;
			default: break;
			}
			Status += FString(TEXT("\n\n")) + Advice;
		}
		Status += TEXT("\n\n") + GalleryMessage;
		Controls = bInspecting ? TEXT("[Esc] or [E] return to your gallery") : TEXT("[K] keep     [R] retry this seed     [N] or [Space] new universe     [Esc] step back");
		if (bChoosingSlot)
		{
			Status = TEXT("KEEP IN YOUR GALLERY\n");
			const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
			for (int32 Slot = 0; Slot < UProgressionSubsystem::UniverseSlots; ++Slot)
			{
				const FUniverseRecord* Kept = Progress ? Progress->UniverseInSlot(Slot) : nullptr;
				Status += FString::Printf(TEXT("\n[%d] %s"), Slot + 1, Kept ? *Kept->Name : TEXT("Empty"));
			}
			if (PendingReplaceSlot != INDEX_NONE)
			{
				Status += FString::Printf(TEXT("\n\nReplace slot %d? Press [%d] again to confirm."), PendingReplaceSlot + 1, PendingReplaceSlot + 1);
			}
			Status += TEXT("\n\n") + GalleryMessage;
			Controls = TEXT("[1] [2] [3] choose a slot     [Esc] cancel without replacing");
		}
		break;
	}
	default:
		return;
	}
	// While the city answers, the panel says so: the light alone is easy to miss from the console.
	if (!bInspecting && bContactSent && !bChoosingSlot)
	{
		for (TActorIterator<AAICity> It(GetWorld()); It; ++It)
		{
			if (It->IsAnswering())
			{
				Status += TEXT("\n\nA signal leaves your universe. The city answers, shell by shell.\nThey have noticed you.");
				break;
			}
		}
	}
	// The screen is the forge's while he is at it: a lease renewed every frame, as the chess
	// and pool tables do - without it the HUD never draws the panel at all.
	Hud->HoldCinematic(0.25f);
	Hud->ShowTablePanel(Status, Controls);
}
