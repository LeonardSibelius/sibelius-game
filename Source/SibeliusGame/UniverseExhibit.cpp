// UniverseExhibit.cpp - see the header.

#include "UniverseExhibit.h"

#include "ProgressionSubsystem.h"
#include "UniverseForge.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace UniverseExhibitDetail
{
	constexpr int32 StarCount = 96;
	constexpr float StarRoom = 43.0f;     // how far from the orb's centre a star may sit (the sphere is 50)
	constexpr float GoldenAngle = 2.399963f;
}

AUniverseExhibit::AUniverseExhibit()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassFinder(TEXT("/Game/Genesis/M_GenesisGlass.M_GenesisGlass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowFinder(TEXT("/Game/Genesis/M_GenesisGlow.M_GenesisGlow"));
	// The plinth borrows the Go board's surface, as the city's hulls and the forge's console do.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	GlassMaterial = GlassFinder.Object;
	GlowMaterial = GlowFinder.Object;
	SurfaceMaterial = SurfaceFinder.Object;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The glass the universe sits in, at eye height. No collision: the plinth is what stops him.
	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orb"));
	Orb->SetupAttachment(RootComponent);
	Orb->SetStaticMesh(SphereMesh.Object);
	Orb->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));
	Orb->SetRelativeScale3D(FVector(1.8f));
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetCastShadow(false);

	// The plinth blocks walking but not the Visibility trace - Reach is what takes the E.
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	Pedestal->SetupAttachment(RootComponent);
	Pedestal->SetStaticMesh(CylinderMesh.Object);
	Pedestal->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	Pedestal->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.7f));
	Pedestal->SetCollisionProfileName(TEXT("BlockAll"));
	Pedestal->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// A thin ring round the plinth's top, lit in the universe's colour.
	Rim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rim"));
	Rim->SetupAttachment(RootComponent);
	Rim->SetStaticMesh(CylinderMesh.Object);
	Rim->SetRelativeLocation(FVector(0.0f, 0.0f, 71.0f));
	Rim->SetRelativeScale3D(FVector(1.45f, 1.45f, 0.025f));
	Rim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rim->SetCastShadow(false);

	// The universe: one tiny sphere per star, attached to the orb so they scale with it.
	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(Orb);
	Stars->SetStaticMesh(SphereMesh.Object);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);

	// What the interactor's trace finds: one box round plinth and orb, answering Visibility only.
	Reach = CreateDefaultSubobject<UBoxComponent>(TEXT("Reach"));
	Reach->SetupAttachment(RootComponent);
	Reach->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	Reach->SetBoxExtent(FVector(100.0f, 100.0f, 120.0f));
	Reach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Reach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Reach->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// The label on the plinth's face, turned back toward the gallery's arrival side (-X).
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(-80.0f, 0.0f, 75.0f));
	Label->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Label->SetWorldSize(18.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::White);
}

void AUniverseExhibit::SetSlot(int32 InSlot)
{
	Slot = InSlot;

	/* A DARK PLATE BEHIND THE LABEL (Walt, 2026-10-18: "the "Your Universe" signs need a
	   background so i can read them easier" - white letters over the bright floor and the sun).
	   Made here, once, at run time - a component owned by the actor like any other - and sized
	   to its text in Refresh, so a long name gets a wide plate. The label is centred on it. */
	bool bHasPlate = false;
	for (UActorComponent* Part : GetComponents())
	{
		bHasPlate |= Part && Part->GetFName() == TEXT("LabelPlate");
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!bHasPlate && Cube)
	{
		UStaticMeshComponent* Plate = NewObject<UStaticMeshComponent>(this, TEXT("LabelPlate"));
		Plate->SetupAttachment(RootComponent);
		Plate->SetStaticMesh(Cube);
		Plate->SetRelativeLocation(FVector(-77.0f, 0.0f, 75.0f));   // just behind the letters at -80
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plate->SetCastShadow(false);
		if (GlowMaterial)
		{
			UMaterialInstanceDynamic* Dark = UMaterialInstanceDynamic::Create(GlowMaterial, this);
			Dark->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.004f, 0.006f, 0.012f));
			Dark->SetScalarParameterValue(TEXT("Intensity"), 1.0f);
			Plate->SetMaterial(0, Dark);
		}
		Plate->RegisterComponent();
	}
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Refresh();
}

