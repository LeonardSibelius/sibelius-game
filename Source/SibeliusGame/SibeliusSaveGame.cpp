// SibeliusSaveGame.cpp — SIB-29 Ch5 Phase 3. Version migration + structural check.

#include "SibeliusSaveGame.h"

bool USibeliusSaveGame::MigrateToCurrent()
{
	if (SaveVersion > CurrentSaveVersion)
	{
		return false; // newer than this build understands — can't downgrade
	}

	// Ordered steps: a v(N) save walks N -> N+1 -> … -> current. Each step bumps
	// SaveVersion to its target, so adding the next is mechanical:
	//   if (SaveVersion < 4) { Migrate_v3_to_v4(); }
	if (SaveVersion < 2)
	{
		Migrate_v1_to_v2();
	}
	if (SaveVersion < 3)
	{
		Migrate_v2_to_v3();
	}

	return SaveVersion == CurrentSaveVersion;
}

void USibeliusSaveGame::Migrate_v1_to_v2()
{
	// v2 added FormatNote; the deltas are shape-compatible across v1->v2. Fill the
	// new field with a marker so the migration is observable, then bump the version.
	FormatNote = TEXT("migrated:v1->v2");
	SaveVersion = 2;
}

void USibeliusSaveGame::Migrate_v2_to_v3()
{
	// v3 (SIB-30 P3) added per-record GENERATED provenance + GenerateBudget. A v2 save
	// has no generated records (the new FBranchObjectState fields tagged-deserialize to
	// defaults: bGenerated=false) and no budget (GenerateBudget stays -1 = leave the live
	// budget alone). Nothing to backfill — just bump. FormatNote is left untouched so the
	// v1->v2 provenance marker still reads exactly "migrated:v1->v2".
	SaveVersion = 3;
}

bool USibeliusSaveGame::IsStructurallyValid() const
{
	// A written save is always stamped to >= 1 (BuildDeploySave). A SaveVersion < 1
	// on a loaded object means truncated/garbage data deserialized into defaults.
	return SaveVersion >= 1;
}
