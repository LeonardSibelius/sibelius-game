// Copyright Epic Games, Inc. All Rights Reserved.

#include "SibeliusGame.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"     // FCoreUObjectDelegates::PreLoadMap
#include "STravelShimmerScreen.h"
#include "TravelTransitionSubsystem.h"  // ContextLineForLevel (shared theming)

// Custom game module so we can register the animated MoviePlayer loading screen that covers
// the BLOCKING level load (the long ~15s forest load) in Standalone / packaged builds.
// NOTE: the MoviePlayer screen generally does NOT display in PIE — there the in-viewport
// fade/throbber cover (UTravelTransitionSubsystem) carries the transition instead.
class FSibeliusGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnPreLoadMap(const FString& MapName);
	FDelegateHandle PreLoadMapHandle;
};

void FSibeliusGameModule::StartupModule()
{
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FSibeliusGameModule::OnPreLoadMap);
}

void FSibeliusGameModule::OnPreLoadMap(const FString& MapName)
{
	if (!IsMoviePlayerEnabled())
	{
		return;   // editor / PIE / commandlet — no movie player; the viewport cover handles it
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.MinimumLoadingScreenDisplayTime = 0.75f;   // small floor so a fast load still reads
	Attributes.WidgetLoadingScreen = SNew(STravelShimmerScreen)
		.ContextText(UTravelTransitionSubsystem::ContextLineForLevel(MapName));

	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}

void FSibeliusGameModule::ShutdownModule()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	}
}

IMPLEMENT_PRIMARY_GAME_MODULE( FSibeliusGameModule, SibeliusGame, "SibeliusGame" );

DEFINE_LOG_CATEGORY(LogSibeliusGame)
