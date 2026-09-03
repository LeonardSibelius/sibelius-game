// SupplyCounter.cpp — see the header for why there is no shopping.

#include "SupplyCounter.h"

#include "SibeliusGame.h"            // LogSibeliusGame
#include "SibeliusHUD.h"             // Toast
#include "ProgressionSubsystem.h"    // the wallet and the grant registry

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

/* "City.Supplies" — same namespace as City.Deli, because it is the same kind of fact:
   a thing the player did in the city, once, that the rest of the game reads. */
const FName ASupplyCounter::SuppliesGrant(TEXT("City.Supplies"));

ASupplyCounter::ASupplyCounter()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Usually left empty: the shop already has a cashier table, and a second counter
	// floating inside it would be worse than none.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The volume the interact trace finds. Query-only so he walks through it, blocking so
	// the trace stops on it.
	Reach = CreateDefaultSubobject<USphereComponent>(TEXT("Reach"));
	Reach->SetupAttachment(Root);
	Reach->InitSphereRadius(ReachRadius);
	Reach->SetRelativeLocation(FVector(0.0f, 0.0f, ReachHeight));
	Reach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Reach->SetCollisionResponseToAllChannels(ECR_Block);
	Reach->SetHiddenInGame(true);
}

bool ASupplyCounter::HasSupplies(const UObject* WorldContext)
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(WorldContext);
	return Progression && Progression->HasClaimedGrant(SuppliesGrant);
}

FText ASupplyCounter::GetInteractionPrompt_Implementation() const
{
	/* THE PROMPT TELLS THE TRUTH BEFORE THE PRESS. A counter that offers to sell supplies
	   he already owns, and then refuses, teaches him the shop is broken. */
	return HasSupplies(this) ? DonePrompt : Prompt;
}

void ASupplyCounter::Interact_Implementation(AActor* Interactor)
{
	if (HasSupplies(this))
	{
		// Not silence: silence on a press reads as a dead object. Say what is true.
		ASibeliusHUD::Toast(this, TEXT("You already have your supplies."), 3.0f,
			SibeliusToast::Info);
		return;
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;   // headless / no save: quietly do nothing rather than give it away
	}

	/* TrySpendSauce IS THE CHECK — it refuses and leaves the wallet untouched when the
	   balance is short, so asking "can he afford it" separately would be a second copy of
	   the same rule that could disagree with the first. One call, one answer.
	   (ACoffeeCup in the deli records the same reasoning; this is deliberately identical.) */
	if (!Progression->TrySpendSauce(Price))
	{
		FString Line = TooPoorLine;
		Line.ReplaceInline(TEXT("{0}"), *FString::FromInt(Price));
		ASibeliusHUD::Toast(this, Line, 3.0f, SibeliusToast::Warn);
		return;
	}

	/* CLAIM AFTER THE MONEY IS TAKEN, and claim-and-save in one call, so a quit at the
	   shop door cannot leave him poorer and unprovisioned. If this ever returns false the
	   grant was already held, which the guard at the top should have caught — log it
	   rather than pretend, because it would mean the two checks disagree. */
	if (!Progression->ClaimOneTimeGrant(SuppliesGrant))
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Supplies] Sauce was spent but %s was already claimed - the prompt and "
			     "the purchase disagree."), *SuppliesGrant.ToString());
	}

	ASibeliusHUD::Toast(this, BoughtLine, 4.0f, SibeliusToast::Good);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Supplies] bought for %d Sauce; %s claimed."), Price, *SuppliesGrant.ToString());
}
