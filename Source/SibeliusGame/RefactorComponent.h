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
class UMaterialInterface;
class UStaticMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
class UPoseableMeshComponent;
class UCapsuleComponent;
class UDancerAgentComponent;
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

	/* ...BUT THE INDEX IS ONLY A FIRST GUESS. Etherion's foliage influencer, which Kaia's
	   home hangs on him so her mushrooms answer him, swaps the plants near him for live
	   physics copies by REMOVING their instances and adding them back at the end of the field
	   when he walks away - renumbering the field, and taking the shrunk plant with it the
	   moment he stands at the treasure to turn it back. So restore looks for the plant where
	   it was left: at this transform. */
	UPROPERTY() FTransform HiddenInstanceTransform;
};

/* One shape R can give an agent (UAgentStandIn): its meshes, the body first (Echo's hair is
   a second one). Soft, so a hero is loaded the first time R picks him rather than with every
   level - and so his folder has to be named in DefaultGame.ini to be cooked at all. */
USTRUCT()
struct FAgentShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Refactor|Agents")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Refactor|Agents")
	TArray<TSoftObjectPtr<USkeletalMesh>> Meshes;

	/** Material slots of the body left undrawn. A Paragon hero's cloth (his "APEX" slots,
	 *  Unreal 4's cloth simulation) swings only on a simulated skeletal mesh; posed from code
	 *  it hangs stiff at rest - Walt, 2026-10-13: "i wish i could get rid of those stiff
	 *  purple things hanging off of Shinbi's elbows". */
	UPROPERTY(EditAnywhere, Category = "Refactor|Agents")
	TArray<FName> HiddenSlots;
};

/* R ON AN AGENT (Walt, 2026-10-13: "i don't suppose that the agents can be refactored into
   other human characters and still have their dance animations?" - "try it with Manny
   first" - then "this can cycle through Manny, Echo, Aurora, Shinbi and Yin"). Her MetaHuman
   is hidden, not changed, and the shape's meshes are attached to her body and dance whatever
   she dances - the F menu included. Two ways, picked by the shape's skeleton:

   FOLLOWING (Manny, Echo): a Leader Pose follower takes her body's bones by NAME, every frame.
   Their skeletons are hers by name and their rest poses match hers within 2 degrees
   (measured: Manny lacks only nine IK and helper bones; Echo's scarf and face bones, which
   she lacks, ride their parents at rest).

   RETARGETED (Aurora, Shinbi, Yin): a Paragon hero has three spine bones to her five, longer
   legs and lower forearms, and followed by name his chest would sink to her waist. So a
   posable mesh is driven from code: each of his bones turns as hers turns (his spine_02 and
   spine_03 as her spine_03 and spine_05), from a rest pose with his arms, legs and fingers
   first lined up with hers, while he keeps his own bone lengths; his hips travel as hers do,
   scaled by the height of his. His own extra bones (weapons, whips, face) ride their parents.

   Lives on HER actor; R moves her on to the next shape and after the last takes it off and
   puts every hidden mesh back. Her chat, her voice and her powers are untouched: she is still
   herself, in another shape. */
