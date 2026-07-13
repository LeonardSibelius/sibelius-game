// ProgressionSaveGame.h
//
// FUN-1 — the persisted form of FProgressionState. Its OWN slot ("Progression"),
// deliberately separate from the Ch5 deploy save (USibeliusSaveGame): the deploy
// save is per-level declared-state deltas the player can clear at will (key 9);
// earned powers and Sauce must survive that. UProgressionSubsystem is the only
// runtime writer.
//
// NOTE: save-archive serialization only carries CPF_SaveGame properties, so
// every persisted field here (and inside FProgressionState) is UPROPERTY(SaveGame).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProgressionTypes.h"
#include "ProgressionSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API UProgressionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Bump whenever the persisted shape changes, and migrate on load.
	// v1: FProgressionState (mask + sauce + claimed grants).
	static constexpr int32 CurrentSaveVersion = 1;

	// Stamped on write; a freshly constructed (unwritten) object reads 0, which
	// is how a garbage/blank load is told apart from a real save.
	UPROPERTY(SaveGame)
	int32 SaveVersion = 0;

	UPROPERTY(SaveGame)
	FProgressionState State;
};
