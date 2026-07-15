// HallAlarmSubsystem.h
//
// Owns the one-shot "Hall alarm" event. Lives on the GameInstance so it outlives
// individual actors and is reachable from anywhere. The corkboard fires it; the
// Refuser spawners listen for it.
//
// Guarantees:
//   * Repeatable  — every TriggerAlarm() broadcasts (APPEAL-6b: the corkboard
//                   is a summon, not a one-shot). The latch records only
//                   "has fired at least once".
//   * Live only   — listeners are NOT replayed on subscribe; a wave spawns only
//                   from a real E-press, never from re-entering the level.

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
