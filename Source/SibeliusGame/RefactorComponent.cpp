// RefactorComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorComponent.h"
#include "RefactorableComponent.h"
#include "RefuserController.h"      // wild refactor: only Refusers transmute
#include "Interactable.h"           // interactables keep their jobs (never wild targets)
#include "SibeliusAim.h"            // rays start level with his eyes, even in third person
#include "SibeliusGame.h"
#include "SibeliusHUD.h"            // "REFACTOR NEEDS 10 SAUCE"
#include "ProgressionSubsystem.h"   // the wallet treasure is paid from
#include "Components/InstancedStaticMeshComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"

URefactorComponent::URefactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The hoard, hard-referenced so it cooks via the CDO (the Death_Back lesson) - see the
	// header. A lone coin stood on its edge and fitted to a couch is a gold wheel taller
	// than he is, which is the "big gold coin" Walt asked for. Not SM_Coin_02a: "the gray
	// one - don't use it"; and no bangles or coin heaps, which "look weird" (Walt,
	// 2026-10-03, who chose the gold pieces).
	const TCHAR* GemNames[] = { TEXT("SM_Gems_01a"), TEXT("SM_Gems_01b"), TEXT("SM_Gems_01c"), TEXT("SM_Gems_01d") };
	const TCHAR* OnEdgeNames[] = { TEXT("SM_Coin_01a") };
	const TCHAR* GoldPieceNames[] = {
		TEXT("SM_Chalice_01a"), TEXT("SM_Chalice_02b"), TEXT("SM_GoldBar_01a"),
		TEXT("SM_OrnateChest_02d"), TEXT("SM_Chest_02c"), TEXT("SM_ChestCoins_01a"),
	};
	auto Hoard = [](TArray<TObjectPtr<UStaticMesh>>& Into, TArrayView<const TCHAR* const> Names)
	{
		for (const TCHAR* Name : Names)
		{
			ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(
				*FString::Printf(TEXT("/Game/AncientTreasures/Meshes/%s.%s"), Name, Name));
			if (Finder.Succeeded())
			{
				Into.Add(Finder.Object);
			}
		}
	};
	Hoard(Gems, GemNames);
	Hoard(OnEdge, OnEdgeNames);
	Hoard(GoldPieces, GoldPieceNames);
}

void URefactorComponent::BeginPlay()
{
	Super::BeginPlay();

	// A treasure with no simple collision is one R can never hit again, so the original it
	// hides is gone for good. Leave such meshes out, loudly (Tools/Scripts/prepare_hoard.py).
	int32 Bodiless = 0;
	for (TArray<TObjectPtr<UStaticMesh>>* Pile : { &Gems, &OnEdge, &GoldPieces })
	{
		Bodiless += Pile->RemoveAll([](const TObjectPtr<UStaticMesh>& Mesh)
		{
			const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
			return !Body || Body->AggGeom.GetElementCount() == 0;
		});
	}
	if (Bodiless > 0)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[WildRefactor] %d treasure mesh(es) have no simple collision and are left out - run Tools/Scripts/prepare_hoard.py."),
			Bodiless);
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Hoard: %d gems + %d on edge + %d gold pieces, %d sauce each."),
		Gems.Num(), OnEdge.Num(), GoldPieces.Num(), TreasureSauceCost);
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

	const FVector Aim = PC->PlayerCameraManager->GetCameraRotation().Vector();
	// Level with his eyes while the view is him (first or third person); a table's or a
	// close-up's own camera keeps the old ray from that camera, as it always has.
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Start = PC->GetViewTarget() == Pawn ? SibeliusAim::RayStart(Pawn, CamLoc, Aim) : CamLoc;
	const FVector End = Start + Aim * TraceDistance;

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

namespace
{
	/* OPTED IN BY TAG, and the tag means two things at once: this actor may be
	   transmuted even though it is interactable (see the candidate test further down),
	   and it keeps its labels while transmuted. Both are wanted in exactly the same
	   place -- the legacy machine's stages -- so one tag carries both rather than two
	   that could drift apart.

	   It lives up here rather than in the namespace below because TriggerRefactor is
	   its first caller and C++ will not look ahead. */
	bool KeepsLabelsWhenWild(const AActor* A)
	{
		return A && A->ActorHasTag(TEXT("WildRefactorOK"));
	}
}

