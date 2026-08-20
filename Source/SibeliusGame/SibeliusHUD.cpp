// SibeliusHUD.cpp — center reticle + developer overlay (SIB-39).

#include "SibeliusHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"                   // GetSmallFont
#include "EngineUtils.h"                      // TActorIterator
#include "SlotCabinet.h"                      // the machine hint finds the nearest cabinet
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
#include "BranchSubsystem.h"                  // Test-Drive marker (Shipping-safe HUD draw)
#include "BranchPIEComponent.h"               // Test-Drive near-branchable hint
#include "GameFramework/Character.h"

// FUN-8: default OFF now that the player has real surfaces (Tab menu, sauce
// counter, banners). H brings it back — it's Walt's debug view, not the UI.
bool ASibeliusHUD::bOverlayVisible = false;
double ASibeliusHUD::MemoirVisibleUntil = 0.0;

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

namespace
{
	// MEMOIR_VOICE (docs/MEMOIR_VOICE.md): Walt's messages to former employers,
	// in his own template — one per power, forty years in six lines. The two
	// machine placards (Bally, San Diego County) live in the levels as signs.
	const TCHAR* MemoirLineForVerb(EPowerVerb Verb)
	{
		switch (Verb)
		{
		case EPowerVerb::CodeVision:
			return TEXT("Hey SAIC, you could have used this AI skill on the CHCS project in 1988.");
		case EPowerVerb::Refactor:
			return TEXT("Hey Seagate, you could have used this AI skill in 1998.");
		case EPowerVerb::Compile:
			return TEXT("Hey IBM, you could have used this AI skill in 1995 on the San Francisco Project for distributed Java.");
		case EPowerVerb::TestDrive:
			return TEXT("Hey Motorola, you could have used this AI skill in 2001.");
		case EPowerVerb::Deploy:
			return TEXT("Hey Northrop Grumman, you could have used this AI skill on the Electronic Family Housing system for the Navy in 2002.");
		case EPowerVerb::Generate:
			return TEXT("Hey Army Recruiting, you could have used this AI skill on iKrome in 2022. It is being retired now, like all the rest.");
		default:
			return TEXT("");
		}
	}
}

void ASibeliusHUD::HandlePowerUnlocked(EPowerVerb Verb)
{
	ShowBanner(FString::Printf(TEXT("%s  IS  YOURS"), *PowerVerbDisplayName(Verb)));

	// The memoir line lingers past the banner — the player should have time
	// to read forty years' worth of one sentence.
	MemoirText = MemoirLineForVerb(Verb);
	MemoirUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 12.0 : 0.0;

	// Tell the altar's orbiting cards to stand aside for the next twelve seconds.
	MemoirVisibleUntil = MemoirUntil;
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
	DrawMachineHint();        // "[E] play the machine" — the plinth needs to say it does something
	DrawPlayerLayer();        // FUN-7: sauce count + ceremony banner, always on
	DrawObjective();          // APPEAL-2: the one guided goal, top-center
	DrawBranchLayer();        // Test-Drive marker + discoverability hint (Shipping-safe)
	DrawPresenceLine();       // The Presence's subtitle channel
	DrawWorldName();          // APPEAL extra: which world am I in
	DrawInteractPrompt();     // "[E] ..." — was a screen debug message, invisible in Shipping
	DrawToasts();             // pickups, refusals, chapter ends — likewise

	if (bOverlayVisible)
	{
		DrawDevOverlay();
	}
}

void ASibeliusHUD::ShowPresenceLine(const FString& Text, float Seconds)
{
	PresenceText = Text;
	PresenceUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.0;
}

/* ================= SHIPPING-SAFE PLAYER MESSAGING =================
   See the header for why this exists. Short version: every prompt and pickup line in the
   game went through AddOnScreenDebugMessage, which Shipping compiles out — so no player
   has ever seen one, and nothing logged the fact. */

