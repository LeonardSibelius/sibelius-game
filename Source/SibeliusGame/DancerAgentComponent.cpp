#include "ProteinMachine.h"
// DancerAgentComponent.cpp — see header.

#include "DancerAgentComponent.h"

#include "SibeliusHUD.h"
#include "SibeliusGame.h"                  // LogSibeliusGame
#include "DancerAgentSubsystem.h"          // the aim-assist registry
#include "PowerGrant.h"                    // SPINE: she hands the power over
#include "SibeliusGameCharacter.h"         // HasVisitedDeli - the guide's stage gate
#include "ProgressionSubsystem.h"          // the Grok conversation is recorded as a grant
#include "Spaceport.h"                     // stage 2 asks the world whether one stands
#include "SupplyCounter.h"                 // stage 3 asks the save whether he shopped
#include "TimerManager.h"                  // the restage retry, waiting for him to look away

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomComponent.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"   // the PlayerStart's capsule - she stands on the floor
#include "EngineUtils.h"                   // TActorIterator - finding her stage-1 marker
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerStart.h"     // the tagged arrival spot she waits beside
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"                     // FApp::CanEverRenderAudio - the gates run silent
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * The ten Morro mocap dances, retargeted to the MetaHuman skeleton. The raw
	 * MorroMotion assets are on Morro's OWN skeleton and cannot be played on a
	 * MetaHuman — always use the _MH retargets in Content/Characters/Retargeting.
	 *
	 * Buying more dances from Morro: retarget into that folder and add the line here.
	 * Adding it here is what makes it cook (the folder is gitignored).
	 */
	const TCHAR* const DancePaths[] =
	{
		TEXT("/Game/Characters/Retargeting/Anim_High_Rhythm_Dance_13_MH.Anim_High_Rhythm_Dance_13_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_High_Rhythm_Dance_14_MH.Anim_High_Rhythm_Dance_14_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_10_MH.Anim_Mid_Rhythm_Dance_10_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_11_MH.Anim_Mid_Rhythm_Dance_11_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_12_MH.Anim_Mid_Rhythm_Dance_12_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_11_MH.Anim_Slow_Rhythm_Dance_11_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_12_MH.Anim_Slow_Rhythm_Dance_12_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_13_MH.Anim_Slow_Rhythm_Dance_13_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_14_MH.Anim_Slow_Rhythm_Dance_14_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_15_MH.Anim_Slow_Rhythm_Dance_15_MH"),
	};

	/** "BP_Dancer_Kaia_C_2" / "MHC_Nyra" / "BP_MHC_Isla_2" -> "Kaia" / "Nyra" / "Isla". */
	FString PrettyAgentName(const AActor& Actor)
	{
		FString Name = Actor.GetActorNameOrLabel();

		for (const TCHAR* Prefix : { TEXT("BP_Dancer_"), TEXT("BP_MHC_"), TEXT("BP_MH_"),
									 TEXT("BP_"), TEXT("MHC_"), TEXT("MH_") })
		{
			if (Name.StartsWith(Prefix))
			{
				Name.RightChopInline(FCString::Strlen(Prefix));
				break;
			}
		}

		// Trim the instance tail the editor/spawner adds: "_C", "_2", "_C_2".
		Name.RemoveFromEnd(TEXT("_C"));
		while (true)
		{
			int32 Underscore = INDEX_NONE;
			if (!Name.FindLastChar(TEXT('_'), Underscore)) { break; }

			const FString Tail = Name.RightChop(Underscore + 1);
			const bool bDroppable = Tail.IsNumeric() || Tail == TEXT("C");
			if (!bDroppable || Tail.IsEmpty()) { break; }

			Name.LeftInline(Underscore);
		}

		// FIRST NAMES ONLY (Walt, 2026-08-03). The actor labels carry surnames —
		// BP_MHC_NyraSolmere, BP_MHC_IslaRowan — and "I am AI Agent NyraSolmere" reads
		// like a database row rather than someone introducing herself. Cut at the
		// surname boundary: an underscore, or the second capital of a CamelCase name.
		//
		//   NyraSolmere -> Nyra      IslaRowan -> Isla
		//   Kaia -> Kaia             Aisling -> Aisling      Elise -> Elise
		//
		// Set AgentName by hand on a dancer to override this entirely.
		int32 Underscore = INDEX_NONE;
		if (Name.FindChar(TEXT('_'), Underscore) && Underscore > 0)
		{
			Name.LeftInline(Underscore);
		}

		for (int32 i = 1; i < Name.Len(); ++i)
		{
			if (FChar::IsUpper(Name[i]))
			{
				Name.LeftInline(i);
				break;
			}
		}

		return Name.IsEmpty() ? TEXT("Anonymous") : Name;
	}

	/**
	 * Where the ElevenLabs takes live. Two names are looked for, in this order:
	 *
	 *   dancer_power_<agent>   her own take   (dancer_power_kaia, dancer_power_elise, ...)
	 *   dancer_power           the take every agent shares
	 *
	 * So ONE recording gives all of them a voice today, and dropping in a per-agent file
	 * tomorrow overrides her without a line of code changing. Same trick as MrsHall's
	 * AudioKey lookup, and the same reason: the audio lands after the code, by hand.
	 */
	/**
	 * THE LIP LAYER OF A METAHUMAN FACE.
	 *
	 * Probed on SKM_MHC_Kaia_FaceMesh: 858 morph targets, and NOT ONE of them is named
	 * for the jaw. That is not an oversight — the jaw is a joint, rotated by RigLogic
	 * from a control curve, and control curves are unreachable from C++ at runtime. The
	 * mouth cannot be dropped open from here and never will be.
	 *
	 * The LIPS are a different story: these are genuine blend shapes on the mesh, and
	 * a Leader Pose follower applies its own MorphTargetCurves (SkeletalMeshComponent.cpp,
	 * UpdateFollowerComponent: "if follower also has it, add it here"). So SetMorphTarget
	 * moves them, without touching animation mode, leader pose, or the post-process ABP.
	 *
	 * lod0 names only. The talk shot pins the face to LOD0 anyway (ForceFaceLOD), both
	 * for these names and because a face filling the screen deserves the top LOD.
	 */
	const TCHAR* const LipPartShapes[] = {
		TEXT("head_lod0_mesh__mouth_lowerLipDepress_left"),
		TEXT("head_lod0_mesh__mouth_lowerLipDepress_right"),
	};
	const TCHAR* const LipRaiseShapes[] = {
		TEXT("head_lod0_mesh__mouth_upperLipRaise_left"),
		TEXT("head_lod0_mesh__mouth_upperLipRaise_right"),
	};
	/** Rounded vowels — "oo", "oh". */
	const TCHAR* const LipFunnelShapes[] = {
		TEXT("head_lod0_mesh__mouth_funnel_UL"),
		TEXT("head_lod0_mesh__mouth_funnel_UR"),
		TEXT("head_lod0_mesh__mouth_funnel_DL"),
		TEXT("head_lod0_mesh__mouth_funnel_DR"),
	};
	/** Wide vowels — "ee", "ah". */
	const TCHAR* const LipStretchShapes[] = {
		TEXT("head_lod0_mesh__mouth_stretch_left"),
		TEXT("head_lod0_mesh__mouth_stretch_right"),
	};

	void ApplyShapes(USkeletalMeshComponent& Face, const TCHAR* const* Names, int32 Count, float Value)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			// bRemoveZeroWeight: a shape driven to 0 leaves the curve map entirely,
			// so nothing of ours is still sitting on her face after the shot.
			Face.SetMorphTarget(FName(Names[i]), Value, /*bRemoveZeroWeight=*/true);
		}
	}

	const TCHAR* const VoiceFolder = TEXT("/Game/Audio/Dancers");
	const TCHAR* const SharedVoiceName = TEXT("dancer_power");

	/* THE GUIDE RECORDINGS live beside the power ones and are looked up the same way, so
	   dancer_guide_nyra.uasset is all it takes to give her a voice in the city. Until
	   that asset exists FindTalkVoice falls through to the shared dancer_guide, and if
	   that is missing too she is silent - which is the same graceful nothing an
	   unrecorded agent has always produced, and PlayTalkVoice logs exactly which asset
	   it wanted. */

	/* THE FACE PERFORMANCE IS THE VOICE ASSET'S NAME WITH THIS ON THE END, and that is the
	   whole convention: bake dancer_power_kaia.uasset in MetaHuman Animator, save the
	   result as dancer_power_kaia_face beside it, and Kaia's face starts saying her line
	   with no code change at all. See the header. */
	const TCHAR* const FaceSuffix = TEXT("_face");

	/* HER SECOND SPEECH IS A THIRD BASE NAME, not a special case bolted onto the first.

	   dancer_guide_nyra   -> go and eat
	   dancer_guide2_nyra  -> go and build

	   Same folder, same per-agent fallback, same _face rule. Stage 2 of a future guide is
	   dancer_guide3 and one more entry here — which is the point of picking the base name
	   from a table rather than from an if. */
	const TCHAR* const GuideVoiceNames[] =
	{
		TEXT("dancer_guide"),
		TEXT("dancer_guide2"),
		// Stage 2, the supply run. Bake dancer_guide3_nyra and its _face beside the
		// others; until then she is silent at stage 2 and that is the correct failure.
		TEXT("dancer_guide3"),
		// Stage 3, outside uFoods with the supplies bought - go and board.
		TEXT("dancer_guide4"),
		// Stage 4, on Grok: the apology, and the last words of the game.
		TEXT("dancer_guide5"),
	};

	/** The idle a guide stands in once she is past dancing. See the header on IdleAnim. */
	const TCHAR* const IdlePath =
		TEXT("/Game/Characters/Retargeting/Combat/GS_Idle_MH.GS_Idle_MH");

	/** Clamped, so a stage this build has no recording for still speaks her last line
	    rather than reading off the end of the table and crashing. */
	const TCHAR* GuideVoiceBase(int32 Stage)
	{
		const int32 Last = UE_ARRAY_COUNT(GuideVoiceNames) - 1;
		return GuideVoiceNames[FMath::Clamp(Stage, 0, Last)];
	}

	int32 FindBoneExact(const USkeletalMeshComponent& Mesh, const TCHAR* const* Names, int32 NameCount)
	{
		for (int32 n = 0; n < NameCount; ++n)
		{
			const int32 Index = Mesh.GetBoneIndex(FName(Names[n]));
			if (Index != INDEX_NONE)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool BoneWorldLoc(const USkeletalMeshComponent* Mesh, const TCHAR* const* Names, int32 NameCount, FVector& Out)
	{
		if (!Mesh)
		{
			return false;
		}
		const int32 Index = FindBoneExact(*Mesh, Names, NameCount);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		Out = Mesh->GetBoneLocation(Mesh->GetBoneName(Index));
		return true;
	}

	FVector Flatten(FVector V)
	{
		V.Z = 0.f;
		return V;
	}

}

/* The one conversation on Grok, recorded once and forever. Named here rather than typed
   at each use site for the reason ALegacyMachine::ClosedTicketGrant is: a grant key that
   exists as a string literal in two files is a grant key that will differ in two files. */
const FName UDancerAgentComponent::GrokTalkedGrant(TEXT("Grok.Talked"));

UDancerAgentComponent::UDancerAgentComponent()
{
	/* TICKS FROM THE START, ALWAYS (Walt's log, 2026-08-25).

	   This used to start with tick DISABLED and flip it on in Greet. It never came on.
	   The diagnostic caught it exactly: "tickEnabled=yes ... 0 mouth tick(s)" — the flag
	   said enabled and the tick function never fired once in nine seconds.

	   The reason is that this component is added at RUNTIME by UDancerAgentSubsystem's
	   scan, not placed in the level. SetComponentTickEnabled only moves a flag on the
	   tick function; the function itself is registered with the level during component
	   registration, and with bStartWithTickEnabled false at that moment there is nothing
	   live to enable afterwards. IsComponentTickEnabled() then cheerfully reports true
	   for a tick that will never run — which is why this survived two rounds of
	   "the lips do not move".

	   So: tick always, and let TickComponent early-out. It is a bool test per dancer per
	   frame, and five dancers cost nothing. It also fixes the camera, which tracks her
	   face on this same tick — the original "their heads move out of view". */
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	/* bAutoActivate IS THE ONE THAT MATTERS. Everything above it was necessary and none
	   of it was sufficient.

	   UActorComponent's OWN constructor ends with SetTickFunctionEnable(false)
	   (ActorComponent.cpp), and the tick only ever comes back through Activate(), which
	   OnRegister calls ONLY when bAutoActivate is true. It defaults to FALSE. A component
	   placed in a Blueprint is usually activated by other machinery; one created by
	   NewObject in a runtime scan, like this one, is not — it registers inert.

	   That is why the first two attempts failed in opposite directions and both logged
	   "0 mouth tick(s)": with bStartWithTickEnabled false the flag read enabled and the
	   function was dead, and with it true the activation path came along afterwards and
	   switched it off. IsComponentTickEnabled() is not evidence that anything ticks. The
	   tick COUNTER was. */
	bAutoActivate = true;

	// Hard references on the CDO — this is what drags the gitignored animations into
	// the pak. See the header. A loop of FObjectFinders is legal inside a constructor.
	for (const TCHAR* Path : DancePaths)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(Path);
		if (Finder.Succeeded())
		{
			Dances.Add(Finder.Object);
		}
	}

	// Greeting — same cook rule as the dances. The _MH file is gitignored.
	// Not static: a static finder caches the FIRST result, which was a miss
	// the session the file did not exist yet.
	ConstructorHelpers::FObjectFinder<UAnimSequence> GreetFinder(
		TEXT("/Game/Characters/Retargeting/Anim_Celebration_2_Manny_MH.Anim_Celebration_2_Manny_MH"));
	if (GreetFinder.Succeeded())
	{
		GreetingAnim = GreetFinder.Object;
	}

	/* THE IDLE IS NOT LOADED HERE, AND THAT IS THE WHOLE POINT (2026-09-01).

	   It was, for about ten minutes, as an FObjectFinder sitting right below the greeting
	   one — the obvious place, matching the dances above it. It took the editor down on
	   STARTUP, twice, before a window ever appeared:

	     EXCEPTION_ACCESS_VIOLATION reading 0x0
	     ConstructorHelpersInternal::FindOrLoadObject<UAnimSequence>()  ConstructorHelpers.h:35
	     UDancerAgentComponent::UDancerAgentComponent()                 DancerAgentComponent.cpp:309

	   Line 35 of that header is the LoadObject call, so the fault is in loading
	   GS_Idle_MH itself during CDO construction — not in the path, and not in the finder.
	   The dances survive the same treatment; something about that Combat folder does not.

	   A constructor is the worst possible place to find that out. It runs before the
	   editor exists, so a bad asset there is not a broken animation, it is a project that
	   will not open — and the only way back is a code change and a full rebuild, which is
	   a bad afternoon for anyone who did not write the line.

	   So the idle loads on demand instead, in ApplyGuideStage, in a running world. The
	   cooking that this reference used to guarantee is now DirectoriesToAlwaysCook in
	   DefaultGame.ini, which is the mechanism the voices already ship on. */
}

