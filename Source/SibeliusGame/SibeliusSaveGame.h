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
	// v1: ObjectDeltas + ResourceDeltas. v2: adds FormatNote. v3 (SIB-30 P3): adds
	// per-record generated provenance (FBranchObjectState::bGenerated/EntryId/Transform)
	// + GenerateBudget.
	static constexpr int32 CurrentSaveVersion = 3;

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

	// Added in v2 — a small provenance tag. A fresh deploy stamps the current version;
	// the v1->v2 migrator fills it with a marker so the migration path is observable.
	UPROPERTY()
	FString FormatNote;

	// Added in v3 (SIB-30 P3) — the remaining generation budget at deploy time. It lived
	// only on UGenerateComponent and reset on reload; persisting it here restores the
	// in-fiction economy. Sentinel -1 = NOT recorded (a pre-v3 save, or no generator in
	// the world) — leave the live budget untouched. >= 0 = restore exactly (re-spawning
	// generated objects is a pure recreate and never re-charges this).
	UPROPERTY()
	int32 GenerateBudget = -1;

	// Upgrade this save's shape to CurrentSaveVersion via ordered migration steps.
	// Returns true if it is now at CurrentSaveVersion (was current, or migrated up);
	// false if it is NEWER than CurrentSaveVersion (can't downgrade — caller refuses).
	bool MigrateToCurrent();

	// Cheap structural sanity check for a LOADED save (guards a readable-but-garbage
	// object). A written save is always stamped (>= 1).
	bool IsStructurallyValid() const;

private:
	void Migrate_v1_to_v2(); // ordered step: shape v1 -> v2
	void Migrate_v2_to_v3(); // ordered step: shape v2 -> v3 (SIB-30 P3)
};
