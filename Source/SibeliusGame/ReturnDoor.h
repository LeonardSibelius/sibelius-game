// ReturnDoor.h
//
// THE SAUCE DOOR — the way home from an Elsewhere (SIB-47, §3 step 6). E to leave:
// it discards the staged plan (the room is throwaway — §4) and travels back to the
// house. The Elsewhere level unloads on travel, so the generated geometry is gone
// for free; DiscardStagedElsewhere just clears the in-memory staging.
//
// The builder spawns one of these in every Elsewhere so there's always a way back.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ReturnDoor.generated.h"

class UStaticMeshComponent;
class UTexture2D;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class SIBELIUSGAME_API AReturnDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AReturnDoor();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	// Build the way-home sign plaque, exactly as AHiddenDoor does for the kitchen "Many Worlds"
	// sign: an engine Plane + the masked unlit M_fate_base material (Sprite/Glow params) via a MID,
	// shown only when SignTexture is set. The texture is loaded by path in the ctor (this door is
	// runtime-spawned by AElsewhereBuilder, so there is no placed actor to assign it on — its cook
	// story is /Game/Signs being in DirectoriesToAlwaysCook).
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// --- Self-contained walk-through return (SIB Forest) -----------------------
	// The cathedral's AElsewhereBuilder spawns this door AND seats its own ReturnTrigger, so this
	// door's trigger stays OFF there (default false). The hand-placed FOREST door (a pre-made level,
	// no builder) sets this true, so walking into the door returns home — the same armed Pawn-overlap
	// mechanism as the cathedral, just owned by the door instead of the builder.
	UPROPERTY(EditAnywhere, Category = "Return Door")
	bool bSelfReturnTrigger = false;

	// Armed this many seconds after BeginPlay so arriving AT the door can't instant-return.
	UPROPERTY(EditAnywhere, Category = "Return Door", meta = (ClampMin = "0.05"))
	float ReturnArmDelay = 0.6f;

	UPROPERTY(VisibleAnywhere, Category = "Return Door")
	TObjectPtr<UBoxComponent> ReturnTrigger;

	UFUNCTION()
	void OnReturnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Where "home" is — the house with the Cabinet. Defaults to the office/house map.
	UPROPERTY(EditAnywhere, Category = "Return Door")
	FName HomeLevelName = TEXT("L_Office_v02");

	// ASCII "<-" arrow on purpose: the project builds warnings-as-errors and a non-ASCII glyph
	// in a source literal trips MSVC C4566. The warm beacon at the door carries the visual cue.
	UPROPERTY(EditAnywhere, Category = "Return Door")
	FText ReturnPromptText = NSLOCTEXT("Sibelius", "ReturnDoorPrompt", "<- Back to the kitchen  [E]");

	UPROPERTY(VisibleAnywhere, Category = "Return Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// --- The way-home sign (mirrors AHiddenDoor's sign block) -----------------
	// The plaque texture (Walt's art at /Game/Signs/T_Sign_TheWayHome, committed + auto-cooked).
	// Loaded by path in the ctor; null on a fresh clone before import -> the sign is simply skipped.
	UPROPERTY(EditAnywhere, Category = "Return Door|Sign")
	TObjectPtr<UTexture2D> SignTexture;

	// Sign placement relative to the door mesh. Defaults are a headless best-guess for the kit door
	// frame (SM_Door_A_3x4m_Base, ~3 m wide x 4 m tall) — the exact look is Walt's PIE finalize.
	UPROPERTY(EditAnywhere, Category = "Return Door|Sign")
	FVector SignRelativeLocation = FVector(18.0f, 0.0f, 280.0f);

	// FRotator(Pitch, Yaw, Roll). Mirrors the kitchen sign's working orientation flipped to the
	// room-facing (+X) door face. Nudge in PIE; never recompile to find the angle.
	UPROPERTY(EditAnywhere, Category = "Return Door|Sign")
	FRotator SignRelativeRotation = FRotator(0.0f, -90.0f, 90.0f);

	UPROPERTY(EditAnywhere, Category = "Return Door|Sign")
	float SignWidth = 320.0f;   // cm; art is 1.6:1 (1024x640)

	// 0 = automatic (width/2). A value stretches the plaque vertically. 200 ~ matches the 1.6:1 art.
	UPROPERTY(EditAnywhere, Category = "Return Door|Sign")
	float SignHeight = 200.0f;

	UPROPERTY(VisibleAnywhere, Category = "Return Door|Sign")
	TObjectPtr<UStaticMeshComponent> SignMesh;

private:
	// Discard the staged plan + travel to HomeLevelName. Shared by E-interact and the overlap.
	void GoHome();

	bool bReturningHome = false;   // re-entry guard (OpenLevel is async)
	bool bReturnArmed = false;     // set true after ReturnArmDelay
};