void UDancerAgentComponent::EnsureGreetingAnim()
{
	if (GreetingAnim)
	{
		return;
	}

	// Load AFTER CDO property-copy. ConstructorHelpers can miss a newly saved
	// asset; LoadObject sees the Content Browser.
	static const TCHAR* const GreetPaths[] = {
		TEXT("/Game/Characters/Retargeting/Anim_Celebration_2_Manny_MH.Anim_Celebration_2_Manny_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Bow_MH.Anim_Bow_MH"),
	};
	for (const TCHAR* Path : GreetPaths)
	{
		GreetingAnim = LoadObject<UAnimSequence>(nullptr, Path);
		if (GreetingAnim)
		{
			UE_LOG(LogSibeliusGame, Display,
				TEXT("[Dancer] greeting loaded from %s"), Path);
			return;
		}
	}

	UE_LOG(LogSibeliusGame, Warning,
		TEXT("[Dancer] greeting anim missing — will pause instead"));
}

void UDancerAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Belt and braces. bAutoActivate handles the normal path; this covers a component
	// that got registered before its owner finished initialising, where OnRegister skips
	// the Activate call entirely. Cheap, idempotent, and this bug cost three playtests.
	SetComponentTickEnabled(true);

	EnsureGreetingAnim();

	if (AgentName.IsEmpty())
	{
		if (const AActor* Owner = GetOwner())
		{
			AgentName = PrettyAgentName(*Owner);
		}
	}

	// Remember which dance she starts on so the first shuffle is a real change.
	if (const USkeletalMeshComponent* Mesh = FindDanceMesh())
	{
		CurrentDanceIndex = Dances.IndexOfByKey(Mesh->AnimationData.AnimToPlay);
	}

	// Join the registry the interactor's aim-assist reads. Doing it here rather than in
	// the subsystem's scan means a hand-placed component registers itself too.
	if (UWorld* World = GetWorld())
	{
		if (UDancerAgentSubsystem* Sub = World->GetSubsystem<UDancerAgentSubsystem>())
		{
			Sub->RegisterDancer(this);
		}
	}

	/* LAST, AND AFTER THE REGISTRY. ApplyGuideStage stops her dancing, and the behaviour
	   scan decides what an agent IS by what it is dancing — so she has to be a registered
	   agent BEFORE she stops, or she is a statue that nobody adopted. Order is the whole
	   safety here; a hand-placed component makes her permanent, but this line makes the
	   order legible to whoever moves it. */
	ApplyGuideStage();
}

void UDancerAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelGreeting(/*bResumeDance=*/false);
	Super::EndPlay(EndPlayReason);
}

