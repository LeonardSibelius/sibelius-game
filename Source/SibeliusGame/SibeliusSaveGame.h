// SibeliusSaveGame.h
//
// SIB-29 — Ch5 Phase 1 (SaveGame write). The persisted form of a deployed branch
// manifest: GUID-keyed declared-state DELTAS (objects whose state differs from
// their authored default) plus the non-zero resource entries — NOT a full actor
// dump. Phase 2 will load this and re-apply it to the world on boot.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BranchTypes.h"   // FBranchObjectState (GUID-keyed) + FResourceEntry
#include "SibeliusSaveGame.generated.h"

UCLASS()
class SIBELIUSGAME_API USibeliusSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Bump whenever the persisted shape changes; Phase 2 load reads/migrates on it.
	// The version starts at 1; a freshly constructed (unstamped) save reads 0.
	static constexpr int32 CurrentSaveVersion = 1;

	// Stamped to CurrentSaveVersion on write (see UBranchSubsystem::RequestDeploy).
	UPROPERTY()
	int32 SaveVersion = 0;

	// The deployed manifest, deltas only. Objects keyed by their stable GUID
	// (FBranchObjectState::ObjectId); resource entries carry EntryId + Resource.
	UPROPERTY()
	TArray<FBranchObjectState> ObjectDeltas;

	UPROPERTY()
	TArray<FResourceEntry> ResourceDeltas;
};
