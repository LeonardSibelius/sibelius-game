#include "BuildComponent.h"
#include "BuildSite.h"
#include "InventoryComponent.h"
#include "BranchPIEComponent.h"               // TEMP debug: read the load input-gate state
#include "CompileTypes.h"                      // EResourceType
#include "Components/StaticMeshComponent.h"    // TEMP debug: ghost mesh/visibility/collision
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"                     // TEMP debug: GEngine on-screen messages
#include "TimerManager.h"

UBuildComponent::UBuildComponent()
{
	// TEMP (SIB-37 ghost regression): tick enabled only for the on-screen debug readout.
	// Restore to false once the ghost is fixed. The scan itself remains timer-driven.
	PrimaryComponentTick.bCanEverTick = true;
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
	++DebugScanCount; // TEMP heartbeat

	const AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = GetInventory();
	if (!Owner || !Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BUILD DBG] ScanForSite #%d aborted: Owner=%s Inventory=%s"),
			DebugScanCount, Owner ? TEXT("ok") : TEXT("NULL"), Inventory ? TEXT("ok") : TEXT("NULL"));
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

	UE_LOG(LogTemp, Display, TEXT("[BUILD DBG] ScanForSite #%d: nearest=%s canBuild=%d (Books=%d)"),
		DebugScanCount, Nearest ? *Nearest->GetName() : TEXT("none"),
		(Nearest && Nearest->CanBuild(Inventory)) ? 1 : 0,
		Inventory->GetCount(EResourceType::Book));
}

void UBuildComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	// TEMP (SIB-37 ghost regression): live on-screen readout. Independent of ScanForSite's
	// CurrentSite, so it reports even if the scan isn't finding anything.
	if (!GEngine)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UInventoryComponent* Inv = GetInventory();
	const int32 Books = Inv ? Inv->GetCount(EResourceType::Book) : -1;

	UBranchPIEComponent* PIE = Owner->FindComponentByClass<UBranchPIEComponent>();
	const bool bGated = PIE && PIE->IsLoadInputGated();

	// Independent nearest-site search (so the readout never depends on the scan working).
	const FVector Loc = Owner->GetActorLocation();
	ABuildSite* Near = nullptr;
	float NearDist = -1.f;
	for (TActorIterator<ABuildSite> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(Loc, It->GetActorLocation());
		if (!Near || D < NearDist)
		{
			Near = *It;
			NearDist = D;
		}
	}

	const FColor Col = FColor::Yellow;
	GEngine->AddOnScreenDebugMessage(7701, 0.f, Col, FString::Printf(
		TEXT("[BUILD DBG] scan#%d  inputGate:%s  Book:%d  curSite:%s"),
		DebugScanCount, bGated ? TEXT("ENGAGED") : TEXT("released"), Books,
		CurrentSite.IsValid() ? *CurrentSite->GetName() : TEXT("none")));

	if (Near)
	{
		const bool bGhostHidden = Near->GhostMesh ? Near->GhostMesh->bHiddenInGame : true;
		const bool bHasMesh = Near->GhostMesh && Near->GhostMesh->GetStaticMesh() != nullptr;
		const int32 GhostColl = Near->GhostMesh ? (int32)Near->GhostMesh->GetCollisionEnabled() : -1;
		GEngine->AddOnScreenDebugMessage(7702, 0.f, Col, FString::Printf(
			TEXT("[BUILD DBG] near:%s dist:%.0f/rad:%.0f  built:%d canBuild:%d cost:%d  ghostHidden:%d ghostMesh:%s ghostColl:%d"),
			*Near->GetName(), NearDist, Near->InteractRadius,
			Near->IsBuilt() ? 1 : 0, Near->CanBuild(Inv) ? 1 : 0, Near->Cost,
			bGhostHidden ? 1 : 0, bHasMesh ? TEXT("SET") : TEXT("NONE!"), GhostColl));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(7702, 0.f, Col, TEXT("[BUILD DBG] no ABuildSite in world"));
	}
#endif
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
