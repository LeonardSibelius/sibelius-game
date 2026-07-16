// SauceBowl.cpp — the temple's sauce fountain. See header.

#include "SauceBowl.h"
#include "ProgressionSubsystem.h"
#include "HallAlarmSubsystem.h"   // filling the bowl rings the alarm
#include "Engine/GameInstance.h"

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

	// Real props from the magician's lab (hard CDO refs so they cook):
	// SM_Pot_Table is 106 cm tall (extent 53, origin-centered), SM_Pot is
	// 56 cm (extent 28). The old engine-shape dome read as an UPSIDE-DOWN
	// bowl and hid the sauce disk inside itself — Walt filled it and saw
	// nothing happen.
	ConstructorHelpers::FObjectFinder<UStaticMesh> PotTable(
		TEXT("/Game/MagicianLabatory/Source/Props/PotTable/SM_Pot_Table.SM_Pot_Table"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Pot(
		TEXT("/Game/MagicianLabatory/Source/Props/Pot/SM_Pot.SM_Pot"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// The table (origin-centered → lift by its extent so it stands on the floor).
	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(SceneRoot);
	TableMesh->SetRelativeLocation(FVector(0.f, 0.f, 53.f));
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (PotTable.Succeeded()) { TableMesh->SetStaticMesh(PotTable.Object); }

	// The pot, sitting on the tabletop (106 + 28 = center at 134).
	BowlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowlMesh"));
	BowlMesh->SetupAttachment(SceneRoot);
	BowlMesh->SetRelativeLocation(FVector(0.f, 0.f, 134.f));
	BowlMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (Pot.Succeeded()) { BowlMesh->SetStaticMesh(Pot.Object); }

	// The sauce: a glowing green meniscus cresting the pot's rim (162) so a
	// filled pot is unmissable from anywhere in the room.
	SauceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SauceMesh"));
	SauceMesh->SetupAttachment(SceneRoot);
	SauceMesh->SetRelativeScale3D(FVector(0.30f, 0.30f, 0.04f));
	SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 163.f));
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

	// The pour draws attention (Walt's design): the temple's spawner answers
	// the same alarm the corkboard rings. Survive the visit, then Compile.
	if (bSummonRefusersOnFill)
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UHallAlarmSubsystem* Alarm = GI ? GI->GetSubsystem<UHallAlarmSubsystem>() : nullptr)
		{
			Alarm->TriggerAlarm();
		}
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
