// SauceBowl.cpp — the temple's sauce fountain. See header.

#include "SauceBowl.h"
#include "ProgressionSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASauceBowl::ASauceBowl()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Engine shapes + cook-guaranteed materials (all hard CDO references).
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Marble(
		TEXT("/Game/StainedGlass3D/Materials/M_BlackMarbleFloor.M_BlackMarbleFloor"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// The table: a waist-high marble slab. The interact trace hits it.
	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(SceneRoot);
	TableMesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.9f));
	TableMesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (Cube.Succeeded()) { TableMesh->SetStaticMesh(Cube.Object); }
	if (Marble.Succeeded()) { TableMesh->SetMaterial(0, Marble.Object); }

	// The bowl: a squashed marble sphere sitting on the slab.
	BowlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowlMesh"));
	BowlMesh->SetupAttachment(SceneRoot);
	BowlMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.28f));
	BowlMesh->SetRelativeLocation(FVector(0.f, 0.f, 98.f));
	BowlMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (Sphere.Succeeded()) { BowlMesh->SetStaticMesh(Sphere.Object); }
	if (Marble.Succeeded()) { BowlMesh->SetMaterial(0, Marble.Object); }

	// The sauce: a glowing green disk revealed when the bowl is filled.
	SauceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SauceMesh"));
	SauceMesh->SetupAttachment(SceneRoot);
	SauceMesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.03f));
	SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	SauceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SauceMesh->SetVisibility(false);
	if (Cylinder.Succeeded()) { SauceMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { SauceMesh->SetMaterial(0, Basic.Object); }
}

void ASauceBowl::BeginPlay()
{
	Super::BeginPlay();

	// Tint the sauce disk sauce-green (MID from the hard-ref'd base).
	if (SauceMesh)
	{
		if (UMaterialInstanceDynamic* MID = SauceMesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 1.0f, 0.35f));
		}
	}

	TryEnableInput();
}

void ASauceBowl::TryEnableInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		if (++InputAttempts < 40)
		{
			GetWorldTimerManager().SetTimer(InputRetryHandle, this, &ASauceBowl::TryEnableInput, 0.25f, false);
		}
		return;
	}
	EnableInput(PC);
	if (InputComponent)
	{
		// The Compile key claims the sauce — one verb, disambiguated by
		// distance (the guard in OnCompilePressed). CarouselMachine's pattern.
		InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ASauceBowl::OnCompilePressed);
	}
}

bool ASauceBowl::IsLadleReady() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastClaimTime >= RechargeSeconds);
}

void ASauceBowl::Interact_Implementation(AActor* Interactor)
{
	if (bFilled || !IsLadleReady())
	{
		return;
	}
	bFilled = true;
	if (SauceMesh)
	{
		SauceMesh->SetVisibility(true);
	}
}

void ASauceBowl::OnCompilePressed()
{
	if (!bFilled)
	{
		return;
	}
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > ClaimRadius)
	{
		return;   // a Compile elsewhere in the temple is not for this bowl
	}

	bFilled = false;
	LastClaimTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (SauceMesh)
	{
		SauceMesh->SetVisibility(false);
	}

	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->GrantSauce(SaucePerBowl);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Emerald,
				FString::Printf(TEXT("SAUCE COMPILED  +%d  (total %d)"),
					SaucePerBowl, Progression->GetSauce()));
		}
	}
}

FText ASauceBowl::GetInteractionPrompt_Implementation() const
{
	if (bFilled)
	{
		return FText::FromString(TEXT("Compile the sauce [C]"));
	}
	if (!IsLadleReady())
	{
		const int32 Wait = FMath::CeilToInt(
			RechargeSeconds - (GetWorld()->GetTimeSeconds() - LastClaimTime));
		return FText::FromString(FString::Printf(TEXT("The ladle drips... (%ds)"), Wait));
	}
	return FText::FromString(TEXT("Fill the bowl [E]"));
}
