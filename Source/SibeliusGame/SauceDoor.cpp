// SauceDoor.cpp — see header. Reveal pattern cloned from AHiddenDoor; arming is the
// Sauce, travel is a staged Elsewhere instead of a fixed level.

#include "SauceDoor.h"
#include "SauceCauldron.h"
#include "ElsewhereSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSauceDoor, Log, All);

ASauceDoor::ASauceDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // visual only
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		DoorMesh->SetStaticMesh(Cube);
		DoorMesh->SetRelativeScale3D(FVector(0.2f, 1.2f, 2.2f));   // a doorway slab
	}

	BlockingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockingBox"));
	BlockingBox->SetupAttachment(SceneRoot);
	BlockingBox->SetBoxExtent(FVector(60.f, 15.f, 110.f));
	BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BlockingBox->SetCollisionObjectType(ECC_WorldStatic);
	BlockingBox->SetCollisionResponseToAllChannels(ECR_Block);
}

void ASauceDoor::BeginPlay()
{
	Super::BeginPlay();

	ApplyState(false);   // starts as a solid wall

	if (Cauldron)
	{
		Cauldron->OnSauceComplete.AddDynamic(this, &ASauceDoor::HandleSauceComplete);
		if (Cauldron->IsComplete())
		{
			Arm();   // Sauce already done (late door) — snap armed
		}
	}

	if (bStartArmed)
	{
		Arm();
	}
}

void ASauceDoor::HandleSauceComplete()
{
	UE_LOG(LogSauceDoor, Display, TEXT("[%s] the Sauce completed — the door cracks open."), *GetName());
	Arm();
}

void ASauceDoor::Arm()
{
	if (bArmed)
	{
		return;
	}
	bArmed = true;
	ApplyState(true);
}

void ASauceDoor::ApplyState(bool bRevealed)
{
	// ONE path drives BOTH the visual and the collision (HiddenDoor CV4 discipline).
	DoorMesh->SetVisibility(bRevealed, /*bPropagateToChildren=*/true);

	if (bRevealed)
	{
		// Passable to the pawn, but still blocks the interaction trace so the panel
		// takes E (same as the revealed HiddenDoor).
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

FText ASauceDoor::GetInteractionPrompt_Implementation() const
{
	return bArmed ? TravelPromptText : FText::GetEmpty();   // an unarmed wall keeps its secret
}

void ASauceDoor::Interact_Implementation(AActor* /*Interactor*/)
{
	if (!bArmed)
	{
		return;
	}

	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	if (!Elsewhere)
	{
		UE_LOG(LogSauceDoor, Error, TEXT("[%s] no UElsewhereSubsystem — cannot stage an Elsewhere."), *GetName());
		return;
	}

	// Roll + stage a fresh Elsewhere (held across the travel), then step through.
	const FElsewherePlan Plan = Elsewhere->StageNextElsewhere();
	if (!Plan.IsValid())
	{
		UE_LOG(LogSauceDoor, Error, TEXT("[%s] staged an invalid Elsewhere — not travelling."), *GetName());
		return;
	}

	UE_LOG(LogSauceDoor, Display, TEXT("[%s] stepping through to %s (place=%s, curio=%s, seed=%d)."),
		*GetName(), *ElsewhereLevelName.ToString(), *Plan.PlaceTypeId.ToString(), *Plan.CurioId.ToString(), Plan.Seed);
	UGameplayStatics::OpenLevel(this, ElsewhereLevelName);
}

bool ASauceDoor::RunArmGateSelfTest()
{
	// Unarmed: no prompt, box blocks (reads as wall).
	ApplyState(false);
	bArmed = false;
	const bool bUnarmedOK =
		GetInteractionPrompt_Implementation().IsEmpty() &&
		(BlockingBox->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);

	// Armed: prompt shows, pawn-passable but trace-blocking, panel visible.
	Arm();
	const bool bArmedOK =
		!GetInteractionPrompt_Implementation().IsEmpty() &&
		(BlockingBox->GetCollisionEnabled() == ECollisionEnabled::QueryOnly) &&
		(BlockingBox->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore) &&
		DoorMesh->IsVisible();

	// Restore to wall.
	bArmed = false;
	ApplyState(false);
	return bUnarmedOK && bArmedOK;
}
