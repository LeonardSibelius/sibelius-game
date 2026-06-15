// CarouselSmokeTestCommandlet.h
//
// SIB-46 — headless gate for the Carousel of Fates simulation. Asserts the spin pipeline resolves,
// a known build clears a known seed DETERMINISTICALLY, cascades never run away, and the TUNED
// acceptance band holds (seed 7: hit frequency 45-55%, round-1 clear 50-60%) — so the gate fails
// loudly if anyone knocks the baseline out of tune. Editor-only; pure sim math (no world to clean
// up). Run editor-closed: -run=CarouselSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CarouselSmokeTestCommandlet.generated.h"

UCLASS()
class UCarouselSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCarouselSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