bool UDancerAgentComponent::IsKnownDance(const UAnimSequence* Anim)
{
	if (!Anim)
	{
		return false;
	}

	// Compare against the CDO's already-loaded list: a pointer check, not string
	// matching. The scan calls this for every skeletal mesh on every actor every few
	// seconds, and it guarantees "known dance" means exactly "one of the ones the CDO
	// hard-references" — the same list that reaches the pak.
	const UDancerAgentComponent* CDO = GetDefault<UDancerAgentComponent>();
	if (!CDO)
	{
		return false;
	}

	for (const TObjectPtr<UAnimSequence>& Dance : CDO->Dances)
	{
		if (Dance.Get() == Anim)
		{
			return true;
		}
	}
	return false;
}

USkeletalMeshComponent* UDancerAgentComponent::FindDanceMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);

	// Prefer the one already playing a dance we know — on a MetaHuman that is "Body",
	// and it skips the face/hair/clothing meshes, which share the actor.
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && IsKnownDance(Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay)))
		{
			return Mesh;
		}
	}

	// She has been shuffled to a dance that is no longer in the list, or a manual
	// component was added before she was dancing: fall back to a component named Body.
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetName().Contains(TEXT("Body")))
		{
			return Mesh;
		}
	}

	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

USkeletalMeshComponent* UDancerAgentComponent::FindFaceMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		if (Name.Contains(TEXT("Face")) && !Name.Contains(TEXT("PostProcess")))
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UDancerAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	/* THE WORK MOVED TO TalkTick (a timer) — see the header. This override stays purely
	   as an observer: if the component tick ever does start firing, say so once, and the
	   timer becomes deletable. Doing the work in BOTH places would drive the mouth twice
	   per frame. */
	if (!bComponentTickObserved)
	{
		bComponentTickObserved = true;
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Dancer] %s component tick FIRED — TalkTick's timer is now redundant."),
			*AgentName);
	}
}

FVector UDancerAgentComponent::GetEyeCenter() const
{
	const USkeletalMeshComponent* Face = FindFaceMesh();
	static const TCHAR* const LEye[] = { TEXT("FACIAL_L_Eye"), TEXT("FACIAL_L_EyeParallel") };
	static const TCHAR* const REye[] = { TEXT("FACIAL_R_Eye"), TEXT("FACIAL_R_EyeParallel") };
	FVector L = FVector::ZeroVector;
	FVector R = FVector::ZeroVector;
	const bool bL = BoneWorldLoc(Face, LEye, UE_ARRAY_COUNT(LEye), L);
	const bool bR = BoneWorldLoc(Face, REye, UE_ARRAY_COUNT(REye), R);
	if (bL && bR)
	{
		return (L + R) * 0.5f;
	}
	if (bL)
	{
		return L;
	}
	if (bR)
	{
		return R;
	}
	if (Face)
	{
		return Face->Bounds.Origin;
	}
	if (const USkeletalMeshComponent* Body = FindDanceMesh())
	{
		if (Body->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE)
		{
			return Body->GetBoneLocation(FName(TEXT("head")));
		}
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() + FVector(0.f, 0.f, 155.f) : FVector::ZeroVector;
}

FVector UDancerAgentComponent::GetFaceForward() const
{
	const USkeletalMeshComponent* Face = FindFaceMesh();
	static const TCHAR* const NoseNames[] = {
		TEXT("FACIAL_C_NoseTip"), TEXT("FACIAL_C_Nose"), TEXT("FACIAL_C_NoseLower")
	};
	FVector Nose = FVector::ZeroVector;
	if (BoneWorldLoc(Face, NoseNames, UE_ARRAY_COUNT(NoseNames), Nose))
	{
		FVector Fwd = Flatten(Nose - GetEyeCenter());
		if (Fwd.Normalize())
		{
			return Fwd;
		}
	}
	return GetTalkCamDir();
}

FVector UDancerAgentComponent::GetTalkLookAt() const
{
	const FVector Eyes = GetEyeCenter();
	const USkeletalMeshComponent* Face = FindFaceMesh();
	static const TCHAR* const MouthNames[] = {
		TEXT("FACIAL_C_LipUpper"), TEXT("FACIAL_C_MouthUpper"), TEXT("FACIAL_C_Jaw")
	};
	FVector Mouth = FVector::ZeroVector;
	if (BoneWorldLoc(Face, MouthNames, UE_ARRAY_COUNT(MouthNames), Mouth))
	{
		return Eyes * 0.62f + Mouth * 0.38f;
	}
	return Eyes;
}

FVector UDancerAgentComponent::GetTalkCamDir() const
{
	FVector To = Flatten(TalkPlayerEye - GetEyeCenter());
	if (To.Normalize())
	{
		return To;
	}
	if (const AActor* Owner = GetOwner())
	{
		FVector Fwd = Flatten(Owner->GetActorForwardVector());
		if (Fwd.Normalize())
		{
			return Fwd;
		}
	}
	return FVector::ForwardVector;
}

void UDancerAgentComponent::FaceThePlayer()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector ToPlayer = GetTalkCamDir();
	FVector FaceFwd = Flatten(GetFaceForward());
	if (!FaceFwd.Normalize())
	{
		FaceFwd = ToPlayer;
	}
	if (ToPlayer.IsNearlyZero() || FaceFwd.IsNearlyZero())
	{
		return;
	}

	const float DeltaYaw = FMath::FindDeltaAngleDegrees(FaceFwd.Rotation().Yaw, ToPlayer.Rotation().Yaw);
	FRotator R = Owner->GetActorRotation();
	R.Yaw += DeltaYaw;
	TeleportOwnerYaw(R);
}

void UDancerAgentComponent::TeleportOwnerYaw(const FRotator& Rotation)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	Owner->SetActorLocationAndRotation(Owner->GetActorLocation(), Rotation,
		/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
}

void UDancerAgentComponent::FreezeGrooms(bool bFreeze)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	TArray<UGroomComponent*> Grooms;
	Owner->GetComponents<UGroomComponent>(Grooms);
	for (UGroomComponent* Groom : Grooms)
	{
		if (!Groom)
		{
			continue;
		}
		if (bFreeze)
		{
			Groom->SetEnableSimulation(false);
		}
		else
		{
			Groom->ResetSimulation();
			Groom->SetEnableSimulation(true);
		}
	}
}

void UDancerAgentComponent::BeginTalkShot()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	if (!TalkCamera)
	{
		FActorSpawnParameters Params;
		Params.Owner = Owner;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TalkCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
			Owner->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (TalkCamera)
		{
			if (UCameraComponent* Cam = TalkCamera->GetCameraComponent())
			{
				Cam->bConstrainAspectRatio = false;
				Cam->SetFieldOfView(TalkCameraFOV);
			}
			TalkCamera->SetActorTickEnabled(true);
		}
	}
	if (!TalkCamera)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Dancer] %s talk camera failed to spawn"), *AgentName);
		return;
	}

	if (!bTalkShotActive)
	{
		SavedViewTarget = PC->GetViewTarget();
		TalkPlayerEye = PC->PlayerCameraManager
			? PC->PlayerCameraManager->GetCameraLocation()
			: (PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : Owner->GetActorLocation());
		bTalkShotActive = true;
		LockTalkInput(true);
		SavedActorRotation = Owner->GetActorRotation();
		bSavedActorRotation = true;
		FreezeGrooms(true);
		ForceFaceLOD(true);
		FaceThePlayer();
		UpdateTalkShot();
		PC->SetViewTargetWithBlend(TalkCamera, TalkCameraBlendIn, VTBlend_Cubic);
		// The heartbeat. 60 Hz, looping, cleared in EndTalkShot.
		LastTalkTickTime = 0.0;
		World->GetTimerManager().SetTimer(
			TalkTickTimer, this, &UDancerAgentComponent::TalkTick,
			1.0f / 60.0f, /*bLoop=*/true);

		LogMouthDiag();
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Dancer] %s talk close-up  lookat=%s  camdir=%s"),
			*AgentName,
			*GetTalkLookAt().ToCompactString(),
			*GetTalkCamDir().ToCompactString());
	}
	else
	{
		UpdateTalkShot();
	}
}

void UDancerAgentComponent::UpdateTalkShot()
{
	if (!TalkCamera)
	{
		return;
	}

	const FVector LookAt = GetTalkLookAt();
	const FVector Dir = GetTalkCamDir();

	// Stay at eye height and in front of her — never slide down the face into
	// the chin, an ear, or the chest (nose−skull on a MetaHuman points down).
	FVector CamLoc = LookAt + Dir * TalkCameraDistance + FVector(0.f, 0.f, 4.0f);

	if (const AActor* Owner = GetOwner())
	{
		const FVector Torso = Owner->GetActorLocation() + FVector(0.f, 0.f, 90.f);
		FVector Away = Flatten(CamLoc - Torso);
		if (Away.Size() < 28.f)
		{
			Away = Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
			CamLoc = Torso + Away * 40.f;
			CamLoc.Z = LookAt.Z + 4.0f;
		}
	}

	const FRotator CamRot = (LookAt - CamLoc).Rotation();
	TalkCamera->SetActorLocationAndRotation(CamLoc, CamRot);
	if (UCameraComponent* Cam = TalkCamera->GetCameraComponent())
	{
		Cam->SetFieldOfView(TalkCameraFOV);
	}
}

