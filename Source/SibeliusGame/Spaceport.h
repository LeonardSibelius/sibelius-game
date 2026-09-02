// Spaceport.h — what Generate makes on the lawn across from Jacob's.
//
// ===========================================================================
// docs/SPACEPORT_PLAN.md, Phase B.
//
// Leonard types "spaceport" and the ground opens. Pad slabs, gantry legs, a tower and
// two fuel tanks rise out of the grass over about eight seconds. Phase C puts a rocket
// on the pad; this class is the thing that receives it.
//
// ---------------------------------------------------------------------------
// WHY IT EXTENDS ABuildSite AND NOT AActor.
//
// Everything UGenerateComponent creates is an ABuildSite, and that is load-bearing
// rather than incidental. The site is what carries:
//
//     IBranchable     Test-Drive can branch a spaceport and DISCARD it for free
//     MarkGenerated   provenance, so Deploy knows which catalog row made it
//     a stable GUID   identity that survives save and re-spawn
//
// The plan's best mechanic — branch before you launch, so a failed launch costs
// nothing — therefore needs no branching code written for it at all. It is inherited.
// A plain AActor would have looked identical and sat outside all three systems.
//
// ---------------------------------------------------------------------------
// THE PARTS ARE DATA, AND THE MESHES ARE VENDOR ASSETS THAT COOK BY DIRECTORY RULE.
//
// ModularSciFiEnv_F and _J are gitignored vendor packs, and a soft path from C++ is not
// a package reference — the cooker does not follow it. Without the
// DirectoriesToAlwaysCook lines added alongside this class, the spaceport assembles
// perfectly in PIE and is INVISIBLE in the shipped build. That is the v0.7.4 bug
// exactly, and it is why those lines exist in DefaultGame.ini.
//
// Parts default to a layout built in the constructor, and every field is EditAnywhere,
// so the shape is tunable on a placed instance without a rebuild.

#pragma once

#include "CoreMinimal.h"
#include "BuildSite.h"
#include "Spaceport.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * One piece of the spaceport: a mesh, where it sits, and when it arrives.
 *
 * RiseAt/RiseFor are FRACTIONS of the whole assembly (0..1), not seconds, so retuning
 * AssemblySeconds re-times every part together instead of desynchronising them.
 */
USTRUCT()
struct FSpaceportPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FVector Scale = FVector::OneVector;

	/** When this part starts rising, as a fraction of the whole assembly. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RiseAt = 0.0f;

	/** How long it takes to arrive, as a fraction. Clamped so it can never be zero. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float RiseFor = 0.25f;
};

UCLASS()
class SIBELIUSGAME_API ASpaceport : public ABuildSite
{
	GENERATED_BODY()

public:
	ASpaceport();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/* BRANCH STATE — deliberately wider than ABuildSite's built/unbuilt, and deliberately
	   numbered so Phase C can grow into it without changing what a saved 0 or 1 means.

	       0  Empty        the lawn (ABuildSite's "not built")
	       1  Assembled    standing, pad clear (ABuildSite's "built")
	       2  RocketOnPad  Phase C
	       3  Launched     Phase C

	   0 and 1 keep the parent's meaning exactly, so an existing save reading 1 gets a
	   finished spaceport rather than a surprise. */
	virtual uint8 CaptureBranchState() const override;
	virtual void RestoreBranchState(uint8 InState) override;

	/* THE ASSEMBLY IS A SHOW, AND A RELOAD IS NOT.

	   Both live generation and re-spawn-on-load funnel through AuthorGeneratedSite ->
	   RestoreBranchState(1), which is correct: from the save's point of view both mean
	   "this spaceport exists". But the player should watch it rise ONCE, when he asks for
	   it — not every time he loads a save.

	   So RestoreBranchState always SNAPS, and this hook (empty on ABuildSite) is what the
	   live path calls to replay the show. It is a virtual on the base rather than a cast
	   in UGenerateComponent so the generator never needs to know this class exists. */
	virtual void OnGeneratedFresh() override;

	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Raise the parts over AssemblySeconds. Safe to call when already assembled. */
	void PlayAssembly();

	/** Fully built, instantly, no animation — the reload path and the smoke tests. */
	void SnapAssembled();

	/** Back to bare lawn: every part component destroyed. */
	void ClearParts();

	bool IsAssembled() const { return bAssembled; }

	/** The world-space point a rocket stands on. Phase C's entry point. */
	UFUNCTION(BlueprintPure, Category = "Spaceport")
	FVector GetPadTopLocation() const;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	TArray<FSpaceportPart> Parts;

	/** Total seconds for the whole structure to arrive. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.5"))
	float AssemblySeconds = 8.0f;

	/** How far below its resting place each part starts. It grows OUT of the lawn. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.0"))
	float RiseFromBelow = 700.0f;

	/** Height above the actor origin where a rocket sits. See GetPadTopLocation. */
	UPROPERTY(EditAnywhere, Category = "Spaceport")
	float PadTopHeight = 60.0f;

private:
	/** Create the component for each part, hidden and sunk. Idempotent. */
	void BuildPartComponents();

	/** Place one part at its progress (0 = fully sunk, 1 = home). */
	void ApplyPartProgress(int32 Index, float Alpha);

	/** The authored default layout, filled in the constructor. */
	void MakeDefaultLayout();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	/** Seconds since PlayAssembly began. Meaningless unless bAssembling. */
	float AssemblyElapsed = 0.0f;

	bool bAssembling = false;
	bool bAssembled = false;
};
