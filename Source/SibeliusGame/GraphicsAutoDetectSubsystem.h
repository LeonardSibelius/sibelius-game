// GraphicsAutoDetectSubsystem.h
//
// THE GAME PICKS ITS OWN GRAPHICS ON THE FIRST LAUNCH (2026-11-12, 2.5.3).
//
// The first Steam review, the day after launch, recommended the game and asked for exactly
// one thing: "It would be fine if I could lower the shadows, lighting, and turn off any
// unnecessary effects, but there is no list of settings to mess with." There was not. Nothing
// in the game ever touched UGameUserSettings, so every PC - a 4090 or the store page's minimum
// 2070 - started on Unreal's default, Epic, with Lumen's bounced light and reflections on.
//
// Two halves, both small:
//   - THIS: on the first launch of a packaged build, Unreal's own hardware benchmark measures
//     the CPU and GPU (a second or so, during boot, before Kaia's cutscene) and the quality it
//     recommends is applied and saved. "First" means the save has no benchmark result yet, so
//     every existing player gets measured once on their first launch of 2.5.3 as well.
//   - the GRAPHICS tab in the M menu (GameMenuWidget.cpp), which can change any of it by hand
//     and run this measurement again.
//
// NOT IN THE EDITOR. In PIE the engine's scalability IS the editor viewport's, so measuring
// there would quietly retune Walt's editor. Commandlets have nothing to measure with.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GraphicsAutoDetectSubsystem.generated.h"

UCLASS()
class SIBELIUSGAME_API UGraphicsAutoDetectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Measure this PC, apply the quality it can carry, and save it. The GRAPHICS tab's
	    AUTO-DETECT button calls this too. Returns the overall level chosen (0 Low .. 3 Epic),
	    or -1 when the groups came out mixed or there are no settings to change. */
	static int32 DetectAndApply();

	/** Whether the automatic first-launch measurement may run here: a game, not the editor
	    or a commandlet, with something to render on. */
	static bool CanAutoDetect();
};
