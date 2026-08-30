// SequenceCue.h
//
// A CUTSCENE PLAYED LIVE BY THE ENGINE, then travel onward (docs/CINEMATICS.md).
//
// Kaia's opening: the game boots into L_Cine_KaiaIntro - a level containing nothing but
// her, three lights and a camera against pure black - plays LS_Kaia_Intro, and travels
// to the office when she finishes or the player skips.
//
// WHY NOT THE PRE-RENDERED VIDEO. We built that first (AVideoCue) and the video half
// never reached the screen: Electra decoded all 26 seconds correctly, but the
// UMediaTexture created at runtime reported surface=0x0 - it never received a frame.
// Rather than keep fighting the media stack, this plays the Level Sequence the engine
// already renders perfectly. Walt watched it work in Sequencer before we ever rendered
// an mp4.
//
// WHAT THAT BUYS
//   - no media player, no media texture, no codec, no HUD blitting
//   - no mp4 on disk and no NonUFS staging trap
//   - 6 MB off the download
//   - renders at the PLAYER's resolution instead of upscaling a 1080p file
//
// WHAT IT COSTS
//   - a level load between the cutscene and the game. Harmless here: the loading screen
//     is already registered globally on PreLoadMap (SibeliusGame.cpp), so OpenLevel is
//     covered without this class knowing anything about it.
//
// AVideoCue is kept. It is finished apart from that one texture, it is the right shape
// for a cutscene that is NOT engine-rendered (a HeyGen clip, archive footage), and the
// day a runtime MediaTexture behaves it is a one-line fix.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SequenceCue.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;

UCLASS()
class SIBELIUSGAME_API ASequenceCue : public AActor
{
	GENERATED_BODY()

public:
	ASequenceCue();

	/** The cutscene. Its camera cut track takes the view for the duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue")
	TSoftObjectPtr<ULevelSequence> Sequence;

	/**
	 * Where to go when it ends. Empty = stay put (for a cutscene inside a level the
	 * player is already standing in).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue")
	FName NextLevel = TEXT("L_Office_v02");

	/** Identity for the once-per-session check. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue")
	FName CueId = TEXT("KaiaIntro");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue")
	bool bPlayOnBeginPlay = true;

	/* HOW LONG HER FACE TAKES TO ARRIVE.

	   Walt: "for a fraction of a second at the start of the game, before Kaia speaks,
	   there is a small image of her body launching straight up."

	   That is StartDelay rendering. For half a second before Play() is called the level
	   draws itself normally - Kaia standing wherever the level left her, at whatever
	   pose her animation begins on - and then the sequence takes over and SNAPS her into
	   its own transform and cuts to its own camera. The snap is the launch. Nothing is
	   broken; the cutscene simply has not started yet and the camera is already looking.

	   So the screen is held black from BeginPlay and faded up only once the sequence is
	   actually playing. That both hides the snap and gives her the slow arrival Walt
	   asked for - which is a better opening than a hard cut anyway, because the first
	   thing this game does is a face in the dark.

	   Visual only: audio is deliberately NOT faded. Her first line is lip-synced to
	   within 0.6 s and muffling the front of it to save a picture would be a poor trade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 1.5f;

	/** A breath before it starts, so the level is fully up and not hitching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue", meta = (ClampMin = "0.0"))
	float StartDelay = 0.5f;

	/** Space / Enter / Escape / gamepad A end it early. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue")
	bool bSkippable = true;

	/**
	 * Hard stop if OnFinished never arrives.
	 *
	 * Same reasoning as AVideoCue's MaxSeconds, and it earned its keep there: when the
	 * video silently failed, this timer is what gave Walt his controls back. A cutscene
	 * that can strand the player is worse than one that ends early.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Cue", meta = (ClampMin = "1.0"))
	float MaxSeconds = 90.0f;

	UFUNCTION(BlueprintCallable, Category = "Sequence Cue")
	void PlayCue();

	/** End the shot and move on - from the end of the sequence, a skip, or the timer. */
	UFUNCTION(BlueprintCallable, Category = "Sequence Cue")
	void FinishCue();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> Player;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> PlayerActor;

	UFUNCTION()
	void HandleSequenceFinished();

	void HandleSkip();
	/** Hold or release a full-screen black fade. Duration 0 sets it instantly and holds. */
	void FadeScreen(float From, float To, float Duration);

	void LockPlayer(bool bLock);
	class ASibeliusHUD* GetSibeliusHUD() const;

	/** Travel, or stay if NextLevel is empty. Called exactly once. */
	void TravelOnward();

	bool bPlaying = false;
	bool bFinished = false;
	bool bInputLocked = false;

	FTimerHandle StartTimer;
	FTimerHandle SafetyTimer;
};
