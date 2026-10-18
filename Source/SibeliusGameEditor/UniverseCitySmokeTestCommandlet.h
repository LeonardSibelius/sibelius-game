// UniverseCitySmokeTestCommandlet.h
//
// THE AI CITY's gate (GPT-6 Astra, 2026-10-17): loads the real L_Genesis in a world with no game
// instance - so no player's save can be touched - and drives the forge's own handlers: retry
// keeps the seed, new draws one, F skips only seen stages, contact waits for visible machines and
// never comes for an early ending, cancel keeps the result, inspecting restores input on exit.
//
//   UnrealEditor-Cmd SibeliusGame.uproject -run=UniverseCitySmokeTest -unattended -nullrhi

#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UniverseCitySmokeTestCommandlet.generated.h"

UCLASS()
class UUniverseCitySmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UUniverseCitySmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
	// Editor-only visual QA. Refuses to run unless a separate -UserDir is provided.
	UFUNCTION(BlueprintCallable, Category = "AI City QA")
	static FString PreviewAction(UWorld* World, const FString& Action, int32 Slot = 0);
};
