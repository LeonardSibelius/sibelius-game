// ReturnDoor.cpp — see header.

#include "ReturnDoor.h"
#include "ElsewhereSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogReturnDoor, Log, All);

AReturnDoor::AReturnDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Block);   // takes the interact trace

	// A real kit door frame so the way home reads as a door, not a checkered placeholder
	// cube. Falls back to an engine slab if the kit isn't installed (never a bare cube in
	// the player's face — the builder also seats it in the wall gap, not mid-hall).
	if (UStaticMesh* Door = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ModularSciFiEnv_K/Meshes/Doors/SM_Door_A_3x4m_Base.SM_Door_A_3x4m_Base")))
	{
		DoorMesh->SetStaticMesh(Door);   // kit mesh authored to grid; scale 1
	}
	else if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		DoorMesh->SetStaticMesh(Cube);
		DoorMesh->SetRelativeScale3D(FVector(0.2f, 1.2f, 2.2f));
	}

	// The way-home sign plaque — child of DoorMesh so it rides the door's facing. No mesh until
	// OnConstruction wires it (only when SignTexture loaded).
	SignMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignMesh"));
	SignMesh->SetupAttachment(DoorMesh);
	SignMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SignMesh->SetCastShadow(false);

	// "THE WAY HOME" art (Walt's, committed under /Game/Signs). Loaded by path because this door is
	// runtime-spawned; absent on a fresh clone before import -> SignTexture null -> sign skipped.
	SignTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Signs/T_Sign_TheWayHome.T_Sign_TheWayHome"));

	// Self-contained walk-through return trigger — armed in BeginPlay ONLY when bSelfReturnTrigger.
	// An overlap box around the door: QueryOnly, Pawn-overlap only, hidden — never blocks movement
	// or the interact trace. Same shape/role as AElsewhereBuilder::SeatReturnTrigger's box.
	ReturnTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ReturnTrigger"));
	ReturnTrigger->SetupAttachment(DoorMesh);
	ReturnTrigger->SetBoxExtent(FVector(110.0f, 160.0f, 130.0f));
	ReturnTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReturnTrigger->SetCollisionObjectType(ECC_WorldStatic);
	ReturnTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReturnTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ReturnTrigger->SetGenerateOverlapEvents(true);
	ReturnTrigger->SetCanEverAffectNavigation(false);
	ReturnTrigger->SetHiddenInGame(true);
}

void AReturnDoor::OnConstruction(const FTransform& Transform)
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
		UE_LOG(LogReturnDoor, Error, TEXT("[%s] sign needs Plane + M_fate_base (run build_fate_altar.py first)."), *GetName());
		return;
	}

	SignMesh->SetStaticMesh(Plane);
	SignMesh->SetRelativeLocation(SignRelativeLocation);
	SignMesh->SetRelativeRotation(SignRelativeRotation);
	// Engine plane is 100x100 cm; art is 1.6:1. SignHeight 0 = width/2; else stretch vertically.
	const float SignH = (SignHeight > 0.0f) ? SignHeight : SignWidth * 0.5f;
	SignMesh->SetRelativeScale3D(FVector(SignWidth / 100.0f, SignH / 100.0f, 1.0f));

	if (UMaterialInstanceDynamic* MID = SignMesh->CreateDynamicMaterialInstance(0, Base))
	{
		MID->SetTextureParameterValue(TEXT("Sprite"), SignTexture);
		MID->SetScalarParameterValue(TEXT("Glow"), 3.0f);   // legible, not blinding
	}
}

void AReturnDoor::BeginPlay()
{
	Super::BeginPlay();
	if (bSelfReturnTrigger && ReturnTrigger)
	{
		ReturnTrigger->OnComponentBeginOverlap.AddDynamic(this, &AReturnDoor::OnReturnTriggerBeginOverlap);
		// Arm after a short delay so arriving AT the door (the forest PlayerStart is right here)
		// doesn't instant-return — the player must walk away and come back to leave.
		if (UWorld* World = GetWorld())
		{
			FTimerHandle ArmTimer;
			World->GetTimerManager().SetTimer(ArmTimer,
				FTimerDelegate::CreateWeakLambda(this, [this]() { bReturnArmed = true; }),
				FMath::Max(0.05f, ReturnArmDelay), /*bLoop=*/false);
		}
	}
}

void AReturnDoor::OnReturnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!bSelfReturnTrigger || bReturningHome || !bReturnArmed)
	{
		return;   // disabled, already returning, or still in the spawn-frame arm delay
	}
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;   // only the player returns home
	}
	GoHome();
}

void AReturnDoor::GoHome()
{
	if (bReturningHome)
	{
		return;   // already travelling (OpenLevel is async)
	}
	bReturningHome = true;

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (UElsewhereSubsystem* Elsewhere = GI->GetSubsystem<UElsewhereSubsystem>())
		{
			Elsewhere->DiscardStagedElsewhere();   // the room is throwaway (§4)
		}
	}

	UE_LOG(LogReturnDoor, Display, TEXT("[%s] returning home to %s; Elsewhere discarded."),
		*GetName(), *HomeLevelName.ToString());
	UGameplayStatics::OpenLevel(this, HomeLevelName);
}

void AReturnDoor::Interact_Implementation(AActor* /*Interactor*/)
{
	GoHome();   // E on the door also returns (cathedral + forest)
}

FText AReturnDoor::GetInteractionPrompt_Implementation() const
{
	return ReturnPromptText;
}
