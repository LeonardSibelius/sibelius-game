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
#include "SlotTypes.h"   // FSlotMeters — the lifetime hard meters live in the save
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

/**
 * Walt's message to the employer this power belongs to — docs/MEMOIR_VOICE.md, verbatim.
 * Forty years, one sentence at a time. Empty for an unrecognised verb.
 *
 * Lives here rather than in the HUD because it is now read in two places: the flash at the
 * moment a power is earned, and the player's collected record in the Journal. It was a
 * file-static inside SibeliusHUD.cpp, which meant the strongest writing in the project
 * existed in exactly one place that could show it, for twelve seconds, once.
 */
SIBELIUSGAME_API FString PowerVerbMemoir(EPowerVerb Verb);

/**
 * ALL EIGHT messages, in CHRONOLOGICAL order — 1988 to 2022. What the finale plays.
 *
 * Six of them are the power memoirs above. The other two belong to jobs that never became
 * a power: San Diego County (PCMS, 2005) and Bally (the Slot Data System, 2007 — the data
 * warehouse and the reporting, the closest he got to the floor without ever building the
 * machine). Those two had no home in the code at all until the finale needed them.
 *
 * Chronological rather than gameplay order because this is a career being read back, and
 * the last one has to be the last one: iKrome, 2022, "being retired now, like all the
 * rest."
 */
SIBELIUSGAME_API const TArray<FString>& AllMemoirMessages();

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

/* ONE THING THE PLAYER GENERATED, remembered across a level change.
 *
 * Walt, coming back from uFoods: "I go back to the spaceport, but it is gone - it should
 * be there when we start boarding, right?"
 *
 * It should. Generated objects used to survive only a manual Deploy, because
 * UBranchSubsystem is a WORLD subsystem and dies with the world - so a lamp made before
 * pressing [O] was gone on return too. Nobody noticed until the spaceport, which is the
 * first generated thing the story has to walk away from and come back to.
 *
 * LEVEL NAME IS PART OF THE RECORD. Without it a spaceport built in the city would be
 * rebuilt in the cafe, which is how a save format quietly becomes a haunting.
 *
 * The GUID is the actor's IBranchable id, kept so Deploy and Test-Drive still recognise a
 * respawned object as the same one. Losing it would not be visible until somebody
 * deployed, restored, and found a duplicate.
 */
USTRUCT()
struct SIBELIUSGAME_API FGeneratedSiteRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName LevelName;

	UPROPERTY(SaveGame)
	FName EntryId;

	UPROPERTY(SaveGame)
	FTransform Transform;

	UPROPERTY(SaveGame)
	FGuid ObjectId;
};

USTRUCT(BlueprintType)
struct SIBELIUSGAME_API FProgressionState
{
	GENERATED_BODY()

	// Bit per EPowerVerb. A fresh state has Code Vision only.
	UPROPERTY(SaveGame)
	uint8 UnlockedMask = 1 << static_cast<uint8>(EPowerVerb::CodeVision);

	// The unified currency (FUN_PLAN Step 2). Never negative.
	// Fresh players start with 50 so they can sit at video poker (10 a hand)
	// before they've collected books. Existing saves keep whatever they had.
	UPROPERTY(SaveGame)
	int32 Sauce = 50;

	// One-time grant keys already claimed (power shrines, chapter-end rewards)
	// — so replaying a level can't farm its big grants.
	UPROPERTY(SaveGame)
	TArray<FName> ClaimedGrants;

	/* Everything the player has Generated and not discarded, so it is still standing when
	   he comes back to the level. Old saves load with this empty, which is correct: they
	   were made in a world where nothing persisted anyway. */
	UPROPERTY(SaveGame)
	TArray<FGeneratedSiteRecord> GeneratedSites;

	bool IsUnlocked(EPowerVerb Verb) const;
	bool Unlock(EPowerVerb Verb);        // true only when newly unlocked
	void UnlockAll();
	int32 NumUnlocked() const;

	void AddSauce(int32 Amount);         // amounts <= 0 are ignored
	bool TrySpendSauce(int32 Amount);    // false (and no change) if Sauce < Amount

