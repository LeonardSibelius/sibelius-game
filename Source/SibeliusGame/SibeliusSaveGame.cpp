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
	//   if (SaveVersion < 3) { Migrate_v2_to_v3(); }
	if (SaveVersion < 2)
	{
		Migrate_v1_to_v2();
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

bool USibeliusSaveGame::IsStructurallyValid() const
{
	// A written save is always stamped to >= 1 (BuildDeploySave). A SaveVersion < 1
	// on a loaded object means truncated/garbage data deserialized into defaults.
	return SaveVersion >= 1;
}
