// HallAlarmSubsystem.cpp

#include "HallAlarmSubsystem.h"
#include "SibeliusGame.h"

void UHallAlarmSubsystem::TriggerAlarm()
{
	if (bAlarmTriggered)
	{
		return; // idempotent — broadcast exactly once
	}

	bAlarmTriggered = true;
	UE_LOG(LogSibeliusGame, Display, TEXT("[HallAlarm] Alarm triggered - broadcasting to listeners."));
	OnHallAlarm.Broadcast();
}

FDelegateHandle UHallAlarmSubsystem::AddAlarmListener(const FOnHallAlarm::FDelegate& Listener)
{
	const FDelegateHandle Handle = OnHallAlarm.Add(Listener);

	if (bAlarmTriggered)
	{
		// Replay to a late subscriber so it can't miss an already-fired alarm.
		Listener.ExecuteIfBound();
	}

	return Handle;
}

void UHallAlarmSubsystem::RemoveAlarmListener(FDelegateHandle Handle)
{
	OnHallAlarm.Remove(Handle);
}
