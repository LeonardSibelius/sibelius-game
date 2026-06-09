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
#include "DrawDebugHelpers.h"       // locator sphere + line

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
		Toast(TEXT("[GenSpawn] FAIL: no pawn/world"), FColor::Red);
		return false;
	}

	// Horizontal facing only (pitch zeroed) so the object lands ahead on the floor,
	// not underfoot or up in the air.
	const FVector PlayerLoc = Pawn->GetActorLocation();
	const float YawDeg = Pawn->GetControlRotation().Yaw;
	const FVector Fwd = FRotator(0.0f, YawDeg, 0.0f).Vector();
	const FVector ForwardPoint = PlayerLoc + Fwd * SpawnAheadDistance;

	// Diagnostic: shows whether the forward offset is actually applied (fwdPt should be
	// ~SpawnAheadDistance away from player along Fwd).
	Toast(FString::Printf(TEXT("[GenSpawn] yaw=%.0f fwd=(%.2f,%.2f) ahead=%.0f  player=(%.0f,%.0f) fwdPt=(%.0f,%.0f)"),
		YawDeg, Fwd.X, Fwd.Y, SpawnAheadDistance, PlayerLoc.X, PlayerLoc.Y, ForwardPoint.X, ForwardPoint.Y), FColor::Cyan);

	// Downward floor trace. Start BELOW the ceiling but above the floor (player head
	// height ~= player.Z + 100), then trace straight DOWN a long way so the FIRST hit is
	// the floor — NOT the ceiling. (Starting above the ceiling made the first hit the
	// ceiling, dropping the object overhead.)
	const FVector TraceStart(ForwardPoint.X, ForwardPoint.Y, PlayerLoc.Z + 100.0f);
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 3000.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GenerateSpawnTrace), false, Pawn);
	Params.AddIgnoredActor(Pawn);
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);

	const float FeetZ = PlayerLoc.Z - 90.0f; // ~capsule bottom, fallback Z on a trace miss
	FVector SpawnLoc = bHit ? Hit.ImpactPoint : FVector(ForwardPoint.X, ForwardPoint.Y, FeetZ);

	// Clearance: never let the object sit inside the player capsule. If the chosen spot is
	// too close horizontally, push it out along the forward direction.
	const float MinClearance = 120.0f;
	const FVector2D ToSpawnXY(SpawnLoc.X - PlayerLoc.X, SpawnLoc.Y - PlayerLoc.Y);
	if (ToSpawnXY.SizeSquared() < MinClearance * MinClearance)
	{
		SpawnLoc.X = PlayerLoc.X + Fwd.X * MinClearance;
		SpawnLoc.Y = PlayerLoc.Y + Fwd.Y * MinClearance;
	}

	// Force-load the mesh from the soft pointer; print the path + result.
	const FString MeshPath = Entry.Mesh.ToSoftObjectPath().ToString();
	UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
	Toast(FString::Printf(TEXT("[GenSpawn] mesh '%s' -> %s"),
		MeshPath.IsEmpty() ? TEXT("(none)") : *MeshPath,
		Mesh ? *Mesh->GetName() : TEXT("NULL (failed to load)")), Mesh ? FColor::Green : FColor::Orange);

	// Rest the mesh ON the floor: the actor origin sits at the trace-hit point, but a
	// centered pivot would bury the lower half. Lift the actor by the distance from the
	// pivot down to the bounding-box bottom (scaled), so the mesh BOTTOM rests on the
	// floor. (Yaw rotations don't change the Z extent; good enough for P1.)
	if (Mesh)
	{
		const FBox LocalBox = Mesh->GetBoundingBox();
		SpawnLoc.Z += -LocalBox.Min.Z * Entry.SpawnScale.Z;
	}

	Toast(FString::Printf(TEXT("[GenSpawn] id=%s  trace=%s  spawnLoc=(%.0f,%.0f,%.0f)"),
		*Entry.EntryId.ToString(), bHit ? TEXT("HIT") : TEXT("MISS"),
		SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z), FColor::Cyan);

	// Spawn — adjust out of overlaps but ALWAYS spawn (never silently rejected).
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);

	Toast(FString::Printf(TEXT("[GenSpawn] SpawnActor -> %s"),
		Site ? *Site->GetName() : TEXT("NULL (rejected)")), Site ? FColor::Green : FColor::Red);
	if (!Site)
	{
		return false; // confirmed failure — budget must NOT charge (handled by caller)
	}

	// Author the result onto the buildable's FinalMesh: catalog mesh + the per-entry
	// transform (SIB-40-in-data — e.g. the key's Scale 0.25 / Yaw 90).
	if (Site->FinalMesh)
	{
		if (Mesh)
		{
			Site->FinalMesh->SetStaticMesh(Mesh);
		}
		Site->FinalMesh->SetRelativeScale3D(Entry.SpawnScale);
		Site->FinalMesh->SetRelativeRotation(Entry.SpawnRotation);
	}

	// Reveal it: RestoreBranchState(1) -> ApplyBuiltState(true) unhides FinalMesh (SIB-40
	// lesson: it's hidden until built). Generation pays the BUDGET, not Books, so it does
	// NOT use the Book-spending Build() verb — but it IS a real ABuildSite (GUID + Ch5).
	Site->RestoreBranchState(1);

	const bool bFinalHidden = Site->FinalMesh ? Site->FinalMesh->bHiddenInGame : true;
	const FVector ActualLoc = Site->GetActorLocation(); // after any collision-adjust

	// LOCATE: a red sphere at the object + a green line from the player to it (30s).
	DrawDebugSphere(World, ActualLoc, 60.0f, 12, FColor::Red, false, 30.0f);
	DrawDebugLine(World, PlayerLoc, ActualLoc, FColor::Green, false, 30.0f);

	Toast(FString::Printf(TEXT("[GenSpawn] built=%d scale=(%.2f,%.2f,%.2f) hidden=%d hasMesh=%d actorLoc=(%.0f,%.0f,%.0f)"),
		Site->IsBuilt() ? 1 : 0, Entry.SpawnScale.X, Entry.SpawnScale.Y, Entry.SpawnScale.Z,
		bFinalHidden ? 1 : 0, (Site->FinalMesh && Site->FinalMesh->GetStaticMesh()) ? 1 : 0,
		ActualLoc.X, ActualLoc.Y, ActualLoc.Z), FColor::Cyan);

	// Budget charges only because we reached here with a real, created actor.
	return true;
}

void UGenerateComponent::Toast(const FString& Msg, const FColor& Color) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, Color, Msg);
	}
}
