// SibeliusReticle.cpp - see header.

#include "SibeliusReticle.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"

void SibeliusReticle::Draw(AHUD& Hud, const UCanvas& Canvas, const FStyle& Style)
{
	// Resolution scale. Clamped so a tiny PIE window still gets a usable reticle and a
	// very tall display does not get a silly one.
	const float Scale = FMath::Clamp(Canvas.ClipY / ReferenceHeight, 0.75f, 3.0f);

	// Snap to whole pixels - a half-pixel rect draws as a soft grey smear, which is the
	// opposite of "obvious".
	auto Px = [Scale](float V) { return FMath::RoundToFloat(V * Scale); };

	const float Arm       = FMath::Max(1.0f, Px(Style.ArmLength));
	const float Thick     = FMath::Max(1.0f, Px(Style.Thickness));
	const float Gap       = Px(Style.CenterGap);
	const float Dot       = Px(Style.DotRadius);
	const float Outline   = FMath::Max(1.0f, Px(Style.Outline));
	const float HalfThick = FMath::RoundToFloat(Thick * 0.5f);

	const float CX = FMath::RoundToFloat(Canvas.ClipX * 0.5f);
	const float CY = FMath::RoundToFloat(Canvas.ClipY * 0.5f);

	// The four arms and (optionally) the centre dot, as plain rects.
	struct FBar { float X, Y, W, H; };
	TArray<FBar, TInlineAllocator<5>> Bars;

	Bars.Add({ CX - Gap - Arm,  CY - HalfThick, Arm,   Thick });   // left
	Bars.Add({ CX + Gap,        CY - HalfThick, Arm,   Thick });   // right
	Bars.Add({ CX - HalfThick,  CY - Gap - Arm, Thick, Arm   });   // up
	Bars.Add({ CX - HalfThick,  CY + Gap,       Thick, Arm   });   // down

	if (Dot > 0.0f)
	{
		Bars.Add({ CX - Dot, CY - Dot, Dot * 2.0f, Dot * 2.0f });  // centre dot
	}

	// Two passes: EVERY outline first, then every fill. Drawing outline-then-fill per bar
	// would let a neighbour's outline paint over an already-filled bar where the dot and
	// the arms nearly touch.
	for (const FBar& B : Bars)
	{
		Hud.DrawRect(Style.OutlineColor,
			B.X - Outline, B.Y - Outline,
			B.W + Outline * 2.0f, B.H + Outline * 2.0f);
	}

	for (const FBar& B : Bars)
	{
		Hud.DrawRect(Style.Color, B.X, B.Y, B.W, B.H);
	}
}
