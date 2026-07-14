// CarouselGameMode.cpp — SIB-46 grey-box game mode. See header.

#include "CarouselGameMode.h"
#include "CarouselHUD.h"
#include "TravelTransitionSubsystem.h"
#include "EngineUtils.h"                    // TActorIterator
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerStart.h"

ACarouselGameMode::ACarouselGameMode()
{
	HUDClass = ACarouselHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();   // fly around, press E at the machine
}

AActor* ACarouselGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTravelTransitionSubsystem* Travel = GI->GetSubsystem<UTravelTransitionSubsystem>())
		{
			const FName Tag = Travel->ConsumeArrivalTag();
			if (!Tag.IsNone())
			{
				for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
				{
					if (It->PlayerStartTag == Tag)
					{
						return *It;
					}
				}
			}
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
