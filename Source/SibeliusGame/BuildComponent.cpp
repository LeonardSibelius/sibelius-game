#include "BuildComponent.h"
#include "BuildSite.h"
#include "InventoryComponent.h"
#include "CompileTypes.h"          // EResourceType (scan log)
#include "SibeliusHUD.h"           // SIB-39: gate the scan log behind the overlay toggle
#include "ProgressionSubsystem.h"
#include "ProgressionTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UBuildComponent::UBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // timer-driven scan; the overlay (HUD) does the drawing
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
	++DebugScanCount;

	AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = GetInventory();
	if (!Owner || !Inventory)
	{
		if (ASibeliusHUD::bOverlayVisible)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BUILD DBG] ScanForSite #%d aborted: Owner=%s Inventory=%s"),
				DebugScanCount, Owner ? TEXT("ok") : TEXT("NULL"), Inventory ? TEXT("ok") : TEXT("NULL"));
		}
		return;
	}

	const FVector OwnerLoc = Owner->GetActorLocation();
	ABuildSite* Nearest = nullptr;
	ABuildSite* NearbyKey = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	const bool bCompile = Progression && Progression->IsUnlocked(EPowerVerb::Compile);

	for (TActorIterator<ABuildSite> It(GetWorld()); It; ++It)
	{
		ABuildSite* Site = *It;
		const float DistSq = FVector::DistSquared(OwnerLoc, Site->GetActorLocation());
		const float KeyReach = FMath::Max(Site->InteractRadius, 250.f);
		// Once COMPILE is yours, the missing ladder ghost should be findable from
		// the hall — 350cm hid it unless you were already standing on the hole.
		const float StairReach = (bCompile && Site->Output == EBuildOutput::Structure)
			? FMath::Max(Site->InteractRadius, 800.f)
			: Site->InteractRadius;
		if (DistSq <= FMath::Square(StairReach) && DistSq < NearestDistSq)
		{
			Nearest = Site;
			NearestDistSq = DistSq;
		}
		if (Site->IsAtticKeyOrb() && !Site->IsBuilt() && DistSq <= FMath::Square(KeyReach))
		{
			NearbyKey = Site;
		}
	}

	// Ghost: COMPILE + near is enough to SHOW the ladder. Affordability still
	// gates Build(). Hiding it until you had 8 books made the site feel missing.
	if (CurrentSite.IsValid() && CurrentSite.Get() != Nearest)
	{
		CurrentSite->SetGhostVisible(false);
	}
	CurrentSite = Nearest;
	if (Nearest && !Nearest->IsBuilt())
	{
		const bool bShow = Nearest->IsAtticKeyOrb()
			|| (bCompile && Nearest->Output == EBuildOutput::Structure)
			|| Nearest->CanBuild(Inventory);
		Nearest->SetGhostVisible(bShow);
	}

	// The alcove key is a pickup, not a COMPILE verb. Standing near it with
	// enough books takes it — the locked C key must not be the only way in.
	if (NearbyKey && NearbyKey->CanBuild(Inventory))
	{
		NearbyKey->TryTakeAtticKey(Owner);
	}

	if (ASibeliusHUD::bOverlayVisible)
	{
		// SIB-39: only logged while the dev overlay is on, so the log isn't spammed otherwise.
		UE_LOG(LogTemp, Display, TEXT("[BUILD DBG] ScanForSite #%d: nearest=%s canBuild=%d (Books=%d)"),
			DebugScanCount, Nearest ? *Nearest->GetName() : TEXT("none"),
			(Nearest && Nearest->CanBuild(Inventory)) ? 1 : 0,
			Inventory->GetCount(EResourceType::Book));
	}
}

void UBuildComponent::TriggerBuild()
{
	if (TryTakeNearbyAtticKey())
	{
		return;
	}
	UInventoryComponent* Inventory = GetInventory();
	if (!CurrentSite.IsValid() || !Inventory)
	{
		return;
	}
	if (CurrentSite->Build(Inventory))
	{
		UE_LOG(LogTemp, Display, TEXT("BuildComponent: built %s"), *CurrentSite->GetName());
		return;
	}
	if (CurrentSite->IsBuilt() || CurrentSite->IsAtticKeyOrb())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowBanner(FString::Printf(
			TEXT("THE ATTIC LADDER NEEDS %d BOOKS — YOU HAVE %d"),
			CurrentSite->Cost, Inventory->GetCount(CurrentSite->CostResource)), 3.5f);
	}
}

bool UBuildComponent::TryTakeNearbyAtticKey()
{
	AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = GetInventory();
	if (!Owner || !Inventory)
	{
		return false;
	}
	const FVector OwnerLoc = Owner->GetActorLocation();
	for (TActorIterator<ABuildSite> It(GetWorld()); It; ++It)
	{
		ABuildSite* Site = *It;
		if (!Site || !Site->IsAtticKeyOrb() || Site->IsBuilt())
		{
			continue;
		}
		const float Reach = FMath::Max(Site->InteractRadius, 250.f);
		if (FVector::DistSquared(OwnerLoc, Site->GetActorLocation()) > FMath::Square(Reach))
		{
			continue;
		}
		Site->TryTakeAtticKey(Owner);
		return true; // this press is the key orb, even if the books are short
	}
	return false;
}
