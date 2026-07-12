// SibeliusHUD.cpp — center reticle + developer overlay (SIB-39).

#include "SibeliusHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"                   // GetSmallFont
#include "EngineUtils.h"                      // TActorIterator
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "InventoryComponent.h"
#include "CompileTypes.h"                     // EResourceType
#include "BuildSite.h"
#include "HatchLock.h"
#include "RefactorableComponent.h"
#include "GenerateComponent.h"                // Ch6 budget readout
#include "ProgressionSubsystem.h"             // FUN-2: sauce + powers readout
#include "SibeliusGameCharacter.h"            // IsAwayFromOffice() for the Back-to-Office hint

bool ASibeliusHUD::bOverlayVisible = true; // default ON

// Dev overlay text scale (2.0 = double size for Walt's 4K monitor). Single knob —
// scales both the glyph size and the line spacing. Bump to taste.
static constexpr float OverlayTextScale = 2.0f;

void ASibeliusHUD::BeginPlay()
{
	Super::BeginPlay();

	// FUN-7: the subsystem outlives the HUD (GameInstance vs level), so both
	// handles are released in EndPlay.
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		SauceChangedHandle = Progression->OnSauceChanged.AddUObject(this, &ASibeliusHUD::HandleSauceChanged);
		PowerUnlockedHandle = Progression->OnPowerUnlocked.AddUObject(this, &ASibeliusHUD::HandlePowerUnlocked);
	}
}

void ASibeliusHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->OnSauceChanged.Remove(SauceChangedHandle);
		Progression->OnPowerUnlocked.Remove(PowerUnlockedHandle);
	}
	Super::EndPlay(Reason);
}

void ASibeliusHUD::HandleSauceChanged(int32 /*NewTotal*/, int32 Delta)
{
	if (Delta != 0)
	{
		LastSauceDelta = Delta;
		SauceFlashUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.5 : 0.0;
	}
}

void ASibeliusHUD::HandlePowerUnlocked(EPowerVerb Verb)
{
	ShowBanner(FString::Printf(TEXT("%s  IS  YOURS"), *PowerVerbDisplayName(Verb)));
}

void ASibeliusHUD::ShowBanner(const FString& Text, float Seconds)
{
	BannerText = Text;
	BannerUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.0;
}

void ASibeliusHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawCrosshair();
	DrawBackToOfficeHint();   // independent of the dev overlay toggle — a player affordance
	DrawPlayerLayer();        // FUN-7: sauce count + ceremony banner, always on

	if (bOverlayVisible)
	{
		DrawDevOverlay();
	}
}

void ASibeliusHUD::DrawPlayerLayer()
{
	if (!Canvas)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Sauce count, top-right — the one number the player always sees.
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		const FString SauceLine = FString::Printf(TEXT("SAUCE  %d"), Progression->GetSauce());
		float W = 0.0f, H = 0.0f;
		GetTextSize(SauceLine, W, H, nullptr, OverlayTextScale);
		const float SauceX = Canvas->ClipX - W - 24.0f;
		DrawText(SauceLine, FLinearColor(0.4f, 1.0f, 0.5f, 0.95f), SauceX, 24.0f, nullptr, OverlayTextScale);

		// The +N/-N delta floats under the count, then fades.
		if (Now < SauceFlashUntil && LastSauceDelta != 0)
		{
			const float Alpha = static_cast<float>(FMath::Clamp((SauceFlashUntil - Now) / 2.5, 0.0, 1.0));
			const FString DeltaLine = FString::Printf(TEXT("%+d"), LastSauceDelta);
			const FLinearColor DeltaColor = LastSauceDelta > 0
				? FLinearColor(0.4f, 1.0f, 0.5f, Alpha)
				: FLinearColor(1.0f, 0.55f, 0.3f, Alpha);
			DrawText(DeltaLine, DeltaColor, SauceX, 24.0f + H + 4.0f, nullptr, OverlayTextScale);
		}
	}

	// The ceremony banner — centered, above the reticle.
	if (Now < BannerUntil && !BannerText.IsEmpty())
	{
		const float Scale = OverlayTextScale * 1.6f;
		float W = 0.0f, H = 0.0f;
		GetTextSize(BannerText, W, H, nullptr, Scale);
		const float Alpha = static_cast<float>(FMath::Clamp((BannerUntil - Now) / 0.75, 0.0, 1.0)); // quick fade at the end
		DrawText(BannerText, FLinearColor(0.55f, 0.95f, 1.0f, Alpha),
			(Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.32f, nullptr, Scale);
	}
}

void ASibeliusHUD::DrawBackToOfficeHint()
{
	if (!Canvas)
	{
		return;
	}

	const ASibeliusGameCharacter* PlayerChar = Cast<ASibeliusGameCharacter>(GetOwningPawn());
	if (!PlayerChar || !PlayerChar->IsAwayFromOffice())
	{
		return;
	}

	const FString Hint = TEXT("[O] Back to Office");
	float HintW = 0.0f, HintH = 0.0f;
	GetTextSize(Hint, HintW, HintH, nullptr, OverlayTextScale);
	const float HintX = (Canvas->ClipX - HintW) * 0.5f;
	const float HintY = Canvas->ClipY - HintH - 48.0f;   // a hand above the bottom edge
	DrawText(Hint, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), HintX, HintY, nullptr, OverlayTextScale);
}