void UDancerAgentComponent::EndTalkShot()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TalkTickTimer);
	}
	LastTalkTickTime = 0.0;

	LockTalkInput(false);

	/* GIVE THE HUD BACK - but not this instant. The camera spends TalkCameraBlendOut
	   flying home, and popping the crosshair and the objective back on while it is still
	   on her face undoes the whole point of hiding them. Re-leasing for exactly the
	   blend keeps the screen clean until she is out of frame, and (unlike simply letting
	   the original lease run out) it is also the EARLY exit: F cancels her mid-sentence
	   and the HUD is back a third of a second later, not six seconds later. */
	//
	// Guarded on bTalkShotActive: CancelGreeting reaches here on EVERY F press, and a
	// dancer who was not talking must not blank the HUD for a third of a second.
	if (bTalkShotActive)
	{
		if (ASibeliusHUD* HUD = GetSibeliusHUD())
		{
			HUD->ReleaseCinematic();
			HUD->HoldCinematic(TalkCameraBlendOut);
		}
	}
	ForceFaceLOD(false);

	/* BEFORE the early-out below, not after. EndTalkShot returns early when there was no
	   shot to end, and F-cancelling her mid-sentence comes through here too — the face has
	   to be handed back on EVERY path out, or one cancelled greeting leaves her head
	   detached from her dance for the rest of the session. StopTalkFace is idempotent, so
	   calling it on the paths that never took the face costs nothing. */
	StopTalkFace();
	if (bSavedActorRotation)
	{
		TeleportOwnerYaw(SavedActorRotation);
		bSavedActorRotation = false;
		FreezeGrooms(false);
	}
	if (!bTalkShotActive && !TalkCamera)
	{
		return;
	}
	bTalkShotActive = false;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			AActor* Restore = SavedViewTarget.Get();
			if (!Restore)
			{
				Restore = PC->GetPawn();
			}
					if (Restore)
			{
				PC->SetViewTargetWithBlend(Restore, TalkCameraBlendOut, VTBlend_Cubic);
			}
		}
	}
	SavedViewTarget = nullptr;
	if (TalkCamera)
	{
		TalkCamera->Destroy();
		TalkCamera = nullptr;
	}
}

void UDancerAgentComponent::LockTalkInput(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		bTalkInputLocked = bLock;
		return;
	}
	if (bLock && !bTalkInputLocked)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		bTalkInputLocked = true;
	}
	else if (!bLock && bTalkInputLocked)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		bTalkInputLocked = false;
	}
}

FVector UDancerAgentComponent::GetAimPoint() const
{
	if (const USkeletalMeshComponent* Mesh = FindDanceMesh())
	{
		// Bounds are recomputed from the current pose, so this is her actual body,
		// not the actor's stationary origin.
		return Mesh->Bounds.Origin;
	}

	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

void UDancerAgentComponent::SetPowerGrant(APowerGrant* InGrant, EPowerVerb InVerb)
{
	PowerGrant = InGrant;
	PowerVerb = InVerb;
}

bool UDancerAgentComponent::HasPowerToGive() const
{
	// Destroy() leaves a pending-kill actor; != nullptr would still be true and the
	// prompt would keep offering a power she already handed over.
	return IsValid(PowerGrant);
}

FText UDancerAgentComponent::GetPrompt() const
{
	const TCHAR* Who = AgentName.IsEmpty() ? TEXT("her") : *AgentName;

	/* SAY WHAT THE KEY WILL DO. Pressing E here opens a staked slot machine and rings the
	   Refuser alarm — that is a commitment, and the player is entitled to know before
	   they press it rather than after. The old floating shrine asked nobody and fired on
	   contact, which is how Walt met one on a staircase. */
	if (HasPowerToGive())
	{
		return FText::FromString(FString::Printf(
			TEXT("[E] ask %s for %s     [F] change her dance"),
			Who, *PowerVerbDisplayName(PowerVerb)));
	}

	return FText::FromString(FString::Printf(
		TEXT("[E] talk to %s     [F] change her dance"), Who));
}

void UDancerAgentComponent::Greet()
{
	/* SHE HANDS THE POWER OVER (docs/SPINE.md).

	   Mrs. Hall's position is that a senior developer should not need a machine's help.
	   The dancers introduce themselves as AI agents. So taking a forbidden capability from
	   one of them is not a reskin of the floating shrine — it is the premise of the game
	   happening in front of the player.

	   The slot trial takes the camera. Talk close-up only happens when she has
	   nothing left to give — the "[E] talk to Kaia" prompt. */
	if (HasPowerToGive())
	{
		const AActor* Owner = GetOwner();
		const UWorld* W = Owner ? Owner->GetWorld() : nullptr;
		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		{
			PowerGrant->RequestTrial(PC);
		}
		// Slot takes the camera. Do not pause her or start a close-up underneath it.
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	/* SHE SPEAKS; THE HUD SHUTS UP (Walt, 2026-08-25).

	   This used to post the greeting to the Presence subtitle channel - a line of text
	   across the middle of the screen. Which is where her mouth is, in a portrait framed
	   at 38 degrees. Walt's screenshot of Kaia has four separate HUD layers on her face
	   and a crosshair in her eye.

	   So: no subtitle, and no HUD at all for the length of the shot (ASibeliusHUD::
	   HoldCinematic). The line is a RECORDING now. If the recording is not on this
	   machine yet the shot still runs, silent, with one Warning naming the file. */

	// Re-pressing E on a dancer who is already talking makes her say it again from the
	// top, rather than layering a second copy of her own voice over the first.
	StopTalkVoice();
	bMouthDiagLogged = false;
	const float VoiceSeconds = PlayTalkVoice();

	/* THE SAME LINE OF CODE STARTS BOTH CLOCKS, and that is the entire trick.

	   The performance was solved FROM this recording, so they are the same length and the
	   same shape — they only have to start together. Anywhere else (BeginTalkShot, a
	   timer, the level) is a second start time that can drift, and drift here reads as
	   bad lip sync rather than as the timing bug it is. Voice then face, adjacent, no
	   branch between them. */
	PlayTalkFace();

	TalkHoldSeconds = FMath::Max(GreetingSeconds, VoiceSeconds + TalkTailSeconds);

	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		// The lease covers the blend IN as well: the HUD should already be gone by the
		// time the camera arrives on her face, not blink out once it gets there.
		HUD->HoldCinematic(TalkHoldSeconds + TalkCameraBlendIn);
	}

	if (bGreeting)
	{
		BeginGreetingMotion();
		return;
	}

	BeginGreetingMotion();
	BeginTalkShot();
}

bool UDancerAgentComponent::IsGuide() const
{
	const AActor* Owner = GetOwner();
	return Owner && !GuideTag.IsNone() && Owner->ActorHasTag(GuideTag);
}

int32 UDancerAgentComponent::GuideStage() const
{
	/* A DANCER WHO IS NOT A GUIDE IS ALWAYS STAGE 0. Kaia in the office has been in the
	   deli too — the grant is the player's, not hers — and without this she would start
	   inviting him to Generate buildings from behind a desk in a different world. */
	if (!IsGuide())
	{
		return 0;
	}
	if (!ASibeliusGameCharacter::HasVisitedDeli(this))
	{
		return 0;
	}

	/* STAGE 4 IS "HE IS STANDING ON GROK", and it is the LEVEL, not a grant.

	   Stages 1-3 key off facts in the save - City.Deli, a spaceport in the world,
	   City.Supplies - because he could be anywhere when each becomes true, so the fact has
	   to be carried rather than observed. Stage 4 has no such ambiguity. There is exactly
	   one way to be standing on Grok, and standing on Grok IS the condition.

	   Checked before everything else because it is the furthest along, and because he
	   arrives holding City.Supplies - without this she would meet him at the end of a forty
	   light year journey and tell him to go and buy groceries. */
	if (const UWorld* World = GetWorld())
	{
		if (FName(*UWorld::RemovePIEPrefix(World->GetMapName())) == GrokLevelName)
		{
			return 4;
		}
	}

	/* STAGE 3 IS "HE HAS THE SUPPLIES", and it is checked FIRST because it is the
	   furthest along. The supplies are a fact about HIM, held in the save, so this stays
	   true even if the spaceport is later Test-Drive discarded — which is right: he did
	   go shopping, and no amount of un-building a launch pad undoes that. */
	if (ASupplyCounter::HasSupplies(this))
	{
		return 3;
	}

	/* STAGE 2 IS "A SPACEPORT STANDS" — and it ASKS THE WORLD, not a saved flag.

	   Same reasoning as AHintVolume, and it is right in the three places a bool is
	   wrong: after a load, after a Test-Drive discard that removed the spaceport, and on
	   a New Game. If the thing is not there, the errand it starts makes no sense, and she
	   should be back to inviting him to build one.

	   Iterating actors on a stage query is cheap and rare: this runs when he talks to
	   her, when a spaceport is generated, and on the restage retry — never per frame. */
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASpaceport> It(World); It; ++It)
		{
			return 2;
		}
	}
	return 1;
}

