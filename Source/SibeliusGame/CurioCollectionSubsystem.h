// CurioCollectionSubsystem.h
//
// THE SAUCE DOOR — the Cabinet's source of truth (SIB-47). A UGameInstanceSubsystem
// so the collection survives level travel AND the disk save; the Cabinet actor in the
// house reads it, the Curio pickup writes to it. Wraps the pure FCurioCollection (the
// FCarouselRun split) and owns the save/load through FSibeliusSaveIO.
//
// THE persistence rule (§3): this saves curios + score. It NEVER saves a room — see
// UElsewhereSaveGame, which structurally can't hold one.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ElsewhereTypes.h"
#include "CurioCollectionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCollectionChanged);

UCLASS()
class SIBELIUSGAME_API UCurioCollectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// The save slot for the whole feature. Distinct from the branch DeploySlot so the
	// two systems never alias.
	static const TCHAR* SaveSlotName() { return TEXT("ElsewhereSlot"); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Collect a curio by id (called by ACurio on E). Resolves its def from the
	// Elsewhere registry for rarity/score, adds it, autosaves, broadcasts. Returns
	// true iff this was NEW to the Cabinet (a fresh slot filled). Unknown id -> false,
	// no change. There is no failure-to-collect: you always get the curio (§1).
	UFUNCTION(BlueprintCallable, Category = "Collection")
	bool CollectCurio(FName CurioId, FName PlaceTypeId);

	UFUNCTION(BlueprintPure, Category = "Collection")
	bool OwnsCurio(FName CurioId) const { return Collection.OwnsCurio(CurioId); }

	UFUNCTION(BlueprintPure, Category = "Collection")
	int32 GetScore() const { return Collection.Score; }

	UFUNCTION(BlueprintPure, Category = "Collection")
	int32 GetUniqueCount() const { return Collection.Owned.Num(); }

	UFUNCTION(BlueprintPure, Category = "Collection")
	int32 GetTotalCollected() const { return Collection.TotalCollected; }

	const FCurioCollection& GetCollection() const { return Collection; }

	// Explicit disk I/O (Collect autosaves, so these are mostly for menus/tests).
	UFUNCTION(BlueprintCallable, Category = "Collection") bool SaveToDisk() const;
	UFUNCTION(BlueprintCallable, Category = "Collection") bool LoadFromDisk();

	UPROPERTY(BlueprintAssignable, Category = "Collection")
	FOnCollectionChanged OnCollectionChanged;

private:
	FCurioCollection Collection;
};
