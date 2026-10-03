// SibeliusAim.h
//
// WHERE A POWER'S RAY STARTS. Interact and Refactor both aim along the camera, and the game
// was built in first person, where the camera IS his eyes. In third person - Greystone, on
// Kaia's planet (Walt, 2026-10-03: "it will be good to have the third-person deal where our
// Epic Games character is viewed from behind") - the camera hangs three and a half metres
// behind him, so a ray from the camera starts in mid-air at his back: E stops reaching
// things at arm's length, and R rolls the menagerie on whatever is over his shoulder.
// BattleFormComponent.h describes exactly that, and why battle form switches the powers off.
//
// The fix is the standard one: keep aiming along the camera, but START the ray level with
// his eyes - the point on the camera's line nearest to them. In first person that point is
// the camera, so nothing there changes; the threshold makes sure of it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

namespace SibeliusAim
{
	/** Less than this between the camera and his eyes, along the aim, is first person. */
	constexpr float ThirdPersonThresholdCm = 30.0f;

	inline FVector RayStart(const APawn* Pawn, const FVector& CameraLocation, const FVector& CameraForward)
	{
		if (!Pawn)
		{
			return CameraLocation;
		}
		const float Ahead = FVector::DotProduct(Pawn->GetPawnViewLocation() - CameraLocation, CameraForward);
		return Ahead > ThirdPersonThresholdCm ? CameraLocation + CameraForward * Ahead : CameraLocation;
	}
}
