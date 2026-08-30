// DancerAgentComponent.h
//
// THE DANCING GIRLS ARE AI AGENTS (Walt, 2026-08-03).
//
//   E on a dancer who still has a power -> the slot trial (SPINE). She is the way in.
//   E on a dancer with nothing left to give -> talk close-up:
//        camera zooms her MetaHuman face, she faces you, the dance drops to a
//        crawl, and THE WHOLE HUD GOES DARK while she SPEAKS the line aloud.
//        The Face mesh is left as MetaHuman assembled it. Driving it from C++
//        wrecked the portrait once; leave the rig alone.
//        NOTE (2026-08-25): it is NOT a Leader Pose follower — measured, not
//        assumed: "leaderpose=no" on Kaia. MetaHuman nulls the leader pose when
//        it runs the instance post-process AnimBP, and syncs with Copy Pose From
//        Mesh instead. That is the ANIMATED-face configuration, which means a
//        facial anim sequence could be played on it. See docs/DANCER_VOICE.md.
//        Actor yaw is restored when the shot ends.
//        "I have granted you a power.  Use it wisely."
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
class USoundWave;
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
	 * WHAT SHE SAYS (Walt, 2026-08-25). Every agent says the same line, because every
	 * agent hands over the same kind of thing: a capability the player was not supposed
	 * to have. It is the premise of the game said out loud, once, by the one who did it.
	 *
	 * SPOKEN, NOT WRITTEN. The old greeting was a HUD subtitle across the middle of the
	 * screen — which, in a 38-degree portrait, sat exactly on her mouth. The text is
	 * gone; this string now only names the recording and shows up in the log when the
	 * recording is missing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString TalkLine = TEXT("I am AI agent {0}.  I have granted you a power.  Use it wisely.");

	/* THE GUIDE LINE - what she says when she is not handing over a power.

	   Walt, 2026-08-30: "I want Nyra to be the player's date in the city who shows him
	   around. When you click E on Nyra, she will tell you about destinations there, what
	   you can do together etc."

	   The office dancers give you a capability and say so. In the city nothing is being
	   granted - the powers are all won by then - so the same line would be nonsense. She
	   is there because the man has arrived somewhere new and has nobody to ask.

	   This is deliberately a PROMISE and not a menu. Destinations, things to do together
	   and the rest are not built; a line that offered them today would be writing a
	   cheque the city cannot cash. "Ask me where to go" is true the moment there is
	   somewhere to go, and until then it reads as an invitation rather than a bug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString GuideLine = TEXT("I am AI agent {0}.  I know this city.  Ask me where to go, and I will take you there.");

	/* WHICH DANCERS ARE GUIDES: the ones carrying this ACTOR TAG.

	   Not a level-name check and not a per-instance property, for the two reasons this
	   whole class already exists. A level-name check would be a hardcoded string that
	   silently stops matching the day a map is renamed; a per-instance property cannot be
	   set at all, because UDancerAgentSubsystem ATTACHES this component at runtime and
	   there is nothing in the editor to tick.

	   The tag is already there - place_city_dancers.py stamps CityDancer on everyone it
	   stands on the street, and a tag is real data that cooks, unlike a label. So the
	   same Nyra Blueprint gives a power in the office and gives directions in the city,
	   with nothing to configure and nothing to keep in sync. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FName GuideTag = TEXT("CityDancer");

	/** True when the owning actor carries GuideTag - she guides rather than grants. */
	UFUNCTION(BlueprintPure, Category="Dancer")
	bool IsGuide() const;

	/** Beat of silence held on her face after the voice clip ends, before the camera lets go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0"))
	float TalkTailSeconds = 1.4f;

	/**
	 * Dance speed while she talks, as a fraction of normal. 0 freezes her outright.
	 *
	 * ZERO, AND IT SHOULD STAY ZERO (Walt, 2026-08-25). This shipped at 0.1 for one
	 * afternoon on the theory that a frozen dancer reads as a waxwork. She does — but a
	 * dancer who keeps dancing swings her head clean out of a 38-degree frame, and no
	 * amount of camera tracking saves a portrait whose subject is mid-pirouette. Walt:
	 * "I was hoping to keep a steady focus on their faces."
	 *
	 * The life in the shot comes from her MOUTH now (bTalkMouthMotion), which is where
	 * it belongs while she is speaking. Raise this only if you want the old wander back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0", ClampMax="1.0"))
	float TalkDanceSpeed = 0.0f;

	/**
	 * Move her lips while the voice clip plays.
	 *
	 * WHAT IS AND IS NOT REACHABLE ON A METAHUMAN FACE. Her jaw is a JOINT, driven by
	 * RigLogic inside ABP_Face_PostProcess from control curves that C++ cannot write at
	 * runtime — SetMorphTarget feeds MorphTargetCurves (blend shapes only) and
	 * OverrideCurveValue writes the post-evaluation map, which is discarded next frame.
	 * Both verified in UE 5.7 engine source. So there is no jaw drop, and there is no
	 * true phoneme lip sync, without the MetaHuman Animator route (docs/DANCER_VOICE.md).
	 *
	 * What IS reachable is the LIP layer. SKM_MHC_Kaia_FaceMesh carries 858 morph
	 * targets, among them mouth_lowerLipDepress, mouth_upperLipRaise, mouth_funnel and
	 * mouth_stretch — real blend shapes, and USkeletalMeshComponent::UpdateFollowerComponent
	 * explicitly appends a FOLLOWER's own MorphTargetCurves ("if follower also has it,
	 * add it here"), so these apply even though the Face is a Leader Pose follower.
	 * Nothing here touches the animation mode, the leader pose, or the post-process ABP,
	 * which is exactly why it cannot wreck the portrait the way earlier attempts did.
	 *
	 * Her lips part and purse in time with the actual audio envelope. It is a puppet
	 * mouth, not a phoneme solve — but at 48 cm it reads as speech.
	 */
	/**
	 * OFF, AND IT SHOULD STAY OFF UNTIL THE RIG CHANGES (2026-08-25).
	 *
	 * The code below works. Measured, on Kaia: 563 updates across one close-up, mouth
	 * driven to a peak of 0.67 by the real audio envelope, on a face whose morph target
	 * resolves (shape=FOUND, 858 morphs). Her mouth did not move a millimetre.
	 *
	 * The reason is in USkeletalMeshComponent::PostAnimEvaluation, in this order:
	 *
	 *     UpdateMorphTargetOverrideCurves();              // our SetMorphTarget lands here
	 *     if (PostProcessAnimInstance) { ...
	 *         PostProcessAnimInstance->UpdateCurvesPostEvaluation();   // RigLogic writes
	 *
	 * RigLogic owns every blendshape weight on that mesh and REPLACES the array rather
	 * than merging with it. Our value is applied and overwritten one line later, every
	 * frame. No amount of scaling, smoothing or curve-picking changes that; the write
	 * simply happens after ours.
	 *
	 * Layering on top of RigLogic means being INSIDE the anim graph, after its node —
	 * editing ABP_Face_PostProcess, which is the rig this project deliberately does not
	 * touch (v0.9.7.2: "C++ Control Rig and Flite TTS wrecked the portrait").
	 *
	 * Kept, not deleted, because it costs nothing switched off and it is the working half
	 * of a feature whose other half is a rig change. The real route to a moving mouth is
	 * MetaHuman Animator's audio-driven animation — see docs/DANCER_VOICE.md.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	bool bTalkMouthMotion = false;

	/** How far the lips open at the loudest part of the line. 1 = the full blend shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MouthOpenScale = 0.85f;

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

	/**
	 * Speak TalkLine. Returns the clip's length in seconds, or 0 if there is no clip yet.
	 *
	 * SOFT-LOADED BY PATH, exactly like Mrs. Hall's refusals: the voice is AI-generated
	 * (ElevenLabs), so it is gitignored and local-only, and a machine that has not got
	 * the file yet must still run the close-up rather than fail. Silence plus one
	 * Warning naming the file it wanted — never a soft-lock (the apparition's AP3).
	 *
	 * Gitignored content reaches a shipped pak ONLY via DirectoriesToAlwaysCook —
	 * /Game/Audio/Dancers is listed in DefaultGame.ini for the same reason MrsHall is.
	 * See docs/DANCER_VOICE.md.
	 */
	float PlayTalkVoice();
	void StopTalkVoice();

	/** Per-agent take if she has one, else the take every agent shares. */
	class USoundBase* FindTalkVoice() const;

	/** TalkLine with {0} replaced by her name — what the per-agent recording says. */
	FString GetSpokenLine() const;

	/** The local HUD, when it is ours. Null in a level with a different HUD class. */
	class ASibeliusHUD* GetSibeliusHUD() const;

	/**
	 * THE SHOT'S HEARTBEAT — a looping TIMER, not the component tick.
	 *
	 * TickComponent never runs on this component and three separate fixes failed to make
	 * it: bStartWithTickEnabled, always-on ticking, and bAutoActivate. The last attempt
	 * logged "tickEnabled=yes active=yes ... 0 mouth tick(s)" — registered, enabled,
	 * active, and never called. Something about a component NewObject'd onto an already
	 * live MetaHuman actor keeps it out of the level's tick list.
	 *
	 * So stop fighting it. FTimerManager is already proven in this class — GreetingTimer
	 * has always fired correctly — and a 60 Hz timer is entirely adequate for following a
	 * head with a camera and opening a mouth. Started in BeginTalkShot, cleared in
	 * EndTalkShot, so it only runs during a close-up.
	 */
	void TalkTick();

	/** Drive the lip blend shapes from the voice. Called from TalkTick during the shot. */
	void UpdateMouth(float DeltaTime);

	/**
	 * Open = how far the lips part, Shape = 0 wide/stretched .. 1 rounded/funnelled.
	 * Passing Open = 0 clears every shape back off her face.
	 */
	void SetMouthShapes(float Open, float Shape);

	/**
	 * Envelope of the voice, straight from the audio renderer, so the mouth is driven by
	 * the ACTUAL waveform rather than a blind oscillator. Bound BEFORE Play() — the
	 * audio component only computes an envelope if the delegate is already bound when
	 * the sound starts (AudioComponent.cpp: bUpdateSingleEnvelopeValue), which is why
	 * the voice is created with CreateSound2D and played by hand.
	 */
	UFUNCTION()
	void HandleVoiceEnvelope(const USoundWave* PlayingSoundWave, const float EnvelopeValue);

	/** Pin the face to LOD0 for the close-up: best face, and the lod0 morph names apply. */
	void ForceFaceLOD(bool bForce);

	/**
	 * One line naming every link in the mouth chain. Called from BeginTalkShot, NOT from
	 * UpdateMouth — the first attempt logged from inside UpdateMouth and produced no
	 * output at all, which told us nothing except that the function had not run. A
	 * diagnostic that only fires on the path you are trying to prove is worthless.
	 */
	void LogMouthDiag();

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

	/** Her voice, while it is playing. Stopped when the shot ends so F cuts her off mid-word. */
	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> TalkAudio;

	/**
	 * How long THIS close-up holds — GreetingSeconds, or the voice clip plus its tail if
	 * that is longer. A three-second line and a nine-second line should not both get
	 * seven seconds of camera.
	 */
	float TalkHoldSeconds = 0.0f;

	/** Seconds since the line started; drives the syllable and vowel oscillators. */
	float MouthTime = 0.0f;

	/** Smoothed lip opening, 0..1. Interpolated so the mouth never snaps. */
	float MouthOpen = 0.0f;

	/** Latest envelope value from the audio renderer, 0..1. */
	float VoiceEnvelope = 0.0f;

	/**
	 * True once a single envelope value has arrived. Until then the mouth runs on the
	 * fallback oscillator, so a build where the delegate never fires still moves her
	 * lips rather than leaving her mouthing nothing.
	 */
	bool bVoiceEnvelopeSeen = false;

	bool bFaceLODForced = false;

	/**
	 * One mouth report per close-up. The lips are driven through a chain with four
	 * places to fail silently (no Face component, no morph target of that name on her
	 * mesh, no audio playing, no envelope), and every one of them looks identical from
	 * the player's chair: a still mouth. This logs which link is broken.
	 */
	bool bMouthDiagLogged = false;

	/** Ticks counted during this shot, and the loudest the mouth got. Both reported when
	 *  the shot ends: 0 ticks means the component never ticked, which is a different bug
	 *  from a mouth that ticked and stayed shut. */
	int32 MouthTickCount = 0;
	float MouthPeak = 0.0f;

	/** The close-up heartbeat. See TalkTick. */
	FTimerHandle TalkTickTimer;

	/** World time of the previous TalkTick, for a real delta rather than the nominal rate. */
	double LastTalkTickTime = 0.0;

	/** Set if TickComponent ever fires. If this turns true, the timer can go away. */
	bool bComponentTickObserved = false;

	TWeakObjectPtr<AActor> SavedViewTarget;

	/** Player-camera location at the moment E was pressed. The close-up stands here, not on a bone axis. */
	FVector TalkPlayerEye = FVector::ZeroVector;

	FRotator SavedActorRotation = FRotator::ZeroRotator;
	bool bSavedActorRotation = false;

	bool bTalkShotActive = false;
	bool bTalkInputLocked = false;
};
