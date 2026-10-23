// ArcadeSmokeTestCommandlet.h
//
// Headless gate for Leonard's Arcade (ArcadeLibrary.h): the picks are whole, the prompt carries
// the code word and the no-Escape rule and never the Windows user name, the code word file reads
// back its own number, the browser's offline
// rule lets files through and nothing with a host (a network share included), file URLs encode,
// titles parse, and the folder scan finds games, flags the ones made from an assignment, and
// lists the newest first. Pure - no world, no browser. Run editor-closed: -run=ArcadeSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ArcadeSmokeTestCommandlet.generated.h"

UCLASS()
class UArcadeSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UArcadeSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
