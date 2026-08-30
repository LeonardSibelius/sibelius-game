// SequenceCue.cpp — see the header.

#include "SequenceCue.h"

#include "SibeliusHUD.h"
#include "SibeliusGame.h"                 // LogSibeliusGame
#include "SibeliusProgressSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"

ASequenceCue::ASequenceCue()
{
	PrimaryActorTick.bCanEverTick = false;
}

ASibeliusHUD* ASequenceCue::GetSibeliusHUD() const
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void ASequenceCue::BeginPlay()
{
	Super::BeginPlay();

	if (!bPlayOnBeginPlay)
	{
		return;
	}

	// Once per session, the AAIApparition bIntroPlayed contract, per cue.
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
			{
				if (Progress->PlayedVideoCues.Contains(CueId))
				{
					/* Already seen it this session - do not make them watch it again on
					   the way back. Travel straight through, because this level is a
					   stage set with no game in it; stopping here would strand them in
					   a black room. */
					UE_LOG(LogSibeliusGame, Display,
						TEXT("[SequenceCue] '%s' already played — travelling straight on."),
						*CueId.ToString());
					TravelOnward();
					return;
				}
			}
		}
	}

	// BLACK BEFORE ANYTHING RENDERS. Set here rather than in PlayCue because the whole
	// problem is the half second BEFORE PlayCue runs. Deliberately after the
	// already-played check above: that path travels straight on and must not be dimmed.
	FadeScreen(1.0f, 1.0f, 0.0f);

	GetWorldTimerManager().SetTimer(
		StartTimer, this, &ASequenceCue::PlayCue, FMath::Max(0.01f, StartDelay), false);
}

void ASequenceCue::PlayCue()
{
	UWorld* World = GetWorld();
	if (!World || bPlaying || bFinished)
	{
		return;
	}

	ULevelSequence* Seq = Sequence.LoadSynchronous();
	if (!Seq)
	{
		/* AP3 again: a missing cutscene loses the moment, never the game. And here it
		   matters more than usual - this level has nothing else in it, so failing to
		   travel would leave the player in an empty black room with no way out. */
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[SequenceCue] '%s' has no sequence to play — travelling on."),
			*CueId.ToString());
		FinishCue();
		return;
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableLookAtInput = true;
	Settings.bDisableMovementInput = true;
	Settings.bHidePlayer = true;
	Settings.bHideHud = true;

	// CreateLevelSequencePlayer wants a raw pointer reference, not a TObjectPtr.
	ALevelSequenceActor* SpawnedActor = nullptr;
	Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Seq, Settings, SpawnedActor);
	PlayerActor = SpawnedActor;
	if (!Player)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[SequenceCue] '%s' could not create a sequence player — travelling on."),
			*CueId.ToString());
		FinishCue();
		return;
	}

	Player->OnFinished.AddDynamic(this, &ASequenceCue::HandleSequenceFinished);

	bPlaying = true;

	// Belt and braces over Settings.bHideHud: HoldCinematic is what the rest of this
	// project uses to silence the HUD, and it also covers a HUD that is not ours.
	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		HUD->HoldCinematic(MaxSeconds + 5.0f);
	}

	LockPlayer(true);
	Player->Play();

	// AFTER Play(), never before: by now the camera cut has happened and she is in her
	// sequence transform, so what fades up is the shot rather than the snap into it.
	FadeScreen(1.0f, 0.0f, FadeInSeconds);

	GetWorldTimerManager().SetTimer(
		SafetyTimer, this, &ASequenceCue::FinishCue, MaxSeconds, false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[SequenceCue] '%s' playing %s"),
		*CueId.ToString(), *Seq->GetName());
}

void ASequenceCue::HandleSequenceFinished()
{
	FinishCue();
}

void ASequenceCue::HandleSkip()
{
	if (bPlaying && bSkippable)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[SequenceCue] '%s' skipped"), *CueId.ToString());
		FinishCue();
	}
}

void ASequenceCue::FinishCue()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyTimer);
		World->GetTimerManager().ClearTimer(StartTimer);
	}

	// Exactly once. OnFinished, a skip and the safety timer can all arrive, and two
	// OpenLevel calls in one frame is not something to find out about in a shipped build.
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	/* LET THE SCREEN GO, whatever brought us here. This is the funnel every ending uses
	   - OnFinished, a skip, the safety watchdog, a missing sequence, a player that could
	   not be created - so clearing the fade once here covers all five. The alternative
	   is a failure mode where a cutscene that breaks leaves the player in the dark,
	   which is strictly worse than the flash this change was made to remove. */
	FadeScreen(0.0f, 0.0f, 0.0f);
	bPlaying = false;

	if (Player)
	{
		Player->OnFinished.RemoveDynamic(this, &ASequenceCue::HandleSequenceFinished);
		Player->Stop();
	}

	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		HUD->ReleaseCinematic();
	}
	LockPlayer(false);

	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
			{
				Progress->PlayedVideoCues.Add(CueId);
			}
		}
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[SequenceCue] '%s' finished"), *CueId.ToString());
	TravelOnward();
}

void ASequenceCue::TravelOnward()
{
	if (NextLevel.IsNone() || NextLevel.ToString().IsEmpty())
	{
		return;
	}

	// The animated loading screen is registered globally on PreLoadMap in
	// SibeliusGame.cpp, so a plain OpenLevel is already covered.
	UE_LOG(LogSibeliusGame, Display, TEXT("[SequenceCue] travelling to %s"), *NextLevel.ToString());
	UGameplayStatics::OpenLevel(this, NextLevel);
}

/* THE ONE PLACE THE SCREEN IS DARKENED OR LET GO.

   Everything goes through here so no exit path can leave a player staring at black:
   FinishCue calls it with 0, and FinishCue is the single funnel every ending already
   uses - natural finish, skip, safety watchdog, missing sequence, and a player that
   failed to create. */
void ASequenceCue::FadeScreen(float From, float To, float Duration)
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager : nullptr;
	if (!Cam)
	{
		return;
	}

	if (Duration <= 0.0f)
	{
		// Instant, and HELD - StartCameraFade with a zero duration does not reliably
		// stick, and this is the call that has to be trusted before the first frame.
		Cam->SetManualCameraFade(To, FLinearColor::Black, /*bInFadeAudio=*/false);
		return;
	}
	Cam->StartCameraFade(From, To, Duration, FLinearColor::Black,
		/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
}

void ASequenceCue::LockPlayer(bool bLock)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		bInputLocked = bLock;
		return;
	}

	if (bLock && !bInputLocked)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		bInputLocked = true;

		if (bSkippable)
		{
			// Movement and look are ignored; action bindings still fire, which is how a
			// frozen player can still say "enough". BindKey consumes, and during a
			// cutscene that is exactly right.
			EnableInput(PC);
			if (InputComponent)
			{
				InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASequenceCue::HandleSkip);
				InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ASequenceCue::HandleSkip);
				InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASequenceCue::HandleSkip);
				InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASequenceCue::HandleSkip);
			}
		}
	}
	else if (!bLock && bInputLocked)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		bInputLocked = false;
		DisableInput(PC);
	}
}

void ASequenceCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave the player frozen, whatever tore this down.
	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		HUD->ReleaseCinematic();
	}
	LockPlayer(false);
	Super::EndPlay(EndPlayReason);
}