void UDancerAgentComponent::GetStageAnchor(int32 Stage, FName& OutTag, float& OutDistance) const
{
	/* STAGE 4 HAS NO ANCHOR, and that is deliberate. The other stages move her to a tagged
	   marker because the city is large and she has to be found where the errand starts. On
	   Grok she is placed by Tools/Scripts/place_grok_arrival.py at a spot Walt chose by eye,
	   five metres from where the player materialises. Nothing should move her from it. */
	if (Stage >= 4)
	{
		OutTag = NAME_None;
		OutDistance = 0.0f;
		return;
	}
	if (Stage == 3)
	{
		OutTag = GuideStage3StartTag;
		OutDistance = GuideStage3Distance;
		return;
	}
	if (Stage == 2)
	{
		OutTag = GuideStage2StartTag;
		OutDistance = GuideStage2Distance;
		return;
	}
	OutTag = GuideStage1StartTag;
	OutDistance = GuideStage1Distance;
}

bool UDancerAgentComponent::IsUnseenByPlayer() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		// No camera means nobody is looking, but it also means something is wrong. Say
		// "seen" so we wait rather than teleport into an unknown state.
		return false;
	}

	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Look = PC->PlayerCameraManager->GetCameraRotation().Vector();

	FVector ToHer = Owner->GetActorLocation() - Eye;
	if (!ToHer.Normalize())
	{
		return false;   // he is standing inside her; definitely not the moment
	}

	// Wider than the screen on purpose - a flick of the mouse must not catch her mid-move.
	return FVector::DotProduct(Look, ToHer) < RestageViewDot;
}

void UDancerAgentComponent::RestageGuide()
{
	if (!IsGuide() || bRestagePending)
	{
		return;
	}

	bRestagePending = true;
	TryRestageNow();
}

void UDancerAgentComponent::TryRestageNow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bRestagePending = false;
		return;
	}

	if (!IsUnseenByPlayer())
	{
		/* HE IS LOOKING. Wait, and keep waiting - there is no deadline and no forced
		   move. She is already saying the new stage's line from where she stands, so the
		   cost of never moving is that she is in an older spot, which is exactly what
		   stage 1 looks like right now. The cost of forcing it is a character teleporting
		   in full view, which is the one outcome this whole mechanism exists to avoid. */
		World->GetTimerManager().SetTimer(RestageTimerHandle, this,
			&UDancerAgentComponent::TryRestageNow, RestageRetrySeconds, /*bLoop=*/false);
		return;
	}

	World->GetTimerManager().ClearTimer(RestageTimerHandle);
	bRestagePending = false;

	ApplyGuideStage();

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Dancer] %s restaged to guide stage %d while unseen."),
		*AgentName, GuideStage());
}

bool UDancerAgentComponent::HasShipLeft() const
{
	/* ASK THE WORLD, NEVER A SAVED BOOL — the AHintVolume rule that stage 2 already
	   follows, and it is right in the three places a bool is wrong: after a load, after a
	   Test-Drive discard that removed the spaceport, and on a New Game.

	   It survives the reload of L_City that every door causes, because the launch lives on
	   the branch state rather than on the actor's lifetime: ASpaceport::RestoreBranchState
	   sets bLaunched at state 3 and re-opens the portal immediately for the same reason.

	   AND IT IS FALSE ON GROK, without a level test, because there is no spaceport there.
	   The arrival Nyra is a different actor in a different world and nothing here reaches
	   her — which is what we want, since she is the one with something left to say. */
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASpaceport> It(World); It; ++It)
		{
			if (It->HasLaunched())
			{
				return true;
			}
		}
	}
	return false;
}

void UDancerAgentComponent::SetGuidePresent(bool bPresent)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Owner->SetActorHiddenInGame(!bPresent);
	Owner->SetActorEnableCollision(bPresent);

	/* AND THE COMPONENTS EXPLICITLY, not just the actor flag. A MetaHuman is a bag of
	   parts — body, face, four grooms — and the hair on these actors has already ignored
	   once what was done at the actor level (the talk-yaw groom explosion). Belt and
	   braces costs one loop and the alternative is a floating head. */
	TArray<UPrimitiveComponent*> Prims;
	Owner->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Prim->SetVisibility(bPresent, /*bPropagateToChildren=*/true);
		}
	}

	if (!bPresent)
	{
		StopTalkVoice();   // she does not finish a sentence from inside the floor
	}
}

void UDancerAgentComponent::ApplyGuideStage()
{
	/* SHE IS GONE ONCE THE SHIP IS (docs/FUN_PLAN_2.md A4). Tested BEFORE the stage
	   early-out below, because stage 3 outlives its own reason: "he has the supplies" is
	   true for the rest of the game, so without this she goes on sending him to board a
	   rocket that left.

	   Both callers funnel through here, and each is right for its case. BeginPlay covers
	   the reload of L_City, where the spaceport restores to launched and she should simply
	   never appear. RestageGuide covers the live launch, and it waits until she is unseen
	   before calling this — so she is not deleted out of the frame in front of him. */
	if (IsGuide() && HasShipLeft())
	{
		SetGuidePresent(false);
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Dancer] %s stood down: the ship has gone."), *AgentName);
		return;
	}

	if (GuideStage() < 1)
	{
		return;   // stage 0 is the placed state: she is already where she should be
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	/* MOVE HER FIRST, POSE HER SECOND, and only move her if the level said where.

	   No tagged PlayerStart means this level has not been set up for stage 1 yet, and the
	   right answer then is to leave her exactly where she was placed. Falling back to a
	   default position would put her at the world origin, which is precisely how three
	   coffee cups vanished into the ground under Downtown West. Standing in the old spot
	   with the new line is a visibly half-done feature; standing under the map is a
	   missing character. */
	/* ONE BODY, ANY STAGE. The marker tag and the stand-off both come from the stage, so
	   adding stage 3 is two properties and a line in GetStageAnchor - not another copy of
	   the capsule-drop arithmetic below, which is the part that was got wrong once
	   already. */
	FName StageTag = NAME_None;
	float StageDistance = 0.0f;
	GetStageAnchor(GuideStage(), StageTag, StageDistance);

	bool bMoved = false;
	if (!StageTag.IsNone())
	{
		for (TActorIterator<APlayerStart> It(Owner->GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag != StageTag)
			{
				continue;
			}

			/* SHE STANDS IN FRONT OF WHERE HE APPEARS, FACING HIM. The PlayerStart's
			   forward is the way he will be looking when the level finishes loading, so
			   stepping that far along it puts her in his view without making him turn,
			   and the opposite yaw means she is looking back at him when he arrives. */
			/* DROP HER TO THE PAVEMENT. A PlayerStart's location is the CENTRE of its
			   capsule — roughly a metre up, because that is where a standing player's
			   middle is. Her actor origin is at her FEET. Placing one at the other left
			   her hanging in the air outside the deli, which is what Walt saw first time.

			   The half-height is read off the actor rather than assumed to be 88 or 96,
			   so a PlayerStart someone has scaled still lands her on the ground. */
			float FloorDrop = 0.0f;
			if (const UCapsuleComponent* Capsule = It->GetCapsuleComponent())
			{
				FloorDrop = Capsule->GetScaledCapsuleHalfHeight();
			}

			FVector Spot = It->GetActorLocation()
				+ It->GetActorForwardVector() * StageDistance
				- FVector(0.0f, 0.0f, FloorDrop);

			/* NOW ASK THE GROUND WHERE IT IS, instead of trusting arithmetic about the
			   marker's height.

			   Subtracting the capsule half-height is only correct if the marker's capsule
			   is sitting exactly on the floor, and by 3 Sep that assumption had put a
			   character in the air three separate times: her feet below the pavement at
			   the deli (the half-height was missing), then a metre up on the lawn (the
			   marker inherited a trigger SPHERE's centre), then a few inches up after
			   that was hand-corrected by eye.

			   Every one of those is the same bug — a height derived from something that
			   is not the ground. So derive it from the ground. A downward trace does not
			   care where the marker sits, which means the marker only has to be in the
			   right PLACE, and nobody has to nudge a Z again. It also fixes stage 1 for
			   free, and any stage added later.

			   Falls back to the arithmetic if the trace hits nothing at all, because a
			   guide slightly off the floor is a blemish and a guide at the world origin
			   is a missing character. */
			if (const UWorld* World = Owner->GetWorld())
			{
				FHitResult Floor;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GuideFloor), /*bTraceComplex=*/false, Owner);
				Params.AddIgnoredActor(*It);

				const FVector From = Spot + FVector(0.0f, 0.0f, 300.0f);
				const FVector To = Spot - FVector(0.0f, 0.0f, 3000.0f);
				if (World->LineTraceSingleByChannel(Floor, From, To, ECC_Visibility, Params))
				{
					/* AND HER ORIGIN IS NOT QUITE HER FEET. Putting the actor origin on
					   the impact point left her a few inches up (Walt, 3 Sep) - the trace
					   was right, the assumption about what to do with it was not.

					   So measure the gap instead of assuming it is zero: how far below the
					   actor's origin her rendered body actually reaches. Then stand that
					   bottom on the floor. Read off the mesh, so a different agent, a
					   different rig or a scaled one all land correctly. */
					float FeetBelowOrigin = 0.0f;
					if (const USkeletalMeshComponent* Body = FindDanceMesh())
					{
						const FBoxSphereBounds B = Body->Bounds;
						FeetBelowOrigin = Owner->GetActorLocation().Z
							- (B.Origin.Z - B.BoxExtent.Z);
					}
					Spot.Z = Floor.ImpactPoint.Z + FMath::Max(0.0f, FeetBelowOrigin);
				}
				else
				{
					UE_LOG(LogSibeliusGame, Warning,
						TEXT("[Dancer] %s found no floor under her stage %d spot - "
						     "falling back to the marker's height."),
						*AgentName, GuideStage());
				}
			}
			const FRotator Facing(0.0f, It->GetActorRotation().Yaw + 180.0f, 0.0f);

			Owner->SetActorLocationAndRotation(Spot, Facing,
				/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
			bMoved = true;
			break;
		}
	}

	if (!bMoved)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Dancer] %s is at guide stage %d but this level has no PlayerStart tagged "
			     "'%s' - she stays where she was placed, and still speaks the new line."),
			*AgentName, GuideStage(), *StageTag.ToString());
	}

	/* SHE KEEPS DANCING (Walt, 2026-09-03) — which REVERSES "No more dancing" from
	   2026-09-01, and the reason is better than the original:

	       "actually, you know, she is a dancer with endless energy, so let her dance
	        outside the deli and at the spaceport."

	   The reversal was prompted by her HANDS. GS_Idle_MH is a Paragon (Greystone)
	   animation retargeted onto a MetaHuman skeleton, and the two rigs do not agree about
	   fingers — she stood outside the deli with her hands visibly mangled, gripping a
	   sword that was not there. Every other _MH file in that folder came through the same
	   retarget, so there was no better idle to swap to and no neutral idle anywhere in
	   the project.

	   The Morro dances have no such problem: they retarget cleanly, which is why the city
	   dancers look right. So the fix for a broken standing pose is to not stand. A guide
	   who dances while she waits is also simply a better character than one who powers
	   down between errands.

	   The idle is kept behind bGuideStopsDancing, off by default, because the machinery
	   was correct even though the art was not — and if a properly retargeted idle ever
	   arrives, this becomes a tickbox rather than a rewrite. */
	if (bGuideStopsDancing)
	{
		/* LOADED HERE, NOT IN THE CONSTRUCTOR — see the long note there.
		   LOAD_NoWarn|LOAD_Quiet because a missing idle is a warning we write ourselves
		   below, with the path in it, rather than engine noise. */
		if (!IdleAnim)
		{
			IdleAnim = LoadObject<UAnimSequence>(nullptr, IdlePath, nullptr,
				LOAD_NoWarn | LOAD_Quiet);
		}

		if (USkeletalMeshComponent* Body = FindDanceMesh())
		{
			if (IdleAnim)
			{
				Body->PlayAnimation(IdleAnim, /*bLooping=*/true);
			}
			else
			{
				/* Warning, not silence, and the difference matters: a missing idle leaves
				   her in whatever pose the mesh defaults to, which reads as a bug in the
				   character rather than a missing asset. Name the asset so the log
				   answers it. */
				UE_LOG(LogSibeliusGame, Warning,
					TEXT("[Dancer] %s is at guide stage %d with no IdleAnim - she will stand "
					     "in her default pose. Expected %s"),
					*AgentName, GuideStage(), IdlePath);
			}
		}
	}

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Dancer] %s is guiding at stage %d%s, and %s."), *AgentName, GuideStage(),
		bMoved ? TEXT(", moved to her marker") : TEXT(""),
		bGuideStopsDancing ? TEXT("standing") : TEXT("still dancing"));
}

