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

	UPROPERTY(EditAnywhere, Category = "Finale")
	TObjectPtr<USoundBase> StageSound;

	UPROPERTY(EditAnywhere, Category = "Finale")
	TObjectPtr<USoundBase> CompleteSound;

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
	void Announce(const FString& Text, float Seconds = 6.0f) const;   // HUD banner, GEngine fallback
	FString StagePrompt() const;

	FFinaleSequence Sequence;
	bool bCompleted = false;
	bool bPlayerWasInside = false;          // edge-detect for the entry prompt

	TWeakObjectPtr<ASibeliusGameCharacter> BoundCharacter;
	FDelegateHandle PowerUsedHandle;
	FTimerHandle BindRetryHandle;
	int32 BindAttempts = 0;
};