UCLASS()
class SIBELIUSGAME_API UAgentStandIn : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentStandIn();

	// Hides everything drawn under Body (her body, face, clothes, grooms) and puts Meshes
	// there. Index is the shape's place in URefactorComponent::AgentShapes.
	void PutOn(USkeletalMeshComponent* Body, TArrayView<USkeletalMesh* const> Meshes, int32 Index, const FString& Name,
		TArrayView<const FName> HiddenSlots = {});
	// Back to herself: the stand-in goes, the hidden meshes come back.
	void TakeOff();

	int32 GetShapeIndex() const { return ShapeIndex; }
	const FString& GetShapeName() const { return ShapeName; }

	/** Where the body he can see on Her is, while she wears a shape: false if she wears none.
	 *  A Paragon hero keeps his own height, a head taller than the hidden body she is still
	 *  found by, and his hips travel further than hers, so aiming at him missed her and F
	 *  opened the next agent's dance menu (Walt, 2026-10-13: "refactored to Shinbi and "F"
	 *  does not work" - it was Isla's). His chest, from this frame's pose: a posed mesh's
	 *  bounds are its rest shape at her feet and never follow the dance. False for a follower
	 *  (Manny, Echo), who stands in her own bones, so her own aim point is his.
	 *  UDancerAgentComponent::GetAimPoint aims here. */
	static bool AimPointOf(const AActor* Her, FVector& OutPoint);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// The retarget's fixed part, worked out once from the two rest poses (see the class note).
	void BuildRetarget();

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Leader;
	UPROPERTY() TArray<TObjectPtr<USkinnedMeshComponent>> Parts;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Posed;   // the retargeted body, if any
	// ...and a capsule his height that only the interaction trace sees: a posed mesh has no
	// collision, and her own capsule stops at her height, so the ray at his head went past
	// her to whoever stood behind.
	UPROPERTY() TObjectPtr<UCapsuleComponent> Reach;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> Hidden;

	// A hidden leader stops posing unless told to keep going; put back as it was.
	uint8 SavedTickOption = 0;
	int32 ShapeIndex = INDEX_NONE;
	FString ShapeName;

	// Per bone of Posed's skeleton: her bone it turns with (INDEX_NONE: rides its parent at
	// rest), the rotation from hers to his, whether it is placed by her hips' travel, and
	// his rest pose; plus where her bone stood at rest.
	TArray<int32> SourceBone;
	TArray<FQuat> RotationOffset;
	TArray<bool> bFromHips;
	TArray<FVector> SourceRestLocation;
	TArray<FTransform> RestLocal;
	TArray<FTransform> RestGlobal;
	TArray<FTransform> Global;   // this frame's, component space
	float HipScale = 1.f;
	int32 HipsBone = INDEX_NONE;    // his pelvis: Reach stands over it
	int32 ChestBone = INDEX_NONE;   // his top spine bone: what F and E aim at
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

	/** R on an AI agent only: her next shape (CycleStandIn), if she is what he is aiming at
	 *  and no authored refactorable is under the crosshair. The character calls it BEFORE the
	 *  power gate - an agent is fair game from the first minute (Walt, 2026-10-13: "Yes, from
	 *  the start"), while everything else R does waits for Refactor to be his. True if it was
	 *  an agent (paid for or not). */
	bool TryAgentStandIn();

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

	/* THE HOARD, named one by one and hard-referenced from the constructor, so it cooks via
	   the CDO with no DirectoriesToAlwaysCook - and only these, not whole packs. Bought
	   packs, gitignored like every one: pick by name, Walt judges the look. Two piles drawn
	   half each: half the treasure is gems, half chalices.

	   THE GEMS ARE GEMSHADER'S (Walt added the pack 2026-10-13: "I want to use these for
	   Refactor instead of the ugly blurry coins and gems"). Three cuts, each 2 m across and
	   about 1.2 m tall, point down and table up like a real diamond, so they stand as made -
	   Dekogon's lay flat and had to be stood on edge, and its coin with them, both gone now.
	   Each gem gets one of GemColors. They ship with convex collision, so they need no
	   prepare_hoard.py; BeginPlay still drops any mesh without simple collision. */
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> Gems;

	// One of these on every gem: GemShader's blue, green, mixed, pink, red and white.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UMaterialInterface>> GemColors;

	// As made, front to him: the two chalices from Dekogon's "Treasures VOL.1 - Ancient
	// Treasures" (Content/AncientTreasures, "Allows usage with AI: No"), all Walt kept of it
	// 2026-10-13 ("keep the dekogon chalices, but drop the gold bar and chests"). Its meshes
	// ship with no simple collision; Tools/Scripts/prepare_hoard.py gives each a box.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> Chalices;

	// Who an agent becomes under R, in turn (see UAgentStandIn): Manny, Echo, Aurora, Shinbi
	// and Yin, then herself again.
	UPROPERTY(EditAnywhere, Category = "Refactor|Agents")
	TArray<FAgentShape> AgentShapes;

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

	/** When he cannot afford it; {0} becomes the cost, {1} what he has. The hint is where sauce
	 *  is (Walt, 2026-10-13: "can a hint be added ... "(find the Temple)"?"), now that R on an
	 *  agent works from the first minute, when he has none. */
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	FString TooPoorLine = TEXT("REFACTOR NEEDS {0} SAUCE — YOU HAVE {1} (FIND THE TEMPLE)");

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
	// The agent he is aiming at, found the way F finds her: the interactor's focus, then the
	// pawn sweep, then the first thing the thin ray meets. Null if none.
	UDancerAgentComponent* FindAgentInAim() const;

	// R on an agent: her next shape, or herself after the last. True if Agent was one (even
	// when he could not pay - the press was for her, and nothing behind her should turn to gold).
	bool CycleStandIn(UDancerAgentComponent* Agent);

	// A treasure mesh, and for a gem its colour (null: the mesh's own material).
	UStaticMesh* PickTreasure(UMaterialInterface*& OutColor) const;

	// Treasure the size of MatchSize, standing on the ground under Space (what it replaces,
	// which the ground search looks past), facing him and clear of him.
	AActor* SpawnTreasureActor(UStaticMesh* TreasureMesh, UMaterialInterface* Color, const FBox& Space, float MatchSize,
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
