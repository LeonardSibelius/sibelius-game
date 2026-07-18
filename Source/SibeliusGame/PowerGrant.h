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

	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "150"))
	int64 TrialStartCredits = 750;

	UPROPERTY(EditAnywhere, Category = "Power Grant", meta = (ClampMin = "150"))
	int64 TrialTargetCredits = 1500;

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
