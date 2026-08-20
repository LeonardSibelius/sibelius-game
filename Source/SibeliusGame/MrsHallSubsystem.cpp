// MrsHallSubsystem.cpp — see header.

#include "MrsHallSubsystem.h"

#include "MrsHallMessageWidget.h"
#include "SibeliusHUD.h"                 // toast fallback when there is no memo widget
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMrsHall, Log, All);

UMrsHallSubsystem* UMrsHallSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UMrsHallSubsystem>() : nullptr;
}

void UMrsHallSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimer);
	}
	Dismiss();
	Super::Deinitialize();
}

void UMrsHallSubsystem::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;   // set first: a failed load must not retry every line

	FString Error;
	if (!LoadMrsHallStoryLines(StoryLines, Error))
	{
		// Loud, because the failure mode is silence. If this ever fires in a packaged
		// build the CSV was not staged and she simply stops speaking.
		UE_LOG(LogMrsHall, Error, TEXT("[MrsHall] story lines failed to load: %s"), *Error);
	}
}

bool UMrsHallSubsystem::HasLinesFor(FName Reason)
{
	EnsureLoaded();
	const TArray<FMrsHallLine>* Group = StoryLines.Find(Reason);
	return Group && Group->Num() > 0;
}

void UMrsHallSubsystem::Say(FName Reason)
{
	EnsureLoaded();

	const FMrsHallLine Line = PickMrsHallStoryLine(StoryLines, Reason, Selector);
	if (Line.IsEmpty())
	{
		UE_LOG(LogMrsHall, Warning, TEXT("[MrsHall] nothing to say for Reason '%s'"), *Reason.ToString());
		return;
	}
	++Selector;   // rotate for next time within this Reason's group

	SayLine(Line.Line, Line.AudioKey);
}

void UMrsHallSubsystem::SayLine(const FString& Line, const FString& AudioKey)
{
	if (Line.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

	if (!PC)
	{
		// Headless / teardown. Route through the HUD toast helper, which itself falls
		// back safely — a message is never silently dropped.
		ASibeliusHUD::Toast(this, FString::Printf(TEXT("Mrs. Hall: \"%s\""), *Line),
			DismissSeconds, SibeliusToast::Warn);
		PlayClip(AudioKey);
		return;
	}

	if (!Widget)
	{
		Widget = CreateWidget<UMrsHallMessageWidget>(PC, UMrsHallMessageWidget::StaticClass());
	}
	if (!Widget)
	{
		ASibeliusHUD::Toast(this, FString::Printf(TEXT("Mrs. Hall: \"%s\""), *Line),
			DismissSeconds, SibeliusToast::Warn);
		PlayClip(AudioKey);
		return;
	}

	Widget->SetMessage(FText::FromString(Line));
	if (!Widget->IsInViewport())
	{
		Widget->AddToViewport(50);   // above the world, below any modal panel
	}
	// Non-interactive notice — it never steals input; the player keeps playing.
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (World)
	{
		// A fresh line resets the dwell and replaces the text, rather than queueing.
		World->GetTimerManager().ClearTimer(DismissTimer);
		World->GetTimerManager().SetTimer(DismissTimer, this, &UMrsHallSubsystem::Dismiss,
			DismissSeconds, /*bLoop=*/false);
	}

	PlayClip(AudioKey);
	UE_LOG(LogMrsHall, Display, TEXT("[MrsHall] \"%s\""), *Line);
}

void UMrsHallSubsystem::Dismiss()
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
}

void UMrsHallSubsystem::PlayClip(const FString& AudioKey)
{
	if (AudioKey.IsEmpty())
	{
		return;
	}

	// Headless safety: a commandlet / -nosound run has no audio device, and the gates
	// must stay green and silent.
	if (IsRunningCommandlet() || !FApp::CanEverRenderAudio())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Soft-load by path: a not-yet-recorded clip is harmless, so the TEXT can ship before
	// the voice does. LOAD_NoWarn|LOAD_Quiet keeps a missing clip out of the log.
	// These reach a package via DirectoriesToAlwaysCook=/Game/Audio/MrsHall.
	const FString Path = FString::Printf(TEXT("/Game/Audio/MrsHall/%s.%s"), *AudioKey, *AudioKey);
	if (USoundBase* Clip = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(World, Clip);
	}
}
