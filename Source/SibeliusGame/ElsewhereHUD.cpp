// ElsewhereHUD.cpp — see header. Crosshair + the "[O] Back to Office" wander-world hint.

#include "ElsewhereHUD.h"
#include "Engine/Canvas.h"
#include "SibeliusGameCharacter.h"

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

	// "[O] Back to Office" — shown ONLY while standing in a registered wander world, read
	// from the same allowlist the O key checks (so the hint is visible exactly when the key
	// is live). The pawn owns the list (ASibeliusGameCharacter::WanderWorldLevels).
	if (const ASibeliusGameCharacter* PlayerChar = Cast<ASibeliusGameCharacter>(GetOwningPawn()))
	{
		if (PlayerChar->IsInWanderWorld())
		{
			const FString Hint = TEXT("[O] Back to Office");
			float HintW = 0.0f, HintH = 0.0f;
			GetTextSize(Hint, HintW, HintH, nullptr, 1.0f);
			const float HintX = (Canvas->ClipX - HintW) * 0.5f;
			const float HintY = Canvas->ClipY - HintH - 48.0f;   // a hand above the bottom edge
			DrawText(Hint, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), HintX, HintY, nullptr, 1.0f);
		}
	}
}
