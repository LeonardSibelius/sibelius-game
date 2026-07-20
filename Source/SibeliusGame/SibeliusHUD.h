// SibeliusHUD.h
//
// Always-on center reticle (SIB-37) + a toggleable developer overlay (SIB-39) that
// reads live game state (inventory, nearby build site, branch state, progress).
// Set as the GameMode's HUDClass. Overlay toggled with V (default ON).
//
// FUN-7: a PLAYER-facing layer, independent of the dev overlay: the Sauce count
// (top-right, with a floating +N/-N delta) and a centered ceremony banner when
// a power is earned. The dev overlay stays for Walt; these draw for the player.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ProgressionTypes.h"
#include "SibeliusHUD.generated.h"

UCLASS()
class SIBELIUSGAME_API ASibeliusHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// FUN-7: subscribe to progression events (delta flash + unlock banner).
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// FUN-6: the finale altar borrows the ceremony banner for its stage cues.
	void ShowBanner(const FString& Text, float Seconds = 5.0f);

	// SIB-39 dev overlay visibility. Static so the toggle key (character) and the
	// ScanForSite log gate (UBuildComponent) share one flag without a HUD lookup.
	// Default ON (Walt likes seeing it).
	static bool bOverlayVisible;

	// Reticle look (tweakable on the HUD class / BP).
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ArmLength = 10.0f;   // length of each crosshair arm (px)

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float Thickness = 2.0f;    // arm thickness (px)

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CenterGap = 3.0f;    // gap at the very center (px) — keeps the exact aim point clear

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

private:
	void DrawCrosshair();
	void DrawDevOverlay();

	// "[O] Back to Office" — shown in every away-from-office level
	// (ASibeliusGameCharacter::IsAwayFromOffice()). Lives in the main overlay so it works
	// under the normal GameMode, independent of AElsewhereGameMode/AElsewhereHUD.
	void DrawBackToOfficeHint();

	// --- FUN-7: player-facing layer ---
	void DrawPlayerLayer();

	// APPEAL_PLAN point 2: ONE guided objective line (top-center), derived
	// entirely from existing state — no new save fields, no triggers.
	void DrawObjective();
	FString ComputeObjective() const;

	// APPEAL_PLAN extra: which world am I in (forest name etc., small, top-center).
	void DrawWorldName();

	// Walt (2026-07-19): Test-Drive lived only in debug messages — invisible in
	// Shipping builds and undiscoverable in any build. Branched: the BRANCH ×N
	// marker + exit keys. Depth 0: a hint when standing near a branchable
	// object with the power owned.
	void DrawBranchLayer();

	// THE PRESENCE's own speech channel (her greeting was stomped by the
	// cauldron's +100 ceremony in the shared banner slot — one channel, two
	// speakers). Subtitle register: lower-third, centered, her cyan on a
	// dark strip. She will speak more as her phases build out.
	void DrawPresenceLine();

public:
	void ShowPresenceLine(const FString& Text, float Seconds);

private:
	FString PresenceText;
	double PresenceUntil = 0.0;
	void HandleSauceChanged(int32 NewTotal, int32 Delta);
	void HandlePowerUnlocked(EPowerVerb Verb);

	FDelegateHandle SauceChangedHandle;
	FDelegateHandle PowerUnlockedHandle;

	int32 LastSauceDelta = 0;
	double SauceFlashUntil = 0.0;   // world seconds; +N/-N floats until then

	FString BannerText;
	double BannerUntil = 0.0;

	// MEMOIR_VOICE: Walt's message to a former employer, shown under the
	// ceremony banner when a power is claimed — the memoir surfacing at the
	// exact moment the player is rewarded. Slower fade than the banner.
	FString MemoirText;
	double MemoirUntil = 0.0;
};
