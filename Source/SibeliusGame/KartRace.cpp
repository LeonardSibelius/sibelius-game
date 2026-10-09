// KartRace.cpp — see the header for what this actor is for and what it must never do.

#include "KartRace.h"

#include "BranchSubsystem.h"
#include "CathedralDoor.h"
#include "CodeVisionComponent.h"
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"
#include "TravelTransitionSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace KartRaceDetail
{
	const TCHAR* const Names[6] = { TEXT("Kaia"), TEXT("Nyra"), TEXT("Isla"), TEXT("Aisling"), TEXT("Elise"), TEXT("You") };

	// Each agent's own pace, on top of the level he picks: close, so any of them can win.
	const double Skill[6] = { 1.0, 0.99, 0.97, 0.96, 0.98, 1.0 };

	constexpr int32 BoxCount = 12;
	constexpr int32 ObstacleCount = 10;
	constexpr int32 DotCount = 16;

	FString Ordinal(int32 N)
	{
		const int32 Tens = N % 100;
		const TCHAR* Suffix = (Tens >= 11 && Tens <= 13) ? TEXT("th")
			: (N % 10 == 1 ? TEXT("st") : (N % 10 == 2 ? TEXT("nd") : (N % 10 == 3 ? TEXT("rd") : TEXT("th"))));
		return FString::Printf(TEXT("%d%s"), N, Suffix);
	}

	FString ClockText(double Seconds)
	{
		const int32 Tenths = FMath::Max(0, FMath::FloorToInt32(Seconds * 10.0));
		return FString::Printf(TEXT("%d:%02d.%d"), Tenths / 600, (Tenths / 10) % 60, Tenths % 10);
	}

	const TCHAR* ItemTitle(EKartItem Item)
	{
		switch (Item)
		{
		case EKartItem::Sauce:    return TEXT("the Sauce of All Knowledge");
		case EKartItem::Slap:     return TEXT("a Slap");
		case EKartItem::Generate: return TEXT("Generate");
		case EKartItem::Refactor: return TEXT("Refactor");
		default:                  return TEXT("nothing");
		}
	}

	const TCHAR* ItemShort(EKartItem Item)
	{
		switch (Item)
		{
		case EKartItem::Sauce:    return TEXT("Sauce");
		case EKartItem::Slap:     return TEXT("Slap");
		case EKartItem::Generate: return TEXT("Generate");
		case EKartItem::Refactor: return TEXT("Refactor");
		default:                  return TEXT("none");
		}
	}

	const TCHAR* ItemHow(EKartItem Item)
	{
		switch (Item)
		{
		case EKartItem::Sauce:    return TEXT("gives you a burst of speed");
		case EKartItem::Slap:     return TEXT("spins out the kart beside or ahead of you");
		case EKartItem::Generate: return TEXT("drops an obstacle behind you");
		case EKartItem::Refactor: return TEXT("turns the kart just ahead into a treasure chest");
		default:                  return TEXT("");
		}
	}

	const TCHAR* PaceName(EKartAgentPace Pace)
	{
		switch (Pace)
		{
		case EKartAgentPace::Gentle: return TEXT("gentle");
		case EKartAgentPace::Fierce: return TEXT("fierce");
		default:                     return TEXT("racing");
		}
	}

	UStaticMesh* MeshFor(KartShapes::EShape Shape, UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere)
	{
		switch (Shape)
		{
		case KartShapes::EShape::Cylinder: return Cylinder;
		case KartShapes::EShape::Sphere:   return Sphere;
		default:                           return Cube;
		}
	}
}

// =====================================================================================
// THE PLAIN-SHAPES KART

const TArray<KartShapes::FPart>& KartShapes::Parts()
{
	using P = EPaint;
	using S = EShape;
	static const TArray<FPart> Table = {
		{ TEXT("Body"),      S::Cube,     FVector(0, 0, 26),     FRotator::ZeroRotator,   FVector(1.70, 1.05, 0.28), P::Body,   true },
		{ TEXT("Nose"),      S::Cube,     FVector(102, 0, 22),   FRotator::ZeroRotator,   FVector(0.55, 0.90, 0.18), P::Body,   true },
		{ TEXT("Wing"),      S::Cube,     FVector(-90, 0, 64),   FRotator::ZeroRotator,   FVector(0.14, 1.25, 0.07), P::Body,   true },
		{ TEXT("WingPostL"), S::Cube,     FVector(-90, -42, 48), FRotator::ZeroRotator,   FVector(0.06, 0.06, 0.30), P::Trim,   true },
		{ TEXT("WingPostR"), S::Cube,     FVector(-90, 42, 48),  FRotator::ZeroRotator,   FVector(0.06, 0.06, 0.30), P::Trim,   true },
		{ TEXT("Bumper"),    S::Cube,     FVector(132, 0, 14),   FRotator::ZeroRotator,   FVector(0.12, 1.15, 0.12), P::Trim,   true },
		{ TEXT("SeatBack"),  S::Cube,     FVector(-40, 0, 56),   FRotator::ZeroRotator,   FVector(0.12, 0.62, 0.50), P::Trim,   true },
		{ TEXT("WheelFL"),   S::Cylinder, FVector(60, -62, 19),  FRotator(0, 0, 90),      FVector(0.38, 0.38, 0.24), P::Trim,   false },
		{ TEXT("WheelFR"),   S::Cylinder, FVector(60, 62, 19),   FRotator(0, 0, 90),      FVector(0.38, 0.38, 0.24), P::Trim,   false },
		{ TEXT("WheelRL"),   S::Cylinder, FVector(-58, -64, 21), FRotator(0, 0, 90),      FVector(0.42, 0.42, 0.28), P::Trim,   false },
		{ TEXT("WheelRR"),   S::Cylinder, FVector(-58, 64, 21),  FRotator(0, 0, 90),      FVector(0.42, 0.42, 0.28), P::Trim,   false },
		{ TEXT("Column"),    S::Cube,     FVector(38, 0, 52),    FRotator(-35, 0, 0),     FVector(0.05, 0.05, 0.40), P::Trim,   false },
		{ TEXT("Wheel"),     S::Cylinder, FVector(28, 0, 68),    FRotator(-55, 0, 0),     FVector(0.32, 0.32, 0.04), P::Trim,   false },
		{ TEXT("Torso"),     S::Cylinder, FVector(-14, 0, 72),   FRotator::ZeroRotator,   FVector(0.42, 0.42, 0.50), P::Driver, false },
		{ TEXT("Head"),      S::Sphere,   FVector(-10, 0, 116),  FRotator::ZeroRotator,   FVector(0.34, 0.34, 0.34), P::Driver, false },
		{ TEXT("Flame"),     S::Cube,     FVector(-118, 0, 30),  FRotator::ZeroRotator,   FVector(0.40, 0.36, 0.18), P::Flame,  false },
		{ TEXT("SparkL"),    S::Cube,     FVector(-78, -74, 6),  FRotator::ZeroRotator,   FVector(0.16, 0.16, 0.08), P::Spark,  false },
		{ TEXT("SparkR"),    S::Cube,     FVector(-78, 74, 6),   FRotator::ZeroRotator,   FVector(0.16, 0.16, 0.08), P::Spark,  false },
		{ TEXT("Chest"),     S::Cube,     FVector(0, 0, 48),     FRotator::ZeroRotator,   FVector(1.40, 1.05, 0.85), P::Chest,  false },
	};
	return Table;
}

