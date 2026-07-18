// CarouselHUD.cpp — SIB-46 grey-box Canvas HUD. See header.

#include "CarouselHUD.h"
#include "CarouselMachine.h"
#include "CarouselRunSubsystem.h"
#include "ProgressionSubsystem.h"   // FUN-4: sauce balance + stake lines

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

// Walt 2026-07-17: the old HUD dumped every number at once (Phase, quota,
// bank, multiplier) and he couldn't follow the game. New rule: plain words,
// only what matters RIGHT NOW, and the money has exactly two in-ride names —
// CHIPS (fill the round bar) and COINS (spend in the shop). Sauce stays the
// outside wallet.

UCarouselRunSubsystem* ACarouselHUD::GetRun() const
{
	const UWorld* W = GetWorld();
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCarouselRunSubsystem>() : nullptr;
}

ACarouselMachine* ACarouselHUD::GetMachine()
{
	if (CachedMachine.IsValid()) { return CachedMachine.Get(); }
	for (TActorIterator<ACarouselMachine> It(GetWorld()); It; ++It)
	{
		CachedMachine = *It;
		return *It;
	}
	return nullptr;
}

void ACarouselHUD::DrawHUD()
{
	Super::DrawHUD();

	UCarouselRunSubsystem* RunSub = GetRun();
	if (!RunSub || !Canvas) { return; }

	// Walt: the office reticle is ASibeliusHUD's; this room needs its own.
	// Same "+" with a center gap (values mirror the office defaults).
	{
		const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
		const float Arm = 10.0f, Thick = 2.0f, Gap = 3.0f;
		const FLinearColor RetColor(1.0f, 1.0f, 1.0f, 0.85f);
		DrawRect(RetColor, CX - Gap - Arm, CY - Thick * 0.5f, Arm, Thick);
		DrawRect(RetColor, CX + Gap,       CY - Thick * 0.5f, Arm, Thick);
		DrawRect(RetColor, CX - Thick * 0.5f, CY - Gap - Arm, Thick, Arm);
		DrawRect(RetColor, CX - Thick * 0.5f, CY + Gap,       Thick, Arm);
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const FLinearColor White(1, 1, 1), Gold(1.0f, 0.85f, 0.2f), Dim(0.7f, 0.7f, 0.7f), Green(0.4f, 1.0f, 0.4f);

	// --- Big-win flash overlay (scaled by the machine's reaction state) ---
	if (ACarouselMachine* M = GetMachine())
	{
		const float Flash = M->GetBigWinFlash();
		if (Flash > 0.0f)
		{
			DrawRect(FLinearColor(1.0f, 0.84f, 0.2f, 0.35f * Flash), 0, 0, Canvas->SizeX, Canvas->SizeY);
		}
	}

	const float X = 60.0f;
	float Y = 60.0f;
	const float LineH = 26.0f;
	// Walt QA: readable at 4K desk distance — everything drawn 1.8x.
	const float HudScale = 1.8f;
	auto Line = [&](const FString& S, const FLinearColor& C, float Scale = 1.0f)
	{
		DrawText(S, C, X, Y, Font, Scale * HudScale);
		Y += LineH * Scale * HudScale;
	};

	const ECarouselRunPhase Phase = RunSub->GetPhase();

	Line(TEXT("THE CAROUSEL OF FATES"), Gold, 1.2f);

	// FUN-4: the player's real wallet, so the stakes read against something.
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (Progression)
	{
		Line(FString::Printf(TEXT("SAUCE: %d%s"), Progression->GetSauce(),
			RunSub->IsRunStaked()
				? *FString::Printf(TEXT("     (%d staked on this ride)"), UCarouselRunSubsystem::EntryStake)
				: TEXT("")), Green);
	}

	Y += 10.0f;

	switch (Phase)
	{
	case ECarouselRunPhase::Spinning:
	{
		Line(FString::Printf(TEXT("ROUND %d of %d"), RunSub->GetRoundIndex() + 1, RunSub->GetNumRounds()), White, 1.1f);
		Line(FString::Printf(TEXT("CHIPS  %d / %d        PULLS LEFT  %d"),
			RunSub->GetRoundChips(), RunSub->GetCurrentQuota(), RunSub->GetSpinsRemaining()), White, 1.1f);
		Line(FString::Printf(TEXT("COINS  %d   (shop money — spend between rounds)"), RunSub->GetCurrency()), Gold);

		const FSpinResult Last = RunSub->GetLastSpin();
		if (Last.SpinPayout > 0 || Last.bBonusTriggered)
		{
			Line(FString::Printf(TEXT("Last pull: +%d chips%s%s"),
				Last.SpinPayout,
				Last.bBonusTriggered ? TEXT("   BONUS!") : TEXT(""),
				Last.bWasFreeSpin ? TEXT("   (free pull)") : TEXT("")), Green);
		}

		Y += 10.0f;
		Line(TEXT("[E] pull the lever"), Gold, 1.1f);
		Line(FString::Printf(TEXT("Reach %d chips before your pulls run out."), RunSub->GetCurrentQuota()), Dim, 0.8f);
		break;
	}

	case ECarouselRunPhase::Shop:
	{
		Line(TEXT("ROUND CLEARED — welcome to the shop."), Green, 1.1f);
		Line(FString::Printf(TEXT("COINS  %d   (upgrades last the whole ride)"), RunSub->GetCurrency()), Gold);
		Y += 6.0f;
		const TArray<FShopItem> Offerings = RunSub->GetOfferings();
		for (int32 i = 0; i < Offerings.Num(); ++i)
		{
			const FShopItem& It = Offerings[i];
			const bool bAfford = RunSub->GetCurrency() >= It.Cost;
			Line(FString::Printf(TEXT("  [%d]  %s  — %d coins%s"),
				i + 1, *It.Label.ToString(), It.Cost, bAfford ? TEXT("") : TEXT("   (not enough coins)")),
				bAfford ? White : Dim);
		}
		if (Offerings.Num() == 0) { Line(TEXT("  (sold out)"), Dim); }
		Y += 6.0f;
		Line(TEXT("[1/2/3] buy      [R] new offers      [Enter] next round"), Gold);
		break;
	}

	case ECarouselRunPhase::Won:
		Line(TEXT("YOU BEAT THE CAROUSEL"), Green, 1.2f);
		Line(FString::Printf(TEXT("Paid out: %d sauce, plus %d for every leftover coin."),
			UCarouselRunSubsystem::WinPayout, UCarouselRunSubsystem::SaucePerLeftoverCurrency), White);
		Y += 6.0f;
		Line(FString::Printf(TEXT("[E] ride again — %d sauce"), UCarouselRunSubsystem::EntryStake), Gold);
		break;

	case ECarouselRunPhase::Lost:
		Line(TEXT("The ride bucks you off."), Dim, 1.2f);
		Line(FString::Printf(TEXT("Paid out: %d sauce for each round you beat."),
			UCarouselRunSubsystem::ConsolationPerClearedRound), White);
		Y += 6.0f;
		Line(FString::Printf(TEXT("[E] ride again — %d sauce"), UCarouselRunSubsystem::EntryStake), Gold);
		break;

	default:
		// Walt's lost-refusal lesson: if the player can't afford the stake, the
		// HUD says so PERMANENTLY — never a 4-second toast in a busy corner.
		if (Progression && Progression->GetSauce() < UCarouselRunSubsystem::EntryStake)
		{
			Line(FString::Printf(TEXT("The Carousel demands %d SAUCE — you carry only %d."),
				UCarouselRunSubsystem::EntryStake, Progression->GetSauce()), FLinearColor(1.0f, 0.6f, 0.25f), 1.2f);
			Line(TEXT("Go earn more: books, Refusers, the temple fountain.   [O] back to office"), Dim);
		}
		else
		{
			Line(TEXT("HOW TO PLAY"), White, 1.1f);
			Line(FString::Printf(TEXT("1. Pay %d sauce to climb on  [E]"), UCarouselRunSubsystem::EntryStake), White);
			Line(TEXT("2. Pull the lever. Every pull wins CHIPS."), White);
			Line(TEXT("3. Fill the round's chip bar before your pulls run out."), White);
			Line(TEXT("4. Each round you beat pays COINS — spend them in the shop on upgrades."), White);
			Line(FString::Printf(TEXT("5. Beat every round: %d sauce, plus %d per leftover coin."),
				UCarouselRunSubsystem::WinPayout, UCarouselRunSubsystem::SaucePerLeftoverCurrency), White);
			Line(FString::Printf(TEXT("   Fall short: %d sauce for each round you beat."),
				UCarouselRunSubsystem::ConsolationPerClearedRound), Dim);
			Y += 10.0f;
			Line(FString::Printf(TEXT("[E] climb on — %d sauce"), UCarouselRunSubsystem::EntryStake), Gold, 1.1f);
		}
		break;
	}

	Y += 10.0f;
	Line(TEXT("[O] back to office"), Dim);
}
