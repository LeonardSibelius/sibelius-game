// CabinetOfCuriosities.h
//
// THE SAUCE DOOR — the wonder's home (SIB-47, §6). A shelf in the house with ONE slot
// per known curio: lit when owned, dim when not. It fills as the player collects —
// the soft completion goal and the reason to "go again" beyond raw novelty (§7).
//
// Reads the persistent UCurioCollectionSubsystem and the Elsewhere registry (the full
// known-curio list = the number of slots), and rebuilds on OnCollectionChanged. Slots
// are ISM instances + per-slot point lights (runtime-safe; no authored materials).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CabinetOfCuriosities.generated.h"

class UInstancedStaticMeshComponent;
class UCurioCollectionSubsystem;

// A small read model the gate asserts against: how full is the Cabinet?
USTRUCT(BlueprintType)
struct FCabinetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cabinet") int32 Filled = 0;   // owned curios on display
	UPROPERTY(BlueprintReadOnly, Category = "Cabinet") int32 Total = 0;    // known curios = slot count
	UPROPERTY(BlueprintReadOnly, Category = "Cabinet") int32 Score = 0;
};

UCLASS()
class SIBELIUSGAME_API ACabinetOfCuriosities : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACabinetOfCuriosities();

	// Rebuild the display from the collection + registry. Returns the fill state.
	// Driven on BeginPlay and whenever the collection changes; the gate calls it
	// directly with explicit inputs (see the overload).
	UFUNCTION(BlueprintCallable, Category = "Cabinet")
	FCabinetState Refresh();

	// Headless overload: build the display from explicit registry + owned ids, no
	// subsystem needed (a GameInstance subsystem can't exist in a bare commandlet).
	FCabinetState RefreshFrom(const TArray<FName>& AllCurioIds, const TArray<FName>& OwnedIds, int32 Score);

	// E reads the Cabinet (status line). Wonder, not a menu — kept light for the MVP.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Cabinet") float SlotSpacing = 80.f;
	UPROPERTY(EditAnywhere, Category = "Cabinet") FLinearColor FilledColor = FLinearColor(1.0f, 0.85f, 0.4f);

	UPROPERTY(VisibleAnywhere, Category = "Cabinet") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Cabinet") TObjectPtr<UInstancedStaticMeshComponent> SlotISM;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleCollectionChanged();

private:
	FCabinetState LastState;
};
