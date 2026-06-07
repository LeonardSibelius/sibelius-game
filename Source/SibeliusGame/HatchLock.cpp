#include "HatchLock.h"
#include "Components/StaticMeshComponent.h"
#include "CompileTypes.h"
#include "InventoryComponent.h"

AHatchLock::AHatchLock()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BlockerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockerMesh"));
	BlockerMesh->SetupAttachment(SceneRoot);
	BlockerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BlockerMesh->SetCanEverAffectNavigation(false);
}

bool AHatchLock::TryUnlock(UInventoryComponent* Inventory)
{
	if (!bLocked || !Inventory)
	{
		return false;
	}
	if (!Inventory->Spend(EResourceType::Key, 1))
	{
		UE_LOG(LogTemp, Display, TEXT("HatchLock: locked - no key"));
		return false;
	}
	ApplyLockedState(false);
	OnUnlocked.Broadcast();
	return true;
}

void AHatchLock::Interact_Implementation(AActor* Interactor)
{
	// Inventory lives on the pawn (the interactor); no actor-finds-player race (Ch1/R7).
	UInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	TryUnlock(Inventory);
}

FText AHatchLock::GetInteractionPrompt_Implementation() const
{
	return bLocked ? NSLOCTEXT("Sibelius", "HatchLockPrompt", "Unlock hatch — needs a Key [E]") : FText::GetEmpty();
}

void AHatchLock::RestoreBranchState(uint8 InState)
{
	ApplyLockedState(InState != 0); // RAW: drops/raises the blocker, no Key spend
}

void AHatchLock::ApplyLockedState(bool bNowLocked)
{
	bLocked = bNowLocked;
	if (BlockerMesh)
	{
		BlockerMesh->SetHiddenInGame(!bNowLocked);
		BlockerMesh->SetCollisionEnabled(bNowLocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

bool AHatchLock::RunLockSelfTest(FString& OutError)
{
	// Bar item 7: locked rejects without a key; unlocks with one; key is consumed.
	if (!bLocked)
	{
		OutError = TEXT("Self-test requires a locked hatch (run before gameplay)");
		return false;
	}

	UInventoryComponent* TempInv = NewObject<UInventoryComponent>(this, TEXT("SelfTestInventory"));

	if (TryUnlock(TempInv))
	{
		OutError = TEXT("Unlocked without a key");
		return false;
	}
	TempInv->Add(EResourceType::Key, 1);
	if (!TryUnlock(TempInv))
	{
		OutError = TEXT("Rejected unlock with a key in inventory");
		return false;
	}
	if (TempInv->GetCount(EResourceType::Key) != 0)
	{
		OutError = TEXT("Key not consumed on unlock");
		return false;
	}
	if (bLocked)
	{
		OutError = TEXT("State still locked after successful unlock");
		return false;
	}

	ApplyLockedState(true); // leave the hatch as found
	return true;
}
