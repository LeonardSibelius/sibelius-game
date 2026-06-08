// SibeliusHUD.cpp — center reticle.

#include "SibeliusHUD.h"
#include "Engine/Canvas.h"

void ASibeliusHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float HalfThick = Thickness * 0.5f;

	// A "+" reticle with a small center gap, centered on the exact aim point.
	// Left + right arms (horizontal).
	DrawRect(Color, CX - CenterGap - ArmLength, CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX + CenterGap,             CY - HalfThick, ArmLength, Thickness);
	// Top + bottom arms (vertical).
	DrawRect(Color, CX - HalfThick, CY - CenterGap - ArmLength, Thickness, ArmLength);
	DrawRect(Color, CX - HalfThick, CY + CenterGap,             Thickness, ArmLength);
}