/* THE RACE KART MODEL, measured without being looked at (its seller forbids AI use, so its
   renders are not ours to view - Walt judges its looks). Exported to OBJ, 2026-10-09: the
   wheels' outer faces stand at x = +-96 cm with their axles at y = -80 and +80, so the origin
   is on the ground midway between them; the steering wheel's ring sits at y ~ 45, 96 cm up,
   and the motor and battery at the -Y end - so its nose is +Y. Its seat pan is 30 cm up with
   the back at y ~ -60. Its wheels are 55 cm across, twice a real kart's: hence the scale. The
   colour is a lerp of DiffuseColor and DiffuseColorMap by DiffuseColorMapWeight (Interchange's
   FBXLegacyPhongSurfaceMaterial), so weight 0 paints the body any colour. */
int32 KartShapes::AccentFor(int32 Kart)
{
	switch (Kart)
	{
	case 0:  return 3;   // Kaia, orange: yellow
	case 4:  return 2;   // Elise, green
	case 5:  return 0;   // Leonard, red
	default: return 1;   // Nyra, Isla, Aisling: blue
	}
}

void KartShapes::FindAccents(TArray<TObjectPtr<UTexture2D>>& Out)
{
	static const TCHAR* const Names[8] = {
		TEXT("Wheel_red_D"), TEXT("Wheel_Blue_D"), TEXT("Wheel_green_D"), TEXT("Wheel_yellow_D"),
		TEXT("Steering_Wheel_D"), TEXT("Steering_Wheel_blue_D"), TEXT("Steering_Wheel_green_D"), TEXT("Steering_Wheel_yellow_D"),
	};
	Out.Reset();
	for (const TCHAR* Name : Names)
	{
		// The finder keeps the path's pointer and looks it up in Get(): the string must outlive it.
		const FString Path = FString::Printf(TEXT("/Game/Fab/Race_kart/%s.%s"), Name, Name);
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> Finder(*Path);
		Out.Add(Finder.Get());
	}
}

void KartShapes::PaintModel(UStaticMeshComponent* Model, int32 Kart, const TArray<TObjectPtr<UTexture2D>>& Accents,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& Keep)
{
	if (!Model || !Model->GetStaticMesh())
	{
		return;
	}
	auto Mid = [Model, &Keep](const TCHAR* Slot) -> UMaterialInstanceDynamic*
	{
		const int32 Index = Model->GetMaterialIndex(FName(Slot));
		UMaterialInstanceDynamic* M = Index != INDEX_NONE ? Model->CreateDynamicMaterialInstance(Index) : nullptr;
		if (M)
		{
			Keep.Add(M);
		}
		return M;
	};
	if (UMaterialInstanceDynamic* Body = Mid(TEXT("BodyColor")))
	{
		Body->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 0.0f);
		Body->SetVectorParameterValue(TEXT("DiffuseColor"), DriverColor(Kart));
	}
	const int32 Accent = AccentFor(Kart);
	if (Accents.IsValidIndex(Accent) && Accents[Accent])
	{
		if (UMaterialInstanceDynamic* Wheel = Mid(TEXT("Wheel")))
		{
			Wheel->SetTextureParameterValue(TEXT("DiffuseColorMap"), Accents[Accent]);
		}
	}
	if (Accents.IsValidIndex(4 + Accent) && Accents[4 + Accent])
	{
		if (UMaterialInstanceDynamic* Steer = Mid(TEXT("Steering_Wheel")))
		{
			Steer->SetTextureParameterValue(TEXT("DiffuseColorMap"), Accents[4 + Accent]);
		}
	}
}

FLinearColor KartShapes::DriverColor(int32 Kart)
{
	switch (Kart)
	{
	case 0:  return FLinearColor(1.00f, 0.42f, 0.08f);   // Kaia
	case 1:  return FLinearColor(0.50f, 0.18f, 0.85f);   // Nyra
	case 2:  return FLinearColor(0.08f, 0.70f, 0.95f);   // Isla
	case 3:  return FLinearColor(0.85f, 0.90f, 1.00f);   // Aisling
	case 4:  return FLinearColor(0.20f, 0.78f, 0.28f);   // Elise
	default: return FLinearColor(0.90f, 0.10f, 0.08f);   // Leonard
	}
}

// =====================================================================================
// THE RACE ACTOR

AKartRace::AKartRace()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	ConeMesh = Cone.Object;
	ShapeMaterial = Material.Object;
	// Hard references, so the kart and its colour variants reach the pak. Missing in a clone
	// without the pack: then the race runs on the plain shapes.
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Model(KartShapes::ModelPath);
	KartModel = Model.Get();
	KartShapes::FindAccents(Accents);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TrackSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrackSpline"));
	TrackSpline->SetupAttachment(SceneRoot);
	TrackSpline->SetClosedLoop(true);
	TrackSpline->ClearSplinePoints(false);
	// A placeholder square, 60 m a side, until the script lays the real one on the streets.
	const FVector Square[4] = { FVector(0, 0, 0), FVector(6000, 0, 0), FVector(6000, 6000, 0), FVector(0, 6000, 0) };
	for (int32 I = 0; I < 4; ++I)
	{
		TrackSpline->AddSplinePoint(Square[I], ESplineCoordinateSpace::Local, false);
		TrackSpline->SetSplinePointType(I, ESplinePointType::Linear, false);
	}
	TrackSpline->UpdateSpline();

	Seat = CreateDefaultSubobject<UBoxComponent>(TEXT("Seat"));
	Seat->SetupAttachment(SceneRoot);
	Seat->SetBoxExtent(FVector(160.0, 110.0, 90.0));
	Seat->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Seat->SetCollisionResponseToAllChannels(ECR_Ignore);
	Seat->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Seat->SetGenerateOverlapEvents(false);

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(SceneRoot);
	ChaseCamera->SetUsingAbsoluteLocation(true);
	ChaseCamera->SetUsingAbsoluteRotation(true);

	auto MakeIsm = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Ism = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Ism->SetupAttachment(SceneRoot);
		Ism->SetUsingAbsoluteLocation(true);
		Ism->SetUsingAbsoluteRotation(true);
		Ism->SetUsingAbsoluteScale(true);
		Ism->SetStaticMesh(Mesh);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetGenerateOverlapEvents(false);
		return Ism;
	};
	KerbRed = MakeIsm(TEXT("KerbRed"), CubeMesh);
	KerbWhite = MakeIsm(TEXT("KerbWhite"), CubeMesh);
	LineDark = MakeIsm(TEXT("LineDark"), CubeMesh);
	LineLight = MakeIsm(TEXT("LineLight"), CubeMesh);
	LineDark->SetCastShadow(false);
	LineLight->SetCastShadow(false);

	auto MakeProp = [this](const FString& Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Prop = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Prop->SetupAttachment(SceneRoot);
		Prop->SetUsingAbsoluteLocation(true);
		Prop->SetUsingAbsoluteRotation(true);
		Prop->SetMobility(EComponentMobility::Movable);
		Prop->SetStaticMesh(Mesh);
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prop->SetGenerateOverlapEvents(false);
		Prop->SetVisibility(false);
		return Prop;
	};
	for (int32 I = 0; I < KartRaceDetail::BoxCount; ++I)
	{
		BoxMeshes.Add(MakeProp(FString::Printf(TEXT("Box%02d"), I), CubeMesh));
	}
	for (int32 I = 0; I < KartRaceDetail::ObstacleCount; ++I)
	{
		ObstacleMeshes.Add(MakeProp(FString::Printf(TEXT("Obstacle%02d"), I), ConeMesh));
	}
	for (int32 I = 0; I < KartRaceDetail::DotCount; ++I)
	{
		UStaticMeshComponent* Dot = MakeProp(FString::Printf(TEXT("LineDot%02d"), I), CylinderMesh);
		Dot->SetCastShadow(false);
		LineDots.Add(Dot);
	}
}

