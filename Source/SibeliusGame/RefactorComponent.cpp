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
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

URefactorComponent::URefactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// APPEAL-R: hard-referenced base creatures so the zoo is never empty and
	// these two always cook (the Death_Back lesson). The real variety comes
	// from the MenagerieFolders scan in BeginPlay.
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

void URefactorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Scan the animal-pack folders: every static or skeletal mesh inside is a
	// possible second draft. Lazy-loaded in PickCreature, so this costs a
	// registry query, not forty mesh loads.
	FAssetRegistryModule& RegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (const FName& Folder : MenagerieFolders)
	{
		Filter.PackagePaths.Add(Folder);
	}
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

	ScannedCreatures.Reset();
	RegistryModule.Get().GetAssets(Filter, ScannedCreatures);
	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Menagerie: %d scanned + %d built-in creatures."),
		ScannedCreatures.Num(), Menagerie.Num());
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

	// --- R on a creature: destroy it and un-hide the first draft. ---
	if (UWildRefactorState* State = Target->FindComponentByClass<UWildRefactorState>())
	{
		if (AActor* Original = State->OriginalActor)
		{
			Original->SetActorHiddenInGame(false);
			Original->SetActorEnableCollision(true);
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s restored."), *Original->GetName());
		}
		Target->Destroy();
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
		SpawnCreatureActor(PickCreature(), Victim->GetActorTransform(), RefuserCreatureSize);
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

	// Architecture refuses the treatment BY NAME — a window is desk-sized, so
	// the bounds cap can't catch it, and a window that becomes a dragon leaves
	// a dragon-shaped hole to the outside world (Walt found this immediately).
	// Kit meshes reliably carry these words; furniture doesn't.
	{
		static const TCHAR* ArchitectureWords[] = {
			TEXT("wall"), TEXT("window"), TEXT("door"), TEXT("floor"),
			TEXT("roof"), TEXT("ceiling"), TEXT("fence"), TEXT("stair"),
			TEXT("beam"), TEXT("column"), TEXT("pillar"), TEXT("railing"),
		};
		const FString MeshName = SMC->GetStaticMesh()->GetName().ToLower();
		const FString ActorName = Target->GetName().ToLower();
		for (const TCHAR* Word : ArchitectureWords)
		{
			if (MeshName.Contains(Word) || ActorName.Contains(Word))
			{
				return false;
			}
		}
	}

	// Spawn the creature where the prop stands, then hide the prop — never
	// modify it, so R on the creature restores it exactly as it was.
	AActor* Creature = SpawnCreatureActor(PickCreature(), Target->GetActorTransform(), LargestSide);
	if (!Creature)
	{
		return false;
	}
	UWildRefactorState* State = NewObject<UWildRefactorState>(Creature);
	State->RegisterComponent();
	State->OriginalActor = Target;
	Target->SetActorHiddenInGame(true);
	Target->SetActorEnableCollision(false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s transmuted."), *Target->GetName());
	return true;
}

UObject* URefactorComponent::PickCreature() const
{
	const int32 Total = Menagerie.Num() + ScannedCreatures.Num();
	if (Total == 0)
	{
		return nullptr;
	}
	const int32 Index = FMath::RandRange(0, Total - 1);
	if (Index < Menagerie.Num())
	{
		return Menagerie[Index].Get();
	}
	return ScannedCreatures[Index - Menagerie.Num()].GetAsset();   // lazy load
}

AActor* URefactorComponent::SpawnCreatureActor(UObject* CreatureMesh, const FTransform& Where, float MatchSize)
{
	UWorld* World = GetWorld();
	if (!World || !CreatureMesh)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Uniform-scale the creature so it roughly fills the space of what it
	// replaced (a desk-sized rabbit, a Gideon-sized zebra).
	auto FitScale = [MatchSize](const FBoxSphereBounds& Bounds)
	{
		const float CreatureSize = FMath::Max(Bounds.BoxExtent.GetMax() * 2.f, 1.f);
		return FMath::Clamp(MatchSize / CreatureSize, 0.05f, 25.f);
	};

	if (UStaticMesh* SM = Cast<UStaticMesh>(CreatureMesh))
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			Where.GetLocation(), Where.Rotator(), SpawnParams);
		if (Actor)
		{
			Actor->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(SM);
			Actor->SetActorScale3D(FVector(FitScale(SM->GetBounds())));
		}
		return Actor;
	}

	if (USkeletalMesh* SK = Cast<USkeletalMesh>(CreatureMesh))
	{
		ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>(
			Where.GetLocation(), Where.Rotator(), SpawnParams);
		if (Actor)
		{
			if (USkeletalMeshComponent* Comp = Actor->GetSkeletalMeshComponent())
			{
				Comp->SetSkeletalMesh(SK);
				Actor->SetActorScale3D(FVector(FitScale(SK->GetBounds())));
			}
		}
		return Actor;
	}

	return nullptr;
}
