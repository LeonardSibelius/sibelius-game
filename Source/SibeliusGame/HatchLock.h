// Ch3 - Compile (SIB-27). The locked attic hatch. E (shared IInteractable system) + a built Key unlocks it.
// Owns a blocking mesh that drops collision + hides on unlock.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "Branchable.h"
#include "HatchLock.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHatchUnlocked);

UCLASS()
class SIBELIUSGAME_API AHatchLock : public AActor, public IInteractable, public IBranchable
{
	GENERATED_BODY()

public:
	AHatchLock();

	// IBranchable (SIB-28): declared state is bLocked. RestoreBranchState writes
	// it RAW via ApplyLockedState - no Key spend (that's TryUnlock, the verb).
	virtual uint8 CaptureBranchState() const override { return IsLocked() ? 1 : 0; }
	virtual void RestoreBranchState(uint8 InState) override;

	// IBranchable identity (SIB-29): stable persisted GUID, assign-once.
	virtual FGuid GetOrCreateBranchId() override { AssignBranchIdIfInvalid(BranchId); return BranchId; }
	virtual FGuid GetBranchId() const override { return BranchId; }

	// Authored default: locked.
	virtual uint8 GetDefaultBranchState() const override { return 1; }

	virtual void BeginPlay() override;

	// SIB-38 GUID baking: assign at edit time (OnConstruction in an editor world), and
	// give a copy-pasted hatch a fresh id so it never shares its source's.
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

	// Spends one Key to unlock. Returns false (and stays shut) without a Key.
	UFUNCTION(BlueprintCallable, Category = "Hatch")
	bool TryUnlock(UInventoryComponent* Inventory);

	// IInteractable: unlock on E. Inventory comes from the interactor (the pawn).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Hatch")
	bool IsLocked() const { return bLocked; }

	// Headless self-test for CompileSmokeTest (bar item 7).
	bool RunLockSelfTest(FString& OutError);

	UPROPERTY(VisibleAnywhere, Category = "Hatch")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Hatch")
	TObjectPtr<UStaticMeshComponent> BlockerMesh;

	UPROPERTY(BlueprintAssignable, Category = "Hatch")
	FOnHatchUnlocked OnUnlocked;

private:
	void ApplyLockedState(bool bNowLocked);

	// SIB-29/38: stable cross-reload identity, BAKED into the level package so it
	// survives across sessions (assigned at edit time in OnConstruction). Plain
	// serialized UPROPERTY (not SaveGame-only); VisibleAnywhere to inspect in-editor.
	UPROPERTY(VisibleAnywhere, Category = "Branch")
	FGuid BranchId;

	UPROPERTY(EditAnywhere, Category = "Hatch")
	bool bLocked = true;
};