UMaterialInstanceDynamic* AKartRace::Paint(const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Mid = ShapeMaterial ? UMaterialInstanceDynamic::Create(ShapeMaterial, this) : nullptr;
	if (Mid)
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		Paints.Add(Mid);
	}
	return Mid;
}

double AKartRace::PaceValue() const
{
	switch (AgentPace)
	{
	case EKartAgentPace::Gentle: return 0.84;
	case EKartAgentPace::Fierce: return 0.99;
	default:                     return 0.92;
	}
}

bool AKartRace::BuildModelTrack()
{
	std::vector<FKartTrackPoint> Corners;
	const int32 Count = TrackSpline ? TrackSpline->GetNumberOfSplinePoints() : 0;
	for (int32 I = 0; I < Count; ++I)
	{
		const FVector P = TrackSpline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World);
		FKartTrackPoint C;
		C.X = P.X;
		C.Y = P.Y;
		C.Z = P.Z;
		C.HalfWidth = CornerHalfWidths.IsValidIndex(I) ? CornerHalfWidths[I] : 0.0;
		Corners.push_back(C);
	}
	std::string Error;
	bTrackOk = Track.Build(Corners, HalfWidth, CornerRadius, 150.0, &Error);
	TrackError = bTrackOk ? FString() : FString(UTF8_TO_TCHAR(Error.c_str()));
	if (!bTrackOk)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Kart] the track will not build: %s"), *TrackError);
	}
	return bTrackOk;
}

float AKartRace::GroundAt(const FVector2D& XY, float Guess, FVector* Normal) const
{
	const UWorld* World = GetWorld();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KartGround), false, this);
	const FVector From(XY.X, XY.Y, Guess + 250.0f);
	const FVector To(XY.X, XY.Y, Guess - 700.0f);
	if (World && World->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		if (Normal)
		{
			*Normal = Hit.ImpactNormal;
		}
		return Hit.ImpactPoint.Z;
	}
	if (Normal)
	{
		*Normal = FVector::UpVector;
	}
	return Guess;
}

FVector AKartRace::WorldAt(double S, double D, float Lift) const
{
	const FKartVec P = Track.PointAt(S) + Track.TangentAt(S).Left() * D;
	const float Z = GroundAt(FVector2D(P.X, P.Y), static_cast<float>(Track.HeightAt(S)));
	return FVector(P.X, P.Y, Z + Lift);
}

void AKartRace::RebuildTrack()
{
	if (!BuildModelTrack())
	{
		KerbRed->ClearInstances();
		KerbWhite->ClearInstances();
		LineDark->ClearInstances();
		LineLight->ClearInstances();
		return;
	}
	PlaceKerbs();

	// His kart's place on the grid is where the interaction trace should find the seat.
	FKartRace Grid;
	FKartRaceSetup Setup;
	Setup.Karts = KartCount;
	Setup.Player = PlayerKart;
	if (Grid.Start(Track, Setup))
	{
		const FKartState& K = Grid.Karts()[PlayerKart];
		const float Z = GroundAt(FVector2D(K.P.X, K.P.Y), static_cast<float>(Track.HeightAt(K.S)));
		Seat->SetWorldLocationAndRotation(FVector(K.P.X, K.P.Y, Z + 60.0f), FRotator(0.0f, FMath::RadiansToDegrees(K.Heading), 0.0f));
	}
}

void AKartRace::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildTrack();
}

/* Kerbs every 2.2 m along both edges, red and white in turn, standing just outside the
   corridor - so a kart that touches the wall is seen to touch the kerb. And a chequered
   start line across the road, two rows of 50 cm squares. */
void AKartRace::PlaceKerbs()
{
	KerbRed->ClearInstances();
	KerbWhite->ClearInstances();
	LineDark->ClearInstances();
	LineLight->ClearInstances();
	if (!bTrackOk)
	{
		return;
	}
	const double L = Track.Length();
	const double Step = 220.0;
	int32 Index = 0;
	for (double S = 0.0; S < L - Step * 0.5; S += Step, ++Index)
	{
		const FKartVec T = Track.TangentAt(S);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
		const double Edge = Track.HalfWidthAt(S) + 20.0;
		const float Street = static_cast<float>(Track.HeightAt(S));
		for (const double Side : { -1.0, 1.0 })
		{
			// At the road's edge a trace can land on a lamp post's foot or a planter: a kerb
			// is never more than a step above the street.
			FVector At = WorldAt(S, Side * Edge, 12.0f);
			if (At.Z > Street + 60.0f)
			{
				At.Z = Street + 12.0f;
			}
			const FTransform Xf(FRotator(0.0f, Yaw, 0.0f), At, FVector(0.60, 0.22, 0.24));
			((Index % 2 == 0) ? KerbRed : KerbWhite)->AddInstance(Xf, true);
		}
	}

	const double Square = 50.0;
	const double LineHalf = Track.HalfWidthAt(0.0);
	const int32 Columns = FMath::Max(2, FMath::FloorToInt32(2.0 * LineHalf / Square));
	const FKartVec T0 = Track.TangentAt(0.0);
	const float Yaw0 = FMath::RadiansToDegrees(FMath::Atan2(T0.Y, T0.X));
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Col = 0; Col < Columns; ++Col)
		{
			const double D = -LineHalf + Square * (Col + 0.5);
			const FVector At = WorldAt(Square * (Row - 0.5), D, 1.5f);
			const FTransform Xf(FRotator(0.0f, Yaw0, 0.0f), At, FVector(0.5, 0.5, 0.02));
			(((Row + Col) % 2 == 0) ? LineDark : LineLight)->AddInstance(Xf, true);
		}
	}
}

void AKartRace::BeginPlay()
{
	Super::BeginPlay();

	KerbRed->SetMaterial(0, Paint(FLinearColor(0.75f, 0.04f, 0.03f)));
	KerbWhite->SetMaterial(0, Paint(FLinearColor(0.85f, 0.85f, 0.85f)));
	LineDark->SetMaterial(0, Paint(FLinearColor(0.02f, 0.02f, 0.02f)));
	LineLight->SetMaterial(0, Paint(FLinearColor(0.90f, 0.90f, 0.90f)));
	UMaterialInstanceDynamic* BoxPaint = Paint(FLinearColor(1.0f, 0.78f, 0.10f));
	for (UStaticMeshComponent* Box : BoxMeshes)
	{
		Box->SetMaterial(0, BoxPaint);
	}
	UMaterialInstanceDynamic* ConePaint = Paint(FLinearColor(1.0f, 0.35f, 0.02f));
	for (UStaticMeshComponent* Cone : ObstacleMeshes)
	{
		Cone->SetMaterial(0, ConePaint);
	}
	DotGreen = Paint(FLinearColor(0.15f, 0.95f, 0.25f));
	DotAmber = Paint(FLinearColor(1.0f, 0.70f, 0.05f));
	DotRed = Paint(FLinearColor(1.0f, 0.08f, 0.05f));

	RebuildTrack();
	if (bTrackOk)
	{
		BuildKarts();
		ResetRace(false);
	}
}

