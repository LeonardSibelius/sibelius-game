// SauceBowl.h — the temple's sauce fountain (Walt's ask: a more direct way to
// earn sauce in the AI temple than the one-time blend bounty).
//
// The ritual: [E] fills the bowl from the ladle (it glows green), [C] — the
// Compile key, deliberately — claims the sauce (+SaucePerBowl, default 40).
// Then the ladle drips for RechargeSeconds before the bowl can be filled
// again: a renewable faucet with a drip rate, not an infinite money hose
// (Walt designed slot machines; he knows exactly why this knob exists).
//
// Built from engine shapes + the cathedral's marble so it costs the download
// nothing; all references are HARD (FObjectFinder) per the soft-refs-don't-
// cook rule. The C key rides the CarouselMachine pattern (EnableInput +
// BindKey) with a distance guard so a Compile pressed elsewhere in the temple
// never claims the bowl.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "SauceBowl.generated.h"

class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API ASauceBowl : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASauceBowl();

	// [E]: fill the bowl if the ladle has recharged.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0"))
	int32 SaucePerBowl = 40;

	// The drip rate: seconds after a claim before the bowl can fill again.
	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0.0"))
	float RechargeSeconds = 90.f;

	// [C] only claims when the player is at least this close (cm).
	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0.0"))
	float ClaimRadius = 350.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> TableMesh;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> BowlMesh;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> SauceMesh; // the green glow; hidden until filled

private:
	void TryEnableInput();
	void OnCompilePressed();          // [C]: claim the filled bowl
	bool IsLadleReady() const;

	bool bFilled = false;
	double LastClaimTime = -1.0e9;    // world seconds of the last claim
	int32 InputAttempts = 0;
	FTimerHandle InputRetryHandle;
};
