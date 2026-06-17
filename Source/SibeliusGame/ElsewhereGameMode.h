// ElsewhereGameMode.h
//
// THE SAUCE DOOR — the GameMode for L_Elsewhere (SIB-47). The wonder room must be free
// of the main game's build system: no developer/build HUD overlay, and no re-spawned
// "generated" build sites from the deploy save (UBranchPIEComponent skips
// ApplyDeployedSave when it sees this GameMode — the Elsewhere is a throwaway world,
// not the deployed main world). The curio orb is the only focal object.
//
// It reuses the project's FirstPerson pawn + player controller (so movement / look / E
// interaction keep working — the input mapping contexts live on the BP controller),
// and swaps in the clean AElsewhereHUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ElsewhereGameMode.generated.h"

UCLASS()
class SIBELIUSGAME_API AElsewhereGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AElsewhereGameMode();
};
