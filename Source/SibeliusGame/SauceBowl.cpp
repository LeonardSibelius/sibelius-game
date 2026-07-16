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
	PrimaryActorTick.bCanEverTick = true;   // the drip and the meniscus pulse

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

	// The DRIP made visible (Walt: saw no ladle or dripping): a thin glowing
	// stream pouring from the air into the pot for the whole recharge. Stream
	// stops = the ladle is ready.
	StreamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StreamMesh"));
	StreamMesh->SetupAttachment(SceneRoot);
	StreamMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 1.1f));
	StreamMesh->SetRelativeLocation(FVector(0.f, 0.f, 218.f));   // cylinder center; spans ~163..273
	StreamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StreamMesh->SetVisibility(false);
	if (Cylinder.Succeeded()) { StreamMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { StreamMesh->SetMaterial(0, Basic.Object); }
}

void ASauceBowl::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFilled && SauceMesh)
	{
		// The waiting sauce breathes — a slow pulse so a filled pot reads
		// as alive and claimable from across the room.
		const float T = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
		SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 163.f + 2.5f * FMath::Sin(T * 3.f)));
	}
	if (StreamMesh)
	{
		StreamMesh->SetVisibility(!bFilled && !IsLadleReady());
	}
}

void ASauceBowl::BeginPlay()
{
	Super::BeginPlay();

	// Tint the sauce surfaces sauce-green (MIDs from the hard-ref'd base).
	for (UStaticMeshComponent* Comp : { SauceMesh.Get(), StreamMesh.Get() })
	{
		if (Comp)
		{
			if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 1.0f, 0.35f));
			}
		}
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

bool ASauceBowl::TryClaim(APawn* Claimer)
{
	if (!bFilled)
	{
		return false;
	}
	if (!Claimer || FVector::Dist(Claimer->GetActorLocation(), GetActorLocation()) > ClaimRadius)
	{
		return false;   // a Compile elsewhere in the temple is not for this bowl
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
	return true;
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
