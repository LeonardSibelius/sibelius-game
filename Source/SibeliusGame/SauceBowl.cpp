// SauceBowl.cpp — the temple's sauce fountain. See header.

#include "SauceBowl.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
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
	PrimaryActorTick.bCanEverTick = false;   // event-driven: pour timer + state flips

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Real props (hard CDO refs so they cook): a four-legged old table (85 cm
	// surface — the magician's two-legged pot stand read as broken furniture
	// to Walt) with the magician's pot (56 cm, extent 28) on top.
	ConstructorHelpers::FObjectFinder<UStaticMesh> PotTable(
		TEXT("/Game/HouseFurniture/Meshes/SM_OldTable_A1/SM_OldTable_A1.SM_OldTable_A1"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Pot(
		TEXT("/Game/MagicianLabatory/Source/Props/Pot/SM_Pot.SM_Pot"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// The table (origin-centered → lift by its extent so it stands on the floor).
	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(SceneRoot);
	TableMesh->SetRelativeLocation(FVector(0.f, 0.f, 42.6f));
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (PotTable.Succeeded()) { TableMesh->SetStaticMesh(PotTable.Object); }

	// The pot, sitting on the tabletop (85 + 28 = center at 113).
	BowlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowlMesh"));
	BowlMesh->SetupAttachment(SceneRoot);
	BowlMesh->SetRelativeLocation(FVector(0.f, 0.f, 113.f));
	BowlMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (Pot.Succeeded()) { BowlMesh->SetStaticMesh(Pot.Object); }

	// The sauce: a glowing meniscus sunk just below the pot's rim (Walt-tuned:
	// wide enough to read as a full pot, low enough to sit IN it, not on it).
	SauceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SauceMesh"));
	SauceMesh->SetupAttachment(SceneRoot);
	SauceMesh->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.03f));
	SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 136.f));
	SauceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SauceMesh->SetVisibility(false);
	if (Cylinder.Succeeded()) { SauceMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { SauceMesh->SetMaterial(0, Basic.Object); }

	// The DRIP made visible (Walt: saw no ladle or dripping): a thin glowing
	// stream pouring from the air into the pot for the whole recharge. Stream
	// stops = the ladle is ready.
	StreamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StreamMesh"));
	StreamMesh->SetupAttachment(SceneRoot);
	// Walt-tuned: a 7.5 m pour descending from high above INTO the pot — the
	// recharge should look like the temple itself refilling the vessel.
	StreamMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 7.5f));
	StreamMesh->SetRelativeLocation(FVector(0.f, 0.f, 505.f));   // cylinder center; spans ~130..880
	StreamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StreamMesh->SetVisibility(false);
	if (Cylinder.Succeeded()) { StreamMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { StreamMesh->SetMaterial(0, Basic.Object); }
}

void ASauceBowl::OnPourComplete()
{
	// Walt's ritual grammar: the stream stops, the sauce appears — full pot.
	bPouring = false;
	bFilled = true;
	if (StreamMesh) { StreamMesh->SetVisibility(false); }
	if (SauceMesh) { SauceMesh->SetVisibility(true); }
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
	if (bFilled || bPouring || !IsLadleReady())
	{
		return;
	}

	// E starts the POUR: the stream falls for PourSeconds, then the pot is
	// full (OnPourComplete). Short performance, not a fixture.
	bPouring = true;
	if (StreamMesh) { StreamMesh->SetVisibility(true); }
	GetWorldTimerManager().SetTimer(PourTimer, this, &ASauceBowl::OnPourComplete,
		FMath::Max(0.5f, PourSeconds), /*bLoop=*/false);

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
		ASibeliusHUD::Toast(this,
			FString::Printf(TEXT("SAUCE COMPILED  +%d  (total %d)"),
				SaucePerBowl, Progression->GetSauce()),
			5.0f, SibeliusToast::Good);
	}
	return true;
}

FText ASauceBowl::GetInteractionPrompt_Implementation() const
{
	if (bPouring)
	{
		return FText::FromString(TEXT("The temple pours..."));
	}
	if (bFilled)
	{
		return FText::FromString(TEXT("Compile the sauce [C]"));
	}
	if (!IsLadleReady())
	{
		const int32 Wait = FMath::CeilToInt(
			RechargeSeconds - (GetWorld()->GetTimeSeconds() - LastClaimTime));
		return FText::FromString(FString::Printf(TEXT("The temple gathers sauce... (%ds)"), Wait));
	}
	return FText::FromString(TEXT("Fill the bowl [E]"));
}