void URefactorComponent::TriggerRefactor()
{
	/* Pre-authored refactorables normally win: the office puzzle objects keep their two
	   hand-built states, never roll for treasure, and cost nothing.

	   THE ONE EXCEPTION IS EARNED. A legacy machine stage carries a refactorable too --
	   that authored edit IS the GRADER fix, the whole first ticket. So while the job is
	   open, R on a stage must repair it and nothing else. Once the machine tags its
	   stages WildRefactorOK (ALegacyMachine does that when ticket 1 closes), R starts
	   turning them to treasure instead, and the line keeps running with a heap of gold
	   standing in it -- because the machine drives the workpiece to each part's world
	   LOCATION and never asks what the part looks like.

	   The tag is all-or-nothing across the five stages on purpose. Gate it per stage --
	   say, only the ones that are behaving -- and the stage that refuses to become a
	   gem is the answer to the puzzle. */
	if (CurrentTarget && !KeepsLabelsWhenWild(CurrentTarget->GetOwner()))
	{
		CurrentTarget->ToggleRefactor();
		return;
	}

	TryWildRefactor();
}

namespace
{
	// R on treasure: destroy it, un-hide the first draft, give a restored
	// Refuser his mind back. Shared by the pawn sweep AND the visibility ray —
	// something slapped flat on the floor is exactly what a thin ray misses
	// (Walt's downed fox, in the animal days, refused to become the couch again).
	/* HIDE THE BODY, NOT EVERY MESH. A machine stage carries four static meshes: the
	   crate, and the three dark backing plates behind its plaque, true name and fault
	   lamp. Hiding all of them would leave the plaque's text floating over nothing.

	   So the body is named explicitly with a WildBody component tag, and only that is
	   hidden. The fallback to the first mesh keeps this working for anything that opts
	   in without tagging a body -- FindComponentByClass is what the transmutation
	   already uses to size and vet a target, so it is the same component either way. */
	void SetWildBodyHidden(AActor* A, bool bHidden)
	{
		if (!A)
		{
			return;
		}
		TArray<UStaticMeshComponent*> Meshes;
		A->GetComponents<UStaticMeshComponent>(Meshes);

		TArray<UStaticMeshComponent*> Bodies;
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M && M->ComponentHasTag(TEXT("WildBody")))
			{
				Bodies.Add(M);
			}
		}
		if (Bodies.Num() == 0 && Meshes.Num() > 0)
		{
			Bodies.Add(Meshes[0]);
		}
		for (UStaticMeshComponent* M : Bodies)
		{
			M->SetHiddenInGame(bHidden);
			M->SetVisibility(!bHidden);
		}
	}

	bool RestoreWildDraft(AActor* TreasureActor)
	{
		if (!TreasureActor)
		{
			return false;
		}
		UWildRefactorState* State = TreasureActor->FindComponentByClass<UWildRefactorState>();
		if (!State)
		{
			return false;
		}
		// One plant out of a painted field: put it back exactly where and how it stood.
		if (UInstancedStaticMeshComponent* Field = State->OriginalInstances)
		{
			Field->UpdateInstanceTransform(State->OriginalInstanceIndex, State->OriginalInstanceTransform,
				/*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d restored."),
				*Field->GetName(), State->OriginalInstanceIndex);
		}
		if (AActor* Original = State->OriginalActor)
		{
			// Undo it the way it was done, or a stage comes back with its crate still
			// invisible and a plaque hanging in mid-air.
			if (State->bHidMeshesOnly)
			{
				SetWildBodyHidden(Original, false);
			}
			else
			{
				Original->SetActorHiddenInGame(false);
			}
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
		TreasureActor->Destroy();
		return true;
	}
}

bool URefactorComponent::TryWildRefactor()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !PC->PlayerCameraManager || !World || (Gems.Num() + OnEdge.Num() + GoldPieces.Num() == 0))
	{
		return false;
	}

	const FVector Aim = PC->PlayerCameraManager->GetCameraRotation().Vector();
	// Level with his eyes while the view is him (first or third person); a table's or a
	// close-up's own camera keeps the old ray from that camera, as it always has.
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Start = PC->GetViewTarget() == Pawn ? SibeliusAim::RayStart(Pawn, CamLoc, Aim) : CamLoc;
	const FVector End = Start + Aim * TraceDistance;
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
			// Slapped-flat treasure answers the sweep even when the thin ray misses.
			if (RestoreWildDraft(PawnHit.GetActor()))
			{
				return true;
			}
			ACharacter* Victim = Cast<ACharacter>(PawnHit.GetActor());
			if (Victim && Cast<ARefuserController>(Victim->GetController()))
			{
				bool bUpright = false;
				UStaticMesh* Treasure = PickTreasure(bUpright);
				if (!Treasure || !PayForTreasure())
				{
					return false;   // he keeps working, and keeps his mind
				}
				if (AController* VC = Victim->GetController())
				{
					VC->StopMovement();
					VC->UnPossess();
				}
				AActor* Spawned = SpawnTreasureActor(Treasure, bUpright, Victim->GetComponentsBoundingBox(), RefuserTreasureSize,
					Victim, nullptr);
				if (!Spawned)
				{
					if (!Victim->GetController())
					{
						Victim->SpawnDefaultController();   // nothing spawned — put him back to work
					}
					return false;
				}
				// Same reversible contract as props (Walt: Gideon should come
				// back): hide him behind the treasure, restore re-possesses him.
				UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
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

		// --- R on treasure: destroy it and un-hide the first draft. ---
		if (RestoreWildDraft(HitTarget))
		{
			return true;
		}

		// (Refusers are handled by the pawn sweep above.)
		if (Cast<ACharacter>(HitTarget))
		{
			return false;   // players, Shinbi, and civilians are not clay
		}

		/* --- One plant out of a painted field: only in a virtual home. ---
		   The whole field is one actor far too big to transmute, so without this the
		   size limit below refuses every plant on Kaia's planet. Logged, because the
		   planet's pack may not be looked at by an AI (its licence): what the ray met is
		   how a failure there gets diagnosed. */
		if (bWildInstances)
		{
			UInstancedStaticMeshComponent* Field = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent());
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] ray met %s (%s), component %s (%s), item %d."),
				*HitTarget->GetName(), *HitTarget->GetClass()->GetName(),
				Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("none"),
				Hit.GetComponent() ? *Hit.GetComponent()->GetClass()->GetName() : TEXT("-"), Hit.Item);
			if (Field && Hit.Item != INDEX_NONE && !HitTarget->ActorHasTag(TEXT("NoWildRefactor")))
			{
				return TransmuteInstance(Field, Hit.Item);
			}
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
		return false;   // walls, floors, buildings: never treasure
	}

	// Architecture refuses the treatment BY NAME — a window is desk-sized, so
	// the bounds cap can't catch it, and a window that becomes a gem leaves
	// a gem-shaped hole to the outside world (Walt found this immediately).
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

	// Spawn the treasure where the prop stands, then hide the prop — never
	// modify it, so R on the treasure restores it exactly as it was.
	// Placed by the prop's BOUNDS and the floor under them, never its pivot:
	// actor pivots are unreliable (a belly-pivot animal spawned at a
	// floor-pivot chair ended up underground — invisible AND untraceable,
	// which read as "the chair disappeared completely").
	bool bUpright = false;
	UStaticMesh* Treasure = PickTreasure(bUpright);
	if (!Treasure || !PayForTreasure())
	{
		return false;
	}
	AActor* Spawned = SpawnTreasureActor(Treasure, bUpright, Target->GetComponentsBoundingBox(), LargestSide, Target, nullptr);
	if (!Spawned)
	{
		return false;
	}
	UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
	State->RegisterComponent();
	State->OriginalActor = Target;
	State->bHidMeshesOnly = KeepsLabelsWhenWild(Target);
	if (State->bHidMeshesOnly)
	{
		SetWildBodyHidden(Target, true);   // the plaque stays; that is the joke
	}
	else
	{
		Target->SetActorHiddenInGame(true);
	}
	Target->SetActorEnableCollision(false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s transmuted."), *Target->GetName());
	return true;
}

