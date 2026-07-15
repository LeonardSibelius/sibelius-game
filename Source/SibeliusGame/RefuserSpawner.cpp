#include "RefuserSpawner.h"

#include "HallAlarmSubsystem.h"
#include "SibeliusGame.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "EngineUtils.h"   // TActorIterator (trickle alive-count)

ARefuserSpawner::ARefuserSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ARefuserSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Gated: spawn NOTHING on level start. Subscribe to the alarm and wait.
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UHallAlarmSubsystem* Alarm = GameInstance ? GameInstance->GetSubsystem<UHallAlarmSubsystem>() : nullptr)
	{
		// AddAlarmListener replays immediately if the alarm already fired, so a
		// corkboard-before-BeginPlay broadcast can't be missed.
		AlarmListenerHandle = Alarm->AddAlarmListener(
			FOnHallAlarm::FDelegate::CreateUObject(this, &ARefuserSpawner::OnHallAlarm));
	}
	else
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Spawner] No HallAlarmSubsystem; spawner will never activate."));
	}
}

void ARefuserSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UHallAlarmSubsystem* Alarm = GameInstance ? GameInstance->GetSubsystem<UHallAlarmSubsystem>() : nullptr)
	{
		Alarm->RemoveAlarmListener(AlarmListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARefuserSpawner::OnHallAlarm()
{
	if (!RefuserClass)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Spawner] Alarm fired but RefuserClass is null - nothing to spawn."));
		return;
	}

	if (bPlayAlarmFeedback)
	{
		PlayAlarmFeedback();
	}

	WavesRemaining = FMath::Max(1, NumWaves);
	SpawnWave();
}

void ARefuserSpawner::SpawnWave()
{
	const int32 Lo = FMath::Min(MinPerWave, MaxPerWave);
	const int32 Hi = FMath::Max(MinPerWave, MaxPerWave);
	const int32 Count = FMath::RandRange(Lo, Hi);

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (SpawnOneRefuser())
		{
			++Spawned;
		}
	}

	--WavesRemaining;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Spawner] Wave spawned %d/%d Refusers; %d wave(s) remaining."),
		Spawned, Count, WavesRemaining);

	if (WavesRemaining > 0)
	{
		GetWorldTimerManager().SetTimer(
			WaveTimerHandle, this, &ARefuserSpawner::SpawnWave,
			FMath::Max(0.1f, TimeBetweenWaves), /*bLoop=*/false);
	}
	else if (RespawnInterval > 0.f)
	{
		// APPEAL-6b: waves done — switch to the endless trickle.
		GetWorldTimerManager().SetTimer(
			WaveTimerHandle, this, &ARefuserSpawner::SpawnTrickle,
			RespawnInterval, /*bLoop=*/false);
	}
}

void ARefuserSpawner::SpawnTrickle()
{
	// Quiet by design: no alarm feedback, no wave fanfare — just a fresh
	// visitor or two, and only while the level isn't already crowded.
	const int32 Alive = CountAlive();
	int32 Spawned = 0;
	for (int32 i = 0; i < RespawnCount && Alive + Spawned < MaxAlive; ++i)
	{
		if (SpawnOneRefuser())
		{
			++Spawned;
		}
	}
	if (Spawned > 0)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Spawner] Trickle: +%d Refuser(s) (%d alive)."), Spawned, Alive + Spawned);
	}

	GetWorldTimerManager().SetTimer(
		WaveTimerHandle, this, &ARefuserSpawner::SpawnTrickle,
		RespawnInterval, /*bLoop=*/false);
}

int32 ARefuserSpawner::CountAlive() const
{
	int32 Alive = 0;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (RefuserClass && It->IsA(RefuserClass))
			{
				++Alive;
			}
		}
	}
	return Alive;
}

bool ARefuserSpawner::SpawnOneRefuser()
{
	UWorld* World = GetWorld();
	if (!World || !RefuserClass)
	{
		return false;
	}

	const FVector Origin = GetActorLocation();
	const FVector2D RandOffset = FMath::RandPointInCircle(SpawnRadius);
	FVector SpawnLocation = Origin + FVector(RandOffset.X, RandOffset.Y, 0.f);

	// Project the candidate onto the navmesh so every Refuser lands on valid
	// floor instead of diving from a fixed +Z offset (the old ceiling-diver bug).
	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
	{
		FNavLocation Projected;
		const FVector QueryExtent(SpawnRadius, SpawnRadius, NavProjectionExtent);
		if (Nav->ProjectPointToNavigation(SpawnLocation, Projected, QueryExtent))
		{
			SpawnLocation = Projected.Location;
		}
		else
		{
			UE_LOG(LogSibeliusGame, Verbose, TEXT("[Spawner] Navmesh projection failed; using raw candidate point."));
		}
	}

	// Small nudge up so the pawn capsule doesn't clip the floor on spawn.
	SpawnLocation.Z += SpawnZNudge;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(RefuserClass, SpawnLocation, GetActorRotation(), SpawnParams);
	return Spawned != nullptr;
}

void ARefuserSpawner::PlayAlarmFeedback()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AlarmSound)
	{
		UGameplayStatics::PlaySound2D(World, AlarmSound);
	}

	// Red screen flash via the player camera manager (fade from red to clear).
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(
				/*FromAlpha=*/1.0f, /*ToAlpha=*/0.0f,
				FMath::Max(0.05f, AlarmFlashDuration), FLinearColor::Red,
				/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("ALERT - REFUSAL PROTOCOL ENGAGED"));
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[Spawner] ALERT - REFUSAL PROTOCOL ENGAGED"));
}
