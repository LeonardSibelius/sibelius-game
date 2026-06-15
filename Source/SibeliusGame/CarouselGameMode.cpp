// CarouselGameMode.cpp — SIB-46 grey-box game mode. See header.

#include "CarouselGameMode.h"
#include "CarouselHUD.h"
#include "GameFramework/DefaultPawn.h"

ACarouselGameMode::ACarouselGameMode()
{
	HUDClass = ACarouselHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();   // fly around, press E at the machine
}