bool URefactorComponent::TransmuteInstance(UInstancedStaticMeshComponent* Field, int32 Index)
{
	FTransform Plant;
	if (!Field || !Field->GetStaticMesh() || !Field->GetInstanceTransform(Index, Plant, /*bWorldSpace=*/true))
	{
		return false;
	}

	// The plant's own bounds, not the field's: one coral tree, not the forest.
	const FBox Box = Field->GetStaticMesh()->GetBoundingBox().TransformBy(Plant);
	const float LargestSide = Box.GetSize().GetMax();
	if (LargestSide > MaxWildTargetSize || LargestSide < 1.f)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d is %.0f cm - outside the limits."),
			*Field->GetName(), Index, LargestSide);
		return false;
	}

	// Same placement rule as a prop: on the ground under the plant's own footprint, level.
	// Not at the plant's tilt - foliage leans with the slope, and a treasure copying it
	// went half into the hillside.
	bool bUpright = false;
	UStaticMesh* Treasure = PickTreasure(bUpright);
	if (!Treasure || !PayForTreasure())
	{
		return false;
	}
	AActor* Spawned = SpawnTreasureActor(Treasure, bUpright, Box, LargestSide, nullptr, Field);
	if (!Spawned)
	{
		return false;
	}

	UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
	State->RegisterComponent();
	State->OriginalInstances = Field;
	State->OriginalInstanceIndex = Index;
	State->OriginalInstanceTransform = Plant;

	FTransform Gone = Plant;
	Gone.SetScale3D(FVector(0.001f));
	const bool bHidden = Field->UpdateInstanceTransform(Index, Gone, /*bWorldSpace=*/true,
		/*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d (%s, %.0f cm) transmuted%s."),
		*Field->GetName(), Index, *Field->GetStaticMesh()->GetName(), LargestSide,
		bHidden ? TEXT("") : TEXT(" - BUT THE PLANT COULD NOT BE HIDDEN"));
	return true;
}

UStaticMesh* URefactorComponent::PickTreasure(bool& bOutUpright) const
{
	// A pile first, a third each, then one of that pile: the one coin would otherwise be
	// outvoted by six gold pieces. Gems and the coin stand; the gold pieces stay as made.
	const TArray<TObjectPtr<UStaticMesh>>* Piles[] = { &Gems, &OnEdge, &GoldPieces };
	TArray<int32, TInlineAllocator<3>> Stocked;
	for (int32 Each = 0; Each < int32(UE_ARRAY_COUNT(Piles)); ++Each)
	{
		if (Piles[Each]->Num() > 0)
		{
			Stocked.Add(Each);
		}
	}
	if (Stocked.Num() == 0)
	{
		return nullptr;
	}
	const int32 Kind = Stocked[FMath::RandRange(0, Stocked.Num() - 1)];
	const TArray<TObjectPtr<UStaticMesh>>& Pile = *Piles[Kind];
	bOutUpright = Piles[Kind] == &Gems || Piles[Kind] == &OnEdge;
	return Pile[FMath::RandRange(0, Pile.Num() - 1)].Get();
}

FVector URefactorComponent::GroundBelow(const FBox& Box, const AActor* Replaced, const UPrimitiveComponent* ReplacedField) const
{
	const FVector Middle = Box.GetCenter();
	FVector Ground(Middle.X, Middle.Y, Box.Min.Z);   // the bounds floor, if nothing is found
	UWorld* World = GetWorld();
	if (!World)
	{
		return Ground;
	}

	/* WORLD-STATIC ONLY, from the middle down. The house's invisible interaction boxes block
	   the visibility channel and hover over the furniture (see the hop loop), so a visibility
	   ray from above would stand the treasure on one of them; floors, furniture and the
	   landscape are world-static. Starting at mid-height finds the table under a lamp, not
	   the floor under the table. */
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TreasureGround), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(GetOwner());
	if (Replaced)
	{
		Params.AddIgnoredActor(Replaced);
	}
	if (ReplacedField)
	{
		Params.AddIgnoredComponent(ReplacedField);   // the plant's own field, not just the plant
	}
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Middle, FVector(Middle.X, Middle.Y, Box.Min.Z - 500.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Ground.Z = Hit.ImpactPoint.Z;
	}
	return Ground;
}

