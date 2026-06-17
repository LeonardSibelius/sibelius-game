// ElsewhereHUD.h
//
// THE SAUCE DOOR — a clean HUD for the Elsewhere (SIB-47). Just a center reticle so
// the player can aim at the curio / return door; deliberately NONE of the main game's
// developer overlay (INVENTORY/PROGRESS/GENERATE/CONTROLS, which ASibeliusHUD draws
// from a static default-ON flag). The interaction prompt is drawn by
// UInteractorComponent (on-screen message), independent of the HUD, so E-prompts
// still show. Set as AElsewhereGameMode's HUDClass.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ElsewhereHUD.generated.h"

UCLASS()
class SIBELIUSGAME_API AElsewhereHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair") float ArmLength = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair") float Thickness = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair") float CenterGap = 3.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair") FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);
};
