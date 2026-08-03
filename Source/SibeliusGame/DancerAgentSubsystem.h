// DancerAgentSubsystem.h
//
// Finds the dancing girls and makes them AI Agents (E to talk, F to reshuffle her
// dance). See DancerAgentComponent.h for the feature itself.
//
// WHY A SCAN RATHER THAN PER-DANCER EDITOR WORK
//
// The five dancers are set up inconsistently and always will be: Kaia has a child
// Blueprint (BP_Dancer_Kaia), Nyra and Isla are placed MetaHuman actors with none, and
// the cathedral dancer is spawned at runtime by AFinaleAltar. Worse, RE-ASSEMBLING a
// MetaHuman wipes hand-added components — so a component placed by hand on Nyra would
// silently vanish the next time she is rebuilt, and the bug would show up months later
// as "F stopped working on Nyra".
//
// So the rule is behavioural, not structural: AN ACTOR PLAYING ONE OF THE TEN MORRO
// DANCES IS A DANCER. Nothing to tag, nothing to re-parent, and it survives
// re-assembly. New dancers work the moment they are placed.
//
// NOTE FOR THE COOKER: this scan is NOT a reference. It finds actors at runtime, so it
// drags nothing into the pak — exactly the Refactor Menagerie / Death_Back lesson in
// docs/VENDOR_PACKS.md. The animations reach the build because
// UDancerAgentComponent's CDO hard-references them, not because of anything here.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DancerAgentSubsystem.generated.h"

UCLASS()
class SIBELIUSGAME_API UDancerAgentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * Attach a UDancerAgentComponent to every dancing actor that lacks one, and return
	 * how many were added. Safe to call repeatedly — actors that already have one are
	 * skipped, so a late arrival (the altar's dancer) can be picked up by calling again.
	 */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	int32 ScanForDancers();

	/** Real gameplay worlds only — not the editor preview or the commandlet worlds. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
};
