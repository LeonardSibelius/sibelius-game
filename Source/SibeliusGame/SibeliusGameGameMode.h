// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SibeliusGameGameMode.generated.h"

/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class ASibeliusGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASibeliusGameGameMode();

	/* ARRIVE WHERE THE DOOR SAID, not always at the level's first PlayerStart.

	   ACarouselGameMode has had this since the carousel shipped; it was never on the main
	   game mode, so every door into the office, the temple or the city dropped the player
	   at the default spawn no matter which way he came in. That is invisible until a level
	   has two ways in — and L_City now has two, the [>] key from the meadow and the deli's
	   front door, which are a street apart.

	   A door that names no tag behaves exactly as before. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};



