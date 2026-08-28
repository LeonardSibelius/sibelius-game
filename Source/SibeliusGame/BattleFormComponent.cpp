// BattleFormComponent.cpp — see header.

#include "BattleFormComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"

#include "SibeliusGame.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

/* THE BISECT. Every property reads correct - in form, right camera, view 386 cm behind
   the pawn, mesh present at the pawn, two metres tall, 17 materials none null, main pass
   on, visible to its owner - and a captured frame shows empty meadow. Properties and
   pixels disagree, so one more property will not settle it.

   So: split the problem. With this at 0 the camera swap happens and the mesh swap does
   NOT, leaving his own template body in third person.

     body appears    -> the camera and visibility path is fine, and the fault is in
                        Greystone's mesh or its materials
     nothing appears -> the fault is in the switch itself, and Greystone was never
                        the subject

   Two outcomes, one run, and no third guess. */
static TAutoConsoleVariable<int32> GBattleUseAvatar(
	TEXT("battle.UseAvatar"),
	1,

	TEXT("1: wear Greystone in battle form. 0: keep your own body (bisect test)."),
	ECVF_Default);


UBattleFormComponent::UBattleFormComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Defaults point at Greystone because that is what this project already owns: 174
	// animations including a full melee combo set, on the UE4 skeleton that
	// RTG_UE4_to_MetaHuman already bridges. Soft pointers, so an office-only session
	// never loads a 300 MB hero it is not going to show.
	AvatarMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone.Greystone")));
	AvatarAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Greystone_AnimBlueprint.Greystone_AnimBlueprint_C")));
}

void UBattleFormComponent::BeginPlay()
{
	Super::BeginPlay();
	bInBattleForm = false;
}

ACharacter* UBattleFormComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

USkeletalMeshComponent* UBattleFormComponent::FindArmsMesh() const
{
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner)
	{
		return nullptr;
	}
	// By elimination rather than by name: the arms are the skeletal mesh that is not the
	// body. Reaching for the character's private FirstPersonMesh would couple this
	// component to a member it has no business knowing.
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* M : Meshes)
	{
		if (M && M != Owner->GetMesh())
		{
			return M;
		}
	}
	return nullptr;
}

void UBattleFormComponent::EnsureRig()
{
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner || Boom)
	{
		return;
	}

	// Runtime creation order matters: NewObject, then properties, then RegisterComponent,
	// then attach. Register before attach and the component exists but hangs off nothing;
	// attach before register and the attachment is silently dropped.
	Boom = NewObject<USpringArmComponent>(Owner, TEXT("BattleBoom"));
	Boom->TargetArmLength = BoomLength;
	Boom->SocketOffset = BoomSocketOffset;
	Boom->bUsePawnControlRotation = true;   // the mouse still aims the camera
	Boom->bEnableCameraLag = CameraLagSpeed > 0.0f;
	Boom->CameraLagSpeed = CameraLagSpeed;
	Boom->bDoCollisionTest = true;          // walk backwards into a hill, keep the shot
	Boom->RegisterComponent();
	Boom->AttachToComponent(Owner->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Boom->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));

	BattleCamera = NewObject<UCameraComponent>(Owner, TEXT("BattleCamera"));
	BattleCamera->FieldOfView = BattleFieldOfView;
	BattleCamera->bUsePawnControlRotation = false;   // the boom already did it
	BattleCamera->RegisterComponent();
	BattleCamera->AttachToComponent(Boom, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		USpringArmComponent::SocketName);
	BattleCamera->SetActive(false);
}

void UBattleFormComponent::SetCameraPowersSuspended(bool bSuspended)
{
	if (!bSuspendCameraPowers)
	{
		return;
	}
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner)
	{
		return;
	}
	/* Suspends the TARGETING half only — the per-frame camera ray that decides what is
	   under the crosshair. Without this, in battle form the E prompt and the Code Vision
	   highlight are picking things three metres behind his back.

	   By class name through FindComponentByClass so this file does not need to include
	   three power headers to switch three ticks off. The input half still fires; see the
	   header. */
	static const TCHAR* PowerComponents[] = {
		TEXT("InteractorComponent"), TEXT("CodeVisionComponent"), TEXT("RefactorComponent"),
	};
	for (UActorComponent* C : Owner->GetComponents())
	{
		if (!C)
		{
			continue;
		}
		const FString ClassName = C->GetClass()->GetName();
		for (const TCHAR* Wanted : PowerComponents)
		{
			if (ClassName == Wanted)
			{
				C->SetComponentTickEnabled(!bSuspended);
				break;
			}
		}
	}
}

