// RefactorTypes.h
//
// SIB-26 — Ch2 Refactor. The edit-type enum (hard-listed, R5) and the atomic
// snapshot struct (R6/R9). FRefactorSnapshot is deliberately a clean, complete
// capture of an object's pre-refactor state — Ch4 Test-Drive serializes this
// same shape, so design it well here.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"   // ECollisionEnabled
#include "RefactorTypes.generated.h"

class UMaterialInterface;

// The closed set of edits a refactorable object may declare (R5 — no free-form
// property editing in the MVP). Mass is DEFERRED: it needs a carry system the
// FP template doesn't have yet (R3).
UENUM(BlueprintType)
enum class ERefactorEditType : uint8
{
	Material UMETA(DisplayName = "Material"),
	Scale    UMETA(DisplayName = "Scale")
};

// Everything needed to put an actor back exactly as it was. Captured ONCE, on
// the first edit (R9), and restored as a whole (R6).
USTRUCT()
struct FRefactorSnapshot
{
	GENERATED_BODY()

	UPROPERTY() bool bHasSnapshot = false;

	// Original material per slot on the target mesh.
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// Original world scale.
	UPROPERTY() FVector OriginalWorldScale = FVector::OneVector;

	// Original collision state (so revert restores passability after a
	// collision-dropping material edit or a scale edit).
	UPROPERTY() TEnumAsByte<ECollisionEnabled::Type> OriginalCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
};
