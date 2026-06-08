// RefactorableComponent.h
//
// SIB-26 — Ch2 Refactor. Add this to any object the engineer can refactor.
// It declares WHAT its refactor does, owns its own snapshot, and reverts
// itself exactly. The player's URefactorComponent only selects + triggers it —
// it never stores this object's edit state. That keeps revert correct (R6) and
// gives Ch4 Test-Drive a clean per-object snapshot to branch on.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RefactorTypes.h"
#include "Branchable.h"
#include "RefactorableComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRefactorChanged, bool, bIsRefactored);

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API URefactorableComponent : public UActorComponent, public IBranchable
{
	GENERATED_BODY()

public:
	URefactorableComponent();

	// IBranchable (SIB-28): declared state is just bIsRefactored; the component's
	// own FRefactorSnapshot makes the material/scale/collision restore exact.
	virtual uint8 CaptureBranchState() const override { return IsRefactored() ? 1 : 0; }
	virtual void RestoreBranchState(uint8 InState) override;

	// IBranchable identity (SIB-29): stable persisted GUID, assign-once.
	virtual FGuid GetOrCreateBranchId() override { AssignBranchIdIfInvalid(BranchId); return BranchId; }
	virtual FGuid GetBranchId() const override { return BranchId; }

	// Authored default: not refactored.
	virtual uint8 GetDefaultBranchState() const override { return 0; }

	UFUNCTION(BlueprintCallable, Category = "Refactor")
	void ApplyRefactor();

	UFUNCTION(BlueprintCallable, Category = "Refactor")
	void RevertRefactor();

	UFUNCTION(BlueprintCallable, Category = "Refactor")
	void ToggleRefactor();

	UFUNCTION(BlueprintPure, Category = "Refactor")
	bool IsRefactored() const { return bIsRefactored; }

	UPROPERTY(BlueprintAssignable, Category = "Refactor")
	FOnRefactorChanged OnRefactorChanged;

	ERefactorEditType GetEditType() const { return EditType; }

	// Headless self-test for the smoke commandlet: apply -> apply -> revert,
	// then asserts the object is byte-for-byte back to its original material,
	// scale, and collision (R6/R9), and that the first apply actually changed
	// something. Returns true on success. Needs no spawned player.
	bool RunRefactorSelfTest();

protected:
	virtual void BeginPlay() override;

	// SIB-38 GUID baking: assign at edit time (OnRegister in an editor world), and
	// give a copy-pasted component a fresh id so it never shares its source's.
	virtual void OnRegister() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

	// What this object's refactor does (R5 — closed set).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	ERefactorEditType EditType = ERefactorEditType::Material;

	// Material edit: swap to this material (applied as a per-instance MID — R1).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	TObjectPtr<UMaterialInterface> RefactoredMaterial;

	// Material edit: also drop collision when refactored (e.g., a wall panel
	// that opens to reveal the mechanism behind it).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	bool bDisableCollisionWhenRefactored = false;

	// Scale edit: world scale to set when refactored (e.g., shrink a blocking crate).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	FVector RefactoredScale = FVector(0.4f);

private:
	UMeshComponent* ResolveTargetMesh() const;
	void CaptureSnapshotIfNeeded();
	void ApplyMaterialEdit();
	void ApplyScaleEdit();
	void RestoreFromSnapshot();

	UPROPERTY() FRefactorSnapshot Snapshot;
	UPROPERTY() TObjectPtr<UMeshComponent> CachedMesh;

	// MIDs created ONCE per actor and reused on re-refactor (R8 — no per-toggle leak).
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> CachedMIDs;

	// SIB-29/38: stable cross-reload identity, BAKED into the level package so it
	// survives across sessions (assigned at edit time in OnRegister). Plain serialized
	// UPROPERTY (not SaveGame-only); VisibleAnywhere so it's inspectable in the editor.
	UPROPERTY(VisibleAnywhere, Category = "Branch")
	FGuid BranchId;

	bool bIsRefactored = false;
};