void ASibeliusHUD::DrawCrosshair()
{
	if (!Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float HalfThick = Thickness * 0.5f;

	// A "+" reticle with a small center gap, centered on the exact aim point.
	DrawRect(Color, CX - CenterGap - ArmLength, CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX + CenterGap,             CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX - HalfThick, CY - CenterGap - ArmLength, Thickness, ArmLength);
	DrawRect(Color, CX - HalfThick, CY + CenterGap,             Thickness, ArmLength);
}

void ASibeliusHUD::DrawDevOverlay()
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	float Y = 50.0f;
	auto Line = [this, &Y, Font](const FString& Text, const FLinearColor& Col)
	{
		DrawText(Text, Col, 16.0f, Y, Font, OverlayTextScale); // Scale param doubles the glyphs
		Y += 15.0f * OverlayTextScale;                         // keep line spacing in step
	};

	const FLinearColor Head(0.55f, 0.85f, 1.0f, 1.0f);
	const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor Dim(0.7f, 0.7f, 0.7f, 1.0f);

	Line(TEXT("== HELP ==   (H to hide)"), Head);
	Line(TEXT("== JOURNAL ==   (J to reveal / hide)"), Head);

	APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	UWorld* W = GetWorld();

	// --- INVENTORY: every resource type + count ---
	Line(TEXT("INVENTORY"), Head);
	UInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (Inv)
	{
		if (const UEnum* ResEnum = StaticEnum<EResourceType>())
		{
			for (int32 i = 0; i < ResEnum->NumEnums(); ++i)
			{
				const FString Name = ResEnum->GetNameStringByIndex(i);
				if (Name.Contains(TEXT("_MAX")))
				{
					continue;
				}
				const int32 Count = Inv->GetCount(static_cast<EResourceType>(ResEnum->GetValueByIndex(i)));
				Line(FString::Printf(TEXT("  %s: %d"), *Name, Count), White);
			}
		}
	}
	else
	{
		Line(TEXT("  (no inventory)"), Dim);
	}

	// --- world pass for the PROGRESS counts ---
	int32 RefacTotal = 0, RefacOn = 0, HatchTotal = 0, HatchLocked = 0, SiteTotal = 0, SiteBuilt = 0;
	if (W)
	{
		for (TActorIterator<ABuildSite> It(W); It; ++It)
		{
			++SiteTotal;
			if (It->IsBuilt()) { ++SiteBuilt; }
		}
		for (TActorIterator<AHatchLock> It(W); It; ++It)
		{
			++HatchTotal;
			if (It->IsLocked()) { ++HatchLocked; }
		}
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			TInlineComponentArray<URefactorableComponent*> Comps(*It);
			for (URefactorableComponent* C : Comps)
			{
				++RefacTotal;
				if (C->IsRefactored()) { ++RefacOn; }
			}
		}
	}

	// --- PROGRESS: chapter flags (no score system yet) ---
	Line(TEXT("PROGRESS"), Head);
	Line(FString::Printf(TEXT("  refactored: %d/%d"), RefacOn, RefacTotal), White);
	Line(FString::Printf(TEXT("  hatches locked: %d/%d"), HatchLocked, HatchTotal), White);
	Line(FString::Printf(TEXT("  built sites: %d/%d"), SiteBuilt, SiteTotal), White);

	// --- FUN-2: sauce wallet + earned powers ---
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Line(FString::Printf(TEXT("  SAUCE: %d"), Progression->GetSauce()),
			FLinearColor(0.4f, 1.0f, 0.5f, 1.0f));
		FString Powers;
		for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
		{
			const EPowerVerb Verb = static_cast<EPowerVerb>(i);
			if (Progression->IsUnlocked(Verb))
			{
				Powers += (Powers.IsEmpty() ? TEXT("") : TEXT("  ")) + PowerVerbDisplayName(Verb);
			}
		}
		Line(FString::Printf(TEXT("  powers %d/%d: %s"),
			Progression->NumUnlocked(), static_cast<int32>(EPowerVerb::Count), *Powers), White);
	}

	// --- GENERATE: live budget + catalog size (Ch6) ---
	Line(TEXT("GENERATE"), Head);
	UGenerateComponent* Gen = Pawn ? Pawn->FindComponentByClass<UGenerateComponent>() : nullptr;
	if (Gen)
	{
		Line(FString::Printf(TEXT("  budget: %d    catalog: %d"), Gen->GetRemainingBudget(), Gen->GetCatalogNum()), White);
	}
	else
	{
		Line(TEXT("  (no generate component)"), Dim);
	}

	// --- CONTROLS: every binding, for reference ---
	Line(TEXT("CONTROLS"), Head);
	Line(TEXT("  F slap    E interact    V vision"), White);
	Line(TEXT("  R refactor    B build    G generate / ask"), White);
	Line(TEXT("  6 enter  7 merge  8 discard  9 clear-deploy(dev)  0 deploy"), White);
	Line(TEXT("  J journal / story    H hide/show overlay    Q quit (press twice)"), White);
	Line(TEXT("  O back to office (in a wander world)"), White);
}
