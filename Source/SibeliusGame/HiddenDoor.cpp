// HiddenDoor.cpp
//
// SIB-25 — Ch1 Code Vision hidden door. See header.

#include "HiddenDoor.h"

#include "BranchSubsystem.h"
#include "CathedralDoor.h"          // SIB-44: reuse the static travel guard
#include "CodeVisionComponent.h"
#include "CodeVisionStencil.h"
#include "Engine/GameInstance.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "TravelTransitionSubsystem.h"   // route the level-door OpenLevel through the travel cover
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHiddenDoor, Log, All);

// Bind retry budget: 60 attempts * 0.5s = up to 30s for a late pawn spawn in a
// heavy level. Even if this is exhausted, the MPC fallback keeps the door working.
namespace
{
	constexpr int32 MaxBindAttempts = 60;
	constexpr float BindRetryInterval = 0.5f;
}

AHiddenDoor::AHiddenDoor()
{
	// Tick drives the MPC fallback poll; it's a no-op once the delegate binds.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Visual door panel — outlined by the post-process via custom-depth stencil.
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // visual only
	DoorMesh->SetRenderCustomDepth(true);
	DoorMesh->SetCustomDepthStencilValue(CODEVISION_STENCIL);

	// Blocks the opening when Code Vision is off.
	BlockingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockingBox"));
	BlockingBox->SetupAttachment(SceneRoot);
	BlockingBox->SetBoxExtent(FVector(60.f, 15.f, 110.f)); // ~a doorway; tune in editor
	BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BlockingBox->SetCollisionObjectType(ECC_WorldStatic);
	BlockingBox->SetCollisionResponseToAllChannels(ECR_Block);

	// SIB-44: the inscription. Child of DoorMesh so ApplyState's propagated
	// SetVisibility reveals/hides it with the door. No mesh assigned unless a
	// SignTexture is set (null mesh renders nothing).
	SignMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignMesh"));
	SignMesh->SetupAttachment(DoorMesh);
	SignMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SignMesh->SetCastShadow(false);
}

void AHiddenDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!SignTexture || !SignMesh)
	{
		return;
	}

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SlotFactory/Materials/M_fate_base.M_fate_base"));
	if (!Plane || !Base)
	{
		UE_LOG(LogHiddenDoor, Error, TEXT("[%s] sign needs Plane + M_fate_base (run build_fate_altar.py first)."), *GetName());
		return;
	}

	SignMesh->SetStaticMesh(Plane);
	SignMesh->SetRelativeLocation(SignRelativeLocation);
	SignMesh->SetRelativeRotation(SignRelativeRotation);
	// Engine plane is 100x100 cm; art is 2:1 (1024x512). SignHeight 0 = keep
	// the art's native shape; otherwise stretch vertically as asked.
	const float SignH = (SignHeight > 0.0f) ? SignHeight : SignWidth * 0.5f;
	SignMesh->SetRelativeScale3D(FVector(SignWidth / 100.0f, SignH / 100.0f, 1.0f));

	UMaterialInstanceDynamic* MID = SignMesh->CreateDynamicMaterialInstance(0, Base);
	if (MID)
	{
		MID->SetTextureParameterValue(TEXT("Sprite"), SignTexture);
		MID->SetScalarParameterValue(TEXT("Glow"), 3.0f);   // legible, not blinding
	}
}

void AHiddenDoor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogHiddenDoor, Log, TEXT("[%s] BeginPlay: starting as wall; attempting to bind to player. MPC fallback %s."),
		*GetName(), CodeVisionMPC ? TEXT("available") : TEXT("NOT configured"));

	ApplyState(false); // start as a solid wall
	TryBindToPlayer();
}

