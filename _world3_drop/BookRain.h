// BookRain.h — World Three book-rain spawner.
// P0/P1 (June 13, 2026). Drop in Source/SibeliusGame/ (RUNTIME module).
// Pooled mesh-component spawner: books fall from SourceLocations into the cauldron MouthLocation and recycle.
// This is the consume-on-build float-and-vanish primitive (SIB-27) RUN IN REVERSE (fall + vanish at the pot).
// Books are COSMETIC + TRANSIENT — never IBranchable, never saved (SS3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BookRain.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class ASauceCauldron;

USTRUCT()
struct FFallingBook
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Comp = nullptr;
	bool     bActive  = false;
	float    Elapsed  = 0.f;
	float    Duration = 1.6f;
	FVector  Start    = FVector::ZeroVector;   // world
	FVector  End      = FVector::ZeroVector;   // world (cauldron mouth)
	FVector  Control  = FVector::ZeroVector;   // world (arc apex)
	FRotator SpinAxis = FRotator::ZeroRotator; // per-book tumble
};

UCLASS()
class SIBELIUSGAME_API ABookRain : public AActor
{
	GENERATED_BODY()

public:
	ABookRain();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	// --- Knobs (all relative to this actor unless noted) ---
	UPROPERTY(EditAnywhere, Category = "BookRain")
	TArray<FVector> SourceLocations;            // high points (spire crown); books spawn here

	UPROPERTY(EditAnywhere, Category = "BookRain")
	FVector MouthLocation = FVector::ZeroVector; // the cauldron mouth; books vanish here

	UPROPERTY(EditAnywhere, Category = "BookRain")
	TArray<TObjectPtr<UStaticMesh>> BookMeshes;  // random pick per book

	UPROPERTY(EditAnywhere, Category = "BookRain", meta = (ClampMin = "1"))
	int32 PoolSize = 24;                         // hard cap on simultaneous books (SS1: no destroy)

	UPROPERTY(EditAnywhere, Category = "BookRain", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category = "BookRain", meta = (ClampMin = "0.1"))
	float FallDuration = 1.6f;                   // seconds source -> mouth

	UPROPERTY(EditAnywhere, Category = "BookRain")
	float FallDurationJitter = 0.4f;

	UPROPERTY(EditAnywhere, Category = "BookRain")
	float ArcHeight = 120.f;                     // apex bulge above the midpoint

	UPROPERTY(EditAnywhere, Category = "BookRain")
	float SpinRateDeg = 220.f;

	UPROPERTY(EditAnywhere, Category = "BookRain")
	FVector2D ScaleRange = FVector2D(0.8f, 1.2f);

	// On arrival, trickle this into the cauldron's blend meter (0 = pure spectacle).
	UPROPERTY(EditAnywhere, Category = "BookRain")
	TObjectPtr<ASauceCauldron> Cauldron = nullptr;

	UPROPERTY(EditAnywhere, Category = "BookRain")
	float FeedPerBook = 0.0f;                    // P1 leave 0 (ambient is spectacle); P2 the loop drives the meter

	UPROPERTY(VisibleAnywhere, Category = "BookRain")
	TObjectPtr<USceneComponent> SceneRoot;

private:
	UPROPERTY() TArray<FFallingBook> Pool;
	float SpawnAccum = 0.f;

	void SpawnOne();
	int32 FindFreeIndex() const;

	// TODO (parked, P3.5): per-book random color tint via a dynamic material instance param.
};
