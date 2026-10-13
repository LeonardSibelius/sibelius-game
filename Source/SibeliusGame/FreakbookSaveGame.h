// FreakbookSaveGame.h
//
// What Freakbook remembers: his visits, the conversations, the gossip, what he found and what he
// sent. ITS OWN SLOT, for the reason NpcChatSaveGame.h gives for its own: losing it costs nothing
// a player fought for, and every change to Freakbook's storage should not be a change to the file
// that holds his powers.
//
// ONE STRING. The state is written by FFreakbook::SaveText - plain lines, one fact each - and read
// back by LoadText, which the standalone tests cover byte for byte. Mirroring every field as a
// UPROPERTY would be a second definition of the same data to keep in step, and the save-archive
// path only adds the version check, which lives here.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FreakbookSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API UFreakbookSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// v1: the state as FFreakbook::SaveText ("FREAKBOOK 1" ... "end").
	static constexpr int32 CurrentSaveVersion = 1;

	/** Stamped on write; 0 is a blank or garbage object (UProgressionSaveGame's rule). */
	UPROPERTY(SaveGame)
	int32 SaveVersion = 0;

	UPROPERTY(SaveGame)
	FString State;
};
