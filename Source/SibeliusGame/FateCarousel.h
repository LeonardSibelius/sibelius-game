// FateCarousel.h
//
// SIB-34 — the Carousel of Fates. The altar-piece at the cathedral apse:
// the nine slot symbols (the June 11 vector sprites) as glowing cards,
// orbiting slowly above the marble plinth like relics. Pure set dressing —
// the E-interaction lives on ASlotCabinet (the plinth); this actor only
// tempts.
//
// Cards are built in OnConstruction from /Game/SlotFactory/SymbolSprites/
// T_sym_* via MIDs of /Game/SlotFactory/Materials/M_fate_base (created by
// Tools/Scripts/build_fate_altar.py — RUN THE SCRIPT BEFORE PLACING THIS).
//
// Card orientation is exposed as UPROPERTYs (CardPitch / CardYawOffset) so a
// wrong-facing card is a Details-panel fix, not a recompile — the Rotator
// lesson, institutionalized.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FateCarousel.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AFateCarousel : public AActor
{
	GENERATED_BODY()

public:
	AFateCarousel();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	// --- the dials (all live-tunable in Details) ---
	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float OrbitRadius = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float CardSize = 110.0f;          // cm, square

	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float RotationDegPerSec = 9.0f;   // slow, ceremonial

	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float BobAmplitude = 12.0f;       // cm of gentle float

	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float BobPeriodSeconds = 5.0f;

	// Emissive strength pushed to the card MIDs' "Glow" param. Default matches
	// M_fate_base's authored 5 (the cathedral altar look); Walt's library ring
	// wants ~1-2 — at 5 the symbols white out in a dim room.
	UPROPERTY(EditAnywhere, Category = "Fate Carousel", meta = (ClampMin = "0.1"))
	float CardGlow = 5.0f;

	// Orientation knobs: defaults stand the engine Plane (+Z normal) upright,
	// face outward. If cards lie flat or face inward, fix HERE, not in code.
	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float CardPitch = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Fate Carousel")
	float CardYawOffset = 90.0f;

private:
	void BuildCards();

	UPROPERTY(VisibleAnywhere, Category = "Fate Carousel")
	TObjectPtr<USceneComponent> Hub;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Cards;

	TArray<float> CardBaseZ;
	float RunningTime = 0.0f;
};
