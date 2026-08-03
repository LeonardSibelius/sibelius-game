// DancerAgentComponent.h
//
// THE DANCING GIRLS ARE AI AGENTS (Walt, 2026-08-03).
//
//   E on a dancer -> she introduces herself:
//        "Hi. I am AI Agent Kaia. Wanna Fight? (I don't really fight, I just dance)"
//   F on a dancer -> she switches to a different Morro dance, at random.
//
// F is the FIGHT key. Pointing it at a dancer does not fight her — it reshuffles her
// dance, which is the joke the greeting sets up.
//
// ---------------------------------------------------------------------------
// WHY THE DANCES ARE HARD-REFERENCED HERE
//
// Content/Characters/Retargeting/*_MH.uasset is GITIGNORED (.gitignore:119), as is
// Content/MorroMotion (:114). Gitignored content reaches the pak ONLY by a real
// reference or by DirectoriesToAlwaysCook. A soft path (FSoftObjectPath / LoadObject
// by name) resolves fine in PIE — the editor has the whole disk — and is then MISSING
// from the shipped build. That is the v0.7.4 soft-ref miss, and SlapComponent
// (Death_Back) and RefuserController (Gideon's attack montage) both carry the same
// FObjectFinder guard for the same reason.
//
// The Dances array is a UPROPERTY on the CDO, filled by FObjectFinder at construction,
// so the cooker follows it. See docs/VENDOR_PACKS.md.
//
// ---------------------------------------------------------------------------
// HOW A DANCER GETS ONE OF THESE
//
// Two ways, and you do not have to do the editor work:
//
//   1. AUTOMATIC (default). UDancerAgentSubsystem scans the level and attaches this
//      component to any actor already playing one of the ten dances. That is what
//      makes an actor a dancer — no tag, no re-parenting, no per-actor setup. It
//      covers Kaia (who has BP_Dancer_Kaia), Nyra and Isla (who are placed MetaHuman
//      actors with NO child Blueprint), Aisling and Elise in the AI Temple, and the
//      dancer AFinaleAltar summons at the cathedral.
//
//      This matters because re-assembling a MetaHuman WIPES components added by hand.
//      Detection-by-animation survives re-assembly; a hand-added component would not.
//
//   2. MANUAL. Add the component to a dancer Blueprint if you want to override the
//      name or hand-pick her dances. A manually added one is left alone by the scan.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DancerAgentComponent.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SIBELIUSGAME_API UDancerAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDancerAgentComponent();

	/**
	 * Her name in the greeting. Left blank, it is derived from the actor's name at
	 * BeginPlay — "BP_Dancer_Kaia_C_2" becomes "Kaia".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString AgentName;

	/**
	 * Every dance she can switch to. Filled from Content/Characters/Retargeting in the
	 * constructor — the ten Morro mocap dances retargeted to the MetaHuman skeleton.
	 * Buying more from Morro on Fab means retargeting them into that folder and adding
	 * one line to DancePaths in the .cpp.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	TArray<TObjectPtr<UAnimSequence>> Dances;

	/** How long her greeting stays on screen (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.5"))
	float GreetingSeconds = 6.0f;

	/**
	 * Minimum time between dance changes. F repeats EVERY FRAME while held, so without
	 * this a brief press restarts her animation a dozen times and the dance strobes
	 * instead of changing. One press, one dance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0"))
	float ShuffleCooldown = 0.4f;

	/** E — she introduces herself on the HUD's subtitle channel. */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	void Greet();

	/**
	 * F — switch to a random DIFFERENT dance. Returns false if she has fewer than two
	 * dances or no body to play them on, which is the caller's cue to fall through to
	 * the normal fight.
	 */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	bool ShuffleDance();

	/** The prompt shown while the player is looking at her. */
	FText GetPrompt() const;

	/**
	 * Where to aim at her RIGHT NOW — the centre of her dancing mesh's bounds, which
	 * follows the pose frame by frame.
	 *
	 * Her collision capsule is a fixed 34 cm cylinder at the actor origin and does NOT
	 * move with the animation. Nyra's dance travels far enough that her body spends much
	 * of it outside her own capsule, so a camera trace missed her over and over (Walt:
	 * "I had to hit E seven times even when I moved really close"). Bounds move with her;
	 * the capsule does not.
	 */
	FVector GetAimPoint() const;

	/** True if this sequence is one of the dances we recognise (used by the scan). */
	static bool IsKnownDance(const UAnimSequence* Anim);

	virtual void BeginPlay() override;

private:
	/** The skeletal mesh actually playing the dance (a MetaHuman's "Body"). */
	USkeletalMeshComponent* FindDanceMesh() const;

	/** Index into Dances of what she is playing, so a shuffle never repeats it. */
	int32 CurrentDanceIndex = INDEX_NONE;

	/** World time of the last accepted shuffle. Starts far in the past so the first press always lands. */
	double LastShuffleTime = -1000.0;
};