bool URefactorComponent::PayForTreasure() const
{
	if (TreasureSauceCost <= 0)
	{
		return true;
	}
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return false;   // headless / no save: no wallet, so no treasure
	}
	// TrySpendSauce IS THE CHECK (the coffee cup's rule): one call, one answer.
	// The HUD's sauce counter shows the spend on its own.
	if (!Progression->TrySpendSauce(TreasureSauceCost))
	{
		FString Line = TooPoorLine;
		Line.ReplaceInline(TEXT("{0}"), *FString::FromInt(TreasureSauceCost));
		Line.ReplaceInline(TEXT("{1}"), *FString::FromInt(Progression->GetSauce()));
		ASibeliusHUD::Toast(this, Line, 3.0f, SibeliusToast::Warn);
		return false;
	}
	return true;
}

AActor* URefactorComponent::SpawnTreasureActor(UStaticMesh* TreasureMesh, bool bUpright, const FBox& Space, float MatchSize,
	const AActor* Replaced, const UPrimitiveComponent* ReplacedField)
{
	UWorld* World = GetWorld();
	if (!World || !TreasureMesh)
	{
		return nullptr;
	}
	FVector Ground = GroundBelow(Space, Replaced, ReplacedField);
	const FBoxSphereBounds Bounds = TreasureMesh->GetBounds();
	const FVector Extent = Bounds.BoxExtent;

	// STAND IT UP: its longest side vertical, by a quarter turn about whichever axis
	// brings it there (see the header: every treasure in the pack lies flat).
	FQuat Stand = FQuat::Identity;
	if (bUpright && Extent.Y >= Extent.X && Extent.Y > Extent.Z)
	{
		Stand = FQuat(FVector::XAxisVector, UE_HALF_PI);   // Y up
	}
	else if (bUpright && Extent.X > Extent.Y && Extent.X > Extent.Z)
	{
		Stand = FQuat(FVector::YAxisVector, UE_HALF_PI);   // X up
	}
	const FVector Stood = Stand.RotateVector(Extent).GetAbs();

	const AActor* Me = GetOwner();
	auto FlatToMe = [Me](const FVector& From)
	{
		FVector V = Me ? Me->GetActorLocation() - From : FVector::ForwardVector;
		V.Z = 0.f;
		return V;
	};
	FVector ToMe = FlatToMe(Ground);
	float Distance = ToMe.Size();

	// Uniform-scale the treasure so it roughly fills the space of what it replaced (a
	// desk-tall gem, a Gideon-sized wheel of gold). The ceiling is high because the hoard
	// is hand-sized: a gem a few centimetres across has to reach a coral tree's ten metres
	// on Kaia's planet. But never AROUND him: its footprint stops PlayerClearance short of
	// him - shrinking down to half size, then stepping back away from him for the rest.
	const float Fit = FMath::Max(MatchSize, MinTreasureSize) / FMath::Max(Stood.GetMax() * 2.f, 1.f);
	float Scale = Fit;
	const float Reach = FVector2D(Stood.X, Stood.Y).Size();   // footprint radius at scale 1
	if (Me && Reach > UE_KINDA_SMALL_NUMBER)
	{
		Scale = FMath::Clamp((Distance - PlayerClearance) / Reach, Fit * 0.5f, Fit);
		const float Needed = Scale * Reach + PlayerClearance;
		if (Distance < Needed)
		{
			// Straight away from him; standing on its middle, the way he is facing.
			const FVector Away = Distance > 1.f ? -ToMe / Distance : Me->GetActorForwardVector().GetSafeNormal2D();
			const FVector Step = Away * (Needed - Distance);
			Ground = GroundBelow(Space.ShiftBy(FVector(Step.X, Step.Y, 0.f)), Replaced, ReplacedField);
			ToMe = FlatToMe(Ground);
			Distance = ToMe.Size();
		}
	}
	Scale = FMath::Clamp(Scale, 0.05f, 400.f);

	// BROAD FACE TO HIM: turn it so its thin side points at where he stands - a coin he
	// sees edge-on is a gold line.
	const float Yaw = FMath::Atan2(ToMe.Y, ToMe.X) + (Stood.Y < Stood.X ? UE_HALF_PI : 0.f);
	const FQuat Rotation = FQuat(FVector::ZAxisVector, Yaw) * Stand;

	// Its bounds, not its pivot, go where they should: centred over Ground, bottom on it.
	// Actor pivots are unreliable (belly-pivot animals sank underground and were lost).
	const FVector Offset = Rotation.RotateVector(Bounds.Origin) * Scale;   // pivot to bounds centre
	const FVector Location(Ground.X - Offset.X, Ground.Y - Offset.Y, Ground.Z + Stood.Z * Scale - Offset.Z);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation.Rotator(), SpawnParams);
	if (Actor)
	{
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		Actor->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(TreasureMesh);
		Actor->SetActorScale3D(FVector(Scale));

		// Every treasure must block the R-trace (ECC_Visibility), or it can never
		// be restored — an untraceable treasure over a hidden prop is a softlock.
		// (Only a mesh with simple collision can: BeginPlay keeps the others out.)
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetCanEverAffectNavigation(false);

		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s %s, %.0f cm tall and %.0f across, %.0f cm from him."),
			*TreasureMesh->GetName(), bUpright ? TEXT("standing") : TEXT("lying"),
			Stood.Z * 2.f * Scale, FMath::Max(Stood.X, Stood.Y) * 2.f * Scale, Distance);
	}
	return Actor;
}
