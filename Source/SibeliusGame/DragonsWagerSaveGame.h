#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DragonsWagerSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API UDragonsWagerSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) int32 BestScore = 0;
    UPROPERTY(SaveGame) int32 BestRound = 0;
};
