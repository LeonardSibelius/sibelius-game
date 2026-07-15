// ProgressionSubsystem.h
//
// FUN-1 (docs/FUN_PLAN.md Steps 1+2) — the live owner of FProgressionState:
// which power verbs are earned and how much Sauce the player holds. A
// GameInstance subsystem so it survives level travel (the same reason
// USibeliusProgressSubsystem exists) — but unlike that session-only flag bag,
// this state PERSISTS to its own save slot, autosaved after every mutation.
//
// Not to be confused with USibeliusProgressSubsystem (SIB-43): that one is
// session-only clue-chain flags by design. This one is the player's earned
// progression — powers + wallet — and is the single runtime writer of the
// "Progression" slot.
//
// Game logic emits, UI listens: mutations broadcast OnPowerUnlocked /
// OnSauceChanged; the HUD and future ceremony widgets subscribe.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProgressionTypes.h"
#include "ProgressionSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSauceChanged, int32 /*NewTotal*/, int32 /*Delta*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPowerUnlocked, EPowerVerb);

UCLASS()
class SIBELIUSGAME_API UProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// The save slot this subsystem owns. Nothing else writes it.
	static const FString SlotName;

	// Null-safe fetch from any world-context object (actor, component, widget).
	static UProgressionSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Powers ---
	bool IsUnlocked(EPowerVerb Verb) const { return State.IsUnlocked(Verb); }
	int32 NumUnlocked() const { return State.NumUnlocked(); }

	// True only when newly unlocked; saves + broadcasts on change.
	bool UnlockPower(EPowerVerb Verb);
	void UnlockAllPowers();

	// --- Sauce ---
	int32 GetSauce() const { return State.Sauce; }
	void GrantSauce(int32 Amount);           // <= 0 ignored; saves + broadcasts
	bool TrySpendSauce(int32 Amount);        // false (no change) if short; saves + broadcasts

	// --- One-time grants (shrines, chapter-end rewards) ---
	bool HasClaimedGrant(FName GrantKey) const { return State.HasClaimed(GrantKey); }
	bool ClaimOneTimeGrant(FName GrantKey);  // claim-and-save; false if already claimed

	// --- Purchases (FUN-3, the cauldron shop) ---
	int32 GetPurchaseCount(FName OfferKey) const { return State.GetPurchaseCount(OfferKey); }
	void RecordPurchase(FName OfferKey);     // saves; the shop applies the effect

	// --- Lifetime stats (APPEAL-5, the RECORDS tab) ---
	// Keys live in SibeliusStats:: — every bump site spells them from there.
	// GrantSauce bumps SauceEarned itself, so earn sites never double-book.
	int32 GetStat(FName Key) const { return State.GetStat(Key); }
	void BumpStat(FName Key, int32 Delta = 1);   // saves on change
	void RaiseStat(FName Key, int32 Value);      // record semantics; saves only on a new record

	// Read-only view of the pure state (FSauceShop::BuildOffers takes the state
	// directly so the catalog logic stays headless-testable).
	const FProgressionState& GetStateForRead() const { return State; }

	// Dev: wipe to a fresh state and delete the slot (exec'd from the character).
	void ResetProgression();

	FOnSauceChanged OnSauceChanged;
	FOnPowerUnlocked OnPowerUnlocked;

private:
	FProgressionState State;

	void SaveNow();
};
