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
#include "HallAlarmSubsystem.h"               // APPEAL-2: refuser-wave objective override
#include "RefuserController.h"                // APPEAL-2: live-refuser check
#include "SibeliusProgressSubsystem.h"        // APPEAL-2: bSlotPlayed endgame state
#include "GameFramework/Character.h"

// FUN-8: default OFF now that the player has real surfaces (Tab menu, sauce
// counter, banners). H brings it back — it's Walt's debug view, not the UI.
bool ASibeliusHUD::bOverlayVisible = false;

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
	DrawObjective();          // APPEAL-2: the one guided goal, top-center
	DrawWorldName();          // APPEAL extra: which world am I in

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

	// Walt's ask: a launch hint, upper-left, sauce-green — the two keys that
	// open everything else. Shows for the first stretch of each world, then
	// fades (the M menu itself carries the full list). Suppressed while the
	// dev overlay owns that corner.
	constexpr double HintVisibleSeconds = 40.0;
	constexpr double HintFadeSeconds = 5.0;
	if (!bOverlayVisible && Now < HintVisibleSeconds + HintFadeSeconds)
	{
		const float HintAlpha = static_cast<float>(
			FMath::Clamp((HintVisibleSeconds + HintFadeSeconds - Now) / HintFadeSeconds, 0.0, 1.0));
		DrawText(TEXT("M for Status, J for Journal"),
			FLinearColor(0.4f, 1.0f, 0.5f, 0.95f * HintAlpha), 16.0f, 24.0f, nullptr, OverlayTextScale);
	}

	// Sauce count, top-right — the one number the player always sees.
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		const FString SauceLine = FString::Printf(TEXT("SAUCE  %d"), Progression->GetSauce());
		float W = 0.0f, H = 0.0f;
		GetTextSize(SauceLine, W, H, nullptr, OverlayTextScale);
		const float SauceX = Canvas->ClipX - W - 24.0f;
		DrawText(SauceLine, FLinearColor(0.4f, 1.0f, 0.5f, 0.95f), SauceX, 24.0f, nullptr, OverlayTextScale);

		// Walt's ask: a standing reminder of the menu key, tucked under the count.
		// (M, not Tab — Slate eats Tab in PIE; see the character's key bindings.)
		const FString MenuHint = TEXT("[M] status");
		float HintW = 0.0f, HintH = 0.0f;
		const float HintScale = OverlayTextScale * 0.7f;
		GetTextSize(MenuHint, HintW, HintH, nullptr, HintScale);
		DrawText(MenuHint, FLinearColor(0.7f, 0.7f, 0.7f, 0.7f),
			Canvas->ClipX - HintW - 24.0f, 24.0f + H + 4.0f, nullptr, HintScale);

		// The +N/-N delta floats under the hint, then fades.
		if (Now < SauceFlashUntil && LastSauceDelta != 0)
		{
			const float Alpha = static_cast<float>(FMath::Clamp((SauceFlashUntil - Now) / 2.5, 0.0, 1.0));
			const FString DeltaLine = FString::Printf(TEXT("%+d"), LastSauceDelta);
			const FLinearColor DeltaColor = LastSauceDelta > 0
				? FLinearColor(0.4f, 1.0f, 0.5f, Alpha)
				: FLinearColor(1.0f, 0.55f, 0.3f, Alpha);
			DrawText(DeltaLine, DeltaColor, SauceX, 24.0f + H + HintH + 8.0f, nullptr, OverlayTextScale);
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

FString ASibeliusHUD::ComputeObjective() const
{
	// APPEAL_PLAN point 2: a stranger should always know the ONE next thing.
	// Derived from live state each frame — first unmet beat wins.
	const UWorld* W = GetWorld();
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	const UInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!W || !GI || !Progression)
	{
		return FString();
	}

	// 0) A refuser wave overrides everything — teach the slap under pressure.
	if (const UHallAlarmSubsystem* Alarm = GI->GetSubsystem<UHallAlarmSubsystem>())
	{
		if (Alarm->IsAlarmTriggered())
		{
			for (TActorIterator<ACharacter> It(const_cast<UWorld*>(W)); It; ++It)
			{
				if (Cast<ARefuserController>(It->GetController()))
				{
					return TEXT("Mrs. Hall's Refusers are loose — get close and SLAP them [F]");
				}
			}
		}
	}

	const int32 Books = Inv ? Inv->GetCount(EResourceType::Book) : 0;
	const int32 Keys  = Inv ? Inv->GetCount(EResourceType::Key) : 0;
	const int32 Powers = Progression->NumUnlocked();
	const int32 PowerCount = static_cast<int32>(EPowerVerb::Count);

	// 1) ALL POWERS outranks the early beats (Walt QA: a veteran whose keys
	// were long spent got told to build a key while standing at the finale).
	if (Powers >= PowerCount)
	{
		if (const USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
		{
			if (!Progress->bSlotPlayed)
			{
				// Location-aware: in the cathedral itself, say exactly what to do
				// with the wall in your face.
				FString Map = W->GetMapName();
				Map.RemoveFromStart(W->StreamingLevelsPrefix);
				if (Map.Contains(TEXT("Cathedral")))
				{
					return TEXT("Stand at the ALTAR (the low block before the wall) — it will call your six powers in turn; the wall falls when the rite completes");
				}
				return TEXT("You are whole. The cathedral altar awaits the Synthesis");
			}
		}
		return FString();   // post-game free play: no nagging
	}

	// 2) The opening beat: books are the first thing a stranger can DO.
	if (Books == 0 && Keys == 0 && Powers == 0)
	{
		return TEXT("Explore the office — collect the glowing books [E]");
	}

	// 3) Books in hand but no way to spend them yet: earn Compile.
	if (Keys == 0 && !Progression->IsUnlocked(EPowerVerb::Compile))
	{
		return TEXT("A power is granted somewhere in this house — find COMPILE");
	}

	// 4) Compile earned: build the key from the books.
	if (Keys == 0)
	{
		return TEXT("Take your books upstairs — face the build site and press [B] to build the key");
	}

	// 5) Key in hand, powers remain: the wider house opens.
	return FString::Printf(
		TEXT("Your key opens the attic. Powers earned: %d of %d — seek the granting places"),
		Powers, PowerCount);
}

void ASibeliusHUD::DrawObjective()
{
	if (!Canvas || bOverlayVisible)
	{
		return;   // the dev overlay owns the screen when visible
	}
	const FString Objective = ComputeObjective();
	if (Objective.IsEmpty())
	{
		return;
	}
	// Walt QA: the first cut was unreadable at 4K across a desk. Big, full-bright
	// gold on a dark backing strip — a proper quest banner.
	const float Scale = OverlayTextScale * 1.3f;
	float W = 0.0f, H = 0.0f;
	GetTextSize(Objective, W, H, nullptr, Scale);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 20.0f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - 14.0f, Y - 6.0f, W + 28.0f, H + 12.0f);
	DrawText(Objective, FLinearColor(1.0f, 0.85f, 0.35f, 1.0f), X, Y, nullptr, Scale);
}