void UBattleFormComponent::EnterBattleForm()
{
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner || bInBattleForm)
	{
		return;
	}
	EnsureRig();
	if (!BattleCamera)
	{
		UE_LOG(LogSibeliusGame, Error, TEXT("[BattleForm] no camera rig - owner is not a Character?"));
		return;
	}

	USkeletalMeshComponent* Body = Owner->GetMesh();
	if (!Body)
	{
		return;
	}

	// ---- the body he has never seen -------------------------------------------
	SavedBodyMesh = Body->GetSkeletalMeshAsset();
	SavedAnimClass = Body->GetAnimClass();
	SavedBodyScale = Body->GetRelativeScale3D();

	const bool bWearAvatar = GBattleUseAvatar.GetValueOnGameThread() != 0;
	USkeletalMesh* Avatar = bWearAvatar ? AvatarMesh.LoadSynchronous() : nullptr;
	UE_LOG(LogSibeliusGame, Display, TEXT("[BattleForm] useAvatar=%d avatar=%s"),
		bWearAvatar ? 1 : 0, *GetNameSafe(Avatar));
	if (Avatar)
	{
		/* A FRESH COMPONENT, matching the character body's own relative transform so he
		   stands where the template puts a body - the -90 yaw and the drop to the capsule
		   floor are the template's, not ours to re-derive. Runtime order is NewObject,
		   properties, RegisterComponent, attach: register before attach and the component
		   hangs off nothing, attach before register and the attachment is dropped. */
		AvatarBody = NewObject<USkeletalMeshComponent>(Owner, TEXT("BattleAvatarBody"));
		AvatarBody->SetSkeletalMeshAsset(Avatar);
		if (UClass* AnimClass = AvatarAnimClass.LoadSynchronous())
		{
			/* MODE FIRST, THEN CLASS. A runtime-created component defaults to
			   AnimationSingleNode; handing it an AnimBlueprint class does not by itself
			   put it into blueprint mode, so the graph never runs - the mesh stands in
			   whatever pose it was given and slides. Which is exactly what a skating
			   Greystone looks like. */
			AvatarBody->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			AvatarBody->SetAnimInstanceClass(AnimClass);
		}
		AvatarBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AvatarBody->RegisterComponent();
		AvatarBody->AttachToComponent(Owner->GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		AvatarBody->SetRelativeTransform(Body->GetRelativeTransform());
		AvatarBody->SetRelativeScale3D(FVector(AvatarScale));

		// The template body steps aside entirely - two bodies in one capsule is one too
		// many, and hiding it also kills the shadow that has been the only visible sign
		// of him all evening.
		Body->SetVisibility(false, true);

		// Read it back rather than trust the setters - the lesson of the whole evening.
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[BattleForm] avatar body: mesh=%s animMode=%d animInstance=%s"),
			*GetNameSafe(AvatarBody->GetSkeletalMeshAsset()),
			static_cast<int32>(AvatarBody->GetAnimationMode()),
			*GetNameSafe(AvatarBody->GetAnimInstance()));
	}
	else
	{
		// Not fatal: the switch is still worth having with his own body in it, and a
		// missing hero should not cost the player a working camera.
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[BattleForm] avatar mesh failed to load - entering with the existing body."));
		Body->SetOwnerNoSee(false);
	}
	if (USkeletalMeshComponent* Arms = FindArmsMesh())
	{
		Arms->SetVisibility(false, true);
	}

	// ---- he faces where he runs ------------------------------------------------
	if (UCharacterMovementComponent* Move = Owner->GetCharacterMovement())
	{
		bSavedOrientToMovement = Move->bOrientRotationToMovement;
		SavedRotationRate = Move->RotationRate;
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.0f, TurnRateDegPerSec, 0.0f);
	}
	bSavedUseControllerYaw = Owner->bUseControllerRotationYaw;
	Owner->bUseControllerRotationYaw = false;

	// ---- the camera -------------------------------------------------------------
	// AActor::CalcCamera takes the first ACTIVE camera component, so the swap is two
	// SetActive calls. Every other camera goes off by iteration rather than by naming
	// the character's private first-person one.
	TArray<UCameraComponent*> Cameras;
	Owner->GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Cam : Cameras)
	{
		if (Cam && Cam != BattleCamera)
		{
			Cam->SetActive(false);
		}
	}
	BattleCamera->SetActive(true);

	SetCameraPowersSuspended(true);
	bInBattleForm = true;

	/* NO BLEND, ON PURPOSE. A transformation is a cut. Lerping the camera out of his
	   skull over three quarters of a second reads as a mistake in the level, where a hard
	   change on the beat the agents grant the body reads as the grant. The camera lag on
	   the boom softens the first half second by itself. */
	UE_LOG(LogSibeliusGame, Display, TEXT("[BattleForm] entered."));
}