FString UDancerAgentComponent::GetSpokenLine() const
{
	/* HER NAME IS IN THE LINE (Walt, 2026-08-25). "I am AI agent Kaia. I have granted
	   you a power. Use it wisely." — which only works because the recording is per
	   agent. The SHARED dancer_power clip stays nameless on purpose: it is what any
	   dancer without her own take falls back to, including the one AFinaleAltar summons
	   at the cathedral, and a fallback that confidently announces the wrong name is
	   worse than one that announces none. */
	/* The stage picks the words, and GuideVoiceBase picks the recording from the same
	   number, so the two can never drift apart. Highest stage first: adding stage 3 means
	   one more clause here and one more entry in GuideVoiceNames. */
	FString Line = TalkLine;
	if (IsGuide())
	{
		const int32 Stage = GuideStage();
		if (Stage > 0 && Stage < 4 && !AProteinMachine::IsEnhanced(this))
		{
			return AProteinMachine::HasMeal(this)
				? TEXT("Now that there is fuel in your body, you may enter the Protein Machines Enhancement Station and be enhanced for space travel. You will love it. Those blue guys dancing on the corner had a little too much, so I am letting you only have one dose.")
				: GuideLine;
		}
		Line = (Stage >= 4) ? GuideLine5
			 : (Stage >= 3) ? GuideLine4
			 : (Stage >= 2) ? GuideLine3
			 : (Stage >= 1) ? GuideLine2
			 : GuideLine;
	}
	Line.ReplaceInline(TEXT("{0}"), *AgentName);
	return Line;
}

const TCHAR* UDancerAgentComponent::TalkVoiceBase() const
{
	if (!IsGuide()) return SharedVoiceName;
	const int32 Stage = GuideStage();
	if (Stage > 0 && Stage < 4 && !AProteinMachine::IsEnhanced(this))
	{
		// Visiting the deli alone is not enough. Both purchases qualify for the new take.
		return AProteinMachine::HasMeal(this) ? TEXT("dancer_protein") : GuideVoiceBase(0);
	}
	return GuideVoiceBase(Stage);
}

