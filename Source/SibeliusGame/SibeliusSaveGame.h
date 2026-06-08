// SibeliusSaveGame.h
//
// SIB-29 — Ch5. The persisted form of a deployed branch manifest: GUID-keyed
// declared-state DELTAS (objects whose state differs from their authored default)
// plus the non-zero resource entries — NOT a full actor dump.
//
// Versioning (Phase 3): SaveVersion records the shape a save was written in.
// MigrateToCurrent() walks an older save up through ordered steps (v1->v2, …) to
// CurrentSaveVersion before it's applied; a NEWER save can't be downgraded.

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
	// Bump whenever the persisted shape changes, and add a matching migration step.
	// v1: ObjectDeltas + ResourceDeltas. v2: adds FormatNote.
	static constexpr int32 CurrentSaveVersion = 2;

	// Stamped to CurrentSaveVersion on write (see UBranchSubsystem::RequestDeploy).
	// A freshly constructed (unwritten) save reads 0.
	UPROPERTY()
	int32 SaveVersion = 0;

	// The deployed manifest, deltas only. Objects keyed by their stable GUID
	// (FBranchObjectState::ObjectId); resource entries carry EntryId + Resource.
	UPROPERTY()
	TArray<FBranchObjectState> ObjectDeltas;

	UPROPERTY()
	TArray<FResourceEntry> ResourceDeltas;

	// Added in v2 — a small provenance tag. A fresh v2 deploy stamps "v2"; the
	// v1->v2 migrator fills it with a marker so the migration path is observable.
	UPROPERTY()
	FString FormatNote;

	// Upgrade this save's shape to CurrentSaveVersion via ordered migration steps.
	// Returns true if it is now at CurrentSaveVersion (was current, or migrated up);
	// false if it is NEWER than CurrentSaveVersion (can't downgrade — caller refuses).
	bool MigrateToCurrent();

	// Cheap structural sanity check for a LOADED save (guards a readable-but-garbage
	// object). A written save is always stamped (>= 1).
	bool IsStructurallyValid() const;

private:
	void Migrate_v1_to_v2(); // ordered step: shape v1 -> v2
};