void AUniverseExhibit::Refresh()
{
	using namespace UniverseExhibitDetail;

	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	const FUniverseRecord* Record = Progress ? Progress->UniverseInSlot(Slot) : nullptr;

	Label->SetText(FText::FromString(FString::Printf(TEXT("YOUR UNIVERSE %d\n%s"), Slot + 1,
		Record ? *Record->Name : TEXT("Empty"))));
	// The plate (SetSlot) fits the text, with a margin; the text lies in the label's Y-Z plane.
	for (UActorComponent* Part : GetComponents())
	{
		if (UStaticMeshComponent* Plate = Part && Part->GetFName() == TEXT("LabelPlate") ? Cast<UStaticMeshComponent>(Part) : nullptr)
		{
			const FVector Text = Label->GetTextLocalSize();
			Plate->SetRelativeScale3D(FVector(0.02f, (Text.Y + 36.0f) / 100.0f, (Text.Z + 18.0f) / 100.0f));
		}
	}

	// Its colour is its star's, from red at a cool star to blue at a hot one; an empty slot is a
	// dim teal, so the three read as places waiting to be filled.
	const FLinearColor Hue = Record
		? FMath::Lerp(FLinearColor(1.0f, 0.25f, 0.1f), FLinearColor(0.3f, 0.7f, 1.0f), static_cast<float>(Record->StarHeat))
		: FLinearColor(0.2f, 0.4f, 0.5f);

	if (GlassMaterial)
	{
		UMaterialInstanceDynamic* Glass = UMaterialInstanceDynamic::Create(GlassMaterial, this);
		Glass->SetVectorParameterValue(TEXT("Color"), Hue);
		Glass->SetScalarParameterValue(TEXT("Intensity"), 0.8f);
		Glass->SetScalarParameterValue(TEXT("RimOpacity"), 0.4f);
		Glass->SetScalarParameterValue(TEXT("BaseOpacity"), 0.02f);
		Orb->SetMaterial(0, Glass);
	}
	if (GlowMaterial)
	{
		UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		Glow->SetVectorParameterValue(TEXT("Color"), Hue);
		Glow->SetScalarParameterValue(TEXT("Intensity"), Record ? 4.0f : 0.3f);
		Stars->SetMaterial(0, Glow);
		Rim->SetMaterial(0, Glow);
	}
	if (SurfaceMaterial)
	{
		// Dark and plain: the plinth should vanish under the orb, not compete with it.
		UMaterialInstanceDynamic* Plinth = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
		Plinth->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.04f, 0.05f));
		Plinth->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
		Plinth->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
		Pedestal->SetMaterial(0, Plinth);
	}

	Stars->ClearInstances();
	if (!Record)
	{
		return;
	}

	// The stars take the shape of how it ended, from its own seed so it always looks the same.
	// They start as a flat golden-angle spiral - a galaxy - which each ending then bends.
	FRandomStream Random(static_cast<int32>(Record->Seed ^ (Record->Seed >> 32)));
	for (int32 I = 0; I < StarCount; ++I)
	{
		const float Angle = I * GoldenAngle;
		const float Radius = 40.0f * FMath::Sqrt((I + 1.0f) / StarCount);
		FVector At(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Random.FRandRange(-3.0f, 3.0f));
		switch (static_cast<EUniverseEnding>(Record->Ending))
		{
		case EUniverseEnding::ColdFog:      // never gathered: a thin fog all through the glass
			At = Random.VRand() * Random.FRandRange(10.0f, StarRoom);
			break;
		case EUniverseEnding::Crunch:       // fell back in on itself: a tight knot
			At *= 0.2f;
			break;
		case EUniverseEnding::Burn:         // burned out: embers scattered to the edge
			At = Random.VRand() * 42.0f;
			break;
		case EUniverseEnding::Dim:          // too cold: one frozen ring
			At = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 35.0f;
			break;
		case EUniverseEnding::LookedBack:   // grew machines: the galaxy inside a shell of them
			if (I % 3 == 0)
			{
				At = Random.VRand() * StarRoom;
			}
			break;
		default:                            // too young: the galaxy alone
			break;
		}
		Stars->AddInstance(FTransform(FQuat::Identity, At, FVector(0.012f)));
	}
}

FText AUniverseExhibit::GetInteractionPrompt_Implementation() const
{
	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	const FUniverseRecord* Record = Progress ? Progress->UniverseInSlot(Slot) : nullptr;
	return FText::FromString(Record
		? FString::Printf(TEXT("[E] inspect %s"), *Record->Name)
		: FString(TEXT("Keep a universe at the forge to display it here")));
}

void AUniverseExhibit::Interact_Implementation(AActor* Interactor)
{
	const UProgressionSubsystem* Progress = UProgressionSubsystem::Get(this);
	const FUniverseRecord* Record = Progress ? Progress->UniverseInSlot(Slot) : nullptr;
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	// One forge at a time: if someone is already at it, E here does nothing.
	if (!Record || !PC || AUniverseForge::IsAnyInUse(this))
	{
		return;
	}
	// The forge plays it back - it owns the camera, the panel and the way out.
	for (TActorIterator<AUniverseForge> It(GetWorld()); It; ++It)
	{
		It->InspectSaved(PC, *Record);
		break;
	}
}