void ASibeliusHUD::ShowInteractPrompt(const FString& Text)
{
	const UWorld* World = GetWorld();
	InteractPromptText  = Text;

	// A short lease, re-taken every tick while the interactor holds a target. Letting it
	// lapse on its own means no caller ever has to remember to clear it — the same
	// self-expiring contract the old 0.15s debug message had, which is why the interactor
	// needed no "stop showing" path and still does not.
	InteractPromptUntil = World ? World->GetTimeSeconds() + 0.25 : 0.0;
}

void ASibeliusHUD::ShowToast(const FString& Text, float Seconds, const FLinearColor& InColor)
{
	if (Text.IsEmpty())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// Re-posting the same line refreshes it in place rather than stacking a duplicate.
	// Several call sites fire on a repeatable action (bumping a locked door), and three
	// identical lines stacked up would read as a bug.
	for (FHudToast& T : Toasts)
	{
		if (T.Text == Text)
		{
			T.Until = Now + Seconds;
			T.Color = InColor;
			return;
		}
	}

	if (Toasts.Num() >= MaxToasts)
	{
		Toasts.RemoveAt(0);   // oldest goes; the newest message is the one being read
	}
	Toasts.Add({ Text, Now + Seconds, InColor });
}

void ASibeliusHUD::Toast(const UObject* WorldContext, const FString& Text, float Seconds,
	const FLinearColor& InColor)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

	if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->ShowToast(Text, Seconds, InColor);
		return;
	}

	// No Sibelius HUD — the Elsewhere runs its own HUD class, and commandlets have none
	// at all. Fall back rather than swallow, so a message is never silently lost in a
	// development build the way it used to be in a shipped one.
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Seconds, InColor.ToFColor(true), Text);
	}
#endif
}

void ASibeliusHUD::DrawInteractPrompt()
{
	if (!Canvas || InteractPromptText.IsEmpty()) { return; }
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now >= InteractPromptUntil) { return; }

	const float Scale = OverlayTextScale * 1.6f;
	float W = 0.f, H = 0.f;
	GetTextSize(InteractPromptText, W, H, nullptr, Scale);

	// Just under the reticle and ABOVE the machine hint's band (0.66), so when both are
	// up near the cabinet they read as two lines rather than one overlapping smear.
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.60f;

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), X - 20.f, Y - 9.f, W + 40.f, H + 18.f);
	DrawText(InteractPromptText, FLinearColor(1.0f, 1.0f, 1.0f, 0.97f), X, Y, nullptr, Scale);
}

void ASibeliusHUD::DrawToasts()
{
	if (!Canvas || Toasts.Num() == 0) { return; }

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Toasts.RemoveAll([Now](const FHudToast& T) { return Now >= T.Until; });

	const float Scale = OverlayTextScale * 1.5f;
	float Y = Canvas->ClipY * 0.76f;   // between the machine hint (0.66) and the memoir (0.88)

	for (const FHudToast& T : Toasts)
	{
		const float Alpha = static_cast<float>(FMath::Clamp(T.Until - Now, 0.0, 1.0));
		float W = 0.f, H = 0.f;
		GetTextSize(T.Text, W, H, nullptr, Scale);

		const float X = (Canvas->ClipX - W) * 0.5f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.70f * Alpha), X - 20.f, Y - 8.f, W + 40.f, H + 16.f);
		DrawText(T.Text, FLinearColor(T.Color.R, T.Color.G, T.Color.B, Alpha), X, Y, nullptr, Scale);

		Y += H + 22.0f;
	}
}

void ASibeliusHUD::DrawPresenceLine()
{
	if (!Canvas || PresenceText.IsEmpty()) { return; }
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now >= PresenceUntil) { return; }

	// Subtitle register: lower-third, her cyan, on a soft dark strip. Fades
	// in the final second.
	const float Alpha = static_cast<float>(FMath::Clamp(PresenceUntil - Now, 0.0, 1.0));
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Scale = 4.8f;   // Walt, twice: "tiny", then "at least twice as large" — she speaks LARGE
	float W = 0.f, H = 0.f;
	GetTextSize(PresenceText, W, H, Font, Scale);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.55f * Alpha),
		X - 24.f, Y - 10.f, W + 48.f, H + 20.f);
	DrawText(PresenceText, FLinearColor(0.65f, 0.90f, 1.0f, Alpha), X, Y, Font, Scale);
}