void AKartRace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* AKartRace::MakePart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location,
	const FRotator& Rotation, const FVector& Scale, UMaterialInterface* Material)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	return Part;
}

void AKartRace::BuildKarts()
{
	using namespace KartShapes;
	UMaterialInstanceDynamic* Trim = Paint(FLinearColor(0.04f, 0.04f, 0.05f));
	UMaterialInstanceDynamic* Flame = Paint(FLinearColor(1.0f, 0.45f, 0.05f));
	UMaterialInstanceDynamic* Gold = Paint(FLinearColor(0.95f, 0.65f, 0.08f));

	Karts.SetNum(KartCount);
	for (int32 K = 0; K < KartCount; ++K)
	{
		FKartVisual& V = Karts[K];
		V.Root = NewObject<USceneComponent>(this);
		V.Root->SetMobility(EComponentMobility::Movable);
		V.Root->SetupAttachment(SceneRoot);
		V.Root->SetUsingAbsoluteLocation(true);
		V.Root->SetUsingAbsoluteRotation(true);
		V.Root->RegisterComponent();
		AddInstanceComponent(V.Root);

		const FLinearColor Color = DriverColor(K);
		UMaterialInstanceDynamic* Body = Paint(Color);
		UMaterialInstanceDynamic* Driver = Paint(FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.35f));
		V.SparkMaterial = Paint(FLinearColor::White);

		// The race kart model, when the pack is there: it replaces the plain body and wheels,
		// and the plain driver, flame and sparks move to where its seat and tail are.
		const bool bModel = KartModel != nullptr;
		if (bModel)
		{
			UStaticMeshComponent* Kart = MakePart(V.Root, KartModel, FVector::ZeroVector, ModelRotation, FVector(KartModelScale), nullptr);
			PaintModel(Kart, K, Accents, Paints);
			V.Shell.Add(Kart);
		}
		for (const FPart& Part : Parts())
		{
			if (bModel && (Part.Paint == EPaint::Body || Part.Paint == EPaint::Trim))
			{
				continue;
			}
			FVector Location = Part.Location;
			FVector Scale = Part.Scale;
			if (bModel)
			{
				// Seat pan 26 cm up between the seat back (x -51) and the steering wheel (x 38);
				// the tail at x -105; the rear wheels' outer faces at x -68, y +-82.
				const FString Name = Part.Name;
				if (Name == TEXT("Torso"))       { Location = FVector(-25, 0, 52); }
				else if (Name == TEXT("Head"))   { Location = FVector(-22, 0, 96); }
				else if (Name == TEXT("Flame"))  { Location = FVector(-112, 0, 32); }
				else if (Name == TEXT("SparkL")) { Location = FVector(-68, -84, 6); }
				else if (Name == TEXT("SparkR")) { Location = FVector(-68, 84, 6); }
				else if (Name == TEXT("Chest"))  { Scale = FVector(2.0, 1.4, 0.85); }
			}
			UMaterialInterface* Material = nullptr;
			switch (Part.Paint)
			{
			case EPaint::Body:   Material = Body; break;
			case EPaint::Trim:   Material = Trim; break;
			case EPaint::Driver: Material = Driver; break;
			case EPaint::Flame:  Material = Flame; break;
			case EPaint::Spark:  Material = V.SparkMaterial; break;
			case EPaint::Chest:  Material = Gold; break;
			}
			UStaticMeshComponent* Mesh = MakePart(V.Root, KartRaceDetail::MeshFor(Part.Shape, CubeMesh, CylinderMesh, SphereMesh),
				Location, Part.Rotation, Scale, Material);
			if (Part.bShell)
			{
				V.Shell.Add(Mesh);
			}
			if (Part.Paint == EPaint::Chest)
			{
				V.Chest = Mesh;
				Mesh->SetVisibility(false);
			}
			else if (Part.Paint == EPaint::Flame)
			{
				V.Flame = Mesh;
				Mesh->SetVisibility(false);
				Mesh->SetCastShadow(false);
			}
			else if (Part.Paint == EPaint::Spark)
			{
				V.Sparks.Add(Mesh);
				Mesh->SetVisibility(false);
				Mesh->SetCastShadow(false);
			}
		}

		if (K != PlayerKart)
		{
			UTextRenderComponent* Name = NewObject<UTextRenderComponent>(this);
			Name->SetupAttachment(V.Root);
			Name->SetRelativeLocation(FVector(0.0, 0.0, 175.0));
			Name->SetUsingAbsoluteRotation(true);
			Name->SetText(FText::FromString(KartRaceDetail::Names[K]));
			Name->SetWorldSize(40.0f);
			Name->SetHorizontalAlignment(EHTA_Center);
			Name->SetVerticalAlignment(EVRTA_TextCenter);
			Name->SetTextRenderColor(Color.ToFColor(true));
			Name->RegisterComponent();
			AddInstanceComponent(Name);
			V.Name = Name;
		}
	}
}

void AKartRace::ResetRace(bool bRunning)
{
	FKartRaceSetup Setup;
	Setup.Karts = KartCount;
	Setup.Laps = Laps;
	Setup.Player = PlayerKart;
	Setup.Seed = static_cast<uint32_t>(FMath::Rand()) | 1u;
	Setup.AiPace = PaceValue();
	Setup.CountdownSeconds = 3.0;
	for (int32 K = 0; K < KartCount; ++K)
	{
		Setup.Skill.push_back(KartRaceDetail::Skill[K]);
	}
	Race = FKartRace();
	Race.Start(Track, Setup);
	if (!bRunning)
	{
		Race.TakeEvents();
	}
	bRaceRunning = bRunning;
	FinishedPlace = 0;
	MessageLeft = 0.0f;
	GoShown = 0.0f;
	for (FKartVisual& V : Karts)
	{
		V.bHaveGround = false;
	}
	UpdateKarts(0.0f);
	UpdateTrackProps(0.0f);
}

// =====================================================================================
// EVERY FRAME

