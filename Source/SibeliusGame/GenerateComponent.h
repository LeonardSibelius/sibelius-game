// GenerateComponent.h
//
// SIB-30 — Ch6 P1. Player-side Generate driver: owns the per-session generation budget
// and the loaded catalog, resolves a typed request through the pure matcher, and spawns
// the result as a real ABuildSite (so it rides the Ch3/Ch5 pipeline). Lives on the
// character, mirroring UBuildComponent.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GenerateTypes.h"
#include "GenerateComponent.generated.h"

class UMrsHallMessageWidget;

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UGenerateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGenerateComponent();

	// Resolve a typed request: spawn on Resolved (decrementing the budget) or show a
	// distinct refusal line per reason (the P1 seed of the Mrs. Hall refusal). Returns
	// the matcher outcome.
	EGenerateOutcome SubmitRequest(const FString& RawText);

	int32 GetRemainingBudget() const { return RemainingBudget; }
	int32 GetCatalogNum() const { return Catalog.Num(); }

protected:
	virtual void BeginPlay() override;

	// Per-session generation budget (the in-fiction economy). Tunable; Walt curates.
	UPROPERTY(EditAnywhere, Category = "Generate")
	int32 RemainingBudget = 10;

	// How far in front of the player to place a generated object before the floor trace.
	UPROPERTY(EditAnywhere, Category = "Generate")
	float SpawnAheadDistance = 250.0f;

private:
	bool SpawnEntry(const FGenerateCatalogEntry& Entry);
	const FGenerateCatalogEntry* FindEntry(const FName& Id) const;
	void Toast(const FString& Msg, const FColor& Color) const;

	// P2: present Mrs. Hall's refusal as a styled memo (auto-dismissed); helper-toast fallback.
	void ShowMrsHall(const FString& Line);
	void DismissMrsHall();

	TArray<FGenerateCatalogEntry> Catalog;

	// P2 data (loaded in BeginPlay): refusal lines grouped by outcome + the DC blocklist.
	TMap<EGenerateOutcome, TArray<FString>> RefusalLines;
	TArray<FString> Blocklist;

	// Rotates Mrs. Hall's lines deterministically across refusals (NOT RNG/clock).
	int32 RefusalCount = 0;

	UPROPERTY()
	TObjectPtr<UMrsHallMessageWidget> MrsHallWidget;

	FTimerHandle MrsHallDismissTimer;
};
