#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RefuserSpawner.generated.h"

UCLASS()
class SIBELIUSGAME_API ARefuserSpawner : public AActor
{
	GENERATED_BODY()

public:
	ARefuserSpawner();

	UPROPERTY(EditAnywhere, Category="Spawner")
	TSubclassOf<APawn> RefuserClass;

	UPROPERTY(EditAnywhere, Category="Spawner")
	float SpawnInterval = 5.f;

	UPROPERTY(EditAnywhere, Category="Spawner")
	int32 MaxAlive = 3;

	UPROPERTY(EditAnywhere, Category="Spawner")
	float SpawnRadius = 600.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void TrySpawn();

private:
	FTimerHandle SpawnTimerHandle;
	TArray<TWeakObjectPtr<AActor>> LiveRefusers;
};
