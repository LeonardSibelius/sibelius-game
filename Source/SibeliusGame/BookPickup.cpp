#include "BookPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
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

	// Indoor pickup cue — sauce-green, table-scale. Library books were never
	// actually glowing; the HUD just called them that.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));
	Glow->SetIntensity(600.0f);
	Glow->SetAttenuationRadius(140.0f);
	Glow->SetLightColor(FLinearColor(0.4f, 1.0f, 0.5f));
	Glow->CastShadows = false;
}

void ABookPickup::BeginPlay()
{
	Super::BeginPlay();
	EnsureGlow();
}

void ABookPickup::EnsureGlow()
{
	if (!Glow)
	{
		// Duplicated/saved instances can miss a native component added later.
		// Mobility BEFORE RegisterComponent — a registered component warns and keeps
		// its old mobility, and a Static light cannot be toggled by SetInert at runtime.
		Glow = NewObject<UPointLightComponent>(this, TEXT("Glow"));
		Glow->SetMobility(EComponentMobility::Movable);
		Glow->SetupAttachment(RootComponent);
		Glow->RegisterComponent();
	}

	/* Brightness comes from the INSTANCE, not from the actor's name — see GlowIntensity
	   in the header for why the old label test could not survive a cook.

	   Defaults are the dim library (600 / 140). The living-room table is set brighter and
	   tighter (4000 / 70) by Tools/Scripts/place_living_room_book.py; 22000 bleached the
	   table, so a tight lamp on the book is the right answer in window light. */
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	Glow->SetIntensity(GlowIntensity);
	Glow->SetAttenuationRadius(GlowRadius);
	Glow->SetLightColor(FLinearColor(0.4f, 1.0f, 0.5f));
	Glow->CastShadows = false;
	Glow->SetVisibility(!bInert);
	Glow->SetHiddenInGame(false);
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
	if (Glow)
	{
		Glow->SetVisibility(!bInert);
	}
}
