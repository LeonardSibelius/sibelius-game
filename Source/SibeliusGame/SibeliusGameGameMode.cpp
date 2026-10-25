// Copyright Epic Games, Inc. All Rights Reserved.

#include "SibeliusGameGameMode.h"
#include "SibeliusHUD.h"
#include "TravelTransitionSubsystem.h"   // the arrival tag a door asked for

#include "EngineUtils.h"                 // TActorIterator
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "HatchLock.h"                   // the attic is never a trap (AtticFromIsla)
#include "TimerManager.h"

/* [O] FROM ISLA'S KEEP LANDS IN THE ATTIC, by her portal (2026-10-24; ASibeliusGameCharacter::
   ReturnToOffice, the PlayerStart placed by place_home_portal.py). Walt: "It is a pain in the
   neck to have to gather those books to get up the ladder". But the hatch re-locks whenever the
   office loads, unless he has Deployed it, and a locked hatch is a floor: he would stand in the
   attic with no way down. So arriving there, the hatch opens - after the deployed save has been
   laid over the level (UBranchPIEComponent, the tick after the pawn begins play), or the save
   would lock it again. A raw write, as the save's own: no key is spent, and none is needed. */
static const FName AtticFromIsla(TEXT("AtticFromIsla"));

static void OpenAtticHatchSoon(UWorld* World)
{
	FTimerHandle Handle;
	const TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld]()
	{
		if (UWorld* W = WeakWorld.Get())
		{
			for (TActorIterator<AHatchLock> It(W); It; ++It)
			{
				if (It->IsLocked())
				{
					It->RestoreBranchState(0);
					UE_LOG(LogTemp, Display, TEXT("[GameMode] arrived in the attic from Isla's keep - %s opened."), *It->GetName());
				}
			}
		}
	}), 0.5f, false);
}

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
						if (Tag == AtticFromIsla)
						{
							OpenAtticHatchSoon(GetWorld());
						}
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
