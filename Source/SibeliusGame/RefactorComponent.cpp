// RefactorComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorComponent.h"
#include "RefactorableComponent.h"
#include "RefuserController.h"      // wild refactor: only Refusers transmute
#include "Interactable.h"           // interactables keep their jobs (never wild targets)
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

	// APPEAL-R: hard-referenced base creature so the zoo is never empty and it
	// always cooks (the Death_Back lesson). The real variety comes from the
	// MenagerieFolders scan in BeginPlay.
	// Evicted by Walt: the EasyBiomes insects (world-position-offset wings —
	// a chair-sized butterfly is a "tornado") and SM_Dragons (a fused PAIR of
	// throne dragons that reads as a mess at furniture scale).
	const TCHAR* CreaturePaths[] = {
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

	// Packs ship body PARTS as separate meshes (the elephant's tusk variants).
	// A rolled tusk is two thin ivory curves floating at chair height — Walt
	// read it as "the chair disappeared completely." Whole animals only.
	ScannedCreatures.RemoveAll([](const FAssetData& Asset)
	{
		return Asset.AssetName.ToString().Contains(TEXT("Tusk"));
	});

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

namespace
{
	// R on a creature: destroy it, un-hide the first draft, give a restored
	// Refuser his mind back. Shared by the pawn sweep AND the visibility ray —
	// a ragdolled animal sprawled on the floor is exactly what a thin ray
	// misses (Walt's downed fox refused to become the couch again).
	bool RestoreWildDraft(AActor* CreatureActor)
	{
		if (!CreatureActor)
		{
			return false;
		}
		UWildRefactorState* State = CreatureActor->FindComponentByClass<UWildRefactorState>();
		if (!State)
		{
			return false;
		}
		if (AActor* Original = State->OriginalActor)
		{
			Original->SetActorHiddenInGame(false);
			Original->SetActorEnableCollision(true);
			if (ACharacter* Char = Cast<ACharacter>(Original))
			{
				if (!Char->GetController())
				{
					Char->SpawnDefaultController();
				}
			}
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s restored."), *Original->GetName());
		}
		CreatureActor->Destroy();
		return true;
	}
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

	// Refusers first, hunted with the slap's own generous pawn-channel sweep —
	// the thin visibility ray below often misses a moving pawn entirely
	// (capsules don't block visibility), which made R feel broken on Gideon
	// while F worked fine.
	TArray<FHitResult> PawnHits;
	if (World->SweepMultiByChannel(PawnHits, Start, End, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeSphere(45.f), Params))
	{
		for (const FHitResult& PawnHit : PawnHits)
		{
			// Downed creatures answer the sweep even when the thin ray misses.
			if (RestoreWildDraft(PawnHit.GetActor()))
			{
				return true;
			}
			ACharacter* Victim = Cast<ACharacter>(PawnHit.GetActor());
			if (Victim && Cast<ARefuserController>(Victim->GetController()))
			{
				if (AController* VC = Victim->GetController())
				{
					VC->StopMovement();
					VC->UnPossess();
				}
				FTransform Feet = Victim->GetActorTransform();
				Feet.SetLocation(Feet.GetLocation() - FVector(0.f, 0.f, Victim->GetSimpleCollisionHalfHeight()));
				AActor* Creature = SpawnCreatureActor(PickCreature(), Feet, RefuserCreatureSize);
				if (!Creature)
				{
					if (!Victim->GetController())
					{
						Victim->SpawnDefaultController();   // zoo empty — put him back to work
					}
					return false;
				}
				// Same reversible contract as props (Walt: Gideon should come
				// back): hide him behind the animal, restore re-possesses him.
				UWildRefactorState* State = NewObject<UWildRefactorState>(Creature);
				State->RegisterComponent();
				State->OriginalActor = Victim;
				Victim->SetActorHiddenInGame(true);
				Victim->SetActorEnableCollision(false);
				UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Refuser transmuted."));
				return true;
			}
		}
	}

	// The office is full of INVISIBLE interaction boxes hovering over the
	// furniture (the AI clue terminal over the desk, the corkboard volume,
	// the cauldron zone) — they block the visibility ray so E-prompts work,
	// and they ate R presses aimed at whatever sits underneath. When the ray
	// hits something untransmutable, look past it (a few hops, no more).
	AActor* Target = nullptr;
	for (int32 Hop = 0; Hop < 4; ++Hop)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return false;
		}
		AActor* HitTarget = Hit.GetActor();
		if (!HitTarget)
		{
			return false;
		}

		// --- R on a creature: destroy it and un-hide the first draft. ---
		if (RestoreWildDraft(HitTarget))
		{
			return true;
		}

		// (Refusers are handled by the pawn sweep above.)
		if (Cast<ACharacter>(HitTarget))
		{
			return false;   // players, Shinbi, and civilians are not clay
		}

		// --- Scenery candidate — stop hopping. ---
		// Anything wearing a static mesh qualifies, INCLUDING dumb Blueprint
		// decor (Walt's desk is BP_WorkingTable_A1): transmutation only hides
		// the original, so nothing breaks. The exceptions keep their jobs:
		// interactables (books, curios, doors, the cauldron, machines) are
		// never wild targets, and the NoWildRefactor tag opts anything out.
		const bool bHasMesh = HitTarget->FindComponentByClass<UStaticMeshComponent>() != nullptr;
		const bool bInteractable = HitTarget->GetClass()->ImplementsInterface(UInteractable::StaticClass());
		const bool bCandidate = (bHasMesh && !bInteractable) || HitTarget->ActorHasTag(TEXT("WildRefactorOK"));
		if (bCandidate && !HitTarget->ActorHasTag(TEXT("NoWildRefactor")))
		{
			Target = HitTarget;
			break;
		}

		Params.AddIgnoredActor(HitTarget);   // look past it
	}
	if (!Target)
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
	// Location = the prop's bounds center, dropped to its bounds floor: actor
	// PIVOTS are unreliable (a belly-pivot creature spawned at a floor-pivot
	// chair ends up underground — invisible AND untraceable, which read as
	// "the chair disappeared completely").
	const FBox TargetBox = Target->GetComponentsBoundingBox();
	FTransform Where = Target->GetActorTransform();
	Where.SetLocation(FVector(TargetBox.GetCenter().X, TargetBox.GetCenter().Y, TargetBox.Min.Z));
	AActor* Creature = SpawnCreatureActor(PickCreature(), Where, LargestSide);
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

	// Where.Z is the FLOOR of the space being filled. Actor pivots are
	// unreliable (belly-pivot creatures sank underground and were lost), so
	// place the actor such that the creature's scaled bounds BOTTOM sits at
	// Where.Z, computed from the mesh's local bounds:
	//   world bounds center = ActorZ + Scale * LocalOrigin.Z
	//   bounds bottom       = center - Scale * LocalExtent.Z  == Where.Z
	auto FeetDownZ = [&Where](const FBoxSphereBounds& Bounds, float Scale)
	{
		return Where.GetLocation().Z + Scale * (Bounds.BoxExtent.Z - Bounds.Origin.Z);
	};

	// Every creature must block the R-trace (ECC_Visibility), or it can never
	// be restored — an untraceable creature over a hidden prop is a softlock.
	auto MakeTraceable = [](UPrimitiveComponent* Comp)
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetCanEverAffectNavigation(false);
	};

	if (UStaticMesh* SM = Cast<UStaticMesh>(CreatureMesh))
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			Where.GetLocation(), Where.Rotator(), SpawnParams);
		if (Actor)
		{
			const float Scale = FitScale(SM->GetBounds());
			Actor->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(SM);
			Actor->SetActorScale3D(FVector(Scale));
			Actor->SetActorLocation(FVector(Where.GetLocation().X, Where.GetLocation().Y,
				FeetDownZ(SM->GetBounds(), Scale)));
			MakeTraceable(Actor->GetStaticMeshComponent());
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
				const float Scale = FitScale(SK->GetBounds());
				Comp->SetSkeletalMesh(SK);
				Actor->SetActorScale3D(FVector(Scale));
				Actor->SetActorLocation(FVector(Where.GetLocation().X, Where.GetLocation().Y,
					FeetDownZ(SK->GetBounds(), Scale)));
				MakeTraceable(Comp);
			}
		}
		return Actor;
	}

	return nullptr;
}