void AKartRace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !bTrackOk || Karts.Num() != KartCount)
	{
		if (bPlaying)
		{
			RefreshPanel();
		}
		return;
	}

	if (bRaceRunning)
	{
		if (bPlaying)
		{
			Race.SetInput(PlayerKart, ReadInput(DeltaSeconds));
		}
		Race.Advance(DeltaSeconds);
		HandleEvents();
	}
	UpdateKarts(DeltaSeconds);
	UpdateTrackProps(DeltaSeconds);

	MessageLeft = FMath::Max(0.0f, MessageLeft - DeltaSeconds);
	GoShown = FMath::Max(0.0f, GoShown - DeltaSeconds);

	// On foot: how to race, above the HUD's "[O] Back to Office", before he gets in.
	if (!bPlaying)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
		{
			Hud->ShowAwayHint(FString(TEXT("THE TRANSHUMAN GRAND PRIX - walk to the red kart at the back of the grid and press [E]\n"))
				+ TEXT("[W] [S] or the arrows: go and brake      [A] [D]: steer      hold [Space] round a corner to drift, let go for a boost\n")
				+ TEXT("[F]: use your item - drive through a gold box to get one      hold [V]: see the racing line      [ESC]: get out"));
		}
	}

	if (bPlaying)
	{
		SyncCamera(DeltaSeconds, false);
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
		RefreshPanel();
		if (Engine)
		{
			const FKartState& Me = Race.Karts()[PlayerKart];
			const float Pace = static_cast<float>(FMath::Clamp(Me.Speed() / Race.Params.MaxSpeed, 0.0, 1.4));
			Engine->SetPitchMultiplier(0.55f + 1.25f * Pace + (Me.BoostLeft > 0.0 ? 0.15f : 0.0f));
			Engine->SetVolumeMultiplier(0.18f + 0.22f * FMath::Min(Pace, 1.0f));
		}
	}
}

void AKartRace::UpdateKarts(float DeltaSeconds)
{
	const std::vector<FKartState>& States = Race.Karts();
	if (States.size() != static_cast<size_t>(Karts.Num()))
	{
		return;
	}
	// The names turn to whoever is looking: his own eyes on foot, the chase camera in a kart.
	// Not the chase camera always - out of a kart it waits at the start line, ahead of the grid,
	// and from behind it every name read backwards (Walt, 2026-10-09).
	FVector Eye = ChaseCamera ? ChaseCamera->GetComponentLocation() : FVector::ZeroVector;
	if (const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (Viewer->PlayerCameraManager)
		{
			Eye = Viewer->PlayerCameraManager->GetCameraLocation();
		}
	}
	for (int32 I = 0; I < Karts.Num(); ++I)
	{
		const FKartState& K = States[static_cast<size_t>(I)];
		FKartVisual& V = Karts[I];
		if (!V.Root)
		{
			continue;
		}
		const float Guess = V.bHaveGround ? V.GroundZ : static_cast<float>(Track.HeightAt(K.S));
		FVector Normal;
		const float Z = GroundAt(FVector2D(K.P.X, K.P.Y), Guess, &Normal);
		if (V.bHaveGround && DeltaSeconds > 0.0f)
		{
			V.GroundZ = FMath::FInterpTo(V.GroundZ, Z, DeltaSeconds, 20.0f);
			V.GroundNormal = FMath::VInterpNormalRotationTo(V.GroundNormal, Normal, DeltaSeconds, 180.0f);
		}
		else
		{
			V.GroundZ = Z;
			V.GroundNormal = Normal;
			V.bHaveGround = true;
		}
		const FVector Forward(FMath::Cos(K.Heading), FMath::Sin(K.Heading), 0.0);
		const FRotator Rotation = FRotationMatrix::MakeFromZX(V.GroundNormal, Forward).Rotator();
		V.Root->SetWorldLocationAndRotation(FVector(K.P.X, K.P.Y, V.GroundZ), Rotation);

		const bool bChest = K.ChestLeft > 0.0;
		for (UStaticMeshComponent* Shell : V.Shell)
		{
			Shell->SetVisibility(!bChest);
		}
		if (V.Chest)
		{
			V.Chest->SetVisibility(bChest);
		}
		if (V.Flame)
		{
			const bool bFlame = K.BoostLeft > 0.0 && !bChest;
			V.Flame->SetVisibility(bFlame);
			if (bFlame)
			{
				const float Flicker = 0.8f + 0.4f * FMath::Abs(FMath::Sin(PropClock * 37.0f + I));
				V.Flame->SetRelativeScale3D(FVector(0.40 * Flicker, 0.36, 0.18 * Flicker));
			}
		}
		// Drift sparks: white while it charges, blue once a small boost is earned, orange for the big one.
		const bool bSparks = K.bDrifting && K.DriftCharge > 0.25;
		for (UStaticMeshComponent* Spark : V.Sparks)
		{
			Spark->SetVisibility(bSparks && FMath::Sin(PropClock * 50.0f + Spark->GetRelativeLocation().Y) > -0.3f);
		}
		if (bSparks && V.SparkMaterial)
		{
			const FLinearColor Color = K.DriftCharge >= Race.Params.SuperBoostCharge ? FLinearColor(1.0f, 0.45f, 0.05f)
				: (K.DriftCharge >= Race.Params.MiniBoostCharge ? FLinearColor(0.1f, 0.5f, 1.0f) : FLinearColor(0.9f, 0.9f, 0.9f));
			V.SparkMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		}
		if (V.Name)
		{
			const FVector At = V.Name->GetComponentLocation();
			V.Name->SetWorldRotation(FRotator(0.0f, (Eye - At).Rotation().Yaw, 0.0f));
		}
	}
}

void AKartRace::UpdateTrackProps(float DeltaSeconds)
{
	PropClock += DeltaSeconds;
	const std::vector<FKartBox>& Boxes = Race.Boxes();
	for (int32 I = 0; I < BoxMeshes.Num(); ++I)
	{
		UStaticMeshComponent* Box = BoxMeshes[I];
		const bool bThere = I < static_cast<int32>(Boxes.size()) && Boxes[static_cast<size_t>(I)].RespawnLeft <= 0.0;
		Box->SetVisibility(bThere);
		if (bThere)
		{
			const FKartBox& B = Boxes[static_cast<size_t>(I)];
			const float Bob = 75.0f + 8.0f * FMath::Sin(PropClock * 3.0f + I);
			Box->SetWorldLocationAndRotation(WorldAt(B.S, B.D, Bob), FRotator(35.0f, PropClock * 120.0f + I * 30.0f, 35.0f));
			Box->SetWorldScale3D(FVector(0.6));
		}
	}

	const std::vector<FKartObstacle>& Obstacles = Race.Obstacles();
	for (int32 I = 0; I < ObstacleMeshes.Num(); ++I)
	{
		UStaticMeshComponent* Cone = ObstacleMeshes[I];
		const bool bThere = I < static_cast<int32>(Obstacles.size());
		Cone->SetVisibility(bThere);
		if (bThere)
		{
			const FKartObstacle& O = Obstacles[static_cast<size_t>(I)];
			const float Z = GroundAt(FVector2D(O.P.X, O.P.Y), Karts.Num() > 0 ? Karts[0].GroundZ : 0.0f);
			Cone->SetWorldLocationAndRotation(FVector(O.P.X, O.P.Y, Z + 45.0f), FRotator::ZeroRotator);
			Cone->SetWorldScale3D(FVector(0.7, 0.7, 0.9));
		}
	}

	// CODE VISION: the line the agents drive, from his kart forward, green where full speed
	// is fine, amber where he should lift, red where he must brake.
	const bool bShowLine = bPlaying && bRaceRunning && IsCodeVisionOn() && Race.Phase() == EKartPhase::Racing;
	const FKartState* Me = bShowLine ? &Race.Karts()[PlayerKart] : nullptr;
	for (int32 I = 0; I < LineDots.Num(); ++I)
	{
		UStaticMeshComponent* Dot = LineDots[I];
		Dot->SetVisibility(Me != nullptr);
		if (!Me)
		{
			continue;
		}
		const double S = Me->S + 400.0 + 350.0 * I;
		const double Bend = Track.CurvatureAt(S);
		const double D = Bend != 0.0 ? FMath::Sign(Bend) * 0.45 * Track.HalfWidthAt(S) : 0.0;
		const double Target = Race.TargetSpeedAt(S, 1.0);
		const double Speed = Me->Speed();
		Dot->SetMaterial(0, Target >= Speed - 60.0 ? DotGreen.Get() : (Target >= Speed - 500.0 ? DotAmber.Get() : DotRed.Get()));
		Dot->SetWorldLocationAndRotation(WorldAt(S, D, 4.0f), FRotator::ZeroRotator);
		Dot->SetWorldScale3D(FVector(0.38, 0.38, 0.03));
	}
}

