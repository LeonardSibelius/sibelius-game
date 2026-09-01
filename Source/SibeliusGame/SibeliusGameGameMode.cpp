// Copyright Epic Games, Inc. All Rights Reserved.

#include "SibeliusGameGameMode.h"
#include "SibeliusHUD.h"
#include "TravelTransitionSubsystem.h"   // the arrival tag a door asked for

#include "EngineUtils.h"                 // TActorIterator
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"

ASibeliusGameGameMode::ASibeliusGameGameMode()
{
	// Always-on center reticle (a BP GameMode derived from this inherits it unless it
	// overrides HUDClass).
	HUDClass = ASibeliusHUD::StaticClass();
}

AActor* ASibeliusGameGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTravelTransitionSubsystem* Travel = GI->GetSubsystem<UTravelTransitionSubsystem>())
		{
			/* CONSUME, not peek. The tag belongs to ONE journey: reading it without
			   clearing it would strand it in the subsystem and send the player to the
			   deli door again the next time he entered the city by any other route. */
			const FName Tag = Travel->ConsumeArrivalTag();
			if (!Tag.IsNone())
			{
				for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
				{
					if (It->PlayerStartTag == Tag)
					{
						return *It;
					}
				}

				/* A tag nobody wears falls through to the default start rather than
				   failing. A door that names a spot this level does not have should put
				   the player somewhere sane and let the log say so - the alternative is
				   a black screen for a typo. */
				UE_LOG(LogTemp, Warning,
					TEXT("[GameMode] arrival tag '%s' matches no PlayerStart in this level "
					     "- using the default spawn."), *Tag.ToString());
			}
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
