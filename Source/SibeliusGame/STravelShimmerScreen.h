// STravelShimmerScreen.h
//
// Travel-door transition cover: a lean, procedural Slate screen — solid near-black
// background, an animated throbber (no texture/movie asset), and a themed context line
// ("Entering the Many Worlds...", "Returning to the office..."). ONE widget, reused for
// BOTH the pre/post-OpenLevel viewport cover (UTravelTransitionSubsystem) AND the
// MoviePlayer loading screen registered at module startup. Optional self-driven fade
// (in or out) via a Slate active timer; the throbber animates on its own (so it keeps
// moving on the MoviePlayer loading thread during a blocking load).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STravelShimmerScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STravelShimmerScreen)
		: _ContextText()
		, _FadeIn(false)
		, _FadeOut(false)
		, _FadeDuration(0.3f)
		{}
		/** The themed line under the throbber. */
		SLATE_ARGUMENT(FText, ContextText)
		/** Ramp opacity 0 -> 1 over FadeDuration (the "to black" cover). */
		SLATE_ARGUMENT(bool, FadeIn)
		/** Ramp opacity 1 -> 0 over FadeDuration, then fire OnFadeComplete (reveal into gameplay). */
		SLATE_ARGUMENT(bool, FadeOut)
		SLATE_ARGUMENT(float, FadeDuration)
		/** Fired once a FadeOut finishes, so the owner can remove the widget. */
		SLATE_EVENT(FSimpleDelegate, OnFadeComplete)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EActiveTimerReturnType DriveFade(double InCurrentTime, float InDeltaTime);

	bool bFadeIn = false;
	bool bFadeOut = false;
	float FadeDuration = 0.3f;
	float Elapsed = 0.0f;
	FSimpleDelegate OnFadeComplete;
};
