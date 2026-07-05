// SibeliusHUD.h
//
// Always-on center reticle (SIB-37) + a toggleable developer overlay (SIB-39) that
// reads live game state (inventory, nearby build site, branch state, progress).
// Set as the GameMode's HUDClass. Overlay toggled with V (default ON).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SibeliusHUD.generated.h"

UCLASS()
class SIBELIUSGAME_API ASibeliusHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

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
};
