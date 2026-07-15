// ProgressionTypes.h
//
// FUN-1 (docs/FUN_PLAN.md Step 1+2) — the pure progression model: which power
// verbs the player has EARNED, how much Sauce (the unified currency) they hold,
// and which one-time grants have been claimed. A plain struct with no world or
// subsystem coupling so the headless smoke test exercises it directly — the
// same model-first culture as FCarouselRun / USlotGameModel.
//
// UProgressionSubsystem owns the live instance and is the only runtime writer.

#pragma once

#include "CoreMinimal.h"
#include "ProgressionTypes.generated.h"

// The six earnable power verbs, one per chapter. Code Vision is the starter
// power (unlocked in a fresh state) — the opening minutes need one verb, and
// Ch1's door literally cannot be found without it.
UENUM(BlueprintType)
enum class EPowerVerb : uint8
{
	CodeVision,
	Refactor,
	Compile,
	TestDrive,
	Deploy,
	Generate,

	Count UMETA(Hidden)
};

// Display name for prompts/toasts ("CODE VISION", "TEST-DRIVE", ...).
SIBELIUSGAME_API FString PowerVerbDisplayName(EPowerVerb Verb);

// APPEAL-5 — lifetime stat keys (the RECORDS tab). One namespace so every bump
// site and the menu spell a key identically; a new stat is a new FName here
// plus a bump at its code point — the map needs no schema change.
namespace SibeliusStats
{
	inline const FName RefusersSlapped(TEXT("RefusersSlapped"));
	inline const FName SauceEarned(TEXT("SauceEarned"));         // lifetime gross, not the wallet
	inline const FName BooksCollected(TEXT("BooksCollected"));
	inline const FName CuriosCollected(TEXT("CuriosCollected"));
	inline const FName ChaptersCompleted(TEXT("ChaptersCompleted"));
	inline const FName CarouselRuns(TEXT("CarouselRuns"));
	inline const FName CarouselWins(TEXT("CarouselWins"));
	inline const FName CarouselBestRound(TEXT("CarouselBestRound"));   // max, not sum
	inline const FName CarouselBestSpin(TEXT("CarouselBestSpin"));     // max, not sum
}

// Parse a loose name ("refactor", "TestDrive", "test-drive") — for the exec
// cheats. False if nothing matched.
SIBELIUSGAME_API bool ParsePowerVerb(const FString& Name, EPowerVerb& OutVerb);

USTRUCT(BlueprintType)
struct SIBELIUSGAME_API FProgressionState
{
	GENERATED_BODY()

	// Bit per EPowerVerb. A fresh state has Code Vision only.
	UPROPERTY(SaveGame)
	uint8 UnlockedMask = 1 << static_cast<uint8>(EPowerVerb::CodeVision);

	// The unified currency (FUN_PLAN Step 2). Never negative.
	UPROPERTY(SaveGame)
	int32 Sauce = 0;

	// One-time grant keys already claimed (power shrines, chapter-end rewards)
	// — so replaying a level can't farm its big grants.
	UPROPERTY(SaveGame)
	TArray<FName> ClaimedGrants;

	bool IsUnlocked(EPowerVerb Verb) const;
	bool Unlock(EPowerVerb Verb);        // true only when newly unlocked
	void UnlockAll();
	int32 NumUnlocked() const;

	void AddSauce(int32 Amount);         // amounts <= 0 are ignored
	bool TrySpendSauce(int32 Amount);    // false (and no change) if Sauce < Amount

	bool HasClaimed(FName GrantKey) const;
	bool Claim(FName GrantKey);          // true first time, false on re-claim / NAME_None

	// FUN-3: how many times each cauldron offer has been bought — the record the
	// shop re-applies at spawn so purchased upgrades persist across sessions.
	UPROPERTY(SaveGame)
	TMap<FName, int32> PurchaseCounts;

	int32 GetPurchaseCount(FName OfferKey) const;
	void RecordPurchase(FName OfferKey); // NAME_None ignored

	// APPEAL-5: lifetime stats — counters that only ever go up (slot-floor
	// wisdom: people return for records and streaks, not content). Additive
	// field: old saves default-fill to an empty map, every stat reads 0.
	UPROPERTY(SaveGame)
	TMap<FName, int32> LifetimeStats;

	int32 GetStat(FName Key) const;          // 0 for unknown keys
	void BumpStat(FName Key, int32 Delta = 1);   // Delta <= 0 or NAME_None ignored
	void RaiseStat(FName Key, int32 Value);      // record semantics: only ever raises
};

// Headless self-test (ProgressionSmokeTest). True when every assert passes.
SIBELIUSGAME_API bool RunProgressionSelfTest(FString& OutError);
