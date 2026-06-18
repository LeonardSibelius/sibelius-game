// ElsewhereSubsystem.h
//
// THE SAUCE DOOR — the generation brain (SIB-47). A UGameInstanceSubsystem so the
// STAGED plan survives the OpenLevel from the kitchen into the Elsewhere map (the
// same "survives level travel" guarantee USibeliusProgressSubsystem relies on). The
// Sauce Door stages a plan here, then travels; the builder on the far side reads it.
//
// Owns the content registry (place-types + curios), loaded from the editor DataTables
// when present, else FElsewhereGen's code defaults — same load-or-default pattern as
// the Carousel. Thin wrapper over the pure FElsewhereGen core (the FCarouselRun split).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ElsewhereTypes.h"
#include "ElsewhereSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnElsewhereStaged, const FElsewherePlan&, Plan);

UCLASS()
class SIBELIUSGAME_API UElsewhereSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Roll a fresh Elsewhere and STAGE it (held in memory across the travel). Pass
	// Seed < 0 to draw a non-deterministic seed; pass an explicit seed to reproduce a
	// room. Returns the staged plan. Broadcasts OnElsewhereStaged.
	UFUNCTION(BlueprintCallable, Category = "Elsewhere")
	FElsewherePlan StageNextElsewhere(int32 Seed = -1);

	// The plan the builder should assemble. Invalid if nothing is staged.
	UFUNCTION(BlueprintPure, Category = "Elsewhere")
	const FElsewherePlan& GetStagedPlan() const { return StagedPlan; }

	UFUNCTION(BlueprintPure, Category = "Elsewhere")
	bool HasStagedElsewhere() const { return StagedPlan.IsValid(); }

	// THE discard (§4): drop the in-memory plan on return. The room itself is gone the
	// moment the Elsewhere level unloads; this clears the staging so a stale plan can't
	// leak into the next visit.
	UFUNCTION(BlueprintCallable, Category = "Elsewhere")
	void DiscardStagedElsewhere();

	// Registry access for the builder + cabinet (null if absent).
	const FPlaceTypeDef* FindPlace(const FName& Id) const;
	const FCurioDef* FindCurio(const FName& Id) const;

	const TArray<FPlaceTypeDef>& GetPlaceTypes() const { return PlaceTypes; }
	const TArray<FCurioDef>& GetCurios() const { return Curios; }

	UPROPERTY(BlueprintAssignable, Category = "Elsewhere")
	FOnElsewhereStaged OnElsewhereStaged;

	// --- Headless seam (the gate can't NewObject a GameInstance subsystem). Lets the
	// commandlet load the same registry the runtime uses, then drive FElsewhereGen. ---
	static void LoadRegistry(TArray<FPlaceTypeDef>& OutPlaces, TArray<FCurioDef>& OutCurios);

private:
	UPROPERTY() TArray<FPlaceTypeDef> PlaceTypes;
	UPROPERTY() TArray<FCurioDef> Curios;

	FElsewherePlan StagedPlan;

	// Rolling counter so back-to-back non-deterministic stages can't collide on the
	// same clock tick; XORed into the drawn seed.
	int32 StageCounter = 0;
};