void ASibeliusHUD::DrawBranchLayer()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas) { return; }
	UBranchSubsystem* Branch = World->GetSubsystem<UBranchSubsystem>();
	if (!Branch) { return; }

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Branched: the marker + exit keys, front and center under the objective.
	// (Was an AddOnScreenDebugMessage — compiled out of Shipping builds.)
	const int32 Depth = Branch->GetDepth();
	if (Depth >= 1)
	{
		const FString Marker = FString::Printf(
			TEXT("BRANCH x%d — a test reality      [7] keep it      [8] undo it"), Depth);
		float W = 0.f, H = 0.f;
		GetTextSize(Marker, W, H, Font, 1.6f);
		const float MarkerX = (Canvas->ClipX - W) * 0.5f;
		const float MarkerY = Canvas->ClipY * 0.12f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), MarkerX - 14.0f, MarkerY - 8.0f, W + 28.0f, H + 16.0f);
		DrawText(Marker, FLinearColor(0.75f, 0.63f, 1.0f, 0.95f), MarkerX, MarkerY, Font, 1.6f);
		return;
	}

	// Depth 0: the discoverability hint — near a branchable, power owned.
	APawn* Pawn = GetOwningPawn();
	const UBranchPIEComponent* BranchPIE = Pawn ? Pawn->FindComponentByClass<UBranchPIEComponent>() : nullptr;
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (BranchPIE && BranchPIE->IsNearBranchable()
		&& Progression && Progression->IsUnlocked(EPowerVerb::TestDrive))
	{
		const FString Hint =
			TEXT("TEST-DRIVE: press [6] to branch reality — experiment freely; [7] keeps it, [8] undoes it");
		float W = 0.f, H = 0.f;
		GetTextSize(Hint, W, H, Font, 1.1f);
		const float HintX = (Canvas->ClipX - W) * 0.5f;
		const float HintY = Canvas->ClipY * 0.84f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), HintX - 12.0f, HintY - 6.0f, W + 24.0f, H + 12.0f);
		DrawText(Hint, FLinearColor(0.75f, 0.63f, 1.0f, 0.95f), HintX, HintY, Font, 1.1f);
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
	// dev overlay owns that corner. Dark chip so the green doesn't vanish
	// into wood, stone, or the living-room wall.
	constexpr double HintVisibleSeconds = 40.0;
	constexpr double HintFadeSeconds = 5.0;
	constexpr float ChipPadX = 10.0f;
	constexpr float ChipPadY = 6.0f;
	constexpr FLinearColor ChipBack(0.0f, 0.0f, 0.0f, 0.72f);
	if (!bOverlayVisible && Now < HintVisibleSeconds + HintFadeSeconds)
	{
		const FString LaunchHint = TEXT("M for Status, J for Journal");
		const float HintAlpha = static_cast<float>(
			FMath::Clamp((HintVisibleSeconds + HintFadeSeconds - Now) / HintFadeSeconds, 0.0, 1.0));
		float LaunchW = 0.0f, LaunchH = 0.0f;
		GetTextSize(LaunchHint, LaunchW, LaunchH, nullptr, OverlayTextScale);
		const float LaunchX = 16.0f;
		const float LaunchY = 24.0f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f * HintAlpha),
			LaunchX - ChipPadX, LaunchY - ChipPadY, LaunchW + ChipPadX * 2.0f, LaunchH + ChipPadY * 2.0f);
		DrawText(LaunchHint,
			FLinearColor(0.4f, 1.0f, 0.5f, 0.95f * HintAlpha), LaunchX, LaunchY, nullptr, OverlayTextScale);
	}

	// Sauce count, top-right — the one number the player always sees.
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		const FString SauceLine = FString::Printf(TEXT("SAUCE  %d"), Progression->GetSauce());
		float W = 0.0f, H = 0.0f;
		GetTextSize(SauceLine, W, H, nullptr, OverlayTextScale);

		// Walt's ask: a standing reminder of the menu key, tucked under the count.
		// (M, not Tab — Slate eats Tab in PIE; see the character's key bindings.)
		const FString MenuHint = TEXT("[M] status");
		float HintW = 0.0f, HintH = 0.0f;
		const float HintScale = OverlayTextScale * 0.7f;
		GetTextSize(MenuHint, HintW, HintH, nullptr, HintScale);

		const float SauceY = 24.0f;
		const float LineGap = 4.0f;
		const float MenuY = SauceY + H + LineGap;
		const float BlockRight = Canvas->ClipX - 24.0f;
		const float BlockW = FMath::Max(W, HintW);
		const float BlockH = H + LineGap + HintH;
		const float BlockX = BlockRight - BlockW;
		DrawRect(ChipBack,
			BlockX - ChipPadX, SauceY - ChipPadY, BlockW + ChipPadX * 2.0f, BlockH + ChipPadY * 2.0f);

		const float SauceX = BlockRight - W;
		DrawText(SauceLine, FLinearColor(0.4f, 1.0f, 0.5f, 0.95f), SauceX, SauceY, nullptr, OverlayTextScale);
		DrawText(MenuHint, FLinearColor(0.85f, 0.85f, 0.85f, 0.9f),
			BlockRight - HintW, MenuY, nullptr, HintScale);

		// The +N/-N delta floats under the hint, then fades.
		if (Now < SauceFlashUntil && LastSauceDelta != 0)
		{
			const float Alpha = static_cast<float>(FMath::Clamp((SauceFlashUntil - Now) / 2.5, 0.0, 1.0));
			const FString DeltaLine = FString::Printf(TEXT("%+d"), LastSauceDelta);
			const FLinearColor DeltaColor = LastSauceDelta > 0
				? FLinearColor(0.4f, 1.0f, 0.5f, Alpha)
				: FLinearColor(1.0f, 0.55f, 0.3f, Alpha);
			float DeltaW = 0.0f, DeltaH = 0.0f;
			GetTextSize(DeltaLine, DeltaW, DeltaH, nullptr, OverlayTextScale);
			const float DeltaY = MenuY + HintH + 8.0f;
			const float DeltaX = BlockRight - DeltaW;
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f * Alpha),
				DeltaX - ChipPadX, DeltaY - ChipPadY, DeltaW + ChipPadX * 2.0f, DeltaH + ChipPadY * 2.0f);
			DrawText(DeltaLine, DeltaColor, DeltaX, DeltaY, nullptr, OverlayTextScale);
		}
	}

	// The ceremony banner — centered, above the reticle.
	if (Now < BannerUntil && !BannerText.IsEmpty())
	{
		const float Scale = OverlayTextScale * 1.6f;
		float W = 0.0f, H = 0.0f;
		GetTextSize(BannerText, W, H, nullptr, Scale);
		const float Alpha = static_cast<float>(FMath::Clamp((BannerUntil - Now) / 0.75, 0.0, 1.0)); // quick fade at the end
		const float X = (Canvas->ClipX - W) * 0.5f;
		const float Y = Canvas->ClipY * 0.32f;
		// Backing strip, same reason as the memoir below: these fire at the altar and
		// at the slot machine, where the fate-glyph ring is turning bright gold behind
		// them. Text alone loses that fight.
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.80f * Alpha), X - 18.0f, Y - 8.0f, W + 36.0f, H + 16.0f);
		DrawText(BannerText, FLinearColor(0.55f, 0.95f, 1.0f, Alpha), X, Y, nullptr, Scale);
	}

	// The memoir line — Walt's message to a former employer, under the banner,
	// warm white on a dark backing strip so it reads at 4K desk distance.
	if (Now < MemoirUntil && !MemoirText.IsEmpty())
	{
		const float Scale = OverlayTextScale * 0.95f;
		float W = 0.0f, H = 0.0f;
		GetTextSize(MemoirText, W, H, nullptr, Scale);
		const float Alpha = static_cast<float>(FMath::Clamp((MemoirUntil - Now) / 1.5, 0.0, 1.0));
		const float X = (Canvas->ClipX - W) * 0.5f;
		const float Y = Canvas->ClipY * 0.32f + 90.0f;
		// 0.55 was enough over stone and hopeless over the fate-glyph ring, whose gold
		// symbols turn right behind this line at the machine. Walt: "my message to Bally
		// is being stepped on by the rotating symbols." Raised to 0.88 with more padding
		// — this is the one piece of writing in the game that is his own voice, and it
		// has to be readable wherever it fires.
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f * Alpha), X - 22.0f, Y - 10.0f, W + 44.0f, H + 20.0f);
		DrawText(MemoirText, FLinearColor(0.98f, 0.95f, 0.86f, Alpha), X, Y, nullptr, Scale);
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

	// 0) A refuser wave overrides everything — teach the fight under pressure.
	if (const UHallAlarmSubsystem* Alarm = GI->GetSubsystem<UHallAlarmSubsystem>())
	{
		if (Alarm->IsAlarmTriggered())
		{
			for (TActorIterator<ACharacter> It(const_cast<UWorld*>(W)); It; ++It)
			{
				if (Cast<ARefuserController>(It->GetController()))
				{
					return TEXT("Mrs. Hall's Refusers are loose — get close and FIGHT them [F]");
				}
			}
		}
	}

	const int32 Keys  = Inv ? Inv->GetCount(EResourceType::Key) : 0;
	const int32 Powers = Progression->NumUnlocked();
	const int32 PowerCount = static_cast<int32>(EPowerVerb::Count);

	// 1) ALL POWERS outranks the early beats (Walt QA: a veteran whose keys
	// were long spent got told to build a key while standing at the finale).
	if (Powers >= PowerCount)
	{
		// Walt QA #2: gate on the DURABLE save claim, not the session flag —
		// the session flag sent a finished player to re-perform the rite at a
		// rightly-silent altar (and to hurdle the altar block).
		if (!Progression->HasClaimedGrant(TEXT("Finale.Synthesis")))
		{
			// Location-aware: in the cathedral itself, say exactly what to do
			// with the wall in your face.
			FString Map = W->GetMapName();
			Map.RemoveFromStart(W->StreamingLevelsPrefix);
			if (Map.Contains(TEXT("Cathedral")))
			{
				return TEXT("Stand at the ALTAR (the low block before the wall) and USE your six powers in turn — the wall falls when the rite completes");
			}
			return TEXT("All six powers are yours — the cathedral altar awaits the Synthesis");
		}
		return FString();   // Synthesis done: free play, no nagging
	}

	// 2) Opening: they start with Code Vision. Teach it on the glass behind them
	// (the poker door). Do NOT send a new player hunting COMPILE.
	if (!Progression->HasClaimedGrant(TEXT("Tutorial.Vision")))
	{
		return TEXT("Vision [V] shows hidden doors. Turn around and look at the glass.");
	}

	// 3) Poker through the glass. A living-room table book must not steal this
	// line — they pick that up for E + sauce, then still go play.
	if (!Progression->HasClaimedGrant(TEXT("Tutorial.Poker")) && Keys == 0)
	{
		return TEXT("Vision [V] shows hidden doors. Poker is through the glass behind you.");
	}

	// 4) After poker: a shrine, if they don't have Compile. Not an upstairs invite.
	if (Keys == 0 && !Progression->IsUnlocked(EPowerVerb::Compile))
	{
		return TEXT("Walk into a glowing shrine when you find one");
	}

	// 5) Compile earned, no key: they find the build site upstairs themselves.
	if (Keys == 0)
	{
		return FString();
	}

	// 6) Key in hand, powers remain: the wider house opens.
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
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), X - 14.0f, Y - 6.0f, W + 28.0f, H + 12.0f);
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
	if      (Map.Contains(TEXT("Cathedral")))    { Name = TEXT("THE CATHEDRAL"); }
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
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), HintX - 10.0f, HintY - 6.0f, HintW + 20.0f, HintH + 12.0f);
	DrawText(Hint, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), HintX, HintY, nullptr, OverlayTextScale);
}

