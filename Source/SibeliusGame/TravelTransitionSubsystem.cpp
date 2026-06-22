// TravelTransitionSubsystem.cpp — see header.

#include "TravelTransitionSubsystem.h"
#include "STravelShimmerScreen.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

namespace TravelTransitionNS
{
	constexpr float FadeToBlackTime = 0.30f;   // immediate feedback before the load
	constexpr float HoldBeforeOpen  = 0.35f;   // let the cover paint + fade finish, THEN OpenLevel
	constexpr float FadeInTime      = 0.55f;   // reveal into gameplay on the destination
	constexpr int32 CoverZOrder     = 10000;   // above HUD / UMG
}

void UTravelTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTravelTransitionSubsystem::OnPostLoadMap);
}

void UTravelTransitionSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
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

	RemoveCover();
	CoverWidget = SNew(STravelShimmerScreen)
		.ContextText(ContextLineForLevel(LevelName.ToString()))
		.FadeIn(true)
		.FadeDuration(FadeToBlackTime);
	VP->AddViewportWidgetContent(CoverWidget.ToSharedRef(), CoverZOrder);

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
	if (!bTravelInProgress)
	{
		return;   // only reveal after a travel WE initiated (not the first map / editor loads)
	}
	bTravelInProgress = false;

	using namespace TravelTransitionNS;

	UGameViewportClient* VP = GetViewport();
	if (!VP)
	{
		return;
	}

	// The pre-load cover is cleared by the engine's RemoveAllViewportWidgets during travel;
	// drop our stale handle and add a fresh full-black cover that fades OUT into gameplay.
	CoverWidget.Reset();
	CoverWidget = SNew(STravelShimmerScreen)
		.ContextText(ContextLineForLevel(LoadedWorld ? LoadedWorld->GetMapName() : FString()))
		.FadeOut(true)
		.FadeDuration(FadeInTime)
		.OnFadeComplete(FSimpleDelegate::CreateUObject(this, &UTravelTransitionSubsystem::RemoveCover));
	VP->AddViewportWidgetContent(CoverWidget.ToSharedRef(), CoverZOrder);
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
