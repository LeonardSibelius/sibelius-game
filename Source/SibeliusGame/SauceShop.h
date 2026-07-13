// SauceShop.h
//
// FUN-3 (docs/FUN_PLAN.md Step 3) — the cauldron's catalog + purchase logic,
// kept OUT of the widget so the smoke test exercises it headless. Every offer
// follows the "one stat, one code point" rule (Raymond's upgrade taxonomy):
// the effect application touches exactly one existing mechanical lever.
//
//   Power unlocks  -> UProgressionSubsystem::UnlockPower (alt-path to a shrine)
//   Generate budget-> UGenerateComponent::SetRemainingBudget
//   Mighty Slap    -> USlapComponent::LaunchSpeed / UpwardSpeed
//
// Purchases persist as counts in FProgressionState::PurchaseCounts and are
// re-applied to each fresh pawn by ApplyPersistentPurchases (character BeginPlay).

#pragma once

#include "CoreMinimal.h"
#include "ProgressionTypes.h"

class UProgressionSubsystem;
class APawn;

struct SIBELIUSGAME_API FSauceOffer
{
	FName Key;               // purchase-count key ("Budget.Generate") or power key
	FString Title;
	FString Desc;
	int32 Cost = 0;
	int32 MaxCount = 1;      // 0 = unlimited repeat buys
	bool bIsPowerUnlock = false;
	EPowerVerb Power = EPowerVerb::Refactor;
};

struct SIBELIUSGAME_API FSauceShop
{
	// Tuning constants — one place to price the economy.
	static constexpr int32 PowerUnlockCost = 150;
	static constexpr int32 BudgetCost = 40;
	static constexpr int32 BudgetPerPurchase = 5;
	static constexpr int32 BudgetMaxPurchases = 10;
	static constexpr int32 SlapCost = 60;
	static constexpr float SlapMultiplier = 1.5f;
	static constexpr int32 SlapMaxPurchases = 3;

	// The catalog for the CURRENT progression state: still-locked powers first,
	// then upgrades that still have stock. Takes the PURE state (not the
	// subsystem) so the headless smoke test builds catalogs directly.
	static void BuildOffers(const FProgressionState& State, TArray<FSauceOffer>& OutOffers);

	// Affordability + stock check, then spend -> record -> apply-to-pawn.
	// True only when the sauce was actually spent. Pawn may be null (headless):
	// the purchase still records and persists; the live effect lands at next
	// ApplyPersistentPurchases.
	static bool TryPurchase(UProgressionSubsystem* Progression, APawn* Pawn, const FSauceOffer& Offer);

	// Re-apply the recorded purchases to a freshly spawned pawn. Idempotent per
	// spawn: components start from authored defaults each spawn, so the effects
	// multiply/add from a clean base every time.
	static void ApplyPersistentPurchases(APawn* Pawn);
};