USoundBase* UDancerAgentComponent::FindTalkVoice() const
{
	// LOAD_NoWarn|LOAD_Quiet: "not recorded yet" is an expected state, not a fault, and
	// it must not spray the log every time the player talks to somebody.
	// One switch decides both halves - the words and the recording - so a guide can
	// never end up speaking the granting line or the other way round.
	const TCHAR* const Base = TalkVoiceBase();

	const FString Own = AgentName.ToLower();
	if (!Own.IsEmpty())
	{
		const FString Path = FString::Printf(TEXT("%s/%s_%s.%s_%s"),
			VoiceFolder, Base, *Own, Base, *Own);
		if (USoundBase* Mine = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Mine;
		}
	}

	const FString Shared = FString::Printf(TEXT("%s/%s.%s"),
		VoiceFolder, Base, Base);
	return LoadObject<USoundBase>(nullptr, *Shared, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UAnimSequence* UDancerAgentComponent::FindTalkFace() const
{
	// Deliberately the same shape as FindTalkVoice, so the two can never disagree about
	// which agent they are serving: her own take first, then the shared one.
	const TCHAR* const Base = TalkVoiceBase();

	const FString Own = AgentName.ToLower();
	if (!Own.IsEmpty())
	{
		const FString Path = FString::Printf(TEXT("%s/%s_%s%s.%s_%s%s"),
			VoiceFolder, Base, *Own, FaceSuffix, Base, *Own, FaceSuffix);
		if (UAnimSequence* Mine = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Mine;
		}
	}

	const FString Shared = FString::Printf(TEXT("%s/%s%s.%s%s"),
		VoiceFolder, Base, FaceSuffix, Base, FaceSuffix);
	return LoadObject<UAnimSequence>(nullptr, *Shared, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UDancerAgentComponent::PlayTalkFace()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	USkeletalMeshComponent* Face = FindFaceMesh();
	if (!Face)
	{
		return;
	}

	UAnimSequence* Performance = FindTalkFace();
	if (!Performance)
	{
		/* NOT A WARNING. Four of the five agents have no baked face yet, and the close-up
		   is complete without one — she still turns, still speaks, still holds the frame.
		   Logging a fault for every one of them would train the reader to skim the log,
		   which is how the real faults get missed. Display, once, and only when she
		   HAS one, is below. */
		return;
	}

	// Read it, do not assume it. See the header on SavedFaceMode.
	if (!bFacePlaying)
	{
		SavedFaceMode = static_cast<uint8>(Face->GetAnimationMode());
	}

	/* NOT LOOPING. The voice plays once; a looping face would carry on mouthing the line
	   into the tail of silence and out through the camera's flight home. When the
	   performance runs out she simply holds her last expression, which is what a person
	   does when they finish a sentence. */
	Face->PlayAnimation(Performance, /*bLooping=*/false);
	bFacePlaying = true;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s's face performs (%s, %.2fs)"),
		*AgentName, *Performance->GetName(), Performance->GetPlayLength());
}

void UDancerAgentComponent::StopTalkFace()
{
	if (!bFacePlaying)
	{
		return;   // idempotent: CancelGreeting reaches EndTalkShot on every F press
	}
	bFacePlaying = false;

	if (USkeletalMeshComponent* Face = FindFaceMesh())
	{
		/* HANDING THE FACE BACK IS THE HALF THAT MATTERS. Taking it is visible
		   immediately; giving it back wrong is invisible until she is dancing again with
		   a head that no longer follows her body — a bug that would show up three rooms
		   later and look like something else entirely. */
		Face->SetAnimationMode(static_cast<EAnimationMode::Type>(SavedFaceMode));
	}
}

float UDancerAgentComponent::PlayTalkVoice()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return 0.0f;
	}

	// The smoke-test commandlets run headless with no audio device. They stay green and
	// silent - the MrsHall guard, for the same reason.
	if (IsRunningCommandlet() || !FApp::CanEverRenderAudio())
	{
		return 0.0f;
	}

	USoundBase* Clip = FindTalkVoice();
	if (!Clip)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Dancer] %s has no voice clip - close-up runs silent. Import the recording of ")
			TEXT("\"%s\" as %s/%s (or %s/%s_%s for her own take). See docs/DANCER_VOICE.md"),
			*AgentName, *GetSpokenLine(), VoiceFolder, SharedVoiceName,
			VoiceFolder, SharedVoiceName, *AgentName.ToLower());
		return 0.0f;
	}

	/* 2D, NOT ATTACHED TO HER HEAD. She fills the screen from 48 cm away, so there is no
	   spatial information a pan could carry - only the chance of her voice arriving from
	   slightly off-camera-left. The apparition AP7 lesson: for a voice that IS the scene,
	   2D is immune to every "spawned behind your head" volume bug.

	   CREATED THEN PLAYED, not SpawnSound2D, because the mouth rides the audio envelope
	   and the audio component only bothers computing one if the delegate is ALREADY
	   bound when the sound starts:

	       NewActiveSound.bUpdateSingleEnvelopeValue = OnAudioSingleEnvelopeValue.IsBound()

	   SpawnSound2D plays on creation, so binding afterwards is one frame too late and
	   the envelope never arrives. */
	TalkAudio = UGameplayStatics::CreateSound2D(World, Clip);
	if (TalkAudio)
	{
		VoiceEnvelope = 0.0f;
		bVoiceEnvelopeSeen = false;
		MouthTime = 0.0f;
		TalkAudio->OnAudioSingleEnvelopeValue.AddDynamic(this, &UDancerAgentComponent::HandleVoiceEnvelope);
		TalkAudio->Play();
	}
	else
	{
		// No component, no envelope, no mouth - but the line still has to be heard.
		UGameplayStatics::PlaySound2D(World, Clip);
	}

	const float Seconds = Clip->GetDuration();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s speaks (%s, %.2fs)"),
		*AgentName, *Clip->GetName(), Seconds);

	// A looping sound reports an absurd duration and would hold the camera forever.
	return (Seconds > 0.0f && Seconds < 60.0f) ? Seconds : 0.0f;
}

void UDancerAgentComponent::StopTalkVoice()
{
	if (IsValid(TalkAudio))
	{
		TalkAudio->OnAudioSingleEnvelopeValue.RemoveDynamic(this, &UDancerAgentComponent::HandleVoiceEnvelope);
		TalkAudio->Stop();
		TalkAudio->DestroyComponent();
	}
	TalkAudio = nullptr;

	// Close her mouth on the way out. Skipping this leaves her lips parted for the rest
	// of the level, because morph curves persist until something zeroes them.
	SetMouthShapes(0.0f, 0.0f);
	MouthOpen = 0.0f;
	MouthTime = 0.0f;
	VoiceEnvelope = 0.0f;
	bVoiceEnvelopeSeen = false;
}

void UDancerAgentComponent::HandleVoiceEnvelope(const USoundWave* /*PlayingSoundWave*/, const float EnvelopeValue)
{
	VoiceEnvelope = EnvelopeValue;
	bVoiceEnvelopeSeen = true;
}

void UDancerAgentComponent::TalkTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A real delta, not the nominal rate: FInterpTo on the mouth wants the time that
	// actually passed, and a hitching frame would otherwise snap her lips.
	const double Now = World->GetTimeSeconds();
	const float Delta = (LastTalkTickTime > 0.0)
		? FMath::Clamp(static_cast<float>(Now - LastTalkTickTime), 0.001f, 0.25f)
		: 1.0f / 60.0f;
	LastTalkTickTime = Now;

	if (!bGreeting || !bTalkShotActive)
	{
		return;
	}

	++MouthTickCount;
	UpdateTalkShot();
	UpdateMouth(Delta);
}

void UDancerAgentComponent::UpdateMouth(float DeltaTime)
{
	if (!bTalkMouthMotion)
	{
		return;
	}

	USkeletalMeshComponent* Face = FindFaceMesh();
	if (!Face)
	{
		return;
	}

	const bool bSpeaking = IsValid(TalkAudio) && TalkAudio->IsPlaying();

	float Target = 0.0f;
	if (bSpeaking)
	{
		MouthTime += DeltaTime;

		if (bVoiceEnvelopeSeen)
		{
			/* THE REAL WAVEFORM. The envelope is a linear amplitude, and speech spends
			   most of its time well below full scale, so it is lifted with a square root
			   before driving the lips - otherwise she barely moves except on the loudest
			   syllable. */
			Target = FMath::Clamp(FMath::Sqrt(FMath::Clamp(VoiceEnvelope, 0.0f, 1.0f)) * 1.35f, 0.0f, 1.0f);
		}
		else
		{
			// Fallback: a syllable oscillator, so a build where the delegate never fires
			// still moves her lips instead of leaving her mouthing silence.
			const float Fast = 0.5f + 0.5f * FMath::Sin(MouthTime * 2.0f * PI * 5.5f);
			const float Phrase = 0.65f + 0.35f * FMath::Sin(MouthTime * 2.0f * PI * 1.3f + 0.9f);
			Target = Fast * Phrase;
		}
	}

	// Lips have mass. Interpolated in both directions so nothing snaps, and fast enough
	// (22) that consonants still read at four or five syllables a second.
	MouthOpen = FMath::FInterpTo(MouthOpen, Target, DeltaTime, 22.0f);
	MouthPeak = FMath::Max(MouthPeak, MouthOpen);

	// Vowel colour drifts slowly between wide and rounded. Nobody speaks one vowel for
	// three seconds, and a mouth that only opens and shuts is a nutcracker.
	const float Shape = 0.5f + 0.5f * FMath::Sin(MouthTime * 2.0f * PI * 0.8f + 0.4f);

	SetMouthShapes(MouthOpen * FMath::Clamp(MouthOpenScale, 0.0f, 1.0f), Shape);
}

void UDancerAgentComponent::SetMouthShapes(float Open, float Shape)
{
	USkeletalMeshComponent* Face = FindFaceMesh();
	if (!Face)
	{
		return;
	}

	Open = FMath::Clamp(Open, 0.0f, 1.0f);
	Shape = FMath::Clamp(Shape, 0.0f, 1.0f);

	ApplyShapes(*Face, LipPartShapes,    UE_ARRAY_COUNT(LipPartShapes),    Open);
	ApplyShapes(*Face, LipRaiseShapes,   UE_ARRAY_COUNT(LipRaiseShapes),   Open * 0.45f);
	ApplyShapes(*Face, LipFunnelShapes,  UE_ARRAY_COUNT(LipFunnelShapes),  Open * Shape * 0.60f);
	ApplyShapes(*Face, LipStretchShapes, UE_ARRAY_COUNT(LipStretchShapes), Open * (1.0f - Shape) * 0.45f);
}

void UDancerAgentComponent::LogMouthDiag()
{
	if (bMouthDiagLogged)
	{
		return;
	}
	bMouthDiagLogged = true;

	USkeletalMeshComponent* Face = FindFaceMesh();
	if (!Face)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Dancer] %s MOUTH DIAG: no Face mesh component on this actor — lips cannot move."),
			*AgentName);
		return;
	}

	const USkeletalMesh* FaceAsset = Face->GetSkeletalMeshAsset();
	const int32 MorphCount = FaceAsset ? FaceAsset->GetMorphTargets().Num() : 0;
	const bool bShapeFound = FaceAsset && FaceAsset->FindMorphTarget(FName(LipPartShapes[0])) != nullptr;

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Dancer] %s MOUTH DIAG: mouthMotion=%s tickEnabled=%s active=%s face='%s' mesh='%s' ")
		TEXT("morphs=%d shape=%s leaderpose=%s audio=%s scale=%.2f"),
		*AgentName,
		bTalkMouthMotion ? TEXT("ON") : TEXT("**OFF**"),
		IsComponentTickEnabled() ? TEXT("yes") : TEXT("**no**"),
		IsActive() ? TEXT("yes") : TEXT("**no**"),
		*Face->GetName(),
		FaceAsset ? *FaceAsset->GetName() : TEXT("NONE"),
		MorphCount,
		bShapeFound ? TEXT("FOUND") : TEXT("**MISSING**"),
		Face->LeaderPoseComponent.IsValid() ? TEXT("yes") : TEXT("no"),
		IsValid(TalkAudio) ? TEXT("yes") : TEXT("**none**"),
		MouthOpenScale);
}

