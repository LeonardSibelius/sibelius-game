// Branchable.cpp — SIB-38. Edit-time GUID baking (editor-only).

#include "Branchable.h"

#if WITH_EDITOR
#include "Engine/World.h"

void IBranchable::AssignBranchIdAtEditTime(UObject* Object, FGuid& Id)
{
	if (!Object || Id.IsValid())
	{
		return; // already baked (or nothing to bake into)
	}
	const UWorld* World = Object->GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return; // only the editor world bakes; PIE/Game/preview are left alone
	}
	// Record for undo + dirty the package BEFORE the change, then assign so the new
	// id serializes into the level on the next save.
	Object->Modify();
	Id = FGuid::NewGuid();
	Object->MarkPackageDirty();
}
#endif
