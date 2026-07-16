// RefactorComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorComponent.h"
#include "RefactorableComponent.h"
#include "RefuserController.h"      // wild refactor: only Refusers transmute
#include "SibeliusGame.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"

URefactorComponent::URefactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// APPEAL-R: the default Menagerie — creatures already on disk, hard CDO
	// references so all of them actually cook (the Death_Back lesson).
	// NOT the EasyBiomes insects: their material flaps the wings via world
	// position offset tuned for life size, and a chair-sized butterfly is a
	// room-filling smear (Walt: "it turned into a tornado").
	const TCHAR* CreaturePaths[] = {
		TEXT("/Game/Dragon_Rise/Meshes/SM_Dragons.SM_Dragons"),
		TEXT("/Game/HouseFurniture/Meshes/SM_ToyRabbit_A1.SM_ToyRabbit_A1"),
	};
	for (const TCHAR* Path : CreaturePaths)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(Path);
		if (Finder.Succeeded())
		{
			Menagerie.Add(Finder.Object);
		}
	}
}

void URefactorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CurrentTarget = TraceForRefactorable();
}

URefactorableComponent* URefactorComponent::TraceForRefactorable() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetCameraRotation().Vector() * TraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			// R4: only ever return an actor that is actually refactorable.
			return HitActor->FindComponentByClass<URefactorableComponent>();
		}
	}
	return nullptr;
}

void URefactorComponent::TriggerRefactor()
{
	// Pre-authored refactorables always win — the office puzzle objects keep
	// their two hand-built states and never roll the menagerie dice.
	if (CurrentTarget)
	{
		CurrentTarget->ToggleRefactor();
		return;
	}

	TryWildRefactor();
}

bool URefactorComponent::TryWildRefactor()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !PC->PlayerCameraManager || !World || Menagerie.Num() == 0)
	{
		return false;
	}

	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetCameraRotation().Vector() * TraceDistance;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}
	AActor* Target = Hit.GetActor();
	if (!Target)
	{
		return false;
	}

	// --- R on an already-refactored prop: restore the first draft. ---
	if (UWildRefactorState* State = Target->FindComponentByClass<UWildRefactorState>())
	{
		if (UStaticMeshComponent* SMC = Target->FindComponentByClass<UStaticMeshComponent>())
		{
			SMC->SetStaticMesh(State->OriginalMesh);
			SMC->SetWorldScale3D(State->OriginalScale);
		}
		State->DestroyComponent();
		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s restored."), *Target->GetName());
		return true;
	}

	// --- R on a Refuser: he was always meant to be something gentler. ---
	if (ACharacter* Victim = Cast<ACharacter>(Target))
	{
		if (Cast<ARefuserController>(Victim->GetController()) == nullptr)
		{
			return false;   // players, Shinbi, and civilians are not clay
		}
		if (AController* VC = Victim->GetController())
		{
			VC->StopMovement();
			VC->UnPossess();
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Creature = World->SpawnActor<AStaticMeshActor>(
			Victim->GetActorLocation(), Victim->GetActorRotation(), SpawnParams);
		if (Creature)
		{
			Creature->SetMobility(EComponentMobility::Movable);
			ApplyCreature(Creature->GetStaticMeshComponent(), PickCreature(), RefuserCreatureSize);
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Refuser transmuted."));
		Victim->Destroy();
		return true;
	}

	// --- R on a plain prop: desk becomes creature, size-matched. ---
	// Plain AStaticMeshActor ONLY (Blueprint props carry logic — a microwave
	// that is secretly a rabbit still owes the player a working door), unless
	// the actor opts in with the WildRefactorOK tag.
	const bool bPlainProp = Target->IsA<AStaticMeshActor>() || Target->ActorHasTag(TEXT("WildRefactorOK"));
	if (!bPlainProp || Target->ActorHasTag(TEXT("NoWildRefactor")))
	{
		return false;
	}
	UStaticMeshComponent* SMC = Target->FindComponentByClass<UStaticMeshComponent>();
	if (!SMC || !SMC->GetStaticMesh())
	{
		return false;
	}

	const FVector Extent = Target->GetComponentsBoundingBox().GetSize();
	const float LargestSide = Extent.GetMax();
	if (LargestSide > MaxWildTargetSize)
	{
		return false;   // walls, floors, buildings: not zoo material
	}

	UWildRefactorState* State = NewObject<UWildRefactorState>(Target);
	State->RegisterComponent();
	State->OriginalMesh = SMC->GetStaticMesh();
	State->OriginalScale = SMC->GetComponentScale();

	ApplyCreature(SMC, PickCreature(), LargestSide);
	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s transmuted."), *Target->GetName());
	return true;
}

UStaticMesh* URefactorComponent::PickCreature() const
{
	return Menagerie.Num() > 0 ? Menagerie[FMath::RandRange(0, Menagerie.Num() - 1)].Get() : nullptr;
}

void URefactorComponent::ApplyCreature(UStaticMeshComponent* Target, UStaticMesh* Creature, float MatchSize)
{
	if (!Target || !Creature)
	{
		return;
	}
	// Uniform-scale the creature so it roughly fills the space of what it
	// replaced (a desk-sized rabbit, a Gideon-sized dragonfly).
	const float CreatureSize = FMath::Max(Creature->GetBounds().BoxExtent.GetMax() * 2.f, 1.f);
	const float Scale = FMath::Clamp(MatchSize / CreatureSize, 0.05f, 25.f);

	Target->SetMobility(EComponentMobility::Movable);
	Target->SetStaticMesh(Creature);
	Target->SetWorldScale3D(FVector(Scale));
}
