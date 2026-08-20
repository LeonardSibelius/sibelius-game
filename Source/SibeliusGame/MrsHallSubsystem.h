// MrsHallSubsystem.h
//
// THE VOICE OF MRS. HALL — docs/SPINE.md Move 2.
//
// Until now she could only speak from UGenerateComponent, which made the antagonist of
// this game a validation failure message in Chapter 6 of 7: for the first five chapters
// nothing opposed the player at all. Her memo widget, her dismiss timer and her clip
// playback all lived inside that one component, so nothing else could reach them.
//
// This is that channel, lifted out. Any system can now say something as her.
//
// WORLD subsystem, not GameInstance: it owns a viewport widget and a world timer, and a
// GameInstance subsystem would outlive a level load and leak the widget. The rotating
// line selector resetting per level is fine — the smoke-test discipline requires that
// selection use NO RNG and no clock, not that it persist.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MrsHallLines.h"
#include "MrsHallSubsystem.generated.h"

class UMrsHallMessageWidget;

UCLASS()
class SIBELIUSGAME_API UMrsHallSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor; null in headless//teardown, and every caller must tolerate that. */
	static UMrsHallSubsystem* Get(const UObject* WorldContext);

	/**
	 * Say a line from Data/MrsHallStory.csv for this Reason.
	 *
	 * No-op when the Reason has no rows — see the gate note below. Reasons in use:
	 * "Ticket" (the opening assignment). Move 2's remaining beats add Power.* and Final.
	 */
	void Say(FName Reason);

	/** Say an exact line. Used by Chapter 6, which picks from its own refusal table. */
	void SayLine(const FString& Line, const FString& AudioKey);

	/** True when the story table loaded and holds at least one row for Reason. The gate
	 *  asserts this: a missing Reason makes her say NOTHING, with no error and no log —
	 *  the same silent-failure class that hid every interaction prompt for eight
	 *  releases. */
	bool HasLinesFor(FName Reason);

	virtual void Deinitialize() override;

private:
	void EnsureLoaded();
	void PlayClip(const FString& AudioKey);
	void Dismiss();

	UPROPERTY()
	TObjectPtr<UMrsHallMessageWidget> Widget;

	FTimerHandle DismissTimer;

	TMap<FName, TArray<FMrsHallLine>> StoryLines;
	bool bLoaded = false;

	/** Rotates within a Reason group. Deterministic — no RNG, no clock. */
	int32 Selector = 0;

	/** Matches the Chapter 6 memo's dwell so her voice reads the same everywhere. */
	static constexpr float DismissSeconds = 6.0f;
};
