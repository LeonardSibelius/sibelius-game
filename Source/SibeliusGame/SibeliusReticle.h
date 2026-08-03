// SibeliusReticle.h
//
// THE one reticle. All three HUDs (ASibeliusHUD, AElsewhereHUD, ACarouselHUD) draw
// through Draw() below, so the crosshair looks identical in the office, the Elsewhere
// and the Carousel room.
//
// Walt (2026-08-03): "make the reticule bigger and much more obvious - it is a tiny
// little plus sign now." Three things were wrong with the old one:
//
//   1. It was small - 26 px across, total.
//   2. The size was in RAW PIXELS, so it did not grow with resolution. On Walt's 4K
//      desktop that 26 px covered 0.7% of the screen width; the same numbers that
//      look thin at 1080p look like a speck at 2160p. Draw() now scales off
//      Canvas.ClipY against a 1080p reference, so it reads the same at any res.
//   3. It was solid white with NO outline, so it vanished against anything bright -
//      lit concrete, the cathedral glass, a white wall. Every arm is now drawn twice:
//      a black bar first, the white bar on top. That is the standard shooter trick and
//      it is most of the "much more obvious".
//
// Tune the look HERE and all three HUDs follow. ASibeliusHUD/AElsewhereHUD still expose
// their own EditDefaultsOnly properties for editor tweaking, but those initialise from
// these constants - one source of truth for the numbers.

#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;

namespace SibeliusReticle
{
	// Sizes are authored at this screen height and scaled from it (see Draw).
	constexpr float ReferenceHeight = 1080.0f;

	constexpr float DefaultArmLength = 22.0f;   // was 10 - each arm, in 1080p px
	constexpr float DefaultThickness = 4.0f;    // was 2
	constexpr float DefaultCenterGap = 8.0f;    // was 3 - clear space around the aim point
	constexpr float DefaultDotRadius = 2.5f;    // 0 disables the centre dot
	constexpr float DefaultOutline   = 2.0f;    // black border thickness, 1080p px

	/** Look of the crosshair. Defaults match the constants above. */
	struct FStyle
	{
		float ArmLength = DefaultArmLength;
		float Thickness = DefaultThickness;
		float CenterGap = DefaultCenterGap;
		float DotRadius = DefaultDotRadius;
		float Outline   = DefaultOutline;

		FLinearColor Color        = FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);
		FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);
	};

	/**
	 * Draw the centre reticle. Sizes in Style are 1080p-reference pixels and are scaled
	 * by Canvas height, so callers pass the same numbers regardless of resolution.
	 */
	void Draw(AHUD& Hud, const UCanvas& Canvas, const FStyle& Style = FStyle());
}
