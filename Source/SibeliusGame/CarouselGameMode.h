// CarouselGameMode.h
//
// SIB-46 Presentation (grey-box) — minimal game mode for L_Carousel_Test: the Canvas HUD + a plain
// flying DefaultPawn (no project character/temple assets, so the slice stays portable to the lean
// fork). Assign as the map's GameMode Override.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CarouselGameMode.generated.h"

UCLASS()
class SIBELIUSGAME_API ACarouselGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACarouselGameMode();
};
