// ElsewhereSaveGame.h
//
// THE SAUCE DOOR — the persisted payload (SIB-47). This class IS the discard rule
// (§3/§4) made structural: it carries the collected curios + score and NOTHING about
// the generated room. There is deliberately no place-type, no seed, no geometry here
// — a room is reproducible from its seed if ever needed, so saving it would be waste.
// If you're ever tempted to add an FElsewherePlan field here, re-read §4 first.
//
// Written/read through FSibeliusSaveIO (the headless-safe chokepoint), so the smoke
// gate round-trips it without a GameInstance — same pattern as USibeliusSaveGame.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ElsewhereTypes.h"
#include "ElsewhereSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API UElsewhereSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Bump on any persisted-shape change (mirrors USibeliusSaveGame's discipline).
	static constexpr int32 CurrentSaveVersion = 1;

	UPROPERTY(SaveGame) int32 SaveVersion = 0;

	// The ENTIRE persisted state: the Cabinet's owned set + score + lifetime count.
	// (FCurioCollection is a reflected USTRUCT; its SaveGame fields serialise here.)
	UPROPERTY(SaveGame) FCurioCollection Collection;
};
