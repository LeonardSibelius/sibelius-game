#include "BookPickup.h"
#include "Components/StaticMeshComponent.h"
#include "InventoryComponent.h"
#include "ProgressionSubsystem.h"   // FUN-2: books pay Sauce

ABookPickup::ABookPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// CP3 lesson: every placeable actor gets an explicit root.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block); // visible to the interact trace
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false); // books must not carve the navmesh
}

bool ABookPickup::Collect(UInventoryComponent* Inventory)
{
	if (bCollected || !Inventory)
	{
		return false;
	}
	bCollected = true;
	Inventory->Add(Resource, Amount);

	// FUN-2: the same collect also feeds the Sauce wallet (null-safe: headless
	// smoke tests have no game instance and simply skip the grant).
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->GrantSauce(SauceOnCollect);
		Progression->BumpStat(SibeliusStats::BooksCollected);
	}

	Destroy();
	return true;
}

void ABookPickup::Interact_Implementation(AActor* Interactor)
{
	if (bInert)
	{
		return; // SIB-36: non-interactable while a branch is open
	}
	// Inventory lives on the pawn (the interactor); no actor-finds-player race (Ch1/R7).
	UInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	Collect(Inventory);
}

FText ABookPickup::GetInteractionPrompt_Implementation() const
{
	return (bCollected || bInert) ? FText::GetEmpty() : NSLOCTEXT("Sibelius", "BookPickupPrompt", "Collect book [E]");
}

void ABookPickup::SetInert(bool bNewInert)
{
	if (bInert == bNewInert || bCollected)
	{
		return;
	}
	bInert = bNewInert;
	if (Mesh)
	{
		// Drop the interact-trace collision (UInteractorComponent's trace skips it)
		// and shrink as the inert cue; the camera desaturate provides the grey.
		Mesh->SetCollisionEnabled(bInert ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
		SetActorScale3D(bInert ? FVector(0.6f) : FVector(1.0f));
	}
}
