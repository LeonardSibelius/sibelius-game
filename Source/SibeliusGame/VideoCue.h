// VideoCue.h
//
// A PRE-RENDERED CUTSCENE, PLAYED FULL-SCREEN (2026-08-25, docs/CINEMATICS.md).
//
// Built for Kaia's opening — she introduces herself and invites the player upstairs —
// but it is deliberately generic: one actor, one filename, any moment in the game.
// Drop another one in a level, point it at another mp4, and that is a new cutscene.
//
// WHAT IT DOES WHILE IT PLAYS
//   - draws the video full-screen through the HUD canvas (see the note below)
//   - blanks every other HUD layer via ASibeliusHUD::HoldCinematic
//   - holds movement and look
//   - listens for Space / Enter / Escape / gamepad A so it can be skipped
//
// NO WIDGET, NO MATERIAL, NO UMG ASSET. The obvious way to show video in UE is a
// UMG Image fed by a material that samples a Media Texture — which means authoring a
// material asset AND a widget asset before a single frame appears. AHUD::DrawTexture
// takes a UTexture, and UMediaTexture IS a UTexture, so the HUD can draw the video
// directly. ASibeliusHUD was already the thing suppressing the rest of the HUD for
// close-ups; letting it also draw the video keeps the whole feature in C++.
//
// THE FILE IS NOT IN THE PAK. Video is read from disk at runtime:
//
//     <Project>/Movies/<VideoFileName>
//
// so DefaultGame.ini stages "Movies" as NonUFS. Miss that and the cutscene plays
// perfectly in PIE and is silently absent from the shipped build — the same shape as
// the v0.7.4 soft-ref miss and the gitignored dancer audio. A missing file is not a
// crash: it logs the path it wanted and hands control straight back.
//
// ONCE PER SESSION, not per save. USibeliusProgressSubsystem is session-only by
// design (see its header) and that is the same contract AAIApparition's bIntroPlayed
// uses for the office bang. Matching it means no save-format change.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VideoCue.generated.h"

class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;

UCLASS()
class SIBELIUSGAME_API AVideoCue : public AActor
{
	GENERATED_BODY()

public:
	AVideoCue();

	/** File under <Project>/Movies/. Extension included. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue")
	FString VideoFileName = TEXT("kaia_intro.mp4");

	/** Identity for the once-per-session check. Two cues must not share one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue")
	FName CueId = TEXT("KaiaIntro");

	/** Fire on BeginPlay. Off = something else calls PlayCue(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue")
	bool bPlayOnBeginPlay = true;

	/**
	 * Beat of ordinary game before it starts. The apparition uses 3 s of frumpy office
	 * before its bang; this is shorter because the screen goes black either way and a
	 * long wait just reads as a hitch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue", meta = (ClampMin = "0.0"))
	float StartDelay = 0.75f;

	/** Space / Enter / Escape / gamepad A end it early. Leave this on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue")
	bool bSkippable = true;

	/**
	 * Hard stop if the player never reports finishing.
	 *
	 * OnEndReached is not guaranteed — a codec that will not open, a file truncated by
	 * a half-finished copy, a platform that reports nothing. Without this the player
	 * sits looking at a black screen with movement locked, which is the worst failure
	 * this class could have. Generous, then let go regardless.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Cue", meta = (ClampMin = "1.0"))
	float MaxSeconds = 60.0f;

	UFUNCTION(BlueprintCallable, Category = "Video Cue")
	void PlayCue();

	/** End it now — from the skip key, the end of the video, or the safety timer. */
	UFUNCTION(BlueprintCallable, Category = "Video Cue")
	void StopCue();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	/** What the HUD draws. Created at runtime, so no Media Texture asset is needed. */
	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> MediaTexture;

	UPROPERTY(VisibleAnywhere, Category = "Video Cue")
	TObjectPtr<UMediaSoundComponent> MediaSound;

	UFUNCTION()
	void HandleMediaEnd();

	void HandleSkip();

	/** Absolute path to the file, or empty if it is not there. */
	FString ResolveVideoPath() const;

	class ASibeliusHUD* GetSibeliusHUD() const;
	void LockPlayer(bool bLock);

	bool bPlaying = false;
	bool bInputLocked = false;

	FTimerHandle StartTimer;
	FTimerHandle SafetyTimer;
};
