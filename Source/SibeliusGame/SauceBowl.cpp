// SauceBowl.cpp — the temple's sauce fountain. See header.

#include "SauceBowl.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "SauceFluidComponent.h"
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
#include "Misc/PackageName.h"

ASauceBowl::ASauceBowl()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;



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
	ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
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

	// Calm pool in the pot (a disk, not a sphere with world-XY waves — that
	// was the circular saw). Bubbles do the boiling.
	SauceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SauceMesh"));
	SauceMesh->SetupAttachment(SceneRoot);
	SauceMesh->SetRelativeScale3D(FVector(0.53f, 0.53f, 0.16f));
	SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 127.f));
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

	Fluid = CreateDefaultSubobject<USauceFluidComponent>(TEXT("SauceFluid"));
	Fluid->SetupAttachment(SceneRoot);
	Fluid->Role = ESauceFluidRole::Bowl;
	Fluid->SetRelativeLocation(FVector(0.f, 0.f, 140.f));   // steam just above the pool
	Fluid->SimmerScale = 0.08f;
	Fluid->PourScale = 0.07f;
	Fluid->PoolScale = 0.05f;
	Fluid->PourOffset = FVector(0.f, 0.f, 30.f);
	Fluid->PourRotation = FRotator::ZeroRotator;

	// Never-claimed: a zeroed LastClaimTime would lock the bowl for RechargeSeconds
	// after spawn (the SauceSmokeTest "E starts the pour" miss). Negative = ready.
	LastClaimTime = -1.0e9;
}

void ASauceBowl::OnPourComplete()
{
	// Walt's ritual grammar: the stream stops, the sauce appears — full pot.
	bPouring = false;
	bFilled = true;
	if (StreamMesh) { StreamMesh->SetVisibility(false); }
	if (SauceMesh) { SauceMesh->SetVisibility(true); }
	SetBubblesVisible(true);
	if (Fluid)
	{
		Fluid->SetPouring(false);
		Fluid->SetFilled(true);
		Fluid->NotifyBoilOver();
	}
	SetActorTickEnabled(true);
}

void ASauceBowl::BeginPlay()
{
	Super::BeginPlay();

	USauceFluidComponent::DressLiquidMesh(SauceMesh);
	USauceFluidComponent::DressLiquidMesh(StreamMesh);

	if (Bubbles.Num() == 0)
	{
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		for (int32 i = 0; i < 16; ++i)
		{
			UStaticMeshComponent* Bubble = NewObject<UStaticMeshComponent>(this,
				*FString::Printf(TEXT("SauceBubble%d"), i));
			if (!Bubble)
			{
				continue;
			}
			Bubble->SetupAttachment(SceneRoot);
			Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bubble->SetCastShadow(false);
			Bubble->SetVisibility(false);
			if (SphereMesh)
			{
				Bubble->SetStaticMesh(SphereMesh);
			}
			Bubble->RegisterComponent();
			USauceFluidComponent::DressBubbleMesh(Bubble);
			Bubbles.Add(Bubble);
		}
	}
	else
	{
		for (UStaticMeshComponent* Bubble : Bubbles)
		{
			USauceFluidComponent::DressBubbleMesh(Bubble);
		}
	}
	SetBubblesVisible(false);

	if (Fluid)
	{
		Fluid->SetPouring(false);
		Fluid->SetFilled(false);
	}
}

bool ASauceBowl::IsLadleReady() const
{
	if (LastClaimTime < 0.0)
	{
		return true;   // never claimed this session
	}
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastClaimTime >= RechargeSeconds);
}

void ASauceBowl::Interact_Implementation(AActor* /*Interactor*/)
{
	TryStartPour();
}

bool ASauceBowl::TryStartPour()
{
	if (bFilled || bPouring || !IsLadleReady())
	{
		return false;
	}

	// E starts the POUR: the stream falls for PourSeconds, then the pot is
	// full (OnPourComplete). Short performance, not a fixture.
	bPouring = true;
	if (StreamMesh)
	{
		// Always show the green stream. Niagara 2D liquid templates look like
		// a swimming pool; the cylinder is the readable pour.
		StreamMesh->SetVisibility(true);
	}
	if (Fluid)
	{
		Fluid->SetPouring(true);
		Fluid->NotifyBoilOver();
	}
	SetActorTickEnabled(true);
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
	return true;
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
	if (Fluid)
	{
		Fluid->SetFilled(false);
		Fluid->SetPouring(false);
	}
	SetBubblesVisible(false);
	SetActorTickEnabled(false);

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

void ASauceBowl::SetBubblesVisible(bool bVis)
{
	for (UStaticMeshComponent* Bubble : Bubbles)
	{
		if (Bubble)
		{
			Bubble->SetVisibility(bVis);
		}
	}
}

void ASauceBowl::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (bFilled && SauceMesh)
	{
		// Tiny bob only — no XY squash (that spun the saw).
		SauceMesh->SetRelativeLocation(FVector(0.f, 0.f, 127.f + 0.4f * FMath::Sin(T * 2.2f)));
		SauceMesh->SetRelativeScale3D(FVector(0.53f, 0.53f, 0.16f));
	}
	if (bFilled)
	{
		for (int32 i = 0; i < Bubbles.Num(); ++i)
		{
			UStaticMeshComponent* Bubble = Bubbles[i];
			if (!Bubble)
			{
				continue;
			}
			const float Speed = 0.70f + 0.18f * i;
			const float Phase = FMath::Fmod(T * Speed + i * 0.31f, 1.f);
			const float Ang = i * 2.399f + T * 0.35f;
			const float Rad = 6.f + 16.f * FMath::Fmod(i * 0.618f, 1.f);
			const float Wander = 2.2f * FMath::Sin(T * 2.8f + i);
			const float Z = 118.f + Phase * 18.f;
			float Size = 0.040f + 0.055f * FMath::Fmod(i * 0.47f, 1.f);
			Size *= (Phase < 0.82f) ? (0.55f + 0.45f * Phase) : FMath::Max(0.f, 1.f - (Phase - 0.82f) / 0.18f);
			const float Stretch = 1.15f + 0.55f * Phase;
			Bubble->SetRelativeLocation(FVector(
				FMath::Cos(Ang) * Rad + Wander,
				FMath::Sin(Ang) * Rad - 0.6f * Wander,
				Z));
			Bubble->SetRelativeScale3D(FVector(Size, Size, Size * Stretch));
			Bubble->SetVisibility(Size > 0.008f);
		}
	}
	if (bPouring && StreamMesh)
	{
		const float W = 0.030f + 0.010f * FMath::Sin(T * 11.f);
		StreamMesh->SetRelativeScale3D(FVector(W, W * 0.85f, 7.5f));
	}
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
