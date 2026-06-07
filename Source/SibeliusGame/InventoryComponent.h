// Ch3 - Compile (SIB-27). Single-authority inventory (ledger C3).
// Lives on SibeliusGameCharacter. No save interaction this chapter (C5 -> Ch5).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompileTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryChanged, EResourceType, Resource, int32, NewCount);

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Add(EResourceType Resource, int32 Amount);

	// Returns false (and changes nothing) if Count < Amount. C3: no negative counts, ever.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool Spend(EResourceType Resource, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCount(EResourceType Resource) const;

	// SIB-28 (Ch4 branch): RAW overwrite to a captured count - NOT Add/Spend
	// semantics - so a branch discard restores the ledger exactly. Clamped >= 0.
	void RestoreCount(EResourceType Resource, int32 Count);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	// Headless self-test for CompileSmokeTest (bar item 2). Returns true when all asserts pass.
	bool RunInventorySelfTest(FString& OutError);

private:
	TMap<EResourceType, int32> Counts;
};
