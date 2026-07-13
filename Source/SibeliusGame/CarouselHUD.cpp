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

namespace CarouselHUDNS
{
	static const TCHAR* PhaseName(ECarouselRunPhase P)
	{
		switch (P)
		{
		case ECarouselRunPhase::NotStarted: return TEXT("Not started");
		case ECarouselRunPhase::Spinning:   return TEXT("Spinning");
		case ECarouselRunPhase::Shop:       return TEXT("Shop");
		case ECarouselRunPhase::Won:        return TEXT("WON");
		case ECarouselRunPhase::Lost:       return TEXT("LOST");
		default:                            return TEXT("?");
		}
	}
}

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
	auto Line = [&](const FString& S, const FLinearColor& C, float Scale = 1.0f)
	{
		DrawText(S, C, X, Y, Font, Scale);
		Y += LineH * Scale;
	};

	const ECarouselRunPhase Phase = RunSub->GetPhase();

	Line(TEXT("THE CAROUSEL OF FATES"), Gold, 1.2f);

	// FUN-4: the player's real wallet, so the stakes read against something.
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (Progression)
	{
		Line(FString::Printf(TEXT("SAUCE: %d%s"), Progression->GetSauce(),
			RunSub->IsRunStaked() ? TEXT("     (a stake is riding)") : TEXT("")), Green);
	}

	Line(FString::Printf(TEXT("Phase: %s     Round %d / %d"),
		CarouselHUDNS::PhaseName(Phase), RunSub->GetRoundIndex() + 1, RunSub->GetNumRounds()), White);
	Line(FString::Printf(TEXT("Chips this round: %d / %d      Spins left: %d"),
		RunSub->GetRoundChips(), RunSub->GetCurrentQuota(), RunSub->GetSpinsRemaining()), White);
	Line(FString::Printf(TEXT("Currency (bank): %d"), RunSub->GetCurrency()), Gold);

	const FSpinResult Last = RunSub->GetLastSpin();
	Line(FString::Printf(TEXT("Last spin: payout %d   x%.2f%s%s"),
		Last.SpinPayout, Last.GlobalMultiplierPercent / 100.0f,
		Last.bBonusTriggered ? TEXT("   [SCATTER!]") : TEXT(""),
		Last.bWasFreeSpin ? TEXT("   (free spin)") : TEXT("")), Last.SpinPayout > 0 ? Green : Dim);

	Y += 10.0f;

	switch (Phase)
	{
	case ECarouselRunPhase::Spinning:
		Line(TEXT("[E] pull the lever"), Gold);
		break;

	case ECarouselRunPhase::Shop:
	{
		Line(TEXT("-- SHOP --   [1/2/3] buy    [R] reroll    [Enter] continue"), Gold);
		const TArray<FShopItem> Offerings = RunSub->GetOfferings();
		for (int32 i = 0; i < Offerings.Num(); ++i)
		{
			const FShopItem& It = Offerings[i];
			const bool bAfford = RunSub->GetCurrency() >= It.Cost;
			Line(FString::Printf(TEXT("  [%d] %s  (cost %d)%s"),
				i + 1, *It.Label.ToString(), It.Cost, bAfford ? TEXT("") : TEXT("  -- too expensive")),
				bAfford ? White : Dim);
		}
		if (Offerings.Num() == 0) { Line(TEXT("  (sold out — [Enter] to continue)"), Dim); }
		break;
	}

	case ECarouselRunPhase::Won:
		Line(FString::Printf(TEXT("RUN CLEARED!   [E] new run (stakes %d sauce)"),
			UCarouselRunSubsystem::EntryStake), Green, 1.2f);
		break;

	case ECarouselRunPhase::Lost:
		Line(FString::Printf(TEXT("Run over.   [E] new run (stakes %d sauce)"),
			UCarouselRunSubsystem::EntryStake), Dim, 1.2f);
		break;

	default:
		// Walt's lost-refusal lesson: if the player can't afford the stake, the
		// HUD says so PERMANENTLY — never a 4-second toast in a busy corner.
		if (Progression && Progression->GetSauce() < UCarouselRunSubsystem::EntryStake)
		{
			Line(FString::Printf(TEXT("The Carousel demands %d SAUCE — you carry only %d."),
				UCarouselRunSubsystem::EntryStake, Progression->GetSauce()), FLinearColor(1.0f, 0.6f, 0.25f), 1.2f);
			Line(TEXT("Go earn more: books, curios, Refusers.   [O] back to office"), Dim);
		}
		else
		{
			Line(FString::Printf(TEXT("[E] start a run — stakes %d SAUCE. Win: +%d plus %d per banked coin. Lose: +%d per cleared round."),
				UCarouselRunSubsystem::EntryStake, UCarouselRunSubsystem::WinPayout,
				UCarouselRunSubsystem::SaucePerLeftoverCurrency, UCarouselRunSubsystem::ConsolationPerClearedRound), Gold);
		}
		break;
	}

	Y += 10.0f;
	Line(TEXT("[O] back to office"), Dim);
}
