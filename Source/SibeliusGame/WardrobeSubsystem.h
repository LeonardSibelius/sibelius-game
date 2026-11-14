// WardrobeSubsystem.h
//
// OUTFITS NOBODY WEARS. Walt, 2026-11-13, of Kaia on the wrestling mat: "get rid of that pink
// dress. can you remove it from Kaia opening monologue also?"
//
// The dress is Kaia's separate outfit mesh (MHC_Kaia_Outfits) on her MetaHuman blueprint. Her
// dancer blueprint (BP_Dancer_Kaia, the office) already hides it; the plain one (BP_MHC_Kaia)
// shows it - and that is the one the wrestling, the hoops and the karts spawn, and the one her
// opening monologue stands in. Rather than edit each spawner, or the MetaHuman assets (which are
// not in the repo and are rebuilt whenever she is re-assembled), one rule here: in every game
// world, any outfit mesh on the list is hidden - on every actor already there when play
// begins, and on every actor spawned after (a sequence's spawnables included).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WardrobeSubsystem.generated.h"

UCLASS()
class SIBELIUSGAME_API UWardrobeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Hide any retired outfit on this actor. True when something was hidden. */
	static bool Undress(AActor* Actor);

private:
	void HandleSpawned(AActor* Actor);

	FDelegateHandle SpawnedHandle;
	TWeakObjectPtr<UWorld> BoundWorld;
};
