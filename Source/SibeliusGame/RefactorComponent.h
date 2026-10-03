// RefactorComponent.h
//
// SIB-26 — Ch2 Refactor. The player's selection + trigger component. Lives ON
// the character, so there's NO "find the player" race (R7 — the Ch1 door bug
// can't happen here). It camera-traces each tick for a URefactorableComponent
// and, on the Refactor input, toggles whatever it's currently looking at.

// APPEAL-R (Walt): R is also the WILD refactor — point at nearly any prop (the
// office desk, a chair) or a Refuser and rewrite it as TREASURE: a big gemstone
// or gold coins. EVERYTHING is reversible (drafts are drafts): R on the treasure
// restores the original — a prop reappears untouched, a Refuser gets his body
// unhidden and his mind re-possessed. Pre-authored URefactorableComponents
// always win over the wild path.
//
// TREASURE, NOT ANIMALS (Walt, 2026-10-03): "animals were amusing at first, but now I
// want Refactor to make big gemstones and gold coins, not animals. and i want it to cost
// 10 sauce when you refactor" - "throughout the game, no more animals." Making treasure
// costs TreasureSauceCost; turning it back is free, so a draft he regrets never costs
// twice. The office puzzles' authored refactors stay free: a fix the story needs must
// never wait on his wallet.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RefactorComponent.generated.h"

class URefactorableComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

// Bookkeeping stamped onto the spawned TREASURE: which hidden prop it stands
// in for. R on the treasure destroys it and un-hides the original — the prop
// itself is never modified, so restore is always perfect.
UCLASS()
class SIBELIUSGAME_API UWildRefactorState : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY() TObjectPtr<AActor> OriginalActor;

	/** The original was hidden BODY ONLY rather than as a whole actor, so the rest of
	 *  what it renders kept going. A machine stage turned into a gem has to keep its
	 *  brass plaque hanging in front of it -- "GRADER / grade B or better passes"
	 *  bolted to a pile of gold IS the joke, and without it there is only loot in the
	 *  living room. Restore has to undo it the same way it was done, hence the flag. */
	UPROPERTY() bool bHidMeshesOnly = false;

	/* OR ONE PLANT OUT OF A PAINTED FIELD (Kaia's planet, 2026-10-03). Foliage is not an
	   actor per plant: thousands share one instanced component, so the stand-in is a single
	   instance, shrunk to nothing in place while the treasure stands there and put back
	   exactly on R. Shrunk, never removed: removing an instance renumbers every one after
	   it, and the next restore would put the wrong plant back. */
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> OriginalInstances;
	UPROPERTY() int32 OriginalInstanceIndex = INDEX_NONE;
	UPROPERTY() FTransform OriginalInstanceTransform;
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

	/** An agent's virtual home raises the wild refactor's size limit, lets it take single
	 *  foliage instances, and puts a floor under the treasure's size (AVirtualHome). Nowhere
	 *  else changes any of them. */
	void SetMaxWildTargetSize(float Cm) { MaxWildTargetSize = Cm; }
	void SetWildInstancesAllowed(bool bAllowed) { bWildInstances = bAllowed; }
	void SetMinTreasureSize(float Cm) { MinTreasureSize = Cm; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// How far the selection trace reaches (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	float TraceDistance = 800.f;

	// --- Wild refactor (APPEAL-R) --------------------------------------------

	/* THE HOARD: Dekogon's "Treasures VOL.1 - Ancient Treasures" (Fab, in Walt's library;
	   Content/AncientTreasures, gitignored like every bought pack). Named one by one and
	   hard-referenced from the constructor, so they cook via the CDO with no
	   DirectoriesToAlwaysCook - and only these, not the whole pack's pots and swords.
	   Three piles drawn a third each, so the one coin is not outvoted by six gold pieces.
	   The pack may not be shown to an AI (its licence): pick by name, Walt judges the look.
	   He chose the gold pieces and dropped the grey coin, SM_Coin_02a, then the bangles and
	   the eighteen coin heaps, which "look weird" (2026-10-03). Its meshes ship with no
	   simple collision; Tools/Scripts/prepare_hoard.py gives each a box, and BeginPlay drops
	   any that still has none.

	   THE COIN AND GEMS ARE FLAT - a coin is 0.6 cm thick, the gems lie on their sides.
	   Fitted to a rock by its longest side, a coin was a disc in the grass and Walt saw "the
	   object vanishes and there is no treasure". So gems and the coin STAND UP, longest side
	   vertical and broad face to him; the gold pieces stay as made. */
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> Gems;

	// Stood on edge like a wheel: the gold coin.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> OnEdge;

	// As made, front to him: chalices, the gold bar, the chests, the chest of coins.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> GoldPieces;

	// The treasure's footprint stops this far short of him (cm). Fitted to a fourteen-metre
	// tree he was standing INSIDE the treasure, and from inside a mesh there is nothing to see.
	// It shrinks to keep clear, but only to half size; past that it steps back away from him.
	// Shrinking alone made a coin "0 cm tall" for him standing under a mushroom's cap, 65 cm
	// from its middle - "refactored a big mushroom and it vanished" (Walt, 2026-10-03).
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float PlayerClearance = 75.f;

	// What making treasure costs, in sauce. Turning it back is free.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild", meta = (ClampMin = "0"))
	int32 TreasureSauceCost = 10;

	/** When he cannot afford it; {0} becomes the cost, {1} what he has. */
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	FString TooPoorLine = TEXT("REFACTOR NEEDS {0} SAUCE — YOU HAVE {1}");

	// Props whose bounding box exceeds this (largest side, cm) are refused —
	// keeps walls, floors, and whole buildings from turning to gold.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float MaxWildTargetSize = 350.f;

	// A refactored Refuser becomes treasure roughly this tall (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float RefuserTreasureSize = 180.f;

	// Treasure is never smaller than this (longest side, cm). 0 in the house, where a gem
	// the size of the microwave it replaced looks right; a virtual home raises it.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild", meta = (ClampMin = "0.0"))
	float MinTreasureSize = 0.f;

	// Single instances of painted foliage are fair game. Off except in a virtual home.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	bool bWildInstances = false;

	virtual void BeginPlay() override;

private:
	URefactorableComponent* TraceForRefactorable() const;

	// The wild path: prop → hidden + treasure spawned in its place, treasure →
	// original restored, Refuser → treasure until restored. True if anything changed.
	bool TryWildRefactor();
	UStaticMesh* PickTreasure(bool& bOutUpright) const;

	// Treasure the size of MatchSize, standing on the ground under Space (what it replaces,
	// which the ground search looks past), facing him and clear of him (see Gems).
	AActor* SpawnTreasureActor(UStaticMesh* TreasureMesh, bool bUpright, const FBox& Space, float MatchSize,
		const AActor* Replaced, const UPrimitiveComponent* ReplacedField);

	// The floor under the middle of Box, leaving out the thing being replaced: what the
	// treasure stands on, level, whatever tilt the original had.
	FVector GroundBelow(const FBox& Box, const AActor* Replaced, const UPrimitiveComponent* ReplacedField) const;

	// Takes TreasureSauceCost from his wallet, or says he is short. True if paid.
	bool PayForTreasure() const;

	// One plant out of a painted field becomes treasure. True if it did.
	bool TransmuteInstance(UInstancedStaticMeshComponent* Field, int32 Index);

	UPROPERTY() TObjectPtr<URefactorableComponent> CurrentTarget;
};
