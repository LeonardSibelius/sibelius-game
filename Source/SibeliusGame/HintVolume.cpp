// HintVolume.cpp — see the header for the conversation that caused it.

#include "HintVolume.h"
#include "ProteinMachine.h"

#include "SibeliusGame.h"            // LogSibeliusGame
#include "SibeliusHUD.h"
#include "Spaceport.h"               // the default "job already done" class
#include "SibeliusGameCharacter.h"   // DeliVisitedGrant (the constructor default)
#include "ProgressionSubsystem.h"    // HasClaimedGrant - any grant, not just the deli

#include "Components/SphereComponent.h"
#include "EngineUtils.h"             // TActorIterator
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AHintVolume::AHintVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->InitSphereRadius(Radius);
	// Query only, overlap only: a hint you can walk into is a wall, and this is grass.
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetHiddenInGame(true);

	/* DEFAULTS THAT MAKE THE COMMON CASE NEED NO CONFIGURATION. Dragging one onto the
	   lawn should just work: silent until Nyra has sent him, and silent again once a
	   spaceport stands there. Both are overridable per instance. */
	RequiresGrant = ASibeliusGameCharacter::DeliVisitedGrant;
	SatisfiedWhenPresent = ASpaceport::StaticClass();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AHintVolume::OnPlayerEnter);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &AHintVolume::OnPlayerLeave);
}

void AHintVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Live in the editor: drag Radius and the wireframe follows, so covering the grass is
	// a drag rather than a rebuild. Safe here because Trigger IS the root and this is the
	// radius, not a relative location — the CoffeeCup trap was SetRelativeLocation.
	if (Trigger)
	{
		Trigger->SetSphereRadius(Radius);
	}
}

void AHintVolume::OnPlayerEnter(UPrimitiveComponent* /*OverlappedComp*/, AActor* Other,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	// The player only. A ghost wandering through the lawn should not trip the hint.
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || Other != Player)
	{
		return;
	}
	if (bInside)
	{
		return;   // re-overlaps while standing still must not re-fire
	}
	bInside = true;

	/* Not yet invited: he has not met the idea, and a hint before the invitation is
	   just noise on a lawn.

	   THIS ASKS FOR THE GRANT THE PROPERTY NAMES. It used to call HasVisitedDeli
	   regardless of what RequiresGrant said, which was invisible for exactly as long as
	   the only hint in the game used the constructor's default (the deli grant) — and
	   would have gated Phase E's "you have supplies" hint on having eaten a burger.
	   A property the code ignores is worse than no property: it reads as configured. */
	if (RequiresGrant == ASibeliusGameCharacter::DeliVisitedGrant && !AProteinMachine::IsEnhanced(this)) return;
	if (!RequiresGrant.IsNone())
	{
		const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
		if (!Progression || !Progression->HasClaimedGrant(RequiresGrant))
		{
			return;
		}
	}

	/* ALREADY DONE? Ask the WORLD, not a flag. One iteration over actors on a single
	   overlap is nothing, and it is always right - including after a load, a Test-Drive
	   discard that removed the spaceport, or a New Game. A saved bool would be wrong in
	   all three. */
	if (SatisfiedWhenPresent)
	{
		for (TActorIterator<AActor> It(GetWorld(), SatisfiedWhenPresent); It; ++It)
		{
			return;   // it is built; the ground no longer needs to explain itself
		}
	}

	ASibeliusHUD::Toast(this, Line, HoldSeconds, SibeliusToast::Good);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Hint] '%s' shown."), *GetActorNameOrLabel());
}

void AHintVolume::OnPlayerLeave(UPrimitiveComponent* /*OverlappedComp*/, AActor* Other,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (Other == UGameplayStatics::GetPlayerPawn(this, 0))
	{
		bInside = false;   // walk away and it will offer itself again
	}
}
