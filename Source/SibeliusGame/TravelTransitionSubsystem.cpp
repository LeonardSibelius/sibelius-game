// TravelTransitionSubsystem.cpp — see header.

#include "TravelTransitionSubsystem.h"
#include "STravelShimmerScreen.h"
#include "SibeliusGame.h"                // LogSibeliusGame

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"                 // force-dismiss the loading screen on map load
#include "UObject/UObjectGlobals.h"

namespace TravelTransitionNS
{
	constexpr float FadeToBlackTime = 0.30f;   // immediate feedback before the load
	constexpr float HoldBeforeOpen  = 0.35f;   // let the cover paint + fade finish, THEN OpenLevel
	constexpr float FadeInTime      = 0.55f;   // reveal into gameplay on the destination
	constexpr int32 CoverZOrder     = 10000;   // above HUD / UMG
	constexpr float WatchdogSeconds = 60.0f;   // never trap the player behind a stuck loading screen
}

void UTravelTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTravelTransitionSubsystem::OnPostLoadMap);
}

void UTravelTransitionSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	ClearWatchdog();
	RemoveCover();
	Super::Deinitialize();
}

UGameViewportClient* UTravelTransitionSubsystem::GetViewport() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetGameViewportClient() : nullptr;
}

void UTravelTransitionSubsystem::Travel(const UObject* WorldContext, FName LevelName)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UTravelTransitionSubsystem* Sub = GI ? GI->GetSubsystem<UTravelTransitionSubsystem>() : nullptr)
	{
		Sub->BeginTravel(LevelName);
		return;
	}

	// Headless / no subsystem (e.g. a commandlet): just travel, no cover.
	if (World)
	{
		UGameplayStatics::OpenLevel(World, LevelName);
	}
}

void UTravelTransitionSubsystem::BeginTravel(FName LevelName)
{
	if (bTravelInProgress)
	{
		return;   // debounce a second trigger mid-transition
	}

	using namespace TravelTransitionNS;

	UGameViewportClient* VP = GetViewport();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!VP || !World)
	{
		// No viewport (dedicated server / headless): open immediately.
		if (World)
		{
			UGameplayStatics::OpenLevel(World, LevelName);
		}
		return;
	}

	bTravelInProgress = true;
	PendingLevelName = LevelName;

	RemoveCover();
	CoverWidget = SNew(STravelShimmerScreen)
		.ContextText(ContextLineForLevel(LevelName.ToString()))
		.FadeIn(true)
		.FadeDuration(FadeToBlackTime);
	VP->AddViewportWidgetContent(CoverWidget.ToSharedRef(), CoverZOrder);

	// SAFEGUARD: if the destination never finishes loading (map missing / load failure /
	// PostLoadMapWithWorld never fires), abort the loading screen instead of trapping the
	// player forever. The core ticker survives the level travel, unlike a world timer.
	ClearWatchdog();
	WatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTravelTransitionSubsystem::OnWatchdog), WatchdogSeconds);

	// Defer the (blocking) OpenLevel a beat so the cover paints first — the press reads as
	// registered instantly. The MoviePlayer loading screen then covers the load in packaged.
	World->GetTimerManager().SetTimer(
		OpenLevelTimer,
		FTimerDelegate::CreateUObject(this, &UTravelTransitionSubsystem::DoOpenLevel, LevelName),
		HoldBeforeOpen, /*bLoop=*/false);
}

void UTravelTransitionSubsystem::DoOpenLevel(FName LevelName)
{
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		UGameplayStatics::OpenLevel(World, LevelName);
	}
}

void UTravelTransitionSubsystem::OnPostLoadMap(UWorld* LoadedWorld)
{
	// We got a map-load signal — the watchdog is no longer needed.
	ClearWatchdog();

	if (!bTravelInProgress)
	{
		return;   // only reveal after a travel WE initiated (not the first map / editor loads)
	}
	bTravelInProgress = false;

	using namespace TravelTransitionNS;

	// Force-dismiss the MoviePlayer loading screen. A FAST load (e.g. the ~1s L_AI_Temple)
	// can miss bAutoCompleteWhenLoadingCompletes' "loading done" edge and otherwise spin
	// forever; an explicit stop on the map-load signal guarantees it comes down.
	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}

	UGameViewportClient* VP = GetViewport();
	if (!VP)
	{
		RemoveCover();
		return;
	}

	// Remove the stale fade-in cover (don't orphan it behind the new one), then add a fresh
	// full-black cover that fades OUT into gameplay.
	RemoveCover();
	CoverWidget = SNew(STravelShimmerScreen)
		.ContextText(ContextLineForLevel(LoadedWorld ? LoadedWorld->GetMapName() : FString()))
		.FadeOut(true)
		.FadeDuration(FadeInTime)
		.OnFadeComplete(FSimpleDelegate::CreateUObject(this, &UTravelTransitionSubsystem::RemoveCover));
	VP->AddViewportWidgetContent(CoverWidget.ToSharedRef(), CoverZOrder);
}

bool UTravelTransitionSubsystem::OnWatchdog(float /*DeltaTime*/)
{
	using namespace TravelTransitionNS;
	WatchdogHandle.Reset();

	if (bTravelInProgress)
	{
		bTravelInProgress = false;

		// Surface the failure — do NOT mask it.
		UE_LOG(LogSibeliusGame, Error,
			TEXT("[Travel] timed out after %.0fs waiting for '%s' to finish loading — aborting the loading screen "
			     "(the level may be missing or failed to load)."),
			WatchdogSeconds, *PendingLevelName.ToString());

		// Tear down BOTH covers so the player is never trapped behind an opaque screen.
		if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
		{
			GetMoviePlayer()->StopMovie();
		}
		RemoveCover();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("Travel to %s failed to load — please try again."), *PendingLevelName.ToString()));
		}
	}
	return false;   // one-shot
}

void UTravelTransitionSubsystem::ClearWatchdog()
{
	if (WatchdogHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WatchdogHandle);
		WatchdogHandle.Reset();
	}
}

void UTravelTransitionSubsystem::RemoveCover()
{
	if (CoverWidget.IsValid())
	{
		if (UGameViewportClient* VP = GetViewport())
		{
			VP->RemoveViewportWidgetContent(CoverWidget.ToSharedRef());
		}
		CoverWidget.Reset();
	}
}

FText UTravelTransitionSubsystem::ContextLineForLevel(const FString& LevelName)
{
	if (LevelName.Contains(TEXT("Poplar_Forest")) || LevelName.Contains(TEXT("Elsewhere")))
	{
		return NSLOCTEXT("Travel", "EnterForest", "Entering the Many Worlds...");
	}
	if (LevelName.Contains(TEXT("Office")))
	{
		return NSLOCTEXT("Travel", "ReturnOffice", "Returning to the office...");
	}
	if (LevelName.Contains(TEXT("Cathedral")))
	{
		return NSLOCTEXT("Travel", "EnterCathedral", "Entering the Cathedral...");
	}
	if (LevelName.Contains(TEXT("Temple")))
	{
		return NSLOCTEXT("Travel", "EnterTemple", "Entering the Temple...");
	}
	return NSLOCTEXT("Travel", "Traveling", "Traveling...");
}
