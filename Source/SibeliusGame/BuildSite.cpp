#include "BuildSite.h"
#include "Components/StaticMeshComponent.h"
#include "InventoryComponent.h"
#include "Navigation/NavLinkProxy.h"

ABuildSite::ABuildSite()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GhostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	GhostMesh->SetupAttachment(SceneRoot);
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetCanEverAffectNavigation(false);
	GhostMesh->SetHiddenInGame(true); // shown only when affordable + near (UBuildComponent)

	FinalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinalMesh"));
	FinalMesh->SetupAttachment(SceneRoot);
	FinalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FinalMesh->SetCanEverAffectNavigation(false); // nav comes from the NavLink, not the mesh (C2)
	FinalMesh->SetHiddenInGame(true);
}

void ABuildSite::BeginPlay()
{
	Super::BeginPlay();
	GetOrCreateBranchId();     // SIB-29: stable identity from spawn (assign-once if invalid)
	ApplyBuiltState(bIsBuilt); // enforce ghost/final/nav-link coherence from one source of truth
}

bool ABuildSite::CanBuild(const UInventoryComponent* Inventory) const
{
	return !bIsBuilt && Inventory && Inventory->GetCount(CostResource) >= Cost;
}

bool ABuildSite::Build(UInventoryComponent* Inventory)
{
	if (!CanBuild(Inventory))
	{
		return false;
	}
	if (!Inventory->Spend(CostResource, Cost)) // C3: Spend is the gatekeeper
	{
		return false;
	}

	if (Output == EBuildOutput::KeyItem)
	{
		Inventory->Add(EResourceType::Key, 1);
	}

	ApplyBuiltState(true);
	OnBuildStateChanged.Broadcast(true);
	return true;
}

bool ABuildSite::Dismantle(UInventoryComponent* Inventory)
{
	if (!bIsBuilt || !Inventory)
	{
		return false;
	}
	if (Output == EBuildOutput::KeyItem)
	{
		// Key must come back so the refund can't dupe resources (C3/C4).
		if (!Inventory->Spend(EResourceType::Key, 1))
		{
			return false;
		}
	}

	Inventory->Add(CostResource, Cost); // full refund (C4)
	ApplyBuiltState(false);
	OnBuildStateChanged.Broadcast(false);
	return true;
}

void ABuildSite::Interact_Implementation(AActor* Interactor)
{
	if (!bIsBuilt)
	{
		return; // E only dismantles; an unbuilt site has no collision to focus anyway. Build is the B verb.
	}
	// Inventory lives on the pawn (the interactor); no actor-finds-player race (Ch1/R7).
	UInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	Dismantle(Inventory); // full refund (C4)
}

FText ABuildSite::GetInteractionPrompt_Implementation() const
{
	return bIsBuilt ? NSLOCTEXT("Sibelius", "BuildSiteDismantlePrompt", "Dismantle — full refund [E]") : FText::GetEmpty();
}

void ABuildSite::SetGhostVisible(bool bVisible)
{
	if (!bIsBuilt && GhostMesh)
	{
		GhostMesh->SetHiddenInGame(!bVisible);
	}
}

void ABuildSite::RestoreBranchState(uint8 InState)
{
	ApplyBuiltState(InState != 0); // RAW: swaps mesh/collision/nav, no inventory
}

void ABuildSite::ApplyBuiltState(bool bBuilt)
{
	bIsBuilt = bBuilt;

	if (GhostMesh)
	{
		GhostMesh->SetHiddenInGame(true); // ghost never lingers (C8)
		GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (FinalMesh)
	{
		FinalMesh->SetHiddenInGame(!bBuilt);
		FinalMesh->SetCollisionEnabled(bBuilt ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	SetNavLinkEnabled(bBuilt);
}

void ABuildSite::SetNavLinkEnabled(bool bEnabled)
{
	if (NavLink)
	{
		NavLink->SetSmartLinkEnabled(bEnabled); // C2: pre-baked link, flipped at runtime
	}
}

bool ABuildSite::RunBuildSelfTest(FString& OutError)
{
	// Bar item 4, headless: insufficient rejected; sufficient builds + decrements; dismantle refunds.
	UInventoryComponent* TempInv = NewObject<UInventoryComponent>(this, TEXT("SelfTestInventory"));
	const bool bWasBuilt = bIsBuilt;

	// Insufficient inventory must be rejected.
	if (bIsBuilt)
	{
		OutError = TEXT("Self-test requires an un-built site (run before gameplay)");
		return false;
	}
	if (Build(TempInv))
	{
		OutError = TEXT("Build accepted with empty inventory");
		return false;
	}

	// Sufficient inventory must build, decrement, flip state (+ grant key for KeyItem sites).
	TempInv->Add(CostResource, Cost + 2);
	if (!Build(TempInv))
	{
		OutError = TEXT("Build rejected with sufficient inventory");
		return false;
	}
	if (!bIsBuilt || TempInv->GetCount(CostResource) != 2)
	{
		OutError = TEXT("Build did not set state or decrement cost correctly");
		return false;
	}
	if (Output == EBuildOutput::KeyItem && TempInv->GetCount(EResourceType::Key) != 1)
	{
		OutError = TEXT("KeyItem build did not grant a Key");
		return false;
	}
	if (FinalMesh && FinalMesh->bHiddenInGame)
	{
		OutError = TEXT("FinalMesh still hidden after build (C8)");
		return false;
	}
	if (GhostMesh && !GhostMesh->bHiddenInGame)
	{
		OutError = TEXT("GhostMesh visible after build (C8)");
		return false;
	}

	// Dismantle must refund in full and restore pre-build state (C4).
	if (!Dismantle(TempInv))
	{
		OutError = TEXT("Dismantle rejected on a built site");
		return false;
	}
	if (bIsBuilt || TempInv->GetCount(CostResource) != Cost + 2)
	{
		OutError = TEXT("Dismantle did not refund in full (C4)");
		return false;
	}
	if (Output == EBuildOutput::KeyItem && TempInv->GetCount(EResourceType::Key) != 0)
	{
		OutError = TEXT("Dismantle left a duplicate Key (C3)");
		return false;
	}

	ApplyBuiltState(bWasBuilt); // leave the site as found
	return true;
}