void ASibeliusHUD::DrawWorldName()
{
	if (!Canvas)
	{
		return;
	}
	const ASibeliusGameCharacter* PlayerChar = Cast<ASibeliusGameCharacter>(GetOwningPawn());
	if (!PlayerChar || !PlayerChar->IsAwayFromOffice())
	{
		return;   // the office needs no nameplate
	}

	FString Map = GetWorld() ? GetWorld()->GetMapName() : FString();
	Map.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);   // strip the PIE prefix

	FString Name;
	if      (Map.Contains(TEXT("Forest_01")))    { Name = TEXT("THE FIRST FOREST"); }
	else if (Map.Contains(TEXT("Forest_03")))    { Name = TEXT("THE THIRD FOREST"); }
	else if (Map.Contains(TEXT("Forest_06")))    { Name = TEXT("THE SIXTH FOREST"); }
	else if (Map.Contains(TEXT("Forest_08")))    { Name = TEXT("THE EIGHTH FOREST"); }
	else if (Map.Contains(TEXT("Poplar")))       { Name = TEXT("THE POPLAR FOREST"); }
	else if (Map.Contains(TEXT("Cathedral")))    { Name = TEXT("THE CATHEDRAL"); }
	else if (Map.Contains(TEXT("AI_Temple")))    { Name = TEXT("THE AI TEMPLE"); }
	else                                          { return; }

	const float Scale = OverlayTextScale * 0.9f;
	float W = 0.0f, H = 0.0f;
	GetTextSize(Name, W, H, nullptr, Scale);
	// Under the objective banner, readable at 4K desk distance (Walt QA).
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 20.0f + H * 2.6f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), X - 10.0f, Y - 4.0f, W + 20.0f, H + 8.0f);
	DrawText(Name, FLinearColor(0.9f, 0.85f, 0.7f, 0.95f), X, Y, nullptr, Scale);
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
	Line(TEXT("  M menu    J how to play    H hide/show overlay    Q quit (press twice)"), White);
	Line(TEXT("  O back to office (in a wander world)"), White);
}
