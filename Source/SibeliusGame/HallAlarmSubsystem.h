// HallAlarmSubsystem.h
//
// Owns the one-shot "Hall alarm" event. Lives on the GameInstance so it outlives
// individual actors and is reachable from anywhere. The corkboard fires it; the
// Refuser spawners listen for it.
//
// Two guarantees that defeat ordering bugs:
//   * Idempotent  — only the first TriggerAlarm() broadcasts.
//   * Replayed    — AddAlarmListener() invokes a late subscriber immediately if
//                   the alarm already fired, so a corkboard-before-spawner-
//                   BeginPlay race can never drop the alarm.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HallAlarmSubsystem.generated.h"

/** Broadcast once when the Hall alarm fires. */
DECLARE_MULTICAST_DELEGATE(FOnHallAlarm);

UCLASS()
class SIBELIUSGAME_API UHallAlarmSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Fire the alarm. Idempotent — only the first call broadcasts. */
	UFUNCTION(BlueprintCallable, Category="Hall Alarm")
	void TriggerAlarm();

	/** True once the alarm has fired. */
	UFUNCTION(BlueprintCallable, Category="Hall Alarm")
	bool IsAlarmTriggered() const { return bAlarmTriggered; }

	/**
	 * Register a listener. If the alarm has ALREADY fired, the listener is
	 * invoked immediately (replay), so late subscribers don't miss it.
	 * Returns a handle for RemoveAlarmListener().
	 */
	FDelegateHandle AddAlarmListener(const FOnHallAlarm::FDelegate& Listener);

	void RemoveAlarmListener(FDelegateHandle Handle);

private:
	FOnHallAlarm OnHallAlarm;
	bool bAlarmTriggered = false;
};