void AHiddenDoor::TryBindToPlayer()
{
	if (bBoundToComponent)
	{
		return; // already bound — nothing to do
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCodeVisionComponent* CV = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;

	if (CV)
	{
		CV->OnCodeVisionChanged.AddDynamic(this, &AHiddenDoor::HandleCodeVisionChanged);
		bBoundToComponent = true;
		ApplyState(CV->IsCodeVisionActive()); // snap to current state
		UE_LOG(LogHiddenDoor, Log,
			TEXT("[%s] BOUND to UCodeVisionComponent on pawn '%s' after %d attempt(s); initial active=%s."),
			*GetName(), *GetNameSafe(Pawn), BindAttempts + 1,
			CV->IsCodeVisionActive() ? TEXT("true") : TEXT("false"));
		return;
	}

	// Not ready yet — log why and retry.
	UE_LOG(LogHiddenDoor, Verbose,
		TEXT("[%s] bind attempt %d/%d: pawn=%s, component=%s"),
		*GetName(), BindAttempts + 1, MaxBindAttempts,
		Pawn ? *GetNameSafe(Pawn) : TEXT("NOT FOUND"),
		CV ? TEXT("found") : TEXT("NOT FOUND"));

	if (++BindAttempts < MaxBindAttempts)
	{
		GetWorldTimerManager().SetTimer(BindRetryHandle, this,
			&AHiddenDoor::TryBindToPlayer, BindRetryInterval, false);
	}
	else
	{
		UE_LOG(LogHiddenDoor, Warning,
			TEXT("[%s] gave up binding to the player's UCodeVisionComponent after %d attempts. "
			     "Falling back to MPC poll (%s)."),
			*GetName(), MaxBindAttempts,
			CodeVisionMPC ? TEXT("MPC set") : TEXT("MPC NOT set — door will be stuck as a wall!"));
	}
}

void AHiddenDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Primary path: once the delegate is bound it is authoritative (collision flips
	// on the hard boolean per CV4/CV10). Skip the poll entirely.
	if (bBoundToComponent || !CodeVisionMPC)
	{
		return;
	}

	// Fallback: read the same source of truth via the MPC the component drives.
	const float Active = UKismetMaterialLibrary::GetScalarParameterValue(
		this, CodeVisionMPC, ActiveParameterName);
	const bool bShouldReveal = Active >= RevealThreshold;
	if (bShouldReveal != bRevealedState)
	{
		UE_LOG(LogHiddenDoor, Log,
			TEXT("[%s] MPC fallback: Active=%.2f -> reveal=%s"),
			*GetName(), Active, bShouldReveal ? TEXT("true") : TEXT("false"));
		ApplyState(bShouldReveal);
	}
}

void AHiddenDoor::HandleCodeVisionChanged(bool bIsActive)
{
	UE_LOG(LogHiddenDoor, Log, TEXT("[%s] HandleCodeVisionChanged(%s)"),
		*GetName(), bIsActive ? TEXT("true") : TEXT("false"));
	ApplyState(bIsActive);
}

void AHiddenDoor::ApplyState(bool bRevealed)
{
	bRevealedState = bRevealed;

	// ONE path drives BOTH the visual and the collision (CV4/CV8).
	DoorMesh->SetVisibility(bRevealed, /*bPropagateToChildren=*/true);

	// SIB-44: when revealed the box stops the PAWN no longer, but keeps
	// blocking ECC_Visibility so the interaction trace can FOCUS the obelisk
	// (the panel itself takes E now). Unrevealed = solid wall as ever.
	if (bRevealed)
	{
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BlockingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		BlockingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	else
	{
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		BlockingBox->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

FText AHiddenDoor::GetInteractionPrompt_Implementation() const
{
	return (bRevealedState && !TravelTargetLevel.IsNone()) ? TravelPromptText : FText::GetEmpty();
}

void AHiddenDoor::Interact_Implementation(AActor* Interactor)
{
	if (!bRevealedState || TravelTargetLevel.IsNone())
	{
		return;   // unrevealed wall keeps its secret; no target = plain reveal door
	}

	const UWorld* World = GetWorld();
	UBranchSubsystem* Branch = World ? World->GetSubsystem<UBranchSubsystem>() : nullptr;
	if (!ACathedralDoor::IsTravelAllowed(Branch))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red,
				TEXT("Merge or discard your branches before leaving this world"));
		}
		return;
	}

	UE_LOG(LogHiddenDoor, Display, TEXT("[%s] the wall opens: travel to %s (doorstep %s)"),
		*GetName(), *TravelTargetLevel.ToString(), *ArrivalTag.ToString());
	UTravelTransitionSubsystem::Travel(this, TravelTargetLevel, ArrivalTag);
}

bool AHiddenDoor::RunCollisionSelfTest()
{
	// OFF: blocking + hidden.
	ApplyState(false);
	const bool bOffOK =
		(BlockingBox->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics) &&
		!DoorMesh->IsVisible();

	// ON: pawn-passable + visible. (SIB-44: revealed is QueryOnly — it still
	// blocks the interaction trace so the panel can take E, but never a pawn.)
	ApplyState(true);
	const bool bOnOK =
		(BlockingBox->GetCollisionEnabled() == ECollisionEnabled::QueryOnly) &&
		(BlockingBox->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore) &&
		DoorMesh->IsVisible();

	ApplyState(false); // restore to wall
	return bOffOK && bOnOK;
}
