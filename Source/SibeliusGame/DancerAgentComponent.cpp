// DancerAgentComponent.cpp — see header.

#include "DancerAgentComponent.h"

#include "SibeliusHUD.h"
#include "SibeliusGame.h"                  // LogSibeliusGame
#include "DancerAgentSubsystem.h"          // the aim-assist registry
#include "PowerGrant.h"                    // SPINE: she hands the power over

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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

UDancerAgentComponent::UDancerAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

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
	if (bGreeting && bTalkShotActive)
	{
		UpdateTalkShot();
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
		FaceThePlayer();
		UpdateTalkShot();
		PC->SetViewTargetWithBlend(TalkCamera, TalkCameraBlendIn, VTBlend_Cubic);
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
	LockTalkInput(false);
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

	const FString Line = FString::Printf(
		TEXT("Hi.  I am AI Agent %s.  Wanna Fight?  (I don't really fight, I just dance)"),
		*AgentName);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowPresenceLine(Line, GreetingSeconds);
		}
		else
		{
			UE_LOG(LogSibeliusGame, Display,
				TEXT("[Dancer] %s greeted, but this level's HUD has no subtitle channel"), *AgentName);
		}
	}

	if (bGreeting)
	{
		BeginGreetingMotion();
		return;
	}

	BeginGreetingMotion();
	SetComponentTickEnabled(true);
	BeginTalkShot();
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

	// Already mid-talk: keep the pause, just hold a bit longer.
	if (bGreeting)
	{
		World->GetTimerManager().SetTimer(
			GreetingTimer, this, &UDancerAgentComponent::ResumeAfterGreeting,
			GreetingSeconds, /*bLoop=*/false);
		return;
	}

	SavedDance = Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay);
	SavedDanceTime = Mesh->GetPosition();
	bGreeting = true;

	float HoldSeconds = GreetingSeconds;

	// Talk is a close-up, not a celebration. Freeze the dance so her head stays
	// in frame; the Face mesh does the talking.
	if (UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance())
	{
		Node->SetPlaying(false);
		UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s paused dance to talk (%.2fs)"),
			*AgentName, HoldSeconds);
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
	CancelGreeting(/*bResumeDance=*/true);
}

void UDancerAgentComponent::CancelGreeting(bool bResumeDance)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GreetingTimer);
	}

	const bool bWasGreeting = bGreeting;
	bGreeting = false;
	SetComponentTickEnabled(false);
	EndTalkShot();

	USkeletalMeshComponent* Mesh = FindDanceMesh();
	if (!Mesh)
	{
		SavedDance = nullptr;
		SavedDanceTime = 0.0f;
		return;
	}

	Mesh->bPauseAnims = false;

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

	if (SavedDance)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->PlayAnimation(SavedDance, /*bLooping=*/true);
		Mesh->SetPosition(SavedDanceTime, /*bFireNotifies=*/false);
	}
	else if (UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance())
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