void UBattleFormComponent::ExitBattleForm()
{
	ACharacter* Owner = GetCharacterOwner();
	if (!Owner || !bInBattleForm)
	{
		return;
	}

	// The avatar was our component, so taking it away is destroying it - not unpicking a
	// swap. Nothing of the character's own body was changed while it was worn.
	if (AvatarBody)
	{
		AvatarBody->DestroyComponent();
		AvatarBody = nullptr;
	}
	if (USkeletalMeshComponent* Body = Owner->GetMesh())
	{
		Body->SetVisibility(true, true);
		Body->SetOwnerNoSee(true);
	}
	if (USkeletalMeshComponent* Arms = FindArmsMesh())
	{
		Arms->SetVisibility(true, true);
	}

	if (UCharacterMovementComponent* Move = Owner->GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = bSavedOrientToMovement;
		Move->RotationRate = SavedRotationRate;
	}
	Owner->bUseControllerRotationYaw = bSavedUseControllerYaw;

	if (BattleCamera)
	{
		BattleCamera->SetActive(false);
	}
	TArray<UCameraComponent*> Cameras;
	Owner->GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Cam : Cameras)
	{
		if (Cam && Cam != BattleCamera)
		{
			Cam->SetActive(true);
			break;   // the first-person camera, and only it
		}
	}

	SetCameraPowersSuspended(false);
	bInBattleForm = false;

	UE_LOG(LogSibeliusGame, Display, TEXT("[BattleForm] exited."));
}

// ---------------------------------------------------------------- console commands
//
// So the form can be tried before anything grants it. The fiction is that the agents
// hand him the body; until that scene exists, these stand in for it.

#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	UBattleFormComponent* FindBattleForm(UWorld* World)
	{
		APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UBattleFormComponent>() : nullptr;
	}
}

/* battle.Status - what the battle form ACTUALLY is right now, not what it should be.

   Walt, in battle form with 30 demons converging: "I am up in the air and they run
   under me. I see no sword." Three different things could produce that sentence - the
   first-person camera never handed over, the avatar mesh never swapped, or 30 character
   capsules converging on one capsule shoved him onto the top of the pile - and they
   need completely different fixes. Guessing between them has cost this project a round
   trip per guess all evening.

   So: print all three at once. Height above the floor comes from a downward trace, not
   from Z, because Z on a kilometre-wide landscape means nothing on its own. */
