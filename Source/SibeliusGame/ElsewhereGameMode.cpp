// ElsewhereGameMode.cpp — see header.

#include "ElsewhereGameMode.h"
#include "ElsewhereHUD.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AElsewhereGameMode::AElsewhereGameMode()
{
	HUDClass = AElsewhereHUD::StaticClass();

	// Reuse the working FirstPerson pawn + controller (the EnhancedInput mapping
	// contexts are assigned on the BP controller, so movement/look/E only work with
	// these). The build system rides on the character, but AElsewhereGameMode is the
	// flag UBranchPIEComponent checks to SKIP the deploy-save restore here — so no
	// generated build sites leak into the Elsewhere.
	static ConstructorHelpers::FClassFinder<APawn> PawnBP(
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	if (PawnBP.Succeeded())
	{
		DefaultPawnClass = PawnBP.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> ControllerBP(
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	if (ControllerBP.Succeeded())
	{
		PlayerControllerClass = ControllerBP.Class;
	}
}
