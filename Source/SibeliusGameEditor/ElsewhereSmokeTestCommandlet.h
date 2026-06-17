// ElsewhereSmokeTestCommandlet.h
//
// THE SAUCE DOOR — headless ship gate (SIB-47). Modeled on the sibling gates
// (USauceSmokeTestCommandlet / UCarouselSmokeTestCommandlet). Editor-only module
// (PK12). Proves the WHOLE loop as logic: deterministic generation, curio-fits-place,
// the collection save/load round-trip (curios + score), the discard rule (no room in
// the save), the builder spawns the one curio + return door, and the Cabinet fills.
//
//   -run=ElsewhereSmokeTest -unattended -nopause -nosplash -stdout   (editor CLOSED)

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ElsewhereSmokeTestCommandlet.generated.h"

UCLASS()
class UElsewhereSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UElsewhereSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