static FAutoConsoleCommandWithWorld GBattleStatus(
	TEXT("battle.Status"),
	TEXT("Dump the live battle-form state: camera, avatar mesh, and height off the floor."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			APawn* P = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			ACharacter* C = Cast<ACharacter>(P);
			if (!C) { UE_LOG(LogSibeliusGame, Warning, TEXT("[BattleStatus] no player character.")); return; }

			const UBattleFormComponent* B = C->FindComponentByClass<UBattleFormComponent>();

			// How far off the floor is he really?
			float Height = -1.0f;
			FHitResult Hit;
			const FVector From = C->GetActorLocation();
			if (World->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 100000.0f),
					ECC_WorldStatic, FCollisionQueryParams(SCENE_QUERY_STAT(BattleStatus), false, C)))
			{
				Height = static_cast<float>(From.Z - Hit.ImpactPoint.Z);
			}

			FString CamName = TEXT("NONE ACTIVE");
			TArray<UCameraComponent*> Cams;
			C->GetComponents<UCameraComponent>(Cams);
			FString AllCams;
			for (const UCameraComponent* Cam : Cams)
			{
				if (!Cam) { continue; }
				AllCams += FString::Printf(TEXT("%s(%s) "), *Cam->GetName(), Cam->IsActive() ? TEXT("ON") : TEXT("off"));
				if (Cam->IsActive()) { CamName = Cam->GetName(); }
			}

			FString MeshName = TEXT("none"), AnimName = TEXT("none");
			bool bOwnerNoSee = false, bVisible = false;
			if (USkeletalMeshComponent* M = C->GetMesh())
			{
				MeshName = GetNameSafe(M->GetSkeletalMeshAsset());
				AnimName = GetNameSafe(M->GetAnimClass());
				bOwnerNoSee = M->bOwnerNoSee;
				bVisible = M->IsVisible();
			}

			UE_LOG(LogSibeliusGame, Display,
				TEXT("[BattleStatus] inForm=%d | heightOffFloor=%.0f cm | activeCam=%s | cams: %s"),
				B ? (B->IsInBattleForm() ? 1 : 0) : -1, Height, *CamName, *AllCams);
			UE_LOG(LogSibeliusGame, Display,
				TEXT("[BattleStatus] mesh=%s anim=%s ownerNoSee=%d visible=%d"),
				*MeshName, *AnimName, bOwnerNoSee ? 1 : 0, bVisible ? 1 : 0);

			/* WHERE THE CAMERA ACTUALLY IS. Everything above reported correct - in form,
			   right camera, right mesh, visible to its owner - and Walt still saw only a
			   shadow. So the remaining question is not WHICH camera but WHERE, and a name
			   does not answer that. A spring arm with bDoCollisionTest collapses toward
			   the pawn when its probe hits anything, and a collapsed arm puts the camera
			   inside the head it was supposed to be behind. */
			FVector CamLoc = FVector::ZeroVector;
			float Arm = -1.0f, ArmActual = -1.0f;
			for (const UCameraComponent* Cam : Cams)
			{
				if (Cam && Cam->IsActive()) { CamLoc = Cam->GetComponentLocation(); }
			}
			if (const USpringArmComponent* B2 = C->FindComponentByClass<USpringArmComponent>())
			{
				Arm = B2->TargetArmLength;
				ArmActual = static_cast<float>(
					FVector::Dist(B2->GetComponentLocation(), B2->GetSocketTransform(USpringArmComponent::SocketName).GetLocation()));
			}
			/* AND WHERE THE MESH IS, AND WHERE THE CAMERA LOOKS.

			   Camera correct, mesh visible, owner allowed to see it - and still nothing on
			   screen. The two things never measured are whether the mesh is WHERE the pawn
			   is (visible=1 says it is drawing, not that it is drawing HERE) and whether
			   the camera is pointed AT it. A mesh at the far end of the level and a camera
			   aimed at the sky look identical from the player's chair. */
			FVector MeshOrigin = FVector::ZeroVector, MeshExtent = FVector::ZeroVector;
			if (USkeletalMeshComponent* M2 = C->GetMesh())
			{
				const FBoxSphereBounds Bx = M2->Bounds;
				MeshOrigin = Bx.Origin;
				MeshExtent = Bx.BoxExtent;
			}
			FRotator CamRot = FRotator::ZeroRotator;
			for (const UCameraComponent* Cam : Cams)
			{
				if (Cam && Cam->IsActive()) { CamRot = Cam->GetComponentRotation(); }
			}
			// Is the mesh in front of the camera at all? Positive means ahead of it.
			const float Ahead = static_cast<float>(FVector::DotProduct(
				(MeshOrigin - CamLoc).GetSafeNormal(), CamRot.Vector()));
			/* WHERE THE PLAYER'S EYE ACTUALLY IS - the measurement missing all along.

			   Every number so far describes the camera COMPONENT: its Active flag, its
			   world position, its rotation. None of them is the view. The engine only uses
			   that component if the player controller's view target resolves to this pawn
			   and AActor::CalcCamera picks it; if anything else holds the view target, the
			   component can sit in exactly the right place, active and correct, while the
			   player looks through something else entirely.

			   Which would explain everything at once: a first-person camera left inside
			   Greystone's skull sees past his head to empty meadow, sits higher than usual
			   because Greystone is taller than the template body, and leaves his shadow on
			   the grass beneath. Measured now instead of reasoned about. */
			if (APlayerController* PC = Cast<APlayerController>(C->GetController()))
			{
				FVector EyeLoc = FVector::ZeroVector;
				FRotator EyeRot = FRotator::ZeroRotator;
				PC->GetPlayerViewPoint(EyeLoc, EyeRot);
				UE_LOG(LogSibeliusGame, Display,
					TEXT("[BattleStatus] VIEWPOINT eye=%s rot=%s | distFromPawn=%.0f cm | viewTarget=%s"),
					*EyeLoc.ToCompactString(), *EyeRot.ToCompactString(),
					static_cast<float>(FVector::Dist(EyeLoc, C->GetActorLocation())),
					*GetNameSafe(PC->GetViewTarget()));
			}

			/* EVERY FLAG THAT CAN SUPPRESS A MAIN-PASS DRAW WHILE KEEPING THE SHADOW.

			   Walt sees a Greystone-shaped shadow holding a greatsword and no Greystone.
			   Position, bounds, camera and aim have all measured correct, so the body is
			   in frame and simply not painted. Exactly one family of causes does that:
			   bRenderInMainPass off, bOnlyOwnerSee pointing at somebody else, hidden with
			   bCastHiddenShadow on, or a material array that failed to resolve.

			   Printed together rather than one per build. Picking a favourite and testing
			   it has cost a round trip every time tonight. */
			if (USkeletalMeshComponent* M3 = C->GetMesh())
			{
				int32 NullMats = 0;
				const int32 NumMats = M3->GetNumMaterials();
				for (int32 Mi = 0; Mi < NumMats; ++Mi)
				{
					if (!M3->GetMaterial(Mi)) { ++NullMats; }
				}
				UE_LOG(LogSibeliusGame, Display,
					TEXT("[BattleStatus] renderFlags mainPass=%d onlyOwnerSee=%d hiddenInGame=%d ")
					TEXT("castHiddenShadow=%d visFlag=%d owner=%s | materials=%d null=%d | boundScale=%.2f"),
					M3->bRenderInMainPass ? 1 : 0, M3->bOnlyOwnerSee ? 1 : 0,
					M3->bHiddenInGame ? 1 : 0, M3->bCastHiddenShadow ? 1 : 0,
					M3->GetVisibleFlag() ? 1 : 0, *GetNameSafe(M3->GetOwner()),
					NumMats, NullMats, M3->GetComponentScale().X);
			}

			UE_LOG(LogSibeliusGame, Display,
				TEXT("[BattleStatus] meshBounds origin=%s extent=%s | camRot=%s | meshAheadOfCam=%.2f"),
				*MeshOrigin.ToCompactString(), *MeshExtent.ToCompactString(),
				*CamRot.ToCompactString(), Ahead);

			UE_LOG(LogSibeliusGame, Display,
				TEXT("[BattleStatus] pawn=%s cam=%s camDist=%.0f cm | boom target=%.0f actual=%.0f"),
				*C->GetActorLocation().ToCompactString(), *CamLoc.ToCompactString(),
				static_cast<float>(FVector::Dist(C->GetActorLocation(), CamLoc)), Arm, ArmActual);
		}));

static FAutoConsoleCommandWithWorld GBattleToggle(
	TEXT("battle.Toggle"),
	TEXT("Take or give back the battle avatar: third-person camera, visible body, Greystone."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			if (UBattleFormComponent* C = FindBattleForm(World))
			{
				C->IsInBattleForm() ? C->ExitBattleForm() : C->EnterBattleForm();
			}
			else
			{
				UE_LOG(LogSibeliusGame, Error, TEXT("[BattleForm] player pawn has no UBattleFormComponent."));
			}
		}));

static FAutoConsoleCommandWithWorld GBattleEnter(
	TEXT("battle.Enter"),
	TEXT("Take the battle avatar."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World) { if (UBattleFormComponent* C = FindBattleForm(World)) { C->EnterBattleForm(); } }));

static FAutoConsoleCommandWithWorld GBattleExit(
	TEXT("battle.Exit"),
	TEXT("Give it back. Restores every value entering saved."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World) { if (UBattleFormComponent* C = FindBattleForm(World)) { C->ExitBattleForm(); } }));
