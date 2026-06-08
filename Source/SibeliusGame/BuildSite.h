// Ch3 - Compile (SIB-27). An authored build location (scope decision A - no free placement).
// Owns its ghost mesh, final mesh, and a pre-placed NavLinkProxy (enabled only on build, C2).
// The site owns and reverts its own state - same principle as Ch2's URefactorableComponent.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompileTypes.h"
#include "Interactable.h"
#include "Branchable.h"
#include "BuildSite.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UInventoryComponent;
class ANavLinkProxy;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildStateChanged, bool, bIsBuilt);

UCLASS()
class SIBELIUSGAME_API ABuildSite : public AActor, public IInteractable, public IBranchable
{
	GENERATED_BODY()

public:
	ABuildSite();

	// IBranchable (SIB-28): declared state is bIsBuilt. RestoreBranchState writes
	// it RAW via ApplyBuiltState - no inventory spend/refund (that's the verb).
	virtual uint8 CaptureBranchState() const override { return IsBuilt() ? 1 : 0; }
	virtual void RestoreBranchState(uint8 InState) override;

	// IBranchable identity (SIB-29): stable persisted GUID, assign-once.
	virtual FGuid GetOrCreateBranchId() override { AssignBranchIdIfInvalid(BranchId); return BranchId; }
	virtual FGuid GetBranchId() const override { return BranchId; }

	// Authored default: unbuilt.
	virtual uint8 GetDefaultBranchState() const override { return 0; }

	virtual void BeginPlay() override;

	// SIB-38 GUID baking: assign at edit time (OnConstruction in an editor world), and
	// give a copy-pasted site a fresh id so it never shares its source's.
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

	// IInteractable: E dismantles a built site (refund). Building is the B verb (UBuildComponent::TriggerBuild).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// True when the inventory can afford this site and it isn't built yet.
	bool CanBuild(const UInventoryComponent* Inventory) const;

	// Spend -> swap ghost->final -> enable collision + nav-link (C2) / grant key (EBuildOutput::KeyItem).
	bool Build(UInventoryComponent* Inventory);

	// Full refund (C4), re-disable nav-link, back to ghost state.
	bool Dismantle(UInventoryComponent* Inventory);

	// Ghost shown/hidden by UBuildComponent as the player approaches with enough resources.
	void SetGhostVisible(bool bVisible);

	bool IsBuilt() const { return bIsBuilt; }

	// Headless self-test for CompileSmokeTest (bar item 4). Uses a transient inventory.
	bool RunBuildSelfTest(FString& OutError);

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<USceneComponent> SceneRoot; // CP3 lesson: explicit root or GetActorLocation() lies

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<UStaticMeshComponent> GhostMesh; // translucent preview, never collides (C8: separate component)

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<UStaticMeshComponent> FinalMesh; // hidden + no collision until built

	UPROPERTY(EditAnywhere, Category = "Build")
	EBuildOutput Output = EBuildOutput::Structure;

	UPROPERTY(EditAnywhere, Category = "Build")
	EResourceType CostResource = EResourceType::Book;

	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "1"))
	int32 Cost = 8;

	// Pre-placed in the level, smart-link disabled until built (C2). Optional for key sites.
	UPROPERTY(EditInstanceOnly, Category = "Build")
	TObjectPtr<ANavLinkProxy> NavLink;

	// How close the player must be for the ghost/prompt (cm).
	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "50.0"))
	float InteractRadius = 350.f;

	UPROPERTY(BlueprintAssignable, Category = "Build")
	FOnBuildStateChanged OnBuildStateChanged;

private:
	void ApplyBuiltState(bool bBuilt);
	void SetNavLinkEnabled(bool bEnabled);

	// SIB-29/38: stable cross-reload identity, BAKED into the level package so it
	// survives across sessions (assigned at edit time in OnConstruction). Plain
	// serialized UPROPERTY (not SaveGame-only); VisibleAnywhere to inspect in-editor.
	UPROPERTY(VisibleAnywhere, Category = "Branch")
	FGuid BranchId;

	bool bIsBuilt = false;
};
