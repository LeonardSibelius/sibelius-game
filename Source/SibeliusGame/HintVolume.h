// HintVolume.h — a place that tells you what it is for.
//
// ===========================================================================
// WHY THIS EXISTS (Walt, 2026-09-02).
//
// He walked to the lawn, pressed G, and typed "spaceship". The catalog only knew
// "spaceport", so Mrs. Hall refused him — and his verdict was exact:
//
//     "There is no way the player would know that."
//
// Two things were wrong and both are fixed. The keyword list is now generous
// (spaceship, spacecraft, rocketship, nasa, shuttle, capsule, liftoff...), because a
// closed catalog matched by keyword is only as good as its synonyms and that is a DATA
// problem, not a player problem.
//
// But widening keywords still leaves a player standing on grass with no idea that this
// particular grass is special. Nyra tells him to Generate here; nothing tells him WHEN he
// has arrived. So: a volume that says so, once he is standing in it.
//
// ---------------------------------------------------------------------------
// IT STOPS TALKING ONCE THE JOB IS DONE, and needs no save flag to manage that.
//
// The hint hides itself when an ASpaceport already exists in the world. That is the real
// condition — not "has been shown once", which would fail the player who wandered off and
// came back, and not a saved bool, which would be a new field in FSibeliusSave for
// something the world can simply be asked.
//
// ---------------------------------------------------------------------------
// PLACING ONE. Drag it onto the lawn in L_City and set Radius so it covers the grass.
// The sphere is drawn in the editor and invisible in game.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HintVolume.generated.h"

class USphereComponent;

UCLASS()
class SIBELIUSGAME_API AHintVolume : public AActor
{
	GENERATED_BODY()

public:
	AHintVolume();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void OnPlayerEnter(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& Sweep);

	/* THE OTHER HALF, and forgetting it is how a hint becomes a one-shot. Without an
	   exit that clears bInside, the volume fires the first time and is mute forever
	   after — which is precisely the player this was written for: the one who walked
	   away, forgot the word, and came back. */
	UFUNCTION()
	void OnPlayerLeave(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Hint")
	TObjectPtr<USphereComponent> Trigger;

	/** How far out the hint reaches. Editable live — drag it to cover the grass. */
	UPROPERTY(EditAnywhere, Category = "Hint", meta = (ClampMin = "50.0"))
	float Radius = 900.0f;

	/** What it says. The word the player has to type belongs IN this sentence. */
	UPROPERTY(EditAnywhere, Category = "Hint")
	FString Line = TEXT("This ground is clear.  Press G and generate a spaceport.");

	/* SILENT UNTIL NYRA HAS SENT HIM. Defaults to the deli grant, so the lawn says
	   nothing to a player who has not yet been told the city is for building — he has
	   not met the idea, and a hint before the invitation is just noise on a lawn.
	   Set to None to make it unconditional. */
	UPROPERTY(EditAnywhere, Category = "Hint")
	FName RequiresGrant;

	/* THE CLASS WHOSE EXISTENCE ENDS THE HINT. Default ASpaceport: once one stands on
	   the lawn, the ground has stopped needing to explain itself. Leave None and the
	   hint keeps offering itself every time he walks back in. */
	UPROPERTY(EditAnywhere, Category = "Hint")
	TSubclassOf<AActor> SatisfiedWhenPresent;

	/** Seconds the message holds. */
	UPROPERTY(EditAnywhere, Category = "Hint", meta = (ClampMin = "0.5"))
	float HoldSeconds = 5.0f;

	/* Re-arm distance is the trigger itself: leaving and re-entering shows it again,
	   which is the behaviour a player who forgot the word actually needs. This flag only
	   stops it firing repeatedly while he stands still and the volume re-overlaps. */
	bool bInside = false;
};