// =====================================================================================
// WHAT HAPPENED

void AKartRace::Say(const FString& Text, float Seconds)
{
	Message = Text;
	MessageLeft = Seconds;
}

FString AKartRace::DriverName(int32 Kart) const
{
	return (Kart >= 0 && Kart < KartCount) ? FString(KartRaceDetail::Names[Kart]) : FString(TEXT("somebody"));
}

void AKartRace::HandleEvents()
{
	using namespace KartRaceDetail;
	for (const FKartEventRecord& E : Race.TakeEvents())
	{
		const bool bMe = E.Kart == PlayerKart;
		const float Pitch = FMath::FRandRange(0.94f, 1.06f);
		switch (E.Type)
		{
		case EKartEvent::Countdown:
			TableSounds::Play(this, ETableSound::KartBeep, 0.7f);
			break;
		case EKartEvent::Go:
			TableSounds::Play(this, ETableSound::KartGo, 0.8f);
			GoShown = 1.2f;
			break;
		case EKartEvent::WallHit:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartBump, 0.25f + 0.6f * static_cast<float>(E.Value), Pitch);
			}
			break;
		case EKartEvent::KartHit:
			if (bMe || E.Other == PlayerKart)
			{
				TableSounds::Play(this, ETableSound::KartBump, 0.3f + 0.6f * static_cast<float>(E.Value), Pitch * 1.15f);
			}
			break;
		case EKartEvent::Pickup:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartPickup, 0.7f);
				Say(FString::Printf(TEXT("You got %s - it %s.  [F] to use it"), ItemTitle(E.Item), ItemHow(E.Item)), 3.5f);
			}
			break;
		case EKartEvent::UsedItem:
			if (bMe)
			{
				switch (E.Item)
				{
				case EKartItem::Sauce:    Say(TEXT("The Sauce of All Knowledge: boost!")); break;
				case EKartItem::Slap:     Say(E.Other >= 0 ? FString::Printf(TEXT("You slapped %s!"), *DriverName(E.Other)) : FString(TEXT("Your slap hit nothing but air."))); break;
				case EKartItem::Generate: Say(TEXT("You generated an obstacle behind you.")); break;
				case EKartItem::Refactor: if (E.Other < 0) { Say(TEXT("Refactor found nobody to refactor.")); } break;
				default: break;
				}
			}
			break;
		case EKartEvent::Boosted:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartBoost, 0.65f);
				if (E.Value < 1.0)
				{
					Say(TEXT("Drift boost!"), 1.2f);
				}
				else if (E.Value < 1.4)
				{
					Say(TEXT("Super drift boost!"), 1.4f);
				}
			}
			break;
		case EKartEvent::SpunOut:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartSlap, 0.8f, Pitch);
				if (E.Value < 1.5)
				{
					Say(FString::Printf(TEXT("%s slapped you!"), *DriverName(E.Other)));
				}
				else
				{
					Say(E.Other == PlayerKart ? FString(TEXT("You hit your own obstacle!"))
						: (E.Other >= 0 ? FString::Printf(TEXT("You hit %s's obstacle!"), *DriverName(E.Other)) : FString(TEXT("You hit an obstacle!"))));
				}
			}
			else if (E.Other == PlayerKart)
			{
				TableSounds::Play(this, ETableSound::KartSlap, 0.6f, Pitch);
				if (E.Value >= 1.5)
				{
					Say(FString::Printf(TEXT("%s hit your obstacle!"), *DriverName(E.Kart)));
				}
			}
			break;
		case EKartEvent::Refactored:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartChest, 0.8f);
				Say(FString::Printf(TEXT("%s refactored you into a treasure chest!"), *DriverName(E.Other)));
			}
			else if (E.Other == PlayerKart)
			{
				TableSounds::Play(this, ETableSound::KartChest, 0.7f);
				Say(FString::Printf(TEXT("You refactored %s into a treasure chest!"), *DriverName(E.Kart)));
			}
			break;
		case EKartEvent::Lap:
			if (bMe)
			{
				TableSounds::Play(this, ETableSound::KartLap, 0.7f);
				const int32 Lap = static_cast<int32>(E.Value);
				Say(Lap >= Laps ? FString(TEXT("Final lap!")) : FString::Printf(TEXT("Lap %d of %d"), Lap, Laps), 2.0f);
			}
			break;
		case EKartEvent::Finished:
			if (bMe)
			{
				FinishedPlace = static_cast<int32>(E.Value);
				TableSounds::Play(this, ETableSound::KartFinish, 0.8f);
				Say(FString::Printf(TEXT("You finished %s!"), *Ordinal(FinishedPlace)), 6.0f);
				UE_LOG(LogSibeliusGame, Display, TEXT("[Kart] he finished %s in %s (agents: %s)"), *Ordinal(FinishedPlace),
					*ClockText(Race.Karts()[PlayerKart].FinishTime), PaceName(AgentPace));
			}
			break;
		case EKartEvent::RaceOver:
			UE_LOG(LogSibeliusGame, Display, TEXT("[Kart] race over"));
			break;
		default:
			break;
		}
	}
}

// =====================================================================================
// SITTING IN AND GETTING OUT

void AKartRace::Interact_Implementation(AActor* Interactor)
{
	if (bPlaying)
	{
		return;
	}
	if (!bTrackOk || Karts.Num() != KartCount)
	{
		ASibeliusHUD::Toast(this, FString::Printf(TEXT("The race track is not ready: %s"), *TrackError), 4.0f, SibeliusToast::Bad);
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

FText AKartRace::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Kart", "Prompt", "[E] get in your kart and race the agents");
}

void AKartRace::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bThrottleKey = bBrakeKey = bLeftKey = bRightKey = bDriftKey = bItemPressed = false;
	SteerSmoothed = 0.0f;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetShowMouseCursor(false);
	PC->SetInputMode(FInputModeGameOnly());
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->SetActorHiddenInGame(true);   // he is in the kart now, not standing beside it
	}

	ResetRace(true);
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	EnableInput(PC);
	BindRaceKeys();

	if (!Engine)
	{
		Engine = UGameplayStatics::CreateSound2D(this, TableSounds::MakeKartEngine(), 0.2f, 0.6f, 0.0f, nullptr, false, false);
	}
	if (Engine)
	{
		Engine->Play();
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Kart] he gets in (agents: %s, %d laps, track %.0f m)."),
		KartRaceDetail::PaceName(AgentPace), Laps, Track.Length() / 100.0);
}

