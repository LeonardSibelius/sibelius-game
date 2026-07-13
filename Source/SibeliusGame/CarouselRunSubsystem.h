// CarouselRunSubsystem.h
//
// SIB-46 — the live-play wrapper around the headless FCarouselRun state machine. A
// UGameInstanceSubsystem so run state survives level loads; exposes a BlueprintCallable API and
// BlueprintReadOnly getters, and broadcasts pipeline/run events for PRESENTATION to react to
// (lever, room reactions). Presentation only READS results — all math lives in FCarouselRun / the
// sim. The headless gate + carousel.RunDemo drive FCarouselRun directly, no subsystem needed.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CarouselTypes.h"
#include "CarouselRun.h"
#include "CarouselRunSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCarouselSpinResolved, const FSpinResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarouselRoundCleared, int32, RoundIndex, int32, CurrencyAwarded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCarouselShopOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCarouselRunEnded, bool, bWon);

UCLASS()
class SIBELIUSGAME_API UCarouselRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// FUN-4 (docs/FUN_PLAN.md Step 4) — the Carousel is where Sauce takes RISK.
	// Entry stakes Sauce; a won run pays out big plus leftover bank conversion;
	// a lost run pays a small consolation per cleared round (Raymond's rule R5:
	// the floor of every interaction is "you still got something"). The cauldron
	// stays deterministic; the gamble lives here, opt-in, currency-for-currency.
	static constexpr int32 EntryStake = 50;
	static constexpr int32 WinPayout = 150;
	static constexpr int32 SaucePerLeftoverCurrency = 5;
	static constexpr int32 ConsolationPerClearedRound = 10;

	// Charge EntryStake sauce, then start. With NO progression subsystem (the
	// bare grey-box slice) the run starts free — the slice stays portable.
	// False = couldn't afford the stake (nothing starts, nothing spent).
	UFUNCTION(BlueprintCallable, Category = "Carousel") bool StartStakedRun(int32 Seed);
	UFUNCTION(BlueprintPure, Category = "Carousel") bool IsRunStaked() const { return bStakedRun; }

	UFUNCTION(BlueprintCallable, Category = "Carousel") void StartRun(int32 Seed);
	UFUNCTION(BlueprintCallable, Category = "Carousel") bool Spin();
	UFUNCTION(BlueprintCallable, Category = "Carousel") bool Reroll();
	UFUNCTION(BlueprintCallable, Category = "Carousel") bool BuyOffering(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "Carousel") bool AdvanceToNextRound();

	UFUNCTION(BlueprintPure, Category = "Carousel") ECarouselRunPhase GetPhase() const { return Run.Phase; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetRoundIndex() const { return Run.RoundIndex; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetCurrentQuota() const { return Run.CurrentQuota; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetRoundChips() const { return Run.RoundChips; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetSpinsRemaining() const { return Run.SpinsRemaining; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetCurrency() const { return Run.Currency; }
	UFUNCTION(BlueprintPure, Category = "Carousel") int32 GetNumRounds() const { return Run.NumRounds(); }
	UFUNCTION(BlueprintPure, Category = "Carousel") bool CanSpin() const { return Run.CanSpin(); }
	UFUNCTION(BlueprintPure, Category = "Carousel") FSpinResult GetLastSpin() const { return Run.LastSpin; }
	UFUNCTION(BlueprintPure, Category = "Carousel") TArray<FShopItem> GetOfferings() const { return Run.Offerings; }

	UPROPERTY(BlueprintAssignable, Category = "Carousel") FOnCarouselSpinResolved OnSpinResolved;
	UPROPERTY(BlueprintAssignable, Category = "Carousel") FOnCarouselRoundCleared OnRoundCleared;
	UPROPERTY(BlueprintAssignable, Category = "Carousel") FOnCarouselShopOpened OnShopOpened;
	UPROPERTY(BlueprintAssignable, Category = "Carousel") FOnCarouselRunEnded OnRunEnded;

private:
	FCarouselRun Run;

	// FUN-4: true while a sauce-staked run is unresolved; settled exactly once.
	bool bStakedRun = false;
	void SettleStake(bool bWon);
};
