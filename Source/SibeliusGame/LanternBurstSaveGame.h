// LanternBurstSaveGame.h
//
// What the lantern remembers between visits: his best score and the furthest stage he reached.
// ITS OWN SLOT, for the reason FreakbookSaveGame.h gives: losing it costs nothing a player fought
// for in the story, and a change to it should never be a change to the file that holds his powers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LanternBurstSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API ULanternBurstSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 1;

	/** Stamped on write; 0 is a blank or garbage object (UProgressionSaveGame's rule). */
	UPROPERTY(SaveGame)
	int32 SaveVersion = 0;

	UPROPERTY(SaveGame)
	int64 BestScore = 0;

	UPROPERTY(SaveGame)
	int32 BestStage = 0;
};
