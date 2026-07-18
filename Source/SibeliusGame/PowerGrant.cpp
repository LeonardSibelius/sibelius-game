// PowerGrant.cpp — placeable power/sauce grant shrine (FUN-1). See header.

#include "PowerGrant.h"
#include "ProgressionSubsystem.h"
#include "SlotScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APowerGrant::APowerGrant()
{
	PrimaryActorTick.bCanEverTick = true;

	// CP3 lesson: every placeable actor gets an explicit root. The trigger is the
	// root so the overlap volume sits exactly where Walt places the actor.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->InitSphereRadius(110.0f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // walk-through; the sphere does the work
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	// The shrine glow (Walt's ask, after standing 2 m from an invisible DEPLOY
	// shrine): the curio-beacon recipe at indoor scale — a slim room-height
	// pillar + a colored point light. Hard CDO refs so both always cook.
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
	BeaconMesh->SetupAttachment(Trigger);
	BeaconMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 2.4f));   // 12 cm wide, 2.4 m tall
	BeaconMesh->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconMesh->SetCanEverAffectNavigation(false);
	BeaconMesh->SetCastShadow(false);
	if (Cylinder.Succeeded()) { BeaconMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { BeaconMesh->SetMaterial(0, Basic.Object); }

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Glow->SetIntensity(3000.f);
	Glow->SetAttenuationRadius(600.f);
	Glow->CastShadows = false;
}

void APowerGrant::BeginPlay()
{
	Super::BeginPlay();

	// Already claimed in a previous session -> this shrine is spent; vanish before
	// the player sees it. (The progression save is the authority, not the level.)
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		if (Progression->HasClaimedGrant(EffectiveGrantKey()))
		{
			Destroy();
			return;
		}
	}

	if (Mesh)
	{
		RestLocation = Mesh->GetRelativeLocation();
	}

	// Tint the beacon: cyan = a power waits here, gold = a sauce stash.
	const FLinearColor GlowColor = bGrantsPower
		? FLinearColor(0.35f, 0.9f, 1.0f)
		: FLinearColor(1.0f, 0.85f, 0.3f);
	if (Glow)
	{
		Glow->SetLightColor(GlowColor);
	}
	if (BeaconMesh)
	{
		BeaconMesh->SetVisibility(bShowBeacon);
		if (UMaterialInstanceDynamic* MID = BeaconMesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), GlowColor);
		}
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APowerGrant::OnTriggerOverlap);
}

void APowerGrant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Pickup read: slow spin + gentle bob. Cheap, no material or animation needed.
	if (Mesh)
	{
		BobPhase += DeltaSeconds;
		Mesh->SetRelativeLocation(RestLocation + FVector(0, 0, 12.0f * FMath::Sin(BobPhase * 2.0f)));
		Mesh->AddRelativeRotation(FRotator(0.0f, 45.0f * DeltaSeconds, 0.0f));
	}
}

FName APowerGrant::EffectiveGrantKey() const
{
	if (!GrantKey.IsNone())
	{
		return GrantKey;
	}
	// Power shrines key on the verb (one claim per verb, wherever it's placed);
	// sauce-only markers key on their own stable level name.
	return bGrantsPower ? FName(*PowerVerbDisplayName(Power)) : GetFName();
}

void APowerGrant::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (bConsumed || bTrialOpen || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	// Walt's trial: a POWER must be won at the machine, not collected like
	// loose change. Sauce-only markers keep the instant walk-in grant.
	if (bSlotTrial && bGrantsPower)
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			OpenTrial(PC);
		}
		return;
	}

	ClaimNow();
}

void APowerGrant::OpenTrial(APlayerController* PC)
{
	// Fresh widget per entry = fresh stake per entry, no state to reset.
	TrialWidget = CreateWidget<USlotScreenWidget>(PC, USlotScreenWidget::StaticClass());
	if (!TrialWidget)
	{
		return;
	}
	TrialWidget->InitModel(FMath::Rand());
	TrialWidget->SetTrial(TrialStartCredits, TrialTargetCredits);
	TrialWidget->OnTrialWon.BindUObject(this, &APowerGrant::HandleTrialWon);
	TrialWidget->OnClosed.BindUObject(this, &APowerGrant::HandleTrialClosed);
	TrialWidget->AddToViewport(80);

	// SC1's pattern: UIOnly + focus so Space reaches the reels and WASD stops.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TrialWidget->TakeWidget());
	PC->SetInputMode(Mode);
	TrialWidget->SetFocus();
	bTrialOpen = true;
}

void APowerGrant::HandleTrialWon()
{
	CloseTrialWidget();
	ClaimNow();
}

void APowerGrant::HandleTrialClosed()
{
	// Retreat or bust: put the world back; step out and in again to retry.
	CloseTrialWidget();
}

void APowerGrant::CloseTrialWidget()
{
	if (TrialWidget && TrialWidget->IsInViewport())
	{
		TrialWidget->RemoveFromParent();
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	TrialWidget = nullptr;
	bTrialOpen = false;
}

void APowerGrant::ClaimNow()
{
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression || !Progression->ClaimOneTimeGrant(EffectiveGrantKey()))
	{
		return; // no subsystem (headless) or already claimed — nothing to give
	}
	bConsumed = true;

	// The grant moment: the HUD's ceremony banner (OnPowerUnlocked) and the
	// sauce delta flash (OnSauceChanged) fire off these two calls — FUN-7.
	if (bGrantsPower)
	{
		Progression->UnlockPower(Power);
	}
	Progression->GrantSauce(SauceReward);

	if (GrantSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GrantSound, GetActorLocation());
	}

	Destroy();
}