void ASibeliusHUD::DrawMachineHint()
{
	if (!Canvas)
	{
		return;
	}

	const APawn* PlayerPawn = GetOwningPawn();
	UWorld* World = GetWorld();
	if (!PlayerPawn || !World)
	{
		return;
	}

	// Nearest cabinet within reach. The cathedral holds one machine, so this is a
	// handful of comparisons — not worth caching, and a cache would go stale when the
	// finale rearranges the apse.
	const FVector Here = PlayerPawn->GetActorLocation();
	const float RangeSq = MachineHintRange * MachineHintRange;

	const ASlotCabinet* Nearest = nullptr;
	float BestSq = RangeSq;
	for (TActorIterator<ASlotCabinet> It(World); It; ++It)
	{
		const ASlotCabinet* Cab = *It;
		if (!Cab || Cab->IsScreenOpen())
		{
			continue;   // already playing — the screen has its own controls line
		}
		const float DSq = FVector::DistSquared(Here, Cab->GetActorLocation());
		if (DSq < BestSq)
		{
			BestSq = DSq;
			Nearest = Cab;
		}
	}

	if (!Nearest)
	{
		return;
	}

	// WHY THIS EXISTS. With the fate carousel removed from the apse the cabinet reads as
	// a bare marble block, and a player has no reason to think it does anything. The
	// IInteractable prompt would normally say so — but prompts go through
	// AddOnScreenDebugMessage, which is SUPPRESSED in Shipping, so every actual player
	// would walk past a plinth. Drawn on the HUD canvas it survives packaging.
	const FString Hint = TEXT("[E]  PLAY THE MACHINE");
	const float Scale = OverlayTextScale * 2.1f;
	float W = 0.0f, H = 0.0f;
	GetTextSize(Hint, W, H, nullptr, Scale);

	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.66f;   // below the reticle, clear of the memoir band

	// FULL brightness at any distance inside the range.
	//
	// This first faded with distance, which had it exactly backwards: the hint was
	// faintest when the player was furthest away, and that is precisely when they have
	// not yet worked out the plinth does anything. Its whole job is to be noticed from
	// across the apse. Once you are close enough to read the cabinet, you no longer need
	// telling.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f), X - 26.0f, Y - 12.0f, W + 52.0f, H + 24.0f);
	DrawText(Hint, FLinearColor(1.0f, 0.86f, 0.25f, 1.0f), X, Y, nullptr, Scale);
}

void ASibeliusHUD::DrawCrosshair()
{
	if (!Canvas)
	{
		return;
	}

	// One reticle for the whole game — see SibeliusReticle.h. Sizes below are
	// 1080p-reference px; Draw() scales them by resolution and adds the outline.
	SibeliusReticle::FStyle Style;
	Style.ArmLength = ArmLength;
	Style.Thickness = Thickness;
	Style.CenterGap = CenterGap;
	Style.DotRadius = DotRadius;
	Style.Color     = Color;

	SibeliusReticle::Draw(*this, *Canvas, Style);
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
	Line(TEXT("  F fight   E interact    V vision"), White);
	Line(TEXT("  R refactor    C compile    G generate / ask"), White);
	Line(TEXT("  6 enter  7 merge  8 discard  9 clear-deploy(dev)  0 deploy"), White);
	Line(TEXT("  M menu    J how to play    H hide/show overlay    Q quit (press twice)"), White);
	Line(TEXT("  O back to office (in a wander world)"), White);
}