	bool HasClaimed(FName GrantKey) const;
	bool Claim(FName GrantKey);          // true first time, false on re-claim / NAME_None

	/* Remember one generated object. Keyed on ObjectId, so re-recording the same actor
	   updates it rather than growing a duplicate - which matters because a Test-Drive
	   branch can re-author a site that is already remembered. */
	void RememberGeneratedSite(FName LevelName, FName EntryId,
		const FTransform& Transform, const FGuid& ObjectId);

	/** Forget one, by the id the world knows it by. True if something was removed. */
	bool ForgetGeneratedSite(const FGuid& ObjectId);

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

	/**
	 * The player's par sheet, stored as the FOUR DIAL VALUES rather than as a par sheet.
	 *
	 * A saved FSlotParSheet would pin the strip, paytable and paylines of whatever build
	 * wrote it. Retune the shipped machine in a later version and every returning player
	 * would silently still be running the old one, with no way to tell. Dials re-apply to
	 * the CURRENT factory sheet, so "more wilds, bigger jackpot" survives changes to the
	 * machine underneath it.
	 *
	 * It is also four numbers instead of a hundred, and clamping four numbers on load is
	 * trivial where validating a whole sheet is not.
	 *
	 * Negative sentinels mean "never edited — use the factory value", so old saves and
	 * fresh ones both yield the shipped machine. Additive fields: old saves default-fill.
	 */
	UPROPERTY(SaveGame)
	float SlotPaysMultiplier = -1.0f;

	UPROPERTY(SaveGame)
	int32 SlotWildCount = -1;

	UPROPERTY(SaveGame)
	float SlotJackpotPay = -1.0f;

	UPROPERTY(SaveGame)
	int32 SlotBonusCount = -1;

	/** True once the player has actually turned a dial. */
	bool HasEditedParSheet() const
	{
		return SlotPaysMultiplier > 0.0f || SlotWildCount >= 0
			|| SlotJackpotPay > 0.0f || SlotBonusCount >= 0;
	}

	/**
	 * THE HARD METERS — lifetime play on the cathedral machine (docs/FLOOR_REPORT.md).
	 *
	 * Locked decision 2: nothing clears these, ever. There is no player-facing reset,
	 * because a lifetime meter you can zero is not a lifetime meter — on a real cabinet
	 * that property is a regulatory requirement, and here it is what makes the number
	 * mean anything.
	 *
	 * Additive field: old saves default-fill to all zeroes, which reads correctly as
	 * "never played". Zero is the right initial value, so unlike the dials above these
	 * need no negative sentinel.
	 *
	 * NOTE these can span more than one par sheet — see HasEditedParSheet(), which the
	 * meters page uses to footnote exactly that.
	 */
	UPROPERTY(SaveGame)
	FSlotMeters SlotLifetimeMeters;

	/* ---------------- THE TOLL ----------------

	   Mrs. Hall's Refuser Army of Arrogant Architects is not something you walk up to.
	   The Cathedral machine is the toll, and this is the one place the price lives.

	   IT IS COINOUT - what the machine has PAID OUT over its life - and the choice
	   matters more than the number does.

	   Net profit was the obvious reading and it is unreachable by design: the shipped
	   sheet measures 95.43% RTP, so play trends DOWN about seven credits a spin. A gate
	   on being ahead is a jackpot lottery, not a goal, and most players would never see
	   the battle. CoinOut accrues whether you are winning or losing - at 150 a spin it
	   is roughly 203 spins - so the door is a matter of sitting there, which is the
	   correct thing to ask of a man who spent forty years doing exactly that.

	   And it can never be laundered. Locked decision 2 already says nothing clears these
	   meters, ever. That decision was made for a different reason and it is what makes a
	   door hung on this number honest. */
	static constexpr int64 BattleQualifyingCoinOut = 5000;

	int64 BattleCreditsPaid() const { return SlotLifetimeMeters.CoinOut; }
	bool IsBattleQualified() const { return SlotLifetimeMeters.CoinOut >= BattleQualifyingCoinOut; }
};

// Headless self-test (ProgressionSmokeTest). True when every assert passes.
SIBELIUSGAME_API bool RunProgressionSelfTest(FString& OutError);