void AKartRace::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	if (Engine)
	{
		Engine->Stop();
	}
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(false);
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	if (bTrackOk && Karts.Num() == KartCount)
	{
		ResetRace(false);   // everyone back on the grid, waiting for him
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Kart] he gets out."));
}

void AKartRace::BindRaceKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (AKartRace::*)();
	auto Bind = [this](std::initializer_list<FKey> Keys, FHandler Down, FHandler Up)
	{
		for (const FKey& Key : Keys)
		{
			InputComponent->BindKey(Key, IE_Pressed, this, Down);
			if (Up)
			{
				InputComponent->BindKey(Key, IE_Released, this, Up);
			}
		}
	};
	Bind({ EKeys::W, EKeys::Up }, &AKartRace::OnThrottleDown, &AKartRace::OnThrottleUp);
	Bind({ EKeys::S, EKeys::Down }, &AKartRace::OnBrakeDown, &AKartRace::OnBrakeUp);
	Bind({ EKeys::A, EKeys::Left }, &AKartRace::OnLeftDown, &AKartRace::OnLeftUp);
	Bind({ EKeys::D, EKeys::Right }, &AKartRace::OnRightDown, &AKartRace::OnRightUp);
	Bind({ EKeys::SpaceBar, EKeys::LeftShift, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightShoulder }, &AKartRace::OnDriftDown, &AKartRace::OnDriftUp);
	Bind({ EKeys::F, EKeys::LeftControl, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_LeftShoulder }, &AKartRace::OnItem, nullptr);
	Bind({ EKeys::R, EKeys::Gamepad_Special_Right }, &AKartRace::OnRestart, nullptr);
	Bind({ EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right }, &AKartRace::OnLeave, nullptr);
	Bind({ EKeys::One }, &AKartRace::OnPace1, nullptr);
	Bind({ EKeys::Two }, &AKartRace::OnPace2, nullptr);
	Bind({ EKeys::Three }, &AKartRace::OnPace3, nullptr);
}

void AKartRace::OnThrottleDown() { bThrottleKey = true; }
void AKartRace::OnThrottleUp()   { bThrottleKey = false; }
void AKartRace::OnBrakeDown()    { bBrakeKey = true; }
void AKartRace::OnBrakeUp()      { bBrakeKey = false; }
void AKartRace::OnLeftDown()     { bLeftKey = true; }
void AKartRace::OnLeftUp()       { bLeftKey = false; }
void AKartRace::OnRightDown()    { bRightKey = true; }
void AKartRace::OnRightUp()      { bRightKey = false; }
void AKartRace::OnDriftDown()    { bDriftKey = true; }
void AKartRace::OnDriftUp()      { bDriftKey = false; }
void AKartRace::OnItem()         { bItemPressed = true; }
void AKartRace::OnLeave()        { StopPlaying(); }

void AKartRace::OnRestart()
{
	if (bPlaying)
	{
		ResetRace(true);
		SyncCamera(0.0f, true);
	}
}

void AKartRace::OnPace1() { AgentPace = EKartAgentPace::Gentle; Race.SetAiPace(PaceValue()); Say(TEXT("The agents will drive gently.")); }
void AKartRace::OnPace2() { AgentPace = EKartAgentPace::Racing; Race.SetAiPace(PaceValue()); Say(TEXT("The agents will race.")); }
void AKartRace::OnPace3() { AgentPace = EKartAgentPace::Fierce; Race.SetAiPace(PaceValue()); Say(TEXT("The agents will drive fiercely.")); }

FKartInput AKartRace::ReadInput(float DeltaSeconds)
{
	FKartInput In;
	float PadSteer = 0.0f;
	float PadGas = 0.0f;
	float PadBrake = 0.0f;
	if (const APlayerController* PC = PlayingPC.Get())
	{
		PadSteer = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		PadGas = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
		PadBrake = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
	}
	// The keys ease the wheel over, so a tap is a nudge and a hold is full lock; letting go
	// or changing side brings it back faster. +1 is the model's left, the world's right.
	const float Wanted = (bRightKey ? 1.0f : 0.0f) - (bLeftKey ? 1.0f : 0.0f);
	const bool bReturning = Wanted == 0.0f || Wanted * SteerSmoothed < 0.0f;
	SteerSmoothed = FMath::FInterpConstantTo(SteerSmoothed, Wanted, DeltaSeconds, bReturning ? 9.0f : 5.0f);
	In.Steer = FMath::Abs(PadSteer) > 0.12f ? PadSteer : SteerSmoothed;
	In.Throttle = (bThrottleKey ? 1.0f : 0.0f) - (bBrakeKey ? 1.0f : 0.0f);
	if (PadGas > 0.05f || PadBrake > 0.05f)
	{
		In.Throttle = PadGas - PadBrake;
	}
	In.bDrift = bDriftKey;
	In.bUseItem = bItemPressed;
	bItemPressed = false;
	return In;
}

void AKartRace::SyncCamera(float DeltaSeconds, bool bSnap)
{
	if (!ChaseCamera || Karts.Num() != KartCount || Race.Karts().size() != static_cast<size_t>(KartCount))
	{
		return;
	}
	const FKartState& K = Race.Karts()[PlayerKart];
	const FKartVisual& V = Karts[PlayerKart];
	const float Heading = FMath::RadiansToDegrees(static_cast<float>(K.Heading));
	// Behind the kart, swinging round after it - slowly while it spins, so the world does not.
	const float Rate = K.SpinLeft > 0.0 ? 1.2f : 5.0f;
	CameraYaw = bSnap ? Heading : CameraYaw + FMath::FindDeltaAngleDegrees(CameraYaw, Heading) * FMath::Min(1.0f, DeltaSeconds * Rate);
	const FVector Dir = FRotator(0.0f, CameraYaw, 0.0f).Vector();
	const FVector Kart(K.P.X, K.P.Y, V.GroundZ);
	FVector Want = Kart - Dir * CameraBack + FVector(0.0, 0.0, CameraHeight);

	// A camera that would be inside a building comes in to the near side of the wall.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KartCamera), false, this);
	if (PlayingPC.IsValid() && PlayingPC->GetPawn())
	{
		Params.AddIgnoredActor(PlayingPC->GetPawn());
	}
	const FVector From = Kart + FVector(0.0, 0.0, 120.0);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, From, Want, ECC_Camera, Params))
	{
		Want = Hit.Location + (From - Want).GetSafeNormal() * 30.0f;
	}
	CameraEye = bSnap ? Want : FMath::VInterpTo(CameraEye, Want, DeltaSeconds, 9.0f);
	const FVector Look = Kart + Dir * 450.0f + FVector(0.0, 0.0, 70.0);
	ChaseCamera->SetWorldLocationAndRotation(CameraEye, (Look - CameraEye).Rotation());

	const float Speed = static_cast<float>(K.Speed() / Race.Params.MaxSpeed);
	const float Fov = CameraFov + 6.0f * FMath::Min(Speed, 1.2f) + (K.BoostLeft > 0.0 ? 8.0f : 0.0f);
	ChaseCamera->SetFieldOfView(bSnap ? Fov : FMath::FInterpTo(ChaseCamera->FieldOfView, Fov, DeltaSeconds, 4.0f));
}

bool AKartRace::IsCodeVisionOn() const
{
	const APlayerController* PC = PlayingPC.Get();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UCodeVisionComponent* Vision = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;
	return Vision && Vision->IsCodeVisionActive();
}

