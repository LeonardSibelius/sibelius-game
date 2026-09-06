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

class UDancerAgentComponent;

/** A guide finished saying her piece — she was not cut off. See NotifyGuideTalkFinished. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGuideTalkFinished, UDancerAgentComponent*);

UCLASS()
class SIBELIUSGAME_API UDancerAgentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/* SHE GOT TO THE END OF IT (docs/FUN_PLAN_2.md A2).

	   Fired from UDancerAgentComponent::ResumeAfterGreeting, which is the NATURAL end of a
	   talk and only that: CancelGreeting clears the timer that calls it, so an F press
	   mid-sentence produces no event. That distinction is the whole reason this hangs here
	   rather than on EndTalkShot, which every cancel path also runs through.

	   It lives on the subsystem for the same reason RestageGuides does — the listener does
	   not have to know what a dancer is, or when the runtime scan got round to attaching
	   one. A component that does not exist yet at the listener's BeginPlay is the normal
	   case here, not an edge case, so subscribing to a component directly would be a race
	   the listener could not win. */
	FOnGuideTalkFinished OnGuideTalkFinished;

	/** Called by the component. Safe with a null world or no listeners. */
	void NotifyGuideTalkFinished(UDancerAgentComponent* Dancer);

	/**
	 * Attach a UDancerAgentComponent to every dancing actor that lacks one, and return
	 * how many were added. Safe to call repeatedly — actors that already have one are
	 * skipped, so a late arrival (the altar's dancer) can be picked up by calling again.
	 */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	int32 ScanForDancers();

	/**
	 * Every live dancer. UInteractorComponent walks this each tick to aim-assist onto a
	 * dancer, so it must stay cheap — iterating all ~1000 level actors per frame would
	 * not be. Components register themselves in BeginPlay, which covers both the ones
	 * this subsystem adopts and any added by hand.
	 */
	const TArray<TWeakObjectPtr<UDancerAgentComponent>>& GetDancers() const { return Dancers; }

	/** Idempotent — safe to call for a component that is already registered. */
	void RegisterDancer(UDancerAgentComponent* Dancer);

	/**
	 * Something changed in the world that a guide's stage depends on — tell the guides.
	 *
	 * ASpaceport calls this the moment it is freshly generated. Routing it through here
	 * rather than having the spaceport find dancers itself means the spaceport does not
	 * need to know what a dancer IS, and any future trigger (a shop bought out, a rocket
	 * launched) has one obvious place to call.
	 *
	 * Each guide decides for herself whether her stage actually moved and waits until the
	 * player cannot see her, so this is safe to call speculatively and safe to call twice.
	 */
	UFUNCTION(BlueprintCallable, Category="Dancer")
	void RestageGuides();

	/** Real gameplay worlds only — not the editor preview or the commandlet worlds. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** Weak, so a destroyed dancer simply goes stale — no unregister bookkeeping needed. */
	TArray<TWeakObjectPtr<UDancerAgentComponent>> Dancers;
};
