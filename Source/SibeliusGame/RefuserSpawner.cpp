#include "RefuserSpawner.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/World.h"

ARefuserSpawner::ARefuserSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ARefuserSpawner::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ARefuserSpawner::TrySpawn,
		SpawnInterval,
		/*bLoop=*/true,
		/*FirstDelay=*/1.f);
}

void ARefuserSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void ARefuserSpawner::TrySpawn()
{
	if (!RefuserClass)
	{
		return;
	}

	// Prune dead/invalid refusers so the alive count stays accurate.
	LiveRefusers.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr)
	{
		return !Ptr.IsValid();
	});

	if (LiveRefusers.Num() >= MaxAlive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const FVector2D RandOffset = FMath::RandPointInCircle(SpawnRadius);
	const FVector SpawnLocation = Origin + FVector(RandOffset.X, RandOffset.Y, 100.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(RefuserClass, SpawnLocation, GetActorRotation(), SpawnParams);
	if (Spawned)
	{
		LiveRefusers.Add(Spawned);
	}
}
