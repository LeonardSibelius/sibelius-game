// FinaleAltar.h
//
// FUN-6 (docs/FUN_PLAN.md Step 6) — the Ch7 Synthesis: the cathedral's finale.
// The altar asks for each of the six power verbs IN ORDER (enum order = the
// chapters' order); the player demonstrates each within the altar's radius.
// The character's OnPowerVerbUsed delegate (the gated-input chokepoint) is the
// signal, so a locked verb can never advance the rite — the metroidvania
// promise cashes out here. Completing the Synthesis:
//   - destroys every actor tagged WallTag (the last Mrs. Hall wall),
//   - pays SauceReward once across sessions (claim key GrantKey),
//   - summons a dancer (DancerClass) to the apse, who dances on arrival,
//   - leaves the way to the Celestial Fortune coda open.
// Already-claimed on BeginPlay -> the walls drop immediately (a finished finale
// never re-locks the cabinet).
//
// FFinaleSequence is the pure stage machine, headless-gated by FinaleSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "ProgressionTypes.h"
#include "FinaleAltar.generated.h"

class UStaticMeshComponent;
class USoundBase;
class ASibeliusGameCharacter;

// Pure: the six verbs in enum order, advanced only by the exact expected verb.
USTRUCT()
struct SIBELIUSGAME_API FFinaleSequence
{
	GENERATED_BODY()

	int32 StageIndex = 0;   // 0-based; == Num() when complete

	static int32 Num() { return static_cast<int32>(EPowerVerb::Count); }
	bool IsComplete() const { return StageIndex >= Num(); }
	EPowerVerb CurrentVerb() const { return static_cast<EPowerVerb>(FMath::Clamp(StageIndex, 0, Num() - 1)); }

	// Advance only on the expected verb; anything else (or after completion)
	// changes nothing. Returns true when the stage advanced.
	bool Submit(EPowerVerb Verb)
	{
		if (IsComplete() || Verb != CurrentVerb())
		{
			return false;
		}
		++StageIndex;
		return true;
	}
};

UCLASS()
class SIBELIUSGAME_API AFinaleAltar : public AActor
{
	GENERATED_BODY()

public:
	AFinaleAltar();

	virtual void Tick(float DeltaSeconds) override;

	// Verbs only count inside this radius of the altar (cm).
	UPROPERTY(EditAnywhere, Category = "Finale", meta = (ClampMin = "100"))
	float ActivationRadius = 900.0f;

	// Actors carrying this tag fall when the Synthesis completes.
	UPROPERTY(EditAnywhere, Category = "Finale")
	FName WallTag = TEXT("FinaleWall");

	// One-time completion reward + its claim key in the progression save.
	UPROPERTY(EditAnywhere, Category = "Finale", meta = (ClampMin = "0"))
	int32 SauceReward = 200;

	UPROPERTY(EditAnywhere, Category = "Finale")
	FName GrantKey = TEXT("Finale.Synthesis");

	// Key hint shown in the rite's prompt, per verb ("show me DEPLOY  [0]").
	// Without this the altar names a verb the player cannot map to a key: the six
	// powers sit on V / B / 6 / 0 / G and one Enhanced Input binding, deliberately
	// dodging the editor's gizmo keys 1-5. Walt hunted for Deploy by pressing every
	// number key, and 6/7/8 are live branch ops — he ended up two branches deep and
	// the world went monochrome mid-rite.
	//
	// Defaults cover the keys that are hard-bound in C++ or named in the input
	// comments. Refactor's lives in IA_Refactor/IMC_Default and is NOT readable
	// from C++, so it ships empty — fill it in on the placed altar. An empty or
	// missing entry simply prints no hint, so a blank is safe and a wrong guess
	// is a one-field edit rather than a rebuild.
	UPROPERTY(EditAnywhere, Category = "Finale")
	TMap<EPowerVerb, FString> VerbKeyHints;

	UPROPERTY(EditAnywhere, Category = "Finale")
	TObjectPtr<USoundBase> StageSound;

	UPROPERTY(EditAnywhere, Category = "Finale")
	TObjectPtr<USoundBase> CompleteSound;

	// --- The dancer's reward (see SummonDancer) ---------------------------------
	// Assign a dancer Blueprint (BP_MHC_*) on the PLACED altar in L_Cathedral, not
	// on the class default. The level then hard-references her -> she cooks by
	// reference, the same path the office dancers take. A soft path here would
	// pass in PIE and be missing from the shipped pak.
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer")
	TSubclassOf<AActor> DancerClass;

	// Where she appears, relative to the altar and rotated into its facing (cm).
	// Her origin is at her FEET (assembled MetaHumans are AActor-derived), so Z 0
	// puts her on the altar's own floor plane.
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer")
	FVector DancerSpawnOffset = FVector(250.0f, 0.0f, 0.0f);

	// Yaw relative to the altar. 180 turns her back to face it (and the player).
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer")
	float DancerSpawnYaw = 180.0f;

	// EASIEST OPTION: drop any actor (an empty Actor, a Target Point) in the level
	// exactly where you want her, and point this at it. Its transform wins and both
	// Offset and Yaw above are ignored — no guessing coordinates.
	UPROPERTY(EditInstanceOnly, Category = "Finale|Dancer")
	TObjectPtr<AActor> DancerSpawnPoint;

	// Trace down and stand her on the floor.
	// Unreal's spawn-collision handling CANNOT do this for her: an assembled
	// MetaHuman's root is a bare SceneComponent with the capsule as a child, so
	// there is no root collision to test and nothing gets adjusted. Her origin is
	// at her feet, so without this she is planted at whatever Z we name — which is
	// how she ended up buried to the neck.
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer")
	bool bSnapDancerToFloor = true;

	// Start the trace this far ABOVE the chosen point (clears a low step).
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer", meta = (EditCondition = "bSnapDancerToFloor", ClampMin = "0"))
	float FloorTraceHeight = 300.0f;

	// ...and search this far below it.
	UPROPERTY(EditAnywhere, Category = "Finale|Dancer", meta = (EditCondition = "bSnapDancerToFloor", ClampMin = "0"))
	float FloorTraceDepth = 2000.0f;

	UPROPERTY(VisibleAnywhere, Category = "Finale")
	TObjectPtr<USceneComponent> SceneRoot;

	// Assigned in editor (any altar-ish mesh reads fine).
	UPROPERTY(VisibleAnywhere, Category = "Finale")
	TObjectPtr<UStaticMeshComponent> Mesh;

	bool IsSynthesisComplete() const { return bCompleted; }
	int32 GetStageIndex() const { return Sequence.StageIndex; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void TryBindToPlayer();                 // retry until the pawn exists (machine's pattern)
	void HandlePowerUsed(EPowerVerb Verb);
	void CompleteSynthesis(bool bAlreadyClaimed);
	void DropWalls();
	void SummonDancer();                    // no-op if DancerClass is unset
	void Announce(const FString& Text, float Seconds = 6.0f) const;   // HUD banner, GEngine fallback
	FString StagePrompt() const;

	FFinaleSequence Sequence;
	bool bCompleted = false;
	bool bPlayerWasInside = false;          // edge-detect for the entry prompt

	TWeakObjectPtr<AActor> SpawnedDancer;    // guards against a second summoning

	TWeakObjectPtr<ASibeliusGameCharacter> BoundCharacter;
	FDelegateHandle PowerUsedHandle;
	FTimerHandle BindRetryHandle;
	int32 BindAttempts = 0;
};
