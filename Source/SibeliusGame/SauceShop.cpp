// SauceShop.cpp — cauldron catalog + purchase logic (FUN-3). See header.

#include "SauceShop.h"
#include "ProgressionSubsystem.h"
#include "GenerateComponent.h"
#include "SlapComponent.h"
#include "SibeliusGame.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FName BudgetOfferKey(TEXT("Budget.Generate"));
	const FName SlapOfferKey(TEXT("Slap.Mighty"));

	// Apply ONE purchase-step of an upgrade offer to the live pawn. The power
	// unlock is not here — it lands on the subsystem, not the pawn.
	void ApplyUpgradeStep(APawn* Pawn, FName OfferKey)
	{
		if (!Pawn)
		{
			return;
		}
		if (OfferKey == BudgetOfferKey)
		{
			if (UGenerateComponent* Gen = Pawn->FindComponentByClass<UGenerateComponent>())
			{
				Gen->SetRemainingBudget(Gen->GetRemainingBudget() + FSauceShop::BudgetPerPurchase);
			}
		}
		else if (OfferKey == SlapOfferKey)
		{
			if (USlapComponent* Slap = Pawn->FindComponentByClass<USlapComponent>())
			{
				Slap->LaunchSpeed *= FSauceShop::SlapMultiplier;
				Slap->UpwardSpeed *= FSauceShop::SlapMultiplier;
			}
		}
	}
}

void FSauceShop::BuildOffers(const FProgressionState& State, TArray<FSauceOffer>& OutOffers)
{
	OutOffers.Reset();

	// Still-locked powers, in verb order — the expensive alt-path to finding a
	// shrine. Bought powers vanish from the menu (the unlock mask IS the record).
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		const EPowerVerb Verb = static_cast<EPowerVerb>(i);
		if (State.IsUnlocked(Verb))
		{
			continue;
		}
		FSauceOffer Offer;
		Offer.Key = FName(*FString::Printf(TEXT("Power.%s"), *PowerVerbDisplayName(Verb)));
		Offer.Title = FString::Printf(TEXT("The power of %s"), *PowerVerbDisplayName(Verb));
		Offer.Desc = TEXT("Blend it directly from the Sauce — or find where it is granted, free.");
		Offer.Cost = PowerUnlockCost;
		Offer.bIsPowerUnlock = true;
		Offer.Power = Verb;
		OutOffers.Add(Offer);
	}

	// Upgrades with remaining stock.
	const int32 BudgetBought = State.GetPurchaseCount(BudgetOfferKey);
	if (BudgetBought < BudgetMaxPurchases)
	{
		FSauceOffer Offer;
		Offer.Key = BudgetOfferKey;
		Offer.Title = TEXT("Deeper Generate budget");
		Offer.Desc = FString::Printf(TEXT("+%d generation budget, every session. (%d/%d bought)"),
			BudgetPerPurchase, BudgetBought, BudgetMaxPurchases);
		Offer.Cost = BudgetCost;
		Offer.MaxCount = BudgetMaxPurchases;
		OutOffers.Add(Offer);
	}

	const int32 SlapBought = State.GetPurchaseCount(SlapOfferKey);
	if (SlapBought < SlapMaxPurchases)
	{
		FSauceOffer Offer;
		Offer.Key = SlapOfferKey;
		Offer.Title = TEXT("Mighty Slap");
		Offer.Desc = FString::Printf(TEXT("+50%% slap force, every session. (%d/%d bought)"),
			SlapBought, SlapMaxPurchases);
		Offer.Cost = SlapCost;
		Offer.MaxCount = SlapMaxPurchases;
		OutOffers.Add(Offer);
	}
}

bool FSauceShop::TryPurchase(UProgressionSubsystem* Progression, APawn* Pawn, const FSauceOffer& Offer)
{
	if (!Progression)
	{
		return false;
	}

	// Stock check first, so a stale menu row can't oversell.
	if (Offer.bIsPowerUnlock)
	{
		if (Progression->IsUnlocked(Offer.Power))
		{
			return false;
		}
	}
	else if (Offer.MaxCount > 0 && Progression->GetPurchaseCount(Offer.Key) >= Offer.MaxCount)
	{
		return false;
	}

	if (!Progression->TrySpendSauce(Offer.Cost))
	{
		return false; // can't afford — nothing changed
	}

	if (Offer.bIsPowerUnlock)
	{
		Progression->UnlockPower(Offer.Power);
	}
	else
	{
		Progression->RecordPurchase(Offer.Key);
		ApplyUpgradeStep(Pawn, Offer.Key);
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[SauceShop] bought '%s' for %d (sauce left: %d)"),
		*Offer.Title, Offer.Cost, Progression->GetSauce());
	return true;
}

void FSauceShop::ApplyPersistentPurchases(APawn* Pawn)
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(Pawn);
	if (!Pawn || !Progression)
	{
		return;
	}

	// Components spawn with authored defaults, so applying count steps from that
	// clean base is exact — no double-apply across respawns or level travel.
	for (int32 i = 0; i < Progression->GetPurchaseCount(BudgetOfferKey); ++i)
	{
		ApplyUpgradeStep(Pawn, BudgetOfferKey);
	}
	for (int32 i = 0; i < Progression->GetPurchaseCount(SlapOfferKey); ++i)
	{
		ApplyUpgradeStep(Pawn, SlapOfferKey);
	}
}
