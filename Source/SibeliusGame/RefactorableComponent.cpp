// RefactorableComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorableComponent.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"

URefactorableComponent::URefactorableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URefactorableComponent::BeginPlay()
{
	Super::BeginPlay();
	GetOrCreateBranchId();   // SIB-29: stable identity from spawn (assign-once if invalid)
	CachedMesh = ResolveTargetMesh();
	bIsRefactored = false;
}

UMeshComponent* URefactorableComponent::ResolveTargetMesh() const
{
	if (const AActor* Owner = GetOwner())
	{
		// Prefer the root if it's a mesh; otherwise the first mesh component.
		if (UMeshComponent* RootMesh = Cast<UMeshComponent>(Owner->GetRootComponent()))
		{
			return RootMesh;
		}
		return Owner->FindComponentByClass<UMeshComponent>();
	}
	return nullptr;
}

void URefactorableComponent::CaptureSnapshotIfNeeded()
{
	if (Snapshot.bHasSnapshot)
	{
		return; // R9: never overwrite the TRUE original with an already-edited state
	}
	if (!CachedMesh)
	{
		CachedMesh = ResolveTargetMesh();
	}
	if (!CachedMesh)
	{
		return;
	}

	Snapshot.OriginalMaterials.Reset();
	const int32 NumMats = CachedMesh->GetNumMaterials();
	for (int32 i = 0; i < NumMats; ++i)
	{
		Snapshot.OriginalMaterials.Add(CachedMesh->GetMaterial(i));
	}
	Snapshot.OriginalWorldScale = CachedMesh->GetComponentScale();
	Snapshot.OriginalCollisionEnabled = CachedMesh->GetCollisionEnabled();
	Snapshot.bHasSnapshot = true;
}

void URefactorableComponent::ApplyRefactor()
{
	if (bIsRefactored)
	{
		return;
	}
	if (!CachedMesh)
	{
		CachedMesh = ResolveTargetMesh();
	}
	if (!CachedMesh)
	{
		return;
	}

	CaptureSnapshotIfNeeded();

	switch (EditType)
	{
	case ERefactorEditType::Material: ApplyMaterialEdit(); break;
	case ERefactorEditType::Scale:    ApplyScaleEdit();    break;
	default: break;
	}

	bIsRefactored = true;
	OnRefactorChanged.Broadcast(true);
}

void URefactorableComponent::ApplyMaterialEdit()
{
	const int32 NumMats = CachedMesh->GetNumMaterials();

	// R1 + R8: per-instance Dynamic Material Instances, created ONCE and cached.
	// SetMaterial on a component is already per-instance, but routing through a
	// MID (a) proves isolation in the smoke test and (b) leaves room to animate
	// params later. The shared source material is never touched.
	if (CachedMIDs.Num() != NumMats)
	{
		CachedMIDs.Reset();
		for (int32 i = 0; i < NumMats; ++i)
		{
			UMaterialInterface* Parent = RefactoredMaterial ? RefactoredMaterial.Get() : CachedMesh->GetMaterial(i);
			UMaterialInstanceDynamic* MID = CachedMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(i, Parent);
			CachedMIDs.Add(MID);
		}
	}
	else
	{
		for (int32 i = 0; i < NumMats; ++i)
		{
			CachedMesh->SetMaterial(i, CachedMIDs[i]);
		}
	}

	if (bDisableCollisionWhenRefactored)
	{
		CachedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void URefactorableComponent::ApplyScaleEdit()
{
	CachedMesh->SetWorldScale3D(RefactoredScale);
	// R2: rebuild the physics/collision body at the new scale so the object
	// isn't "smaller but still blocking". (Static-mesh query collision scales
	// with the transform too; this is the belt-and-suspenders for physics bodies.)
	CachedMesh->RecreatePhysicsState();
}

void URefactorableComponent::RestoreFromSnapshot()
{
	if (!Snapshot.bHasSnapshot || !CachedMesh)
	{
		return;
	}

	// Restore the WHOLE snapshot together (R6).
	for (int32 i = 0; i < Snapshot.OriginalMaterials.Num(); ++i)
	{
		CachedMesh->SetMaterial(i, Snapshot.OriginalMaterials[i]);
	}
	CachedMesh->SetWorldScale3D(Snapshot.OriginalWorldScale);
	CachedMesh->RecreatePhysicsState();
	CachedMesh->SetCollisionEnabled(Snapshot.OriginalCollisionEnabled);
}

void URefactorableComponent::RevertRefactor()
{
	if (!bIsRefactored)
	{
		return;
	}
	RestoreFromSnapshot();
	bIsRefactored = false;
	OnRefactorChanged.Broadcast(false);
}

void URefactorableComponent::ToggleRefactor()
{
	if (bIsRefactored)
	{
		RevertRefactor();
	}
	else
	{
		ApplyRefactor();
	}
}

void URefactorableComponent::RestoreBranchState(uint8 InState)
{
	// RAW state target. Apply/Revert are idempotent and restore exact visuals
	// from this component's own snapshot.
	if (InState != 0)
	{
		ApplyRefactor();
	}
	else
	{
		RevertRefactor();
	}
}

bool URefactorableComponent::RunRefactorSelfTest()
{
	if (!CachedMesh)
	{
		CachedMesh = ResolveTargetMesh();
	}
	if (!CachedMesh)
	{
		return false;
	}

	const FVector OrigScale = CachedMesh->GetComponentScale();
	UMaterialInterface* OrigMat0 = CachedMesh->GetNumMaterials() > 0 ? CachedMesh->GetMaterial(0) : nullptr;
	const ECollisionEnabled::Type OrigCollision = CachedMesh->GetCollisionEnabled();

	// Apply once — assert it actually changed the relevant property.
	ApplyRefactor();
	bool bChanged = false;
	if (EditType == ERefactorEditType::Scale)
	{
		bChanged = !CachedMesh->GetComponentScale().Equals(OrigScale);
	}
	else if (EditType == ERefactorEditType::Material)
	{
		bChanged = (CachedMesh->GetNumMaterials() > 0) && (CachedMesh->GetMaterial(0) != OrigMat0);
	}

	// Apply again — must NOT re-snapshot (R9).
	ApplyRefactor();

	// Revert — must restore the WHOLE original exactly (R6).
	RevertRefactor();

	const bool bScaleOK = CachedMesh->GetComponentScale().Equals(OrigScale);
	const bool bMatOK = (CachedMesh->GetNumMaterials() == 0) || (CachedMesh->GetMaterial(0) == OrigMat0);
	const bool bCollisionOK = (CachedMesh->GetCollisionEnabled() == OrigCollision);

	return bChanged && bScaleOK && bMatOK && bCollisionOK;
}
