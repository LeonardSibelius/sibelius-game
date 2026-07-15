#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Delegates/IDelegateInstance.h"
#include "RefuserSpawner.generated.h"

class USoundBase;

UCLASS()
class SIBELIUSGAME_API ARefuserSpawner : public AActor
{
	GENERATED_BODY()

public:
	ARefuserSpawner();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	/** Refuser pawn to spawn when the alarm fires. */
	UPROPERTY(EditAnywhere, Category="Spawner")
	TSubclassOf<APawn> RefuserClass;

	/** Horizontal radius (cm) that candidate spawn points are drawn from. Kept
	 *  tight so spawns stay in the open office area. */
	UPROPERTY(EditAnywhere, Category="Spawner", meta=(ClampMin="0.0"))
	float SpawnRadius = 300.f;

	/** Vertical search extent (cm) for projecting candidate points onto the navmesh. */
	UPROPERTY(EditAnywhere, Category="Spawner", meta=(ClampMin="0.0"))
	float NavProjectionExtent = 300.f;

	/** Small upward nudge (cm) applied after navmesh projection to avoid floor clipping. */
	UPROPERTY(EditAnywhere, Category="Spawner", meta=(ClampMin="0.0"))
	float SpawnZNudge = 20.f;

	// --- Wave tuning -------------------------------------------------------

	/** How many waves spawn when the alarm fires (1-2 in the design). */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="1"))
	int32 NumWaves = 2;

	/** Minimum Refusers per wave. */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="1"))
	int32 MinPerWave = 3;

	/** Maximum Refusers per wave. */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="1"))
	int32 MaxPerWave = 4;

	/** Delay (s) between waves. */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="0.0"))
	float TimeBetweenWaves = 6.f;

	// APPEAL-6b (Walt: "I want to slap more often"): the corkboard alarm was a
	// one-shot party — after the scripted waves, the office went dry forever.
	// Now the spawner keeps a slow trickle coming. 0 restores the one-shot.
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="0.0"))
	float RespawnInterval = 90.f;

	/** Refusers per trickle spawn (small — a visitor, not an invasion). */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="1"))
	int32 RespawnCount = 2;

	/** Trickle pauses while this many Refusers are already alive in the level. */
	UPROPERTY(EditAnywhere, Category="Spawner|Waves", meta=(ClampMin="1"))
	int32 MaxAlive = 4;

	// --- Alarm feedback ----------------------------------------------------

	/** If true, this spawner plays the global alarm feedback (sound, red flash,
	 *  alert text). Disable on extra spawners so the feedback doesn't stack. */
	UPROPERTY(EditAnywhere, Category="Spawner|Alarm")
	bool bPlayAlarmFeedback = true;

	/** Sound played once when the alarm fires. */
	UPROPERTY(EditAnywhere, Category="Spawner|Alarm")
	TObjectPtr<USoundBase> AlarmSound;

	/** Duration (s) of the red screen flash. */
	UPROPERTY(EditAnywhere, Category="Spawner|Alarm", meta=(ClampMin="0.0"))
	float AlarmFlashDuration = 0.6f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Subsystem callback fired when the Hall alarm goes off. */
	void OnHallAlarm();

	/** Spawn one wave now, then schedule the next if any remain. */
	void SpawnWave();

	/** Spawn a single Refuser on navmesh-projected floor. Returns true on success. */
	bool SpawnOneRefuser();

	/** APPEAL-6b: the endless slow supply after the scripted waves are done. */
	void SpawnTrickle();

	/** Live Refusers of RefuserClass currently in the level. */
	int32 CountAlive() const;

	/** Alarm sound, red screen flash, on-screen alert text. */
	void PlayAlarmFeedback();

private:
	FDelegateHandle AlarmListenerHandle;
	FTimerHandle WaveTimerHandle;
	int32 WavesRemaining = 0;
};
