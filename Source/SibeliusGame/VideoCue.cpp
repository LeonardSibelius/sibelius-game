// VideoCue.cpp — see the header.

#include "VideoCue.h"

#include "SibeliusHUD.h"
#include "SibeliusGame.h"                 // LogSibeliusGame
#include "SibeliusProgressSubsystem.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"

AVideoCue::AVideoCue()
{
	PrimaryActorTick.bCanEverTick = false;

	MediaSound = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("MediaSound"));
	RootComponent = MediaSound;

	/* A CUTSCENE IS NOT COMING FROM A PLACE. Spatialised, the voice would fade with
	   distance from whatever corner of the office this actor happens to sit in, and a
	   player who walked away before the cue fired would hear nothing. Same reasoning as
	   the dancers' 2D voice lines and the apparition's AP7 note: a voice that IS the
	   scene should not be subject to where its emitter stands. */
	MediaSound->bAllowSpatialization = false;
	MediaSound->bIsUISound = true;
}

FString AVideoCue::ResolveVideoPath() const
{
	if (VideoFileName.IsEmpty())
	{
		return FString();
	}

	// FPaths::ProjectDir resolves to the staged directory in a packaged build, so this
	// one expression works in the editor and in the shipped game.
	const FString Path = FPaths::ProjectDir() / TEXT("Movies") / VideoFileName;
	return IFileManager::Get().FileExists(*Path) ? Path : FString();
}

ASibeliusHUD* AVideoCue::GetSibeliusHUD() const
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void AVideoCue::BeginPlay()
{
	Super::BeginPlay();

	if (!bPlayOnBeginPlay)
	{
		return;
	}

	// Once per session. The apparition's bIntroPlayed contract, per cue.
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
			{
				if (Progress->PlayedVideoCues.Contains(CueId))
				{
					UE_LOG(LogSibeliusGame, Display,
						TEXT("[VideoCue] '%s' already played this session — staying quiet."),
						*CueId.ToString());
					return;
				}
			}
		}

		GetWorldTimerManager().SetTimer(
			StartTimer, this, &AVideoCue::PlayCue, FMath::Max(0.01f, StartDelay), false);
	}
}

void AVideoCue::PlayCue()
{
	UWorld* World = GetWorld();
	if (!World || bPlaying)
	{
		return;
	}

	const FString Path = ResolveVideoPath();
	if (Path.IsEmpty())
	{
		/* NOT A CRASH AND NOT A SOFT-LOCK. The apparition's AP3 rule: a missing asset
		   loses the moment, never the game. Name the exact path so the fix is obvious. */
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[VideoCue] '%s' has no video at %s — skipping the cutscene. Is 'Movies' "
			     "staged as NonUFS in DefaultGame.ini?"),
			*CueId.ToString(), *(FPaths::ProjectDir() / TEXT("Movies") / VideoFileName));
		return;
	}

	if (!MediaPlayer)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this, NAME_None, RF_Transient);
		MediaPlayer->OnEndReached.AddDynamic(this, &AVideoCue::HandleMediaEnd);
	}
	if (!MediaTexture)
	{
		MediaTexture = NewObject<UMediaTexture>(this, NAME_None, RF_Transient);
		MediaTexture->SetMediaPlayer(MediaPlayer);
		MediaTexture->UpdateResource();
	}
	MediaSound->SetMediaPlayer(MediaPlayer);

	MediaPlayer->SetLooping(false);
	if (!MediaPlayer->OpenFile(Path))
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[VideoCue] '%s' could not open %s — codec unsupported? Skipping."),
			*CueId.ToString(), *Path);
		return;
	}

	bPlaying = true;

	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		HUD->SetCinematicVideo(MediaTexture);
		// Generous: the release is explicit in StopCue. The lease only exists so a
		// destroyed actor or a stopped PIE cannot leave the HUD blank forever.
		HUD->HoldCinematic(MaxSeconds + 5.0f);
	}

	LockPlayer(true);

	GetWorldTimerManager().SetTimer(
		SafetyTimer, this, &AVideoCue::StopCue, MaxSeconds, false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[VideoCue] '%s' playing %s"),
		*CueId.ToString(), *VideoFileName);
}

void AVideoCue::HandleMediaEnd()
{
	StopCue();
}

void AVideoCue::HandleSkip()
{
	if (bPlaying && bSkippable)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[VideoCue] '%s' skipped"), *CueId.ToString());
		StopCue();
	}
}

void AVideoCue::StopCue()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyTimer);
		World->GetTimerManager().ClearTimer(StartTimer);
	}

	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;

	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	/* GIVE THE SCREEN BACK. Both halves, in one place, so a skip and a natural end
	   cannot disagree — the AP2 lesson: never two restore paths. */
	if (ASibeliusHUD* HUD = GetSibeliusHUD())
	{
		HUD->SetCinematicVideo(nullptr);
		HUD->ReleaseCinematic();
	}
	LockPlayer(false);

	// Mark it done only once it has actually run, so a missing file does not silently
	// consume the cue for the rest of the session.
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

	UE_LOG(LogSibeliusGame, Display, TEXT("[VideoCue] '%s' finished"), *CueId.ToString());
}

void AVideoCue::LockPlayer(bool bLock)
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
			/* Movement and look are ignored, but ACTION bindings still fire — which is
			   exactly what we want: frozen in place, still able to say "enough".
			   BindKey consumes by default and here that is correct; nothing else should
			   act on a keypress during a cutscene. */
			EnableInput(PC);
			if (InputComponent)
			{
				InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVideoCue::HandleSkip);
				InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AVideoCue::HandleSkip);
				InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AVideoCue::HandleSkip);
				InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AVideoCue::HandleSkip);
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

void AVideoCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Whatever else happens, the player gets their controls and their HUD back.
	StopCue();
	Super::EndPlay(EndPlayReason);
}
