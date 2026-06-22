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

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float HalfThick = Thickness * 0.5f;

	// A "+" reticle with a small center gap (same look as the main game's, minus the
	// developer overlay).
	DrawRect(Color, CX - CenterGap - ArmLength, CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX + CenterGap,             CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX - HalfThick, CY - CenterGap - ArmLength, Thickness, ArmLength);
	DrawRect(Color, CX - HalfThick, CY + CenterGap,             Thickness, ArmLength);
}
