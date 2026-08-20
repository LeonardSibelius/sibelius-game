// CarouselHUD.cpp — SIB-46 grey-box Canvas HUD. See header.

#include "CarouselHUD.h"
#include "CarouselMachine.h"
#include "PokerMachine.h"
#include "CarouselRunSubsystem.h"
#include "ProgressionSubsystem.h"   // FUN-4: sauce balance + stake lines
#include "SibeliusReticle.h"        // the shared crosshair

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
//
// Walt 2026-07-18: the text now sits on a DIALOG PANEL — the family cabinet
// look (gold rim wrapping dark navy, same as the poker/slot screens) —
// instead of floating naked over the room. Lines are buffered, measured,
// then drawn onto the panel.

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

APokerMachine* ACarouselHUD::GetPoker()
{
	if (CachedPoker.IsValid()) { return CachedPoker.Get(); }
	for (TActorIterator<APokerMachine> It(GetWorld()); It; ++It)
	{
		CachedPoker = *It;
		return *It;
	}
	return nullptr;
}

void ACarouselHUD::DrawHUD()
{
	Super::DrawHUD();

	UCarouselRunSubsystem* RunSub = GetRun();
	if (!RunSub || !Canvas) { return; }

	// Walt: the office reticle is ASibeliusHUD's; this room needs its own draw call.
	// It used to re-hard-code the office numbers here, which is how the three reticles
	// drifted — now all three share SibeliusReticle::Draw and cannot diverge.
	SibeliusReticle::Draw(*this, *Canvas);

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const FLinearColor White(1, 1, 1), Gold(1.0f, 0.85f, 0.2f), Dim(0.7f, 0.7f, 0.7f), Green(0.4f, 1.0f, 0.4f);

	// --- The dialog panel machinery (buffer, measure, draw) -----------------
	const float HudScale = 1.8f;   // Walt QA: readable at 4K desk distance
	const float LineH = 26.0f;
	const float Pad = 26.0f;
	struct FHudLine { FString Text; FLinearColor Color; float Scale; float GapPx; };
	TArray<FHudLine> L;
	auto Line = [&L](const FString& S, const FLinearColor& C, float Scale = 1.0f) { L.Add({ S, C, Scale, 0.0f }); };
	auto Gap = [&L](float Px) { L.Add({ FString(), FLinearColor::White, 0.0f, Px }); };
	auto DrawPanel = [&](float X, float Y)
	{
		if (L.Num() == 0) { return; }
		float MaxW = 0.0f, TotH = 0.0f;
		for (const FHudLine& Ln : L)
		{
			if (Ln.Scale <= 0.0f) { TotH += Ln.GapPx; continue; }
			float W = 0.0f, H = 0.0f;
			GetTextSize(Ln.Text, W, H, Font, Ln.Scale * HudScale);
			MaxW = FMath::Max(MaxW, W);
			TotH += LineH * Ln.Scale * HudScale;
		}
		const float Rim = 4.0f;
		DrawRect(FLinearColor(0.83f, 0.66f, 0.21f, 0.95f),
			X - Pad - Rim, Y - Pad - Rim, MaxW + 2.0f * (Pad + Rim), TotH + 2.0f * (Pad + Rim));
		DrawRect(FLinearColor(0.035f, 0.035f, 0.10f, 0.94f),
			X - Pad, Y - Pad, MaxW + 2.0f * Pad, TotH + 2.0f * Pad);
		float CurY = Y;
		for (const FHudLine& Ln : L)
		{
			if (Ln.Scale <= 0.0f) { CurY += Ln.GapPx; continue; }
			DrawText(Ln.Text, Ln.Color, X, CurY, Font, Ln.Scale * HudScale);
			CurY += LineH * Ln.Scale * HudScale;
		}
		L.Reset();
	};
	const float PanelX = 64.0f, PanelY = 64.0f;

	// Two machines share this floor: the carousel's panel — and its KEYS
	// (CarouselMachine gates them the same way) — belong to standing at the
	// carousel. Far away, the screen belongs to the machine you're at: a
	// courtesy panel at the poker cabinet, a reminder if a stake still rides.
	{
		const ECarouselRunPhase P = RunSub->GetPhase();
		const bool bRunLive = (P == ECarouselRunPhase::Spinning || P == ECarouselRunPhase::Shop);
		const ACarouselMachine* M = GetMachine();
		const APawn* Pawn = GetOwningPawn();
		if (M && Pawn && FVector::Dist2D(Pawn->GetActorLocation(), M->GetActorLocation()) > 1000.0f)
		{
			if (APokerMachine* PM = GetPoker())
			{
				if (!PM->IsScreenOpen()
					&& FVector::Dist2D(Pawn->GetActorLocation(), PM->GetActorLocation()) <= 450.0f)
				{
					Line(FString::Printf(TEXT("VIDEO POKER — press E to sit down  (%d sauce a hand)"), PM->BetPerHand),
						FLinearColor(0.55f, 1.0f, 0.65f), 1.2f);
				}
			}
			if (bRunLive)
			{
				if (L.Num() > 0) { Gap(14.0f); }
				Line(TEXT("A stake rides at the Carousel — walk back to finish the round"), Dim, 0.8f);
			}
			if (L.Num() > 0) { Gap(10.0f); }
			Line(TEXT("[O] back to office"), Gold, 1.1f);
			DrawPanel(PanelX, PanelY);
			return;
		}
	}

	// --- Big-win flash overlay (scaled by the machine's reaction state) ---
	if (ACarouselMachine* M = GetMachine())
	{
		const float Flash = M->GetBigWinFlash();
		if (Flash > 0.0f)
		{
			DrawRect(FLinearColor(1.0f, 0.84f, 0.2f, 0.35f * Flash), 0, 0, Canvas->SizeX, Canvas->SizeY);
		}
	}

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

	Gap(10.0f);

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

		Gap(10.0f);
		Line(TEXT("[E] pull the lever"), Gold, 1.1f);
		Line(FString::Printf(TEXT("Reach %d chips before your pulls run out."), RunSub->GetCurrentQuota()), Dim, 0.8f);
		break;
	}

	case ECarouselRunPhase::Shop:
	{
		Line(TEXT("ROUND CLEARED — welcome to the shop."), Green, 1.1f);
		Line(FString::Printf(TEXT("COINS  %d   (upgrades last the whole ride)"), RunSub->GetCurrency()), Gold);
		Gap(6.0f);
		const TArray<FShopItem> Offerings = RunSub->GetOfferings();
		for (int32 i = 0; i < Offerings.Num(); ++i)
		{
			const FShopItem& It = Offerings[i];
			const bool bAfford = RunSub->GetCurrency() >= It.Cost;
			Line(FString::Printf(TEXT("  [%d]  %s   (%d coins%s)"),
				i + 1, *It.Label.ToString(), It.Cost, bAfford ? TEXT("") : TEXT(" — too rich for you")),
				bAfford ? White : Dim);
		}
		if (Offerings.Num() == 0) { Line(TEXT("  (sold out)"), Dim); }
		Gap(6.0f);
		Line(TEXT("[1/2/3] buy      [R] new offers      [Enter] next round"), Gold);
		break;
	}

	case ECarouselRunPhase::Won:
		Line(TEXT("YOU BEAT THE CAROUSEL"), Green, 1.2f);
		Line(FString::Printf(TEXT("Paid out: %d sauce, plus %d for every leftover coin."),
			UCarouselRunSubsystem::WinPayout, UCarouselRunSubsystem::SaucePerLeftoverCurrency), White);
		Gap(6.0f);
		Line(FString::Printf(TEXT("[E] ride again — %d sauce"), UCarouselRunSubsystem::EntryStake), Gold);
		break;

	case ECarouselRunPhase::Lost:
		Line(TEXT("The ride bucks you off."), Dim, 1.2f);
		Line(FString::Printf(TEXT("Paid out: %d sauce for each round you beat."),
			UCarouselRunSubsystem::ConsolationPerClearedRound), White);
		Gap(6.0f);
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
			Gap(10.0f);
			Line(FString::Printf(TEXT("[E] climb on — %d sauce"), UCarouselRunSubsystem::EntryStake), Gold, 1.1f);
		}
		break;
	}

	Gap(10.0f);
	Line(TEXT("[O] back to office"), Dim);

	DrawPanel(PanelX, PanelY);
}