void UDancerAgentComponent::ForceFaceLOD(bool bForce)
{
	USkeletalMeshComponent* Face = FindFaceMesh();
	if (!Face)
	{
		return;
	}

	if (bForce && !bFaceLODForced)
	{
		// 1 means LOD0 (0 means "auto"). The lip morphs are lod0-named, and a face
		// filling the screen should be on the top LOD regardless.
		Face->SetForcedLOD(1);
		bFaceLODForced = true;
	}
	else if (!bForce && bFaceLODForced)
	{
		Face->SetForcedLOD(0);
		bFaceLODForced = false;
	}
}

ASibeliusHUD* UDancerAgentComponent::GetSibeliusHUD() const
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void UDancerAgentComponent::BeginGreetingMotion()
{
	USkeletalMeshComponent* Mesh = FindDanceMesh();
	if (!Mesh)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Already mid-talk: keep the crawl, just hold a bit longer. She has been restarted
	// from the top of the line, so the hold is the whole clip again, not what was left.
	if (bGreeting)
	{
		World->GetTimerManager().SetTimer(
			GreetingTimer, this, &UDancerAgentComponent::ResumeAfterGreeting,
			TalkHoldSeconds > 0.0f ? TalkHoldSeconds : GreetingSeconds, /*bLoop=*/false);
		return;
	}

	SavedDance = Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay);
	SavedDanceTime = Mesh->GetPosition();
	bGreeting = true;

	// However long the voice clip needs, as set by Greet. Falls back to GreetingSeconds
	// when nobody has spoken (a direct BeginGreetingMotion, or no clip on disk).
	const float HoldSeconds = TalkHoldSeconds > 0.0f ? TalkHoldSeconds : GreetingSeconds;

	// Talk is a close-up, not a celebration: the dance drops to TalkDanceSpeed so her
	// head stays in frame. Not a hard freeze - see the property comment.
	if (UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance())
	{
		if (TalkDanceSpeed > 0.0f)
		{
			Node->SetPlayRate(TalkDanceSpeed);
			Node->SetPlaying(true);
		}
		else
		{
			Node->SetPlaying(false);
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s talking for %.2fs (dance at %.0f%%)"),
			*AgentName, HoldSeconds, TalkDanceSpeed * 100.0f);
	}
	else
	{
		Mesh->bPauseAnims = true;
	}

	World->GetTimerManager().SetTimer(
		GreetingTimer, this, &UDancerAgentComponent::ResumeAfterGreeting,
		HoldSeconds, /*bLoop=*/false);
}

void UDancerAgentComponent::ResumeAfterGreeting()
{
	/* SHE GOT TO THE END OF IT (docs/FUN_PLAN_2.md A2).

	   THIS FUNCTION IS THE NATURAL END AND NOTHING ELSE IS. It runs off GreetingTimer,
	   and CancelGreeting — the F-press path, and the path every other interruption takes —
	   clears that timer before doing anything. So arriving here means she finished
	   speaking, which is precisely the thing the ending needs to know and precisely what
	   EndTalkShot cannot tell you, since every cancel goes through there too.

	   THE GRANT IS CLAIMED HERE RATHER THAN WHEN HE PRESSES E, and that ordering is the
	   whole of it. Claimed at the start, an F-cancel would leave the objective banner
	   silenced ("Nyra is waiting" gone) with no ending rolled and nothing on screen to say
	   what to do — the stale-instruction bug of A4 wearing the opposite coat. Claimed at
	   the end, a cancel records nothing: the banner still points at her, and pressing E
	   again gives him the speech and the ending, which is the correct recovery. */
	if (IsGuide() && GuideStage() >= 4)
	{
		if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this))
		{
			Prog->ClaimOneTimeGrant(GrokTalkedGrant);
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (UDancerAgentSubsystem* Dancers = World->GetSubsystem<UDancerAgentSubsystem>())
		{
			// Broadcast for EVERY guide, not just stage 4. The listener decides what it
			// cares about; a channel that pre-filters is a channel the next feature has to
			// widen. UGrokEndingSubsystem checks the stage at its end.
			Dancers->NotifyGuideTalkFinished(this);
		}
	}

	CancelGreeting(/*bResumeDance=*/true);
}

void UDancerAgentComponent::CancelGreeting(bool bResumeDance)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GreetingTimer);
	}

	// Cut her off mid-word. F on a talking dancer means "stop talking and dance",
	// and a voice that kept playing over the resumed dance would be a ghost.
	if (bTalkShotActive)
	{
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Dancer] %s shot ended: %d update(s), peak mouth %.2f"),
			*AgentName, MouthTickCount, MouthPeak);
	}
	MouthTickCount = 0;
	MouthPeak = 0.0f;

	StopTalkVoice();
	TalkHoldSeconds = 0.0f;

	const bool bWasGreeting = bGreeting;
	bGreeting = false;
	// Tick stays ON — see the constructor. Disabling it here is what made it
	// unrecoverable last time.
	EndTalkShot();

	USkeletalMeshComponent* Mesh = FindDanceMesh();
	if (!Mesh)
	{
		SavedDance = nullptr;
		SavedDanceTime = 0.0f;
		return;
	}

	Mesh->bPauseAnims = false;

	// Unconditional, and BEFORE the early-out: the talk crawl (TalkDanceSpeed) must not
	// survive into the dance she goes back to, whichever way this was reached. A dancer
	// stuck at 10% speed for the rest of the level is the bug this line exists to prevent.
	UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance();
	if (Node)
	{
		Node->SetPlayRate(1.0f);
	}

	if (!bWasGreeting)
	{
		return;
	}

	if (!bResumeDance)
	{
		SavedDance = nullptr;
		SavedDanceTime = 0.0f;
		return;
	}

	if (SavedDance && Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay) == SavedDance)
	{
		/* SHE NEVER LEFT THIS DANCE - the talk only slowed it down. Re-playing it and
		   seeking back to SavedDanceTime would rewind the few seconds she crawled
		   through while speaking, which reads as a hitch the moment the camera lets go.
		   Restoring the rate (above) is the whole job. */
		if (Node)
		{
			Node->SetPlaying(true);
		}
	}
	else if (SavedDance)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->PlayAnimation(SavedDance, /*bLooping=*/true);
		Mesh->SetPosition(SavedDanceTime, /*bFireNotifies=*/false);
	}
	else if (Node)
	{
		Node->SetPlaying(true);
	}

	SavedDance = nullptr;
	SavedDanceTime = 0.0f;
}

bool UDancerAgentComponent::ShuffleDance()
{
	if (Dances.Num() < 2)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Dancer] %s has %d dance(s) — nothing to shuffle. Are the _MH retargets missing?"),
			*AgentName, Dances.Num());
		return false;
	}

	// F mid-greeting must not leave her frozen, and must not resume the old dance
	// on top of the shuffle we are about to play.
	CancelGreeting(/*bResumeDance=*/false);

	// One press, one dance. F repeats every frame while held: Walt's log shows a single
	// brief press producing thirteen changes across consecutive frames 389-401, which
	// reads as the dance strobing rather than switching.
	//
	// A repeat returns TRUE, not false — "handled, nothing to do". Returning false would
	// drop through to the fight, so holding F next to a dancer would start swinging at
	// her, which is the one outcome this whole feature exists to prevent.
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastShuffleTime < ShuffleCooldown)
	{
		return true;
	}

	USkeletalMeshComponent* Mesh = FindDanceMesh();
	if (!Mesh)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Dancer] %s has no skeletal mesh to dance with"), *AgentName);
		return false;
	}

	// Pick a different one: roll across the OTHER n-1 slots, then step past the
	// current index. No rejection loop, so it cannot spin.
	int32 NextIndex = FMath::RandRange(0, Dances.Num() - 2);
	if (CurrentDanceIndex != INDEX_NONE && NextIndex >= CurrentDanceIndex)
	{
		++NextIndex;
	}

	UAnimSequence* NextDance = Dances.IsValidIndex(NextIndex) ? Dances[NextIndex].Get() : nullptr;
	if (!NextDance)
	{
		return false;
	}

	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(NextDance, /*bLooping=*/true);
	CurrentDanceIndex = NextIndex;
	LastShuffleTime = Now;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s -> %s"), *AgentName, *NextDance->GetName());
	return true;
}
