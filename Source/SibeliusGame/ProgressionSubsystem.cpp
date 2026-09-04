// ProgressionSubsystem.cpp — live owner of powers + Sauce (FUN-1). See header.

#include "ProgressionSubsystem.h"
#include "ProgressionSaveGame.h"
#include "SaveSubsystem.h"
#include "SibeliusGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

const FString UProgressionSubsystem::SlotName = TEXT("Progression");

UProgressionSubsystem* UProgressionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UProgressionSubsystem>() : nullptr;
}

void UProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Make sure the I/O chokepoint outlives us for SaveNow().
	Collection.InitializeDependency<USaveSubsystem>();

	USaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (Saves && Saves->HasSave(SlotName))
	{
		if (UProgressionSaveGame* Loaded = Cast<UProgressionSaveGame>(Saves->LoadSave(SlotName)))
		{
			// SaveVersion 0 = never stamped = a blank/garbage object; keep the fresh default.
			if (Loaded->SaveVersion >= 1)
			{
				State = Loaded->State;
			}
		}
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("Progression: %d/%d powers, %d sauce"),
		State.NumUnlocked(), static_cast<int32>(EPowerVerb::Count), State.Sauce);
}

bool UProgressionSubsystem::UnlockPower(EPowerVerb Verb)
{
	if (!State.Unlock(Verb))
	{
		return false;
	}
	SaveNow();
	OnPowerUnlocked.Broadcast(Verb);
	UE_LOG(LogSibeliusGame, Display, TEXT("Progression: unlocked %s (%d/%d)"),
		*PowerVerbDisplayName(Verb), State.NumUnlocked(), static_cast<int32>(EPowerVerb::Count));
	return true;
}

void UProgressionSubsystem::UnlockAllPowers()
{
	bool bAnyNew = false;
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		const EPowerVerb Verb = static_cast<EPowerVerb>(i);
		if (State.Unlock(Verb))
		{
			bAnyNew = true;
			OnPowerUnlocked.Broadcast(Verb);
		}
	}
	if (bAnyNew)
	{
		SaveNow();
	}
}

void UProgressionSubsystem::GrantSauce(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	State.AddSauce(Amount);
	State.BumpStat(SibeliusStats::SauceEarned, Amount);   // APPEAL-5: lifetime gross rides every grant
	SaveNow();
	OnSauceChanged.Broadcast(State.Sauce, Amount);
}

bool UProgressionSubsystem::TrySpendSauce(int32 Amount)
{
	if (!State.TrySpendSauce(Amount))
	{
		return false;
	}
	SaveNow();
	OnSauceChanged.Broadcast(State.Sauce, -Amount);
	return true;
}

void UProgressionSubsystem::RecordPurchase(FName OfferKey)
{
	State.RecordPurchase(OfferKey);
	SaveNow();
}

void UProgressionSubsystem::BumpStat(FName Key, int32 Delta)
{
	const int32 Before = State.GetStat(Key);
	State.BumpStat(Key, Delta);
	if (State.GetStat(Key) != Before)
	{
		SaveNow();
	}
}

void UProgressionSubsystem::SaveSlotDials(float PaysMultiplier, int32 WildCount, float JackpotPay, int32 BonusCount)
{
	const bool bChanged =
		   !FMath::IsNearlyEqual(State.SlotPaysMultiplier, PaysMultiplier)
		|| State.SlotWildCount != WildCount
		|| !FMath::IsNearlyEqual(State.SlotJackpotPay, JackpotPay)
		|| State.SlotBonusCount != BonusCount;

	State.SlotPaysMultiplier = PaysMultiplier;
	State.SlotWildCount      = WildCount;
	State.SlotJackpotPay     = JackpotPay;
	State.SlotBonusCount     = BonusCount;

	// Only write when something actually moved. A dial is nudged many times in a sitting
	// and every press would otherwise be a disk write.
	if (bChanged)
	{
		SaveNow();
	}
}

void UProgressionSubsystem::ClearSlotDials()
{
	if (!State.HasEditedParSheet())
	{
		return;
	}

	// Back to the sentinels, meaning "never edited" — so the machine returns to whatever
	// the CURRENT build ships, not to a remembered copy of an older factory sheet.
	State.SlotPaysMultiplier = -1.0f;
	State.SlotWildCount      = -1;
	State.SlotJackpotPay     = -1.0f;
	State.SlotBonusCount     = -1;
	SaveNow();
}

void UProgressionSubsystem::CommitSlotMeters(const FSlotMeters& Delta)
{
	// Nothing played since the last commit — do not write. The cabinet calls this on every
	// close, and most closes follow a look at the machine rather than a session on it.
	if (Delta.TotalSpins() <= 0)
	{
		return;
	}

	// Note there is no clear path here, and no sentinel: the lifetime meters only ever
	// grow. Locked decision 2 (docs/FLOOR_REPORT.md).
	State.SlotLifetimeMeters.Add(Delta);
	SaveNow();
}

void UProgressionSubsystem::RaiseStat(FName Key, int32 Value)
{
	const int32 Before = State.GetStat(Key);
	State.RaiseStat(Key, Value);
	if (State.GetStat(Key) != Before)
	{
		SaveNow();
	}
}

bool UProgressionSubsystem::ClaimOneTimeGrant(FName GrantKey)
{
	if (!State.Claim(GrantKey))
	{
		return false;
	}
	SaveNow();
	return true;
}

void UProgressionSubsystem::RememberGeneratedSite(FName LevelName, FName EntryId,
	const FTransform& Transform, const FGuid& ObjectId)
{
	State.RememberGeneratedSite(LevelName, EntryId, Transform, ObjectId);
	/* SAVE IMMEDIATELY. The moment this has to survive is the player walking out of the
	   level, and nothing tells us that is about to happen - he presses [O], or opens a
	   door, and the world is gone. Saving on the build is the only point we are certain
	   to be alive for. */
	SaveNow();
}

void UProgressionSubsystem::ForgetGeneratedSite(const FGuid& ObjectId)
{
	if (State.ForgetGeneratedSite(ObjectId))
	{
		SaveNow();
	}
}

TArray<FGeneratedSiteRecord> UProgressionSubsystem::GeneratedSitesForLevel(FName LevelName) const
{
	TArray<FGeneratedSiteRecord> Out;
	if (LevelName.IsNone())
	{
		return Out;
	}
	for (const FGeneratedSiteRecord& R : State.GeneratedSites)
	{
		if (R.LevelName == LevelName)
		{
			Out.Add(R);
		}
	}
	return Out;
}

void UProgressionSubsystem::ResetProgression()
{
	State = FProgressionState();
	if (USaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr)
	{
		Saves->DeleteSave(SlotName);
	}
	OnSauceChanged.Broadcast(State.Sauce, 0);
	UE_LOG(LogSibeliusGame, Display, TEXT("Progression: RESET to fresh state"));
}

void UProgressionSubsystem::SaveNow()
{
	USaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return; // headless / teardown — the pure state still works, it just won't persist
	}
	UProgressionSaveGame* Save = NewObject<UProgressionSaveGame>(GetTransientPackage());
	Save->SaveVersion = UProgressionSaveGame::CurrentSaveVersion;
	Save->State = State;
	Saves->CommitSave(Save, SlotName);
}
