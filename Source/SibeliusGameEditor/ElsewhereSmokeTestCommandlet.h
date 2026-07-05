// ElsewhereSmokeTestCommandlet.h
//
// THE MANY WORLDS door — headless ship gate. Modeled on the sibling gates
// (USauceSmokeTestCommandlet / UCarouselSmokeTestCommandlet). Editor-only module
// (PK12). The door now travels to a FIXED authored forest (Mode A) and the curio /
// cabinet / builder flow is set aside, so this gate proves the new shape: the Poplar
// forest level exists and loads, the Back-to-Office travel path exists (office level
// loads + the wander-world allowlist gating the O key includes the forest), and the
// Sauce Door still reveals like a hidden door.
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
