// CarouselHUD.h
//
// SIB-46 Presentation (grey-box) — a Canvas HUD that READS UCarouselRunSubsystem getters and draws
// the live run state, the shop screen between rounds, and a scalable big-win flash (read from the
// machine's reaction state). No payout math here — display only. Canvas drawing means no UMG asset
// authoring, keeping the slice portable.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CarouselHUD.generated.h"

class UCarouselRunSubsystem;
class ACarouselMachine;

UCLASS()
class SIBELIUSGAME_API ACarouselHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	UCarouselRunSubsystem* GetRun() const;
	ACarouselMachine* GetMachine();

	TWeakObjectPtr<ACarouselMachine> CachedMachine;
};
