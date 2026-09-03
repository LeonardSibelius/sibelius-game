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

	/* STANDING STILL, for a guide who is past her dancing stage.

	   GS_Idle_MH — a Greystone idle already retargeted onto the MetaHuman skeleton, in the
	   same gitignored folder as the dances and hard-referenced on the CDO for the same
	   reason: a soft path resolves in PIE and is missing from the pak.

	   It is deliberately NOT one of the ten dances. IsKnownDance is what the subsystem's
	   behaviour scan tests, and an idle that counted as a dance would make every standing
	   MetaHuman in the city an AI agent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	TObjectPtr<UAnimSequence> IdleAnim;

	/* DOES REACHING A GUIDE STAGE STOP HER DANCING? No, since 2026-09-03.

	   It did for two days, and the reason it stopped is worth keeping: GS_Idle_MH is a
	   Paragon animation retargeted to a MetaHuman skeleton, the two rigs disagree about
	   finger bones, and she stood outside the deli with visibly broken hands. There is no
	   correctly retargeted neutral idle in this project, and every _MH animation in that
	   folder shares the same retarget, so there was nothing to swap to.

	   Turn this on if a good idle ever arrives. Leave it off and she dances, which costs
	   nothing, fixes the hands, and suits her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	bool bGuideStopsDancing = false;

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

	   IT NAMES A REAL PLACE NOW. The first version was a deliberate promise rather than a
	   menu, because nothing in the city was enterable and offering destinations would
	   have been a cheque the city could not cash. Then Jacob's Downtown Deli got a door,
	   so Walt rewrote her: she greets him by name, tells him where the deli is relative
	   to where she is standing, and quotes the shop's own sign - "surprisingly adequate"
	   is printed above the door in Downtown West - back at him.

	   SHE DOES NOT SAY "I am AI agent Nyra". Every dancer in the office opens that way
	   because each is handing over a power and the announcement is the point. Here she
	   is not granting anything; she is a person he has met, in a town, telling him where
	   to eat. Skipping the formula is the difference. {0} is therefore unused in this
	   string, which GetSpokenLine handles fine - the substitution simply finds nothing.

	   THE RECORDING IS THE REAL LINE. This string only reaches a player's eyes as a log
	   warning when the clip is missing, so it exists to match dancer_guide_nyra rather
	   than to be read. Change one and change the other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString GuideLine = TEXT("Hello, Leonard.  It is good to see you in Trans Human City.  Jacob's Downtown Deli is surprisingly adequate.  It is on the corner right behind me.  Have some food before you explore.");

	/* ================================================================================
	   THE GUIDE HAS STAGES NOW (Walt, 2026-09-01).

	   "When the player exits the deli let us have Nyra waiting for him outside in idle
	   pose. No more dancing."

	   Stage 0  no City.Deli grant   plaza, GuideLine — go and eat
	   Stage 1  grant claimed        outside the deli, GuideLine2 — go and build
	   Stage 2  a spaceport stands   the lawn's edge, GuideLine3 — go and provision
	   Stage 3  City.Supplies held   outside uFoods, GuideLine4 — go and board

	   STAGE 3 IS STAGE 1 AGAIN, and deliberately so. Walt's stage 2 recording ends "See
	   you there", which he then clarified means she is waiting when he comes OUT of the
	   shop — not standing inside it. That is the deli beat exactly: he goes in, he comes
	   out, she is there with the next thing.

	   Which makes it the CHEAP stage, for the reason this comment gives about stage 1:
	   leaving uFoods RELOADS L_City, so nobody is watching when she takes her place and
	   BeginPlay does all the work. No restage, no view cone, no eight-second window. The
	   marker already exists too — place_ufoods_doors.py made a PlayerStart tagged
	   uFoodsStreet for where he lands coming out, and she stands in front of it.

	   SHE DANCES THROUGH ALL THREE (Walt, 2026-09-03, reversing "No more dancing" from
	   two days earlier): "she is a dancer with endless energy, so let her dance outside
	   the deli and at the spaceport." The stage moves her and changes what she says; it
	   no longer changes what her body is doing. See bGuideStopsDancing.

	   ---------------------------------------------------------------------------------
	   STAGE 2 CAME DUE, AND SO DID THE QUESTION THIS COMMENT DEFERRED (Walt, 2026-09-03).

	   "can she appear after the spaceport appears and tell Leonard to go to uFoods to buy
	   supplies for his space voyage?"

	   The paragraph above used to end: "That decision comes back if a stage ever changes
	   with him standing there." It has. He types "spaceport" on the lawn and a launch
	   complex assembles 160 metres out — no level load, no curtain, and he is very much
	   standing there.

	   THE STAGE IS THEREFORE NO LONGER READ ONCE. GuideStage() is evaluated live and has
	   always been — FindTalkVoice, FindTalkFace and the line all call it at TALK time, so
	   the words and the recording were already correct the instant a spaceport existed.
	   Only ApplyGuideStage — where she STANDS — ran a single time at BeginPlay.

	   HOW SHE MOVES WITHOUT WALKING. L_City has no navmesh (checked: zero
	   NavMeshBoundsVolume, zero RecastNavMesh), so MoveToActor — the way ARefuser chases —
	   is not available here, and building navigation over a landscape city with a
	   runtime-spawned 120 m obstacle is not a side quest.

	   So she moves while nobody can see her, and ASpaceport::OnGeneratedFresh is the
	   perfect moment to try: it plays an EIGHT SECOND assembly of the thing he just
	   summoned, 160 metres away. His attention is not merely elsewhere, it is elsewhere by
	   construction. RestageGuide checks the player's view cone anyway and retries until
	   she is genuinely unseen.

	   AND IT NEVER FORCES. If he somehow stares at her for the whole assembly she simply
	   does not move — and that costs nothing, because she is already saying the stage 2
	   line from wherever she is. A guide in the old spot with the new words is exactly
	   what stage 1 looks like today. A guide teleporting in full view is not.
	   ================================================================================ */

	/** Stage 1: the invitation to Generate. Matches dancer_guide2_nyra — change both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString GuideLine2 = TEXT("There you are, Leonard.  Now I will show you what this city is for.  Look at the empty lawn across the street.  In your old life you would have filled it one brick at a time.  You do not do that any more.  Stand on the grass, and Generate.");

	/* WHERE SHE WAITS AT STAGE 1 — found in the level, not written down here.

	   The first draft of this was a world-space FVector set by the placement script from
	   the deli door's coordinates. That works and it is brittle: two files would hold the
	   same spot, and moving the door in the editor would leave her standing in the road.

	   The travel system already solved this. UTravelTransitionSubsystem::Travel takes an
	   ARRIVAL TAG, and a GameMode's ChoosePlayerStart matches it against PlayerStartTag —
	   which is how the carousel already drops the player at a chosen spot. So the deli's
	   return door names a tag, L_City carries a PlayerStart wearing it, and the player
	   steps out of the cafe onto that exact spot.

	   She then simply stands in front of it. One marker in the level defines both where he
	   arrives and where she is waiting, so they cannot drift apart: drag the PlayerStart
	   and the whole meeting moves with it. */
	UPROPERTY(EditAnywhere, Category="Dancer")
	FName GuideStage1StartTag = TEXT("DeliDoor");

	/** Centimetres in front of that PlayerStart. Far enough to see her, close enough to E.

	   220 put her in the deli's own shadow (Walt, 3 Sep). Stepping further along the
	   PlayerStart's forward is the way OUT of that shadow, because forward points away
	   from the building — it is the direction he is already facing when he walks out.

	   THE CEILING IS UInteractorComponent::InteractRange (450 cm). E is a camera-forward
	   line trace of that length, so past 450 she is visible and cannot be talked to —
	   a bug that looks like the dialogue being broken rather than her being too far.
	   Anything up to ~400 is safe; this leaves 130 cm of margin from the arrival spot,
	   and he can always walk toward her besides. */
	UPROPERTY(EditAnywhere, Category="Dancer", meta=(ClampMin="0", ClampMax="400"))
	float GuideStage1Distance = 320.0f;

	/** Stage 2: the supply run. Matches dancer_guide3_nyra — change both.

	    Until that clip is baked she is SILENT at stage 2, which is deliberate: the voice
	    lookup treats "not recorded yet" as an expected state, and a guide confidently
	    speaking the previous stage's words is worse than one saying nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString GuideLine3 = TEXT("Nice job, Leonard!  You built a Spaceport using your Generate power!  Next, you will be travelling on your new rocket to the planet Grok.  Before you go, you need to go down the block to the You Foods supermarket and buy supplies.  See you there.");

	/* WHERE SHE WAITS AT STAGE 2 — the same marker trick as stage 1.

	   Put a PlayerStart in L_City wearing this tag, somewhere at the lawn's edge with the
	   spaceport behind it, and she stands in front of it exactly as she stands in front
	   of the deli door. Drag the marker and the meeting moves with it.

	   IF NO SUCH MARKER EXISTS SHE SIMPLY DOES NOT MOVE. That is the graceful half of
	   this feature: stage 2 still works — new line, new voice, new face — from wherever
	   she happens to be standing. The placement is a nicety, and a missing nicety must
	   never be a broken guide. (The alternative, falling back to a default position, is
	   how three coffee cups ended up at the world origin.) */
	UPROPERTY(EditAnywhere, Category="Dancer")
	FName GuideStage2StartTag = TEXT("SpaceportLawn");

	/** Centimetres in front of the stage 2 marker. Same 450 cm ceiling as stage 1 —
	    UInteractorComponent::InteractRange is a camera-forward trace, and past it she is
	    visible and unreachable. */
	UPROPERTY(EditAnywhere, Category="Dancer", meta=(ClampMin="0", ClampMax="400"))
	float GuideStage2Distance = 320.0f;

	/** Stage 3: he has the supplies. Matches dancer_guide4_nyra — change both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dancer")
	FString GuideLine4 = TEXT("We are ready to go to Grok!  I will upload myself into the spaceship computer and I will be going with you!  You have plenty of supplies now.  Because you are part AI now, you will be able to compress your sense of time and 40 light years will go by quickly!  Go back to the spaceport and we will do the boarding procedures.");

	/* WHERE SHE WAITS AT STAGE 3 — the marker he arrives on, coming out of the shop.

	   uFoodsStreet is the PlayerStart the uFoods return door aims at, so this is the same
	   trick stage 1 uses on DeliDoor: ONE marker defines both where he lands and where
	   she is standing, and they cannot drift apart because there is only one of them. */
	UPROPERTY(EditAnywhere, Category="Dancer")
	FName GuideStage3StartTag = TEXT("uFoodsStreet");

	UPROPERTY(EditAnywhere, Category="Dancer", meta=(ClampMin="0", ClampMax="400"))
	float GuideStage3Distance = 320.0f;

	/* HOW WIDE "HE CAN SEE HER" IS, as a dot product against the camera's forward.

	   0.0 would be a literal 90-degree half-angle; 0.35 is wider than the screen, so she
	   will not move just outside the frame edge where a flick of the mouse would catch
	   her mid-teleport. Cheaper and steadier than a real frustum test, and erring wide
	   costs nothing — the worst case is she waits a little longer. */
	UPROPERTY(EditAnywhere, Category="Dancer", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float RestageViewDot = 0.35f;

	/** How often to re-check whether he has looked away, in seconds. The assembly runs
	    for 8 s, so half a second gives sixteen chances inside the window it was built for. */
	UPROPERTY(EditAnywhere, Category="Dancer", meta=(ClampMin="0.1"))
	float RestageRetrySeconds = 0.5f;

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

	/**
	 * 0 before he has been in the deli, 1 after. Always 0 for a dancer who is not a guide.
	 *
	 * One function decides the words, the recording, the face, the pose and the place, so
	 * a half-advanced Nyra standing outside the deli still inviting him to lunch is not a
	 * state this can reach.
	 */
	UFUNCTION(BlueprintPure, Category="Dancer")
	int32 GuideStage() const;

	/* SHE STOPS DANCING WITHOUT STOPPING BEING AN AGENT — the catch in this whole feature.

	   UDancerAgentSubsystem decides what an AI agent IS by watching what it does: any
	   skeletal mesh playing one of the ten Morro dances. An idle Nyra is, by that
	   definition, a statue — the scan would walk past her and she would lose E entirely.

	   The system already allows for it. The scan's first line skips any actor that
	   ALREADY has this component ("auto-adopted earlier, or added by hand"), so
	   place_city_dancers.py gives her one explicitly and she never has to dance to
	   qualify. Behaviour-detection stays exactly as it was for everybody else. */
	void ApplyGuideStage();

	/* THE STAGE CHANGED WITH HIM STANDING THERE. Called by ASpaceport when it is freshly
	   generated (through UDancerAgentSubsystem::RestageGuides, so the spaceport does not
	   need to know what a dancer is).

	   Moves her to the new stage's marker AS SOON AS THE PLAYER CANNOT SEE HER, retrying
	   every RestageRetrySeconds until that is true. It never gives up and never forces:
	   an unmoved guide is already correct, just standing somewhere older. */
	void RestageGuide();

private:
	/** True while a restage is pending, so a second trigger does not stack timers. */
	bool bRestagePending = false;

	FTimerHandle RestageTimerHandle;

	/** One retry: move if unseen, otherwise leave the timer running. */
	void TryRestageNow();

	/** Rough view-cone test against the player camera. Generous on purpose — see
	    RestageViewDot. Returns true when moving her would go unnoticed. */
	bool IsUnseenByPlayer() const;

	/** The marker tag and stand-off distance for a given stage, so ApplyGuideStage has
	    one body rather than a branch per stage. */
	void GetStageAnchor(int32 Stage, FName& OutTag, float& OutDistance) const;

public:

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

	/* ---------------------------------------------------------------------------
	   HER FACE SAYS THE WORDS (2026-09-01).

	   The long note on bTalkMouthMotion below explains why C++ cannot move a MetaHuman's
	   mouth: the jaw is a joint driven by RigLogic from control curves, and every runtime
	   write is discarded on the next evaluation. That note also names the one route that
	   works — MetaHuman Animator's audio-driven animation, which BAKES those curves into
	   an Anim Sequence in the editor. It goes THROUGH RigLogic instead of fighting it.

	   So the same ElevenLabs clip now produces two assets that ship side by side:

	       dancer_guide_nyra          the voice
	       dancer_guide_nyra_face     her face saying it

	   Same folder, same naming rule, same fallback: FindTalkFace is FindTalkVoice with
	   "_face" on the end. An agent whose face has not been baked yet simply has no
	   animation and the close-up runs exactly as it did before — no warning, because a
	   missing face performance is an expected state on a machine that has not made one,
	   the same as a missing recording.

	   HEAD MOVEMENT IS DELIBERATELY NOT BAKED IN. The export dialog offers it and it is
	   left unticked: the shot frames her at 38 degrees, and a head that turns while she
	   talks leaves the frame — the same reason TalkDanceSpeed is 0. Because the anim
	   carries no head rotation, her body still positions her head and only the face
	   muscles are driven, which is why the portrait survives. Verified on Nyra in L_City
	   before any of this was written. */

	/** Her baked face performance if one exists, else null. Silence is not a fault. */
	class UAnimSequence* FindTalkFace() const;

	/** Start her face performance on the Face mesh; remembers the mode to restore. */
	void PlayTalkFace();

	/** Put the Face mesh back on its own AnimBP, so it copies the body's pose again. */
	void StopTalkFace();

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

	/* WHAT THE FACE WAS DOING BEFORE SHE SPOKE, so it can be handed back exactly.

	   The Face mesh normally runs its own Animation Blueprint, whose graph copies the
	   body's pose — that is what keeps her head on her neck while she dances. Playing an
	   Anim Sequence on it switches the component to single-node mode and that graph stops.
	   Harmless for the length of the shot, because the body is frozen anyway
	   (TalkDanceSpeed is 0), but it must be given back or her head stops following her
	   dance for the rest of the game.

	   The MODE is read off the component rather than assumed to be AnimationBlueprint.
	   Reading it costs one line and survives somebody configuring a dancer differently;
	   assuming it is the sort of thing that works on all five agents until it doesn't. */
	bool bFacePlaying = false;

	/* The Face component's animation mode before the performance took it over, held as a
	   raw uint8 and cast back in the .cpp. EAnimationMode lives in SkeletalMeshComponent.h
	   and this header goes out of its way to forward-declare USkeletalMeshComponent rather
	   than pull that in; one stored byte is not worth breaking that. 0 is
	   AnimationBlueprint, which is also the value we would guess — but it is READ off the
	   component, never assumed. */
	uint8 SavedFaceMode = 0;

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