ASibeliusHUD* AKartRace::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void AKartRace::RefreshPanel()
{
	using namespace KartRaceDetail;
	ASibeliusHUD* Hud = GetHud();
	if (!Hud)
	{
		return;
	}
	FString Status;
	FString Note;
	if (!bTrackOk || Race.Karts().size() != static_cast<size_t>(KartCount))
	{
		Status = FString::Printf(TEXT("The race track is not ready: %s"), *TrackError);
	}
	else
	{
		const FKartState& Me = Race.Karts()[PlayerKart];
		const EKartPhase Phase = Race.Phase();
		if (Phase == EKartPhase::Countdown)
		{
			Status = FString::Printf(TEXT("THE TRANSHUMAN GRAND PRIX  -  %d laps\nGet ready...  %d"), Laps,
				FMath::Max(1, FMath::CeilToInt32(static_cast<float>(Race.CountdownLeft()))));
		}
		else if (Phase == EKartPhase::Over)
		{
			Status = FString::Printf(TEXT("Race over - you finished %s.\n[R] race again   [ESC] get out"),
				*Ordinal(Me.Place));
		}
		else if (Me.bFinished)
		{
			Status = FString::Printf(TEXT("You finished %s!   %s"), *Ordinal(Me.Place), *ClockText(Me.FinishTime));
		}
		else
		{
			const int32 Lap = FMath::Clamp(Me.Lap + 1, 1, Laps);
			Status = FString::Printf(TEXT("Lap %d of %d     %s of %d     %s     %d km/h"), Lap, Laps, *Ordinal(Me.Place),
				KartCount, *ClockText(Race.Clock()), FMath::RoundToInt32(static_cast<float>(Me.Speed() * 0.036)));
			if (GoShown > 0.0f)
			{
				Status += TEXT("\nGO!");
			}
			else if (Me.bWrongWay)
			{
				Status += TEXT("\nWRONG WAY - turn round!");
			}
			// What the item does, every time it is shown, not only when it is picked up (Walt,
			// 2026-10-09: "what does 'Item: the Sauce of All Knowledge' mean?").
			Status += Me.Item != EKartItem::None
				? FString::Printf(TEXT("\nItem: %s - it %s   [F] to use it"), ItemShort(Me.Item), ItemHow(Me.Item))
				: (Me.ChestLeft > 0.0 ? FString(TEXT("\nYou are a treasure chest!")) : FString(TEXT("\nNo item - drive through a gold box")));
		}

		const std::vector<int> Order = Race.Standings();
		for (size_t I = 0; I < Order.size(); ++I)
		{
			Note += FString::Printf(TEXT("%s%d %s"), I == 0 ? TEXT("") : TEXT("     "), static_cast<int32>(I) + 1, *DriverName(Order[I]));
		}
		if (MessageLeft > 0.0f && !Message.IsEmpty())
		{
			Note += TEXT("\n") + Message;
		}
	}
	const FString Controls = FString(TEXT("[W] [S] or the arrows: go and brake   [A] [D]: steer   hold [Space] round a corner to drift, let go for a boost\n"))
		+ TEXT("[F]: use your item   hold [V]: see the racing line   [R]: start again   [ESC]: get out   [O]: back to the office\n")
		+ FString::Printf(TEXT("[1] [2] [3] the agents' driving: %s"), PaceName(AgentPace));
	Hud->ShowTablePanel(Status, Controls, 0.25f, Note);
}

// =====================================================================================
// THE GO-KART IN THE LIVING ROOM

AKartLauncher::AKartLauncher()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = Material.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// The race kart model, red, empty: the driver's seat is his. The plain shapes only when
	// the pack is missing.
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> KartMesh(KartShapes::ModelPath);
	KartShapes::FindAccents(Accents);
	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetupAttachment(SceneRoot);
	Model->SetRelativeRotation(KartShapes::ModelRotation);
	Model->SetRelativeScale3D(FVector(0.85));
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Model->SetGenerateOverlapEvents(false);
	Model->SetStaticMesh(KartMesh.Get());
	const bool bModel = KartMesh.Get() != nullptr;

	for (const KartShapes::FPart& Part : KartShapes::Parts())
	{
		if (bModel)
		{
			break;
		}
		if (Part.Paint == KartShapes::EPaint::Driver || Part.Paint == KartShapes::EPaint::Flame
			|| Part.Paint == KartShapes::EPaint::Spark || Part.Paint == KartShapes::EPaint::Chest)
		{
			continue;
		}
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Part.Name);
		Mesh->SetupAttachment(SceneRoot);
		Mesh->SetStaticMesh(KartRaceDetail::MeshFor(Part.Shape, Cube.Object, Cylinder.Object, Sphere.Object));
		Mesh->SetRelativeLocationAndRotation(Part.Location, Part.Rotation);
		Mesh->SetRelativeScale3D(Part.Scale);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->ComponentTags.Add(Part.Paint == KartShapes::EPaint::Body ? FName(TEXT("KartBody")) : FName(TEXT("KartTrim")));
		PartMeshes.Add(Mesh);
	}

	Seat = CreateDefaultSubobject<UBoxComponent>(TEXT("Seat"));
	Seat->SetupAttachment(SceneRoot);
	Seat->SetRelativeLocation(FVector(0.0, 0.0, 45.0));
	Seat->SetBoxExtent(bModel ? FVector(122.0, 82.0, 50.0) : FVector(150.0, 75.0, 50.0));
	Seat->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Seat->SetCollisionResponseToAllChannels(ECR_Block);   // a real kart: he walks round it, not through it
	Seat->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Seat->SetGenerateOverlapEvents(false);
}

void AKartLauncher::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Paints.Reset();
	if (Model && Model->GetStaticMesh())
	{
		KartShapes::PaintModel(Model, 5, Accents, Paints);
		return;
	}
	if (!ShapeMaterial)
	{
		return;
	}
	UMaterialInstanceDynamic* Body = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
	UMaterialInstanceDynamic* Trim = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
	Body->SetVectorParameterValue(TEXT("Color"), KartShapes::DriverColor(5));
	Trim->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.04f, 0.05f));
	Paints.Add(Body);
	Paints.Add(Trim);
	for (UStaticMeshComponent* Mesh : PartMeshes)
	{
		if (Mesh)
		{
			Mesh->SetMaterial(0, Mesh->ComponentHasTag(TEXT("KartBody")) ? Body : Trim);
		}
	}
}

void AKartLauncher::Interact_Implementation(AActor* Interactor)
{
	(void)Interactor;
	const UWorld* World = GetWorld();
	UBranchSubsystem* Branch = World ? World->GetSubsystem<UBranchSubsystem>() : nullptr;
	if (!ACathedralDoor::IsTravelAllowed(Branch))
	{
		ASibeliusHUD::Toast(this, TEXT("Merge or discard your branches before leaving this world"), 2.5f, SibeliusToast::Bad);
		return;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Kart] the go-kart: travel to %s (arrival %s)"), *TravelTargetLevel.ToString(), *ArrivalTag.ToString());
	UTravelTransitionSubsystem::Travel(this, TravelTargetLevel, ArrivalTag);
}

FText AKartLauncher::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("Kart", "LauncherPrompt", "[E] race the agents through Transhuman City");
}
