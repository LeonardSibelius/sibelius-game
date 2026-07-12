// PowerGrant.cpp — placeable power/sauce grant shrine (FUN-1). See header.

#include "PowerGrant.h"
#include "ProgressionSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"

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
	if (bConsumed || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

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
