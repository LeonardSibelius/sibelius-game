// BattleFormComponent.cpp — see header.

#include "BattleFormComponent.h"

#include "SibeliusGame.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

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

	if (USkeletalMesh* Avatar = AvatarMesh.LoadSynchronous())
	{
		Body->SetSkeletalMeshAsset(Avatar);
		if (UClass* AnimClass = AvatarAnimClass.LoadSynchronous())
		{
			Body->SetAnimInstanceClass(AnimClass);
		}
		Body->SetRelativeScale3D(FVector(AvatarScale));
	}
	else
	{
		// Not fatal: the switch is still worth having with his own body in it, and a
		// missing hero should not cost the player a working camera.
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[BattleForm] avatar mesh failed to load - entering with the existing body."));
	}

	Body->SetOwnerNoSee(false);
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

	if (USkeletalMeshComponent* Body = Owner->GetMesh())
	{
		if (SavedBodyMesh)
		{
			Body->SetSkeletalMeshAsset(SavedBodyMesh);
		}
		if (SavedAnimClass)
		{
			Body->SetAnimInstanceClass(SavedAnimClass);
		}
		Body->SetRelativeScale3D(SavedBodyScale);
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
