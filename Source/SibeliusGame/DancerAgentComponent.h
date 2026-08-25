// DancerAgentComponent.h
//
// THE DANCING GIRLS ARE AI AGENTS (Walt, 2026-08-03).
//
//   E on a dancer who still has a power -> the slot trial (SPINE). She is the way in.
//   E on a dancer with nothing left to give -> talk close-up:
//        camera zooms her MetaHuman face, the dance pauses, she faces you,
//        HUD line is up. The Face mesh is left as MetaHuman assembled it
//        (Leader Pose + ABP_Face_PostProcess). Driving it from C++ wrecked
//        the portrait. Actor yaw is restored when the shot ends.
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
#include "ProgressionTypes.h"   // EPowerVerb — she can be the way a power is obtained
#include "DancerAgentComponent.generated.h"

class ACameraActor;
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

	/** How long her greeting stays on screen (seconds). Also how long the talk close-up holds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.5"))
	float GreetingSeconds = 7.0f;

	/**
	 * Kept cooked (CDO hard-ref) but talk-E no longer plays it. A victory wave
	 * reads as "she scored", not "she is speaking to you".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	TObjectPtr<UAnimSequence> GreetingAnim;

	/** Distance from her face to the talk camera, centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="20.0"))
	float TalkCameraDistance = 48.0f;

	/** Portrait FOV for the talk close-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="20.0"))
	float TalkCameraFOV = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0"))
	float TalkCameraBlendIn = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0"))
	float TalkCameraBlendOut = 0.35f;

	/**
	 * Minimum time between dance changes. F repeats EVERY FRAME while held, so without
	 * this a brief press restarts her animation a dozen times and the dance strobes
	 * instead of changing. One press, one dance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0"))
	float ShuffleCooldown = 0.4f;

	/** E — she introduces herself; talk-E also zooms her face. */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	void Greet();

	/**
	 * This agent hands over a power (docs/SPINE.md). Called by APowerGrant::BindToAgent —
	 * the grant keeps the trial, the stake, the claim and the Refuser alarm; she is only
	 * the way it is asked for.
	 *
	 * The link is set on the GRANT, not here, because this component is attached at
	 * runtime by DancerAgentSubsystem's scan and never exists in the level to be edited.
	 */
	void SetPowerGrant(class APowerGrant* InGrant, EPowerVerb InVerb);

	/** True while she still has an unclaimed power to give. */
	bool HasPowerToGive() const;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** The power she hands over, if any — set by APowerGrant::BindToAgent, never in the
	 *  editor (this component is attached at runtime by a scan). */
	UPROPERTY()
	TObjectPtr<class APowerGrant> PowerGrant;

	EPowerVerb PowerVerb = EPowerVerb::Refactor;

	/** The skeletal mesh actually playing the dance (a MetaHuman's "Body"). */
	USkeletalMeshComponent* FindDanceMesh() const;

	/** MetaHuman Face mesh (Leader Pose follower). Morphs live here, not on Body. */
	USkeletalMeshComponent* FindFaceMesh() const;

	/** Horizontal facing of the face: flattened (nose − eyes). */
	FVector GetFaceForward() const;

	/** Midpoint of the eyes, else the Face bounds. */
	FVector GetEyeCenter() const;

	/** Eyes with a little mouth so the shot is a talking portrait, not a forehead. */
	FVector GetTalkLookAt() const;

	/** Horizontal direction from her eyes toward where the player was standing. */
	FVector GetTalkCamDir() const;

	void FaceThePlayer();
	void TeleportOwnerYaw(const FRotator& Rotation);
	void FreezeGrooms(bool bFreeze);

	/** Zoom a camera onto her face. Skipped when E is opening the slot trial. */
	void BeginTalkShot();
	void UpdateTalkShot();
	void EndTalkShot();
	void LockTalkInput(bool bLock);

	/** Pause the dance while she talks. ResumeAfterGreeting puts her back. */
	void BeginGreetingMotion();
	void ResumeAfterGreeting();

	/** CDO can be stale after a hot-reload; load the bow if the pointer is still empty. */
	void EnsureGreetingAnim();

	/**
	 * Drop the greeting pose. bResumeDance = true puts the saved dance back;
	 * false is for ShuffleDance, which is about to play something else.
	 */
	void CancelGreeting(bool bResumeDance);

	/** Index into Dances of what she is playing, so a shuffle never repeats it. */
	int32 CurrentDanceIndex = INDEX_NONE;

	/** World time of the last accepted shuffle. Starts far in the past so the first press always lands. */
	double LastShuffleTime = -1000.0;

	/** True while the talk/greet hold is up — extra E refreshes the line, F cancels. */
	bool bGreeting = false;

	/** The dance she was on when E was pressed, so resume is the same move. */
	TObjectPtr<UAnimSequence> SavedDance;

	/** Playback time inside SavedDance. Pause keeps this; a wave replaces it then we seek back. */
	float SavedDanceTime = 0.0f;

	FTimerHandle GreetingTimer;

	/** Spawned for the talk close-up; destroyed when the greeting ends. */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> TalkCamera;

	TWeakObjectPtr<AActor> SavedViewTarget;

	/** Player-camera location at the moment E was pressed. The close-up stands here, not on a bone axis. */
	FVector TalkPlayerEye = FVector::ZeroVector;

	FRotator SavedActorRotation = FRotator::ZeroRotator;
	bool bSavedActorRotation = false;

	bool bTalkShotActive = false;
	bool bTalkInputLocked = false;
};
