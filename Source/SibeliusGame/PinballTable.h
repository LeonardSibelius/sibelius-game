// PinballTable.h
//
// PINBALL — the machine in the living room (Walt, 2026-09-27).
//
// The Singalong cabinet from Speedbump Games' Retro Pinball Machines (1960s-70s
// electromechanical, "Generated with AI: No"), standing in the space where the couch and
// cabinets were. Visible and playable from the first frame: no Code Vision reveal, no
// unlock. Free play; nothing rides on it.
//
// ===========================================================================
// THE MODEL DECIDES, THIS ACTOR SHOWS. FPinballSim owns the ball, the flippers, the
// collisions and the score (PinballSim.h, tested standalone by Tools/Tests). This actor:
//   - draws the sim: a chrome ball, two flippers and three bumper caps, placed from
//     Sim.Layout() on the Playfield frame, so the art and the physics cannot drift apart
//   - hides the cabinet's own MODELLED flippers (its M_Flipper material slot) under ours
//   - takes the player's controls while he plays, and gives them back however it ends
//   - shows the score on the backglass, the way a machine of that era would
//
// THE PLAYFIELD FRAME is the one thing that has to be aligned by eye in the editor. Its
// origin is the bottom-left corner of the cabinet's playfield, its +X points UP the table
// (away from the player), its +Y points across to the right, and its pitch is the table's
// tilt. Tick bDrawPhysicsLayoutInEditor and the sim's walls, bumpers and flippers are drawn
// in the viewport; move and rotate the Playfield component until they sit on the art.
// Table coordinates (X across, Y up) map to the frame as (Y, X): see TableToLocal.
//
// HOUSE LESSONS CARRIED OVER, each of which cost a playtest somewhere else:
//   - EndPlay restores input and the view target (APokerMachine, 2026-09-07: a level
//     travelled with the screen up and left him in a game that ignored him).
//   - Components are built in the constructor, so it can be SEEN and placed in the editor
//     (APokerMachine's OnConstruction note).
//   - The cabinet mesh is a HARD reference (FObjectFinder), or it can be missing from the
//     pak while working in PIE (the v0.7.4 invisible spaceport).
//   - Interaction goes through IInteractable only. No distance-bound E: that is how E on
//     the cathedral door once opened the poker screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "PinballSim.h"
#include "TableSounds.h"
#include "PinballTable.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class SIBELIUSGAME_API APinballTable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APinballTable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	bool IsPlaying() const { return bPlaying; }

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The Singalong cabinet. Visual only; Body does the collision. */
	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UStaticMeshComponent> Cabinet;

	/** What the player bumps into and what the interaction trace lands on (BlockAll, SC9).
	    Size it to the cabinet in the editor. */
	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UBoxComponent> Body;

	/** The table plane. Align this to the cabinet's playfield art — see the header. */
	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<USceneComponent> Playfield;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<USceneComponent> LeftFlipperPivot;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<USceneComponent> RightFlipperPivot;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UStaticMeshComponent> LeftFlipper;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UStaticMeshComponent> RightFlipper;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TArray<TObjectPtr<UStaticMeshComponent>> BumperCaps;

	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TArray<TObjectPtr<UPointLightComponent>> BumperLights;

	/** Score, ball and game-over, on the backglass. Place it on the glass in the editor. */
	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UTextRenderComponent> ScoreText;

	/** Where he stands to play: at the lower end, looking down the table. */
	UPROPERTY(VisibleAnywhere, Category = "Pinball")
	TObjectPtr<UCameraComponent> TableCamera;

	// --- tuning -------------------------------------------------------------------------

	/** Put on every cabinet slot whose material is named HideMaterialsNamed, so the
	    cabinet's modelled static flippers vanish under the moving ones. Defaults to the
	    engine's NaniteHiddenSectionMaterial. If the static flippers still show in the
	    editor, make a Masked material with Opacity Mask 0 and put it here. */
	UPROPERTY(EditAnywhere, Category = "Pinball")
	TObjectPtr<UMaterialInterface> HiddenFlipperMaterial;

	UPROPERTY(EditAnywhere, Category = "Pinball")
	FString HideMaterialsNamed = TEXT("M_Flipper");

	/** Draw the sim's walls, bumpers, rollovers and flippers in the editor viewport, for
	    aligning the Playfield frame to the art. Never drawn in play unless the next is on. */
	UPROPERTY(EditAnywhere, Category = "Pinball")
	bool bDrawPhysicsLayoutInEditor = true;

	UPROPERTY(EditAnywhere, Category = "Pinball")
	bool bDrawPhysicsLayoutInGame = false;

	/** Holding the plunger key this long is a full pull. */
	UPROPERTY(EditAnywhere, Category = "Pinball", meta = (ClampMin = "0.1"))
	float PlungerChargeSeconds = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Pinball", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Pinball", meta = (ClampMin = "0.0"))
	float BumperFlashIntensity = 8.0f;   // candelas: the lights sit a few cm under the caps

	/** Draw our own simple bumper caps. Off for the Singalong, whose mesh has its own
	    modelled caps; the flash lights stay either way. */
	UPROPERTY(EditAnywhere, Category = "Pinball")
	bool bShowBumperCaps = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindTableKeys();

	void OnLeftFlipperPressed();
	void OnLeftFlipperReleased();
	void OnRightFlipperPressed();
	void OnRightFlipperReleased();
	void OnPlungerPressed();
	void OnPlungerReleased();
	void OnLeave();

	void SyncVisuals(float DeltaSeconds);
	void HandleEvents();

	// Sound. An EM machine's score motor rings one chime per digit, a beat apart, so a
	// score is QUEUED rather than played: fifty points is five rings. See TableSounds.h.
	struct FQueuedSound
	{
		double At = 0.0;   // world seconds
		ETableSound Sound = ETableSound::Chime10;
		float Volume = 1.0f;
	};
	void QueueSound(ETableSound Sound, float Volume, double Delay);
	void QueueScoreChimes(int32 Points);
	void PlayQueuedSounds();
	TArray<FQueuedSound> SoundQueue;
	double ChimeFreeAt = 0.0;   // when the score motor is next free
	void RefreshScoreText();
	void ApplyHiddenFlipperMaterial();
	void DrawPhysicsLayout() const;

	/** Table coordinates (cm; X across, Y up the table) to Playfield-relative space. */
	static FVector TableToLocal(FPinVec2 P, double Height);

	ASibeliusHUD* GetHud() const;

	FPinballSim Sim;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;
	bool bPlungerHeld = false;
	float PlungerHeldSeconds = 0.0f;

	int32 HighScore = 0;   // this session; a saved high score is a later job
	TArray<float> BumperFlash;

	// What the backglass last showed, so the text is only rebuilt when it changes.
	int32 ShownScore = -1;
	int32 ShownBall = -1;
	EPinballPhase ShownPhase = EPinballPhase::GameOver;
	bool bScoreTextValid = false;
};
