// PowerGrant.h
//
// FUN-1 — the placeable "earn a power" moment: a shrine the player walks into.
// On overlap it unlocks its EPowerVerb (via UProgressionSubsystem), pays a Sauce
// reward, plays a grant sound, and removes itself. One-time across SESSIONS: the
// claim is recorded in the progression save under GrantKey, and an already-
// claimed shrine destroys itself on BeginPlay — so revisits stay clean.
//
// Also works as a sauce-only reward (bGrantsPower = false) for chapter-end
// bonuses. Place one per chapter at the spot where the chapter's verb should be
// bestowed; Walt assigns the mesh in the editor (any mesh reads fine — it spins
// and bobs so it scans as a pickup).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgressionTypes.h"
#include "PowerGrant.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;

UCLASS()
class SIBELIUSGAME_API APowerGrant : public AActor
{
	GENERATED_BODY()

public:
	APowerGrant();

	// Which verb this shrine bestows (when bGrantsPower).
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	EPowerVerb Power = EPowerVerb::Refactor;

	/**
	 * GIVEN BY AN AI AGENT INSTEAD OF A FLOATING SPHERE.
	 *
	 * Point this at a dancer and she becomes the way this power is obtained: the pole and
	 * its glow are hidden, the overlap trigger is switched off, and pressing E on her opens
	 * the trial. Leave it unset and the shrine behaves exactly as it always has.
	 *
	 * Walt, 2026-08-19: "Can the dancing girls be the ones that give you powers when you
	 * approach them? They are supposed to be AI agents after all."
	 *
	 * It is the right answer for more than the looks. The powers ARE AI assistance made
	 * literal (docs/NARRATIVE.md), Mrs. Hall's whole position is that a senior developer
	 * should not need a machine's help, and the dancers already introduce themselves as
	 * "AI Agent <name>". Taking a forbidden capability from an AI agent IS the story. An
	 * unlabelled sphere on a pole says nothing, and ambushes the player on a staircase.
	 *
	 * The reference lives HERE rather than on the dancer because UDancerAgentComponent is
	 * attached at runtime by a scan (see DancerAgentSubsystem.h) — it does not exist in the
	 * level, so it cannot carry an editor-set property. This actor is placed, so it can.
	 */
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	TObjectPtr<AActor> GrantedByAgent;

	/**
	 * How far above or below this shrine a player may be and still trip it.
	 *
	 * A 110cm trigger on a shrine standing a metre off the ground reaches through the floor
	 * beneath it. The library's COMPILE shrine sits at Z=420 with the staircase running
	 * underneath: a player climbing at Z=213 has a capsule reaching ~309, against a trigger
	 * whose underside is at 310, and the library's reward opened on the stairs — rooms
	 * before it was meant to be met, and looking for all the world like the wrong shrine
	 * had been placed there.
	 *
	 * 140 comfortably separates "standing in front of it" (a few units of difference) from
	 * "on the storey below" (200+).
	 */
	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "0"))
	float SameFloorTolerance = 140.0f;

	/** Open the trial from outside — the agent's E press. Safe to call when spent. */
	void RequestTrial(class APlayerController* PC);

private:
	/** Hand this power's interface to GrantedByAgent, and stand the pole down. Retries:
	 *  the dancer's component is attached by a runtime scan and may not exist yet. */
	void BindToAgent();

	/** Hide the pole and kill its overlap trigger. Called the moment an agent is
	 *  DESIGNATED, not when she is found — her component arrives on a 5s scan cycle, and
	 *  waiting for it left the sphere live and ambushing for the first seconds of play. */
	void StandDown();

	FString GetActorLabelSafe() const;   // labels are editor-only; logs still need a name

	FTimerHandle AgentBindTimer;
	int32 AgentBindAttempts = 0;

public:

	// False = a sauce-only reward marker (chapter-end bonus, hidden stash).
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	bool bGrantsPower = true;

	// Sauce paid alongside the grant. The unified currency arrives WITH the
	// power so the player meets both systems in the same beat.
	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "0"))
	int32 SauceReward = 25;

	// One-time claim key recorded in the progression save. None = derived:
	// the verb name when bGrantsPower, else this actor's level name — override
	// only if two shrines must share a claim.
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	FName GrantKey;

	UPROPERTY(EditAnywhere, Category = "Power Grant")
	TObjectPtr<USoundBase> GrantSound;

	// THE TRIAL (Walt: more should happen than walking in): stepping into a
	// power shrine pops the native Celestial Fortune with a trial stake; reach
	// the target and the power yields. Bust or Esc = step out and back in for
	// a fresh stake. Sauce-only markers never demand a trial.
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	bool bSlotTrial = true;

	// Trial odds (measured, 100k-attempt sim vs the real par sheet, 2026-07-17):
	// 2100 -> 2250 wins ~62% of attempts (~18 spins each) — one good line hit
	// usually seals it. History: 750 -> 1500 was ~24% (5-bet stake = gambler's
	// ruin), 1200 -> 1500 was ~43% (Walt still kept losing). Raw stake barely
	// helps past ~50% (the house edge eats long sessions); the generosity lever
	// is a SHORT climb on a comfortable bankroll.
	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "150"))
	int64 TrialStartCredits = 2100;

	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "150"))
	int64 TrialTargetCredits = 2250;

	// Walt: Gideon appears when Celestial Fortune pops up. Opening the trial
	// rings the hall alarm — any RefuserSpawner in the level answers. They
	// cannot interrupt the reels; they CAN be waiting when you stand up.
	UPROPERTY(EditAnywhere, Category = "Power Grant")
	bool bSummonRefusersOnTrial = true;

	UPROPERTY(VisibleAnywhere, Category = "Power Grant")
	TObjectPtr<USphereComponent> Trigger;

	// Assign in the editor; any mesh works. Spins + bobs (see Tick).
	UPROPERTY(VisibleAnywhere, Category = "Power Grant")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// The shrine glow (Walt: like the curio beacons — he stood two meters from
	// the DEPLOY shrine and couldn't see it). A room-height light pillar +
	// point light, cyan for power shrines, gold for sauce-only markers.
	UPROPERTY(VisibleAnywhere, Category = "Power Grant")
	TObjectPtr<UStaticMeshComponent> BeaconMesh;

	UPROPERTY(VisibleAnywhere, Category = "Power Grant")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditAnywhere, Category = "Power Grant")
	bool bShowBeacon = true;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	FName EffectiveGrantKey() const;

	void OpenTrial(class APlayerController* PC);
	void HandleTrialWon();
	void FinishTrialClaim();   // deferred so THE MACHINE YIELDS moment lands
	void HandleTrialClosed();
	void CloseTrialWidget();
	void ClaimNow();   // the original walk-in grant, now the trial's prize

	UPROPERTY()
	TObjectPtr<class USlotScreenWidget> TrialWidget;

	bool bTrialOpen = false;

	float BobPhase = 0.0f;
	FVector RestLocation = FVector::ZeroVector; // mesh rest point the bob oscillates around
	bool bConsumed = false;                     // re-entrancy guard (BookPickup's C3 lesson)
};
