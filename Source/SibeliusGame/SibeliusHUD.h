// SibeliusHUD.h
//
// SIB-37 follow-up: a minimal always-on center reticle so the interaction/build
// trace's aim point is visible. Set as the GameMode's HUDClass.

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

	// Reticle look (tweakable on the HUD class / BP).
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ArmLength = 10.0f;   // length of each crosshair arm (px)

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float Thickness = 2.0f;    // arm thickness (px)

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CenterGap = 3.0f;    // gap at the very center (px) — keeps the exact aim point clear

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);
};
