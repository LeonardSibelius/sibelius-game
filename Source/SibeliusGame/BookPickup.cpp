#include "BookPickup.h"
#include "Components/StaticMeshComponent.h"
#include "InventoryComponent.h"

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
	Destroy();
	return true;
}

void ABookPickup::Interact_Implementation(AActor* Interactor)
{
	// Inventory lives on the pawn (the interactor); no actor-finds-player race (Ch1/R7).
	UInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	Collect(Inventory);
}

FText ABookPickup::GetInteractionPrompt_Implementation() const
{
	return bCollected ? FText::GetEmpty() : NSLOCTEXT("Sibelius", "BookPickupPrompt", "Collect book [E]");
}
