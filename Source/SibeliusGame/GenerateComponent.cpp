// GenerateComponent.cpp — SIB-30 Ch6 P1. Resolve a typed request -> spawn / refuse.

#include "GenerateComponent.h"
#include "GenerateMatcher.h"
#include "GenerateCatalog.h"

#include "BuildSite.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // GEngine toast
#include "CollisionQueryParams.h"

UGenerateComponent::UGenerateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGenerateComponent::BeginPlay()
{
	Super::BeginPlay();

	FString Err;
	if (LoadGenerateCatalog(Catalog, Err))
	{
		UE_LOG(LogTemp, Display, TEXT("[Generate] catalog loaded: %d entries, budget %d."), Catalog.Num(), RemainingBudget);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Generate] catalog load FAILED: %s"), *Err);
	}
}

const FGenerateCatalogEntry* UGenerateComponent::FindEntry(const FName& Id) const
{
	return Catalog.FindByPredicate([&Id](const FGenerateCatalogEntry& E) { return E.EntryId == Id; });
}

EGenerateOutcome UGenerateComponent::SubmitRequest(const FString& RawText)
{
	const FGenerateResolution R = ClassifyGenerateRequest(RawText, Catalog, RemainingBudget);

	switch (R.Outcome)
	{
	case EGenerateOutcome::Resolved:
		if (const FGenerateCatalogEntry* E = FindEntry(R.EntryId))
		{
			if (SpawnEntry(*E))
			{
				RemainingBudget -= R.Cost;
				Toast(FString::Printf(TEXT("%s appears.   (budget %d)"), *E->DisplayName.ToString(), RemainingBudget), FColor::Green);
			}
			else
			{
				Toast(TEXT("...nothing came (could not place it)."), FColor::Red);
			}
		}
		break;

	// Distinct refusal line per reason — the P1 seed of the Mrs. Hall presentation (P2).
	case EGenerateOutcome::RefusedNoMatch:
		Toast(TEXT("Mrs. Hall: \"We don't keep that here.\""), FColor(220, 160, 60));
		break;
	case EGenerateOutcome::RefusedAmbiguous:
		Toast(TEXT("Mrs. Hall: \"You'll have to be more specific.\""), FColor(220, 160, 60));
		break;
	case EGenerateOutcome::RefusedOverBudget:
		Toast(FString::Printf(TEXT("Mrs. Hall: \"Not within this room's means.\"   (budget %d)"), RemainingBudget), FColor::Red);
		break;
	case EGenerateOutcome::RefusedUnsafe:
		Toast(TEXT("Mrs. Hall: \"Absolutely not.\""), FColor::Red);
		break;
	}

	return R.Outcome;
}

bool UGenerateComponent::SpawnEntry(const FGenerateCatalogEntry& Entry)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return false;
	}

	// A point a short distance ahead (horizontal look direction), then trace down to the
	// floor so the object lands where the player is facing.
	const FRotator YawOnly(0.0f, Pawn->GetControlRotation().Yaw, 0.0f);
	const FVector Ahead = Pawn->GetActorLocation() + YawOnly.Vector() * SpawnAheadDistance;
	const FVector TraceStart = Ahead + FVector(0.0f, 0.0f, 200.0f);
	const FVector TraceEnd = Ahead - FVector(0.0f, 0.0f, 2000.0f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GenerateSpawnTrace), false, Pawn);
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	const FVector SpawnLoc = bHit ? Hit.ImpactPoint : Ahead;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (!Site)
	{
		return false;
	}

	// Author the result onto the buildable's FinalMesh: catalog mesh + the per-entry
	// transform (SIB-40-in-data — e.g. the key's Scale 0.25 / Yaw 90).
	if (Site->FinalMesh)
	{
		if (UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous())
		{
			Site->FinalMesh->SetStaticMesh(Mesh);
		}
		Site->FinalMesh->SetRelativeScale3D(Entry.SpawnScale);
		Site->FinalMesh->SetRelativeRotation(Entry.SpawnRotation);
	}

	// Built directly via the raw state path (the same one Ch5 restore uses). Generation
	// pays the per-area BUDGET, not Books, so it does NOT route through the Book-spending
	// Build() verb — but it IS a real ABuildSite (GUID + Ch5 persistence).
	Site->RestoreBranchState(1);
	return Site->IsBuilt();
}

void UGenerateComponent::Toast(const FString& Msg, const FColor& Color) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, Color, Msg);
	}
}
