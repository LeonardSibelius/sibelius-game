// ElsewhereHUD.cpp — see header. Crosshair only. (The "[O] Back to Office" hint lives in
// the main ASibeliusHUD overlay so it shows under the normal GameMode, with no dependency
// on this HUD / AElsewhereGameMode being active.)

#include "ElsewhereHUD.h"
#include "Engine/Canvas.h"

void AElsewhereHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// Same reticle as the main game — see SibeliusReticle.h.
	SibeliusReticle::FStyle Style;
	Style.ArmLength = ArmLength;
	Style.Thickness = Thickness;
	Style.CenterGap = CenterGap;
	Style.DotRadius = DotRadius;
	Style.Color     = Color;

	SibeliusReticle::Draw(*this, *Canvas, Style);
}
