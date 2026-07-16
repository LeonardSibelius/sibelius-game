// RefactorComponent.h
//
// SIB-26 — Ch2 Refactor. The player's selection + trigger component. Lives ON
// the character, so there's NO "find the player" race (R7 — the Ch1 door bug
// can't happen here). It camera-traces each tick for a URefactorableComponent
// and, on the Refactor input, toggles whatever it's currently looking at.

// APPEAL-R (Walt): R is also the WILD refactor — point at nearly any prop (the
// office desk, a chair) or a Refuser and rewrite it as a creature from the
// Menagerie. Props are reversible (R again restores the original — drafts are
// drafts); a refactored Refuser is gone for good. Pre-authored
// URefactorableComponents always win over the wild path.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetData.h"
#include "RefactorComponent.generated.h"

class URefactorableComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Bookkeeping stamped onto the spawned CREATURE: which hidden prop it stands
// in for. R on the creature destroys it and un-hides the original — the prop
// itself is never modified, so restore is always perfect.
UCLASS()
class SIBELIUSGAME_API UWildRefactorState : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY() TObjectPtr<AActor> OriginalActor;
};

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API URefactorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URefactorComponent();

	// Bind to IA_Refactor (Started). Toggles the currently targeted refactorable.
	UFUNCTION(BlueprintCallable, Category = "Refactor")
	void TriggerRefactor();

	// The refactorable under the crosshair this frame, or null. HUD can read this
	// to show a "[R] Refactor" prompt.
	UFUNCTION(BlueprintPure, Category = "Refactor")
	URefactorableComponent* GetCurrentTarget() const { return CurrentTarget; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// How far the selection trace reaches (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	float TraceDistance = 800.f;

	// --- Wild refactor (APPEAL-R) --------------------------------------------

	// Folders auto-scanned for creatures at BeginPlay (every StaticMesh and
	// SkeletalMesh inside joins the zoo — drop in a new Fab pack, add its
	// folder here, done). These folders MUST also be in DefaultGame.ini's
	// DirectoriesToAlwaysCook or packaged builds ship an empty zoo (the
	// soft-refs-don't-cook lesson: a scan is not a reference).
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<FName> MenagerieFolders = {
		TEXT("/Game/AfricanAnimalsPack"),
		TEXT("/Game/AnimalVarietyPack"),
		// Walt's three keepers (rooster/pig/rabbit statues), pruned out of the
		// 2.6 GB UltimateFarmAnimalsCollection — the rest was deleted.
		TEXT("/Game/FarmKeepers"),
	};

	// Hard-referenced base creatures (dragon statue, toy rabbit) — these cook
	// via the CDO and guarantee the zoo is never empty even with no packs.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UObject>> Menagerie;

	// Props whose bounding box exceeds this (largest side, cm) are refused —
	// keeps walls, floors, and whole buildings out of the zoo.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float MaxWildTargetSize = 350.f;

	// A refactored Refuser becomes a creature roughly this tall (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float RefuserCreatureSize = 180.f;

	virtual void BeginPlay() override;

private:
	URefactorableComponent* TraceForRefactorable() const;

	// The wild path: prop → hidden + creature spawned in its place, creature →
	// original restored, Refuser → creature for good. True if anything changed.
	bool TryWildRefactor();
	UObject* PickCreature() const;
	AActor* SpawnCreatureActor(UObject* CreatureMesh, const FTransform& Where, float MatchSize);

	UPROPERTY() TObjectPtr<URefactorableComponent> CurrentTarget;

	// Creatures found by the folder scan; loaded lazily on first use.
	TArray<FAssetData> ScannedCreatures;
};
