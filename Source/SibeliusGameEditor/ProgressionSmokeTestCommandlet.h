// ProgressionSmokeTestCommandlet.h
//
// FUN-1/FUN-2 — headless gate for the progression model (powers + Sauce).
// Asserts the pure FProgressionState invariants (fresh state = Code Vision only,
// no negative sauce, one-time claims stay one-time), the exec-cheat name parser,
// and a full save round-trip through FSibeliusSaveIO on a sandbox slot (written,
// re-loaded, compared, deleted). Pure model + save I/O — no world to clean up.
// Run editor-closed: -run=ProgressionSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ProgressionSmokeTestCommandlet.generated.h"

UCLASS()
class UProgressionSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UProgressionSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
