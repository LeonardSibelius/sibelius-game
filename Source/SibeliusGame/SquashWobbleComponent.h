// SquashWobbleComponent.h
//
// A SLAPPED PLANT BOUNCES (Kaia's home). Walt, 2026-10-04: "the effect I am looking for is
// some kind of noise and movement when he slaps them with F". Her big balloon mushrooms are
// plain static meshes - some placed one by one, most painted as foliage - which Etherion's
// foliage plugin never brings to life (its live plants in that pack are only the flowers, the
// leafy plant and the mushroom stalks). So USlapComponent adds one of these to a struck
// plant: a squash-and-stretch that rings down over a moment, then puts the plant back exactly
// and removes itself.
//
// ONE PLANT, NEVER ITS FIELD. A painted mushroom is one instance among thousands inside a
// single foliage actor; scaling that actor's root shook every plant on the planet around one
// point ("you hit one mushroom and they all shake" - Walt, the first try). WobbleInstance
// moves only the instance that was hit.
//
// Volume-keeping: when it stretches up it thins, when it squashes it spreads, which is what
// reads as rubbery rather than as a model being resized.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SquashWobbleComponent.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(ClassGroup = (Sibelius))
class SIBELIUSGAME_API USquashWobbleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USquashWobbleComponent();

	/** Wobble a whole (plant-sized) actor by its root, made movable if it was static. */
	static void Wobble(AActor* Target, float Strength);

	/** Wobble one painted plant: instance Index of Field, and nothing else in the field. */
	static void WobbleInstance(UInstancedStaticMeshComponent* Field, int32 Index, float Strength);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// A second slap mid-wobble restarts the existing one rather than stacking two - and
	// keeps the rest pose it recorded first, the true one, not a mid-squash one.
	static USquashWobbleComponent* Find(AActor* Owner, const UInstancedStaticMeshComponent* Field, int32 Index);

	void Start(float InStrength);
	void Apply(const FVector& Squash);
	void Finish();

	// Instance mode when Field is set; otherwise the owner's root.
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Field;
	int32 Index = INDEX_NONE;
	FTransform Rest;          // the instance's world transform, or the root's relative one

	float Strength = 0.25f;
	float Elapsed = 0.0f;

	// How long it rings, how fast, and how quickly it settles. Tuned to read as a balloon.
	static constexpr float Duration = 0.9f;
	static constexpr float Frequency = 3.5f;   // bounces per second
	static constexpr float Damping = 4.5f;     // per second
};
