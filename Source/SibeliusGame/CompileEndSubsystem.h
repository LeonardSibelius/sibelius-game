// CompileEndSubsystem.h
//
// Ch3 - Compile (SIB-27), Phase 4. Fires chapter completion when the player
// overlaps the level's CompileEndTrigger-tagged volume (a TriggerBox placed in
// the level). A world subsystem binds to that trigger on world BeginPlay, so
// there's no extra actor to place and no GameMode dependency.
//
// Mirrors the tag-driven CodeVisionEndTrigger, but the overlap is handled in
// C++ (log + delegate) rather than the Level Blueprint. Fires exactly once
// (bFired), the same idempotency principle UHallAlarmSubsystem uses.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CompileEndSubsystem.generated.h"

/** Broadcast once, when the player reaches the Compile chapter's end trigger. */
DECLARE_MULTICAST_DELEGATE(FOnCompileChapterComplete);

UCLASS()
class SIBELIUSGAME_API UCompileEndSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Tag a level TriggerBox with this to mark the Compile chapter's end volume. */
	static const FName EndTriggerTag;

	// UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** True once the player has reached the end trigger. */
	bool IsChapterComplete() const { return bFired; }

	/** Fires once, when the player first overlaps a CompileEndTrigger volume. */
	FOnCompileChapterComplete OnCompileChapterComplete;

private:
	UFUNCTION()
	void HandleEndTriggerOverlap(AActor* OverlappedActor, AActor* OtherActor);

	bool bFired = false;
};
