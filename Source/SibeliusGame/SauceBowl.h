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

	// [C]: called by the character's Compile verb (one verb, disambiguated by
	// state — a raw BindKey lost the argument with Enhanced Input and never
	// fired). Claims if filled and the claimer is within ClaimRadius.
	bool TryClaim(APawn* Claimer);

	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0"))
	int32 SaucePerBowl = 40;

	// The drip rate: seconds after a claim before the bowl can fill again.
	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0.0"))
	float RechargeSeconds = 90.f;

	// Walt's ritual grammar: E starts a short POUR (the stream falls for this
	// many seconds), then the pour stops and the sauce appears — full pot.
	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0.5"))
	float PourSeconds = 4.f;

	// [C] only claims when the player is at least this close (cm).
	UPROPERTY(EditAnywhere, Category = "SauceBowl", meta = (ClampMin = "0.0"))
	float ClaimRadius = 350.f;

	// Walt's risk-reward: filling the bowl RINGS THE ALARM — any RefuserSpawner
	// in the level answers. Earn the sauce by surviving the pour.
	UPROPERTY(EditAnywhere, Category = "SauceBowl")
	bool bSummonRefusersOnFill = true;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> TableMesh;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> BowlMesh;
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> SauceMesh;  // the green glow; hidden until filled
	UPROPERTY(VisibleAnywhere, Category = "SauceBowl") TObjectPtr<UStaticMeshComponent> StreamMesh; // the visible drip while recharging

private:
	bool IsLadleReady() const;
	void OnPourComplete();            // stream stops, sauce appears

	bool bPouring = false;
	bool bFilled = false;
	double LastClaimTime = -1.0e9;    // world seconds of the last claim
	FTimerHandle PourTimer;
};
