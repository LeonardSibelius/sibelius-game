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
#include "SibeliusReticle.h"
#include "SibeliusHUD.generated.h"

/**
 * Toast colours by MEANING, not by hue, so the palette can be retuned in one place.
 *
 * `inline` because these are used from a dozen translation units and UE packs several
 * .cpp files into one unity blob — the lesson from SND_SR in ProceduralPcm.h.
 */
namespace SibeliusToast
{
	inline const FLinearColor Good (0.25f, 1.00f, 0.45f, 1.0f);   // sauce gained, chapter done
	inline const FLinearColor Prize(1.00f, 0.90f, 0.30f, 1.0f);   // a find worth noting
	inline const FLinearColor Warn (1.00f, 0.65f, 0.25f, 1.0f);   // refused, can't afford
	inline const FLinearColor Bad  (1.00f, 0.35f, 0.30f, 1.0f);   // blocked, failed, alarm
	inline const FLinearColor Info (0.80f, 0.86f, 0.92f, 1.0f);   // neutral statement
}

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

	/**
	 * Show one of Walt's messages to a former employer (docs/MEMOIR_VOICE.md).
	 *
	 * Public because it is no longer only the unlock ceremony's to fire: CODE VISION is
	 * unlocked in a fresh save's default mask, so UnlockPower never broadcasts for it and
	 * its message — SAIC, 1988, the FIRST of the eight — has never been shown to anybody.
	 * It now fires on the first USE of Vision instead, which is the moment the power
	 * becomes real to the player.
	 */
	void ShowMemoir(const FString& Text, float Seconds = 12.0f);

	/**
	 * BLANK THE WHOLE HUD while a close-up owns the screen (2026-08-25).
	 *
	 * Talk-E frames a dancer's face at 38 degrees of FOV, and the HUD then drew the
	 * crosshair through her eye, the objective across her forehead, and her own
	 * greeting over her mouth. A portrait that is half text is not a portrait. For
	 * the length of the shot the HUD says nothing at all — she speaks instead.
	 *
	 * A LEASE, not a flag (the AP2 lesson from the apparition: never two restore
	 * paths that can disagree). ReleaseCinematic ends it early; the lease ends it
	 * anyway if the release never comes because she was destroyed, the level was
	 * torn down, or PIE stopped mid-shot. A HUD that can get stuck blank is a far
	 * worse bug than one that comes back a second late.
	 */
	void HoldCinematic(float Seconds);
	void ReleaseCinematic();
	bool IsCinematicHeld() const;

	// SIB-39 dev overlay visibility. Static so the toggle key (character) and the
	// ScanForSite log gate (UBuildComponent) share one flag without a HUD lookup.
	// Default ON (Walt likes seeing it).
	static bool bOverlayVisible;

	/**
	 * World time until which the memoir line is on screen, or 0.
	 *
	 * AFateCarousel reads this to withdraw its orbiting cards while Walt's message to a
	 * former employer is being read — the cards are set dressing and the memoir is the
	 * one piece of writing in the game in his own voice, so the decoration gives way.
	 *
	 * Static for the same reason bOverlayVisible is: it spares an actor in the level a
	 * HUD lookup every tick, and there is only ever one local HUD.
	 */
	static double MemoirVisibleUntil;

	// Reticle look (tweakable on the HUD class / BP). Values are 1080p-reference pixels —
	// SibeliusReticle::Draw scales them by screen height, so these hold at any resolution.
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ArmLength = SibeliusReticle::DefaultArmLength;   // length of each crosshair arm

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float Thickness = SibeliusReticle::DefaultThickness;   // arm thickness

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CenterGap = SibeliusReticle::DefaultCenterGap;   // clear space around the exact aim point

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float DotRadius = SibeliusReticle::DefaultDotRadius;   // centre dot; set 0 for a plain "+"

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);

private:
	void DrawCrosshair();
	void DrawDevOverlay();

	// "[O] Back to Office" — shown in every away-from-office level
	// (ASibeliusGameCharacter::IsAwayFromOffice()). Lives in the main overlay so it works
	// under the normal GameMode, independent of AElsewhereGameMode/AElsewhereHUD.
	void DrawBackToOfficeHint();

	/**
	 * "[E] play the machine", shown when the player is near a slot cabinet.
	 *
	 * With the fate carousel gone from the apse the cabinet reads as a bare marble
	 * block. The IInteractable prompt would normally announce it, but prompts go through
	 * AddOnScreenDebugMessage, which is suppressed in Shipping — so without this every
	 * actual player walks past a plinth.
	 */
	void DrawMachineHint();

	/** How close (cm) the player must be for the machine hint to appear. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0"))
	float MachineHintRange = 600.0f;

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

	/* ---------------- SHIPPING-SAFE PLAYER MESSAGING ----------------
	   GEngine->AddOnScreenDebugMessage is COMPILED OUT of Shipping builds. Every world
	   interaction prompt and nearly every piece of feedback in the game went through it,
	   which means players have never seen any of them — the doors that explain why they
	   are locked, the sauce pickups, the chapter-end lines, "press N again to ERASE ALL
	   PROGRESS". It all worked perfectly in PIE and vanished in the shipped build, with
	   nothing in any log to say so.

	   These two channels draw on the HUD canvas, which survives packaging. */

	/**
	 * The world interaction prompt — "[E] play the machine". ONE line, expected to be
	 * re-posted every tick by UInteractorComponent while a target is focused; it fades on
	 * its own shortly after the posting stops, so nothing has to clear it.
	 */
	void ShowInteractPrompt(const FString& Text);

	/** A transient message. Up to MaxToasts stack; the oldest expires first. */
	void ShowToast(const FString& Text, float Seconds = 4.0f,
		const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));

	/**
	 * Post a toast to the local HUD from anywhere, without each call site repeating the
	 * lookup. Safe with a null or non-Sibelius HUD.
	 *
	 * Prefer this over AddOnScreenDebugMessage for ANYTHING a player is meant to read.
	 * Screen debug messages remain correct for developer output — they SHOULD disappear
	 * in a shipped build.
	 */
	static void Toast(const UObject* WorldContext, const FString& Text, float Seconds = 4.0f,
		const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));

private:
	void DrawInteractPrompt();
	void DrawToasts();

	FString InteractPromptText;
	double InteractPromptUntil = 0.0;

	struct FHudToast
	{
		FString Text;
		double Until = 0.0;
		FLinearColor Color = FLinearColor::White;
	};

	/** Three at once is plenty; beyond that they collide with the memoir band below. */
	static constexpr int32 MaxToasts = 3;

	TArray<FHudToast> Toasts;

public:

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

	/** World seconds until the HUD un-blanks itself, or 0. See HoldCinematic. */
	double CinematicUntil = 0.0;
};
