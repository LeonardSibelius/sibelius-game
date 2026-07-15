// HallAlarmSubsystem.cpp

#include "HallAlarmSubsystem.h"
#include "SibeliusGame.h"

void UHallAlarmSubsystem::TriggerAlarm()
{
	// APPEAL-6b (Walt): repeatable — every call broadcasts, so the corkboard
	// can summon wave after wave. The latch only marks "has fired at least
	// once" for the late-subscriber replay guarantee.
	bAlarmTriggered = true;
	UE_LOG(LogSibeliusGame, Display, TEXT("[HallAlarm] Alarm triggered - broadcasting to listeners."));
	OnHallAlarm.Broadcast();
}

FDelegateHandle UHallAlarmSubsystem::AddAlarmListener(const FOnHallAlarm::FDelegate& Listener)
{
	// APPEAL-6b: NO replay-on-subscribe anymore. With the corkboard repeatable,
	// replaying would auto-spawn a wave every time the office level reloads —
	// exactly the uninvited-demons feel Walt vetoed. Spawners react only to a
	// live E-press. (The old same-level race this replay guarded against can't
	// happen: the player can't press E before BeginPlay has finished.)
	return OnHallAlarm.Add(Listener);
}

void UHallAlarmSubsystem::RemoveAlarmListener(FDelegateHandle Handle)
{
	OnHallAlarm.Remove(Handle);
}
