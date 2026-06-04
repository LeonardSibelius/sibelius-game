#include "BuildComponent.h"
#include "BuildSite.h"
#include "InventoryComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UBuildComponent::UBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // timer-driven scan; MoveTo-every-frame lesson generalized
}

void UBuildComponent::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetTimerManager().SetTimer(
		SiteScanTimer, this, &UBuildComponent::ScanForSite, SiteScanInterval, true);
}

void UBuildComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(SiteScanTimer);
	Super::EndPlay(EndPlayReason);
}

UInventoryComponent* UBuildComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UBuildComponent::ScanForSite()
{
	const AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = GetInventory();
	if (!Owner || !Inventory)
	{
		return;
	}

	const FVector OwnerLoc = Owner->GetActorLocation();
	ABuildSite* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ABuildSite> It(GetWorld()); It; ++It)
	{
		ABuildSite* Site = *It;
		const float DistSq = FVector::DistSquared(OwnerLoc, Site->GetActorLocation());
		if (DistSq <= FMath::Square(Site->InteractRadius) && DistSq < NearestDistSq)
		{
			Nearest = Site;
			NearestDistSq = DistSq;
		}
	}

	// Ghost visibility follows proximity + affordability; one site at a time (C12).
	if (CurrentSite.IsValid() && CurrentSite.Get() != Nearest)
	{
		CurrentSite->SetGhostVisible(false);
	}
	CurrentSite = Nearest;
	if (Nearest)
	{
		Nearest->SetGhostVisible(Nearest->CanBuild(Inventory));
	}
}

void UBuildComponent::TriggerBuild()
{
	UInventoryComponent* Inventory = GetInventory();
	if (CurrentSite.IsValid() && Inventory)
	{
		// CanBuild gates inside Build(); pressing B anywhere else is a silent no-op (C12).
		if (CurrentSite->Build(Inventory))
		{
			UE_LOG(LogTemp, Display, TEXT("BuildComponent: built %s"), *CurrentSite->GetName());
		}
	}
}
