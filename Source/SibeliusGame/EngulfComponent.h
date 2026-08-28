// EngulfComponent.h — four hundred men who cannot hurt you, and the trouble that is.
//
// WHY THIS EXISTS, AND WHY IT IS NOT A HEALTH BAR.
//
// Walt, 2026-08-27, looking at 400 Refusers on the meadow: "Greystone and his five AI
// agents can slap all of those refusers - the refusers never do any damage, right?"
//
// Right. There is no damage anywhere in this game and no health system at all.
// ARefuserController::TryAttack plays a montage and a sound and returns. That is not an
// oversight — it is the joke, and it is Walt's own: Mrs. Hall's demonic enforcer "can be
// knocked down with one slap, so she is not so sinister after all." Her factory is
// comically bad, her authority is hollow, and her enforcer falls over when swatted.
//
// Giving the Architects teeth would fix the battle by throwing away the reason the
// battle is funny. So they keep none. The threat is not that they can hurt you.
//
// THE THREAT IS BEING ENGULFED. Not one of them has ever been on call. But four hundred
// BODIES can surround a man, slow him, pin him, and push him off his own ground. Each
// one is nothing. Together they are a wall of certainty, which is exactly what they were
// in every office Walt ever worked in.
//
// ---------------------------------------------------------------------------
// AND THE FAIL STATE IS NOT DEATH, BECAUSE DEATH IS THE WRONG WORD FOR THIS.
//
// Enough of them, long enough, and you are OVERRULED: the avatar drops and you are back
// to being a pair of eyes. They do not kill Leonard Sibelius. They outnumber him and put
// him back in his place — which is the thing that actually happened, for forty years,
// and no health bar has ever expressed it.
//
// ---------------------------------------------------------------------------
// TWO IMPLEMENTATION NOTES THAT ARE REALLY THE SAME NOTE.
//
// ONE SPHERE OVERLAP, NOT AN ACTOR ITERATION. With 400 Refusers in the level, walking
// every actor every frame to measure a crowd would be the navigation-invoker mistake
// again — a per-frame cost that scales with the army rather than with the crowd actually
// touching you. The overlap asks the physics scene, which already has the answer.
//
// AND IT TICKS AT 10 Hz, not every frame. A crowd does not close on you in 16 ms, and
// this runs in the office too, where the answer is always zero.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EngulfComponent.generated.h"

class ACharacter;

/** Fired when the crowd wins: sustained pressure held past OverruleSeconds. */
DECLARE_MULTICAST_DELEGATE(FOnOverruled);

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UEngulfComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEngulfComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** How many Refusers are pressing right now. Zero everywhere but a battle. */
	UFUNCTION(BlueprintPure, Category = "Engulf")
	int32 GetPressure() const { return Pressure; }

	/** 0..1 toward being overruled. The HUD can draw this if it ever wants to. */
	UFUNCTION(BlueprintPure, Category = "Engulf")
	float GetOverruleFraction() const;

	/** Listeners: the HUD, and whatever ends the battle. */
	FOnOverruled OnOverruled;

	/** Called by USlapComponent on every swing. See SwingHoldsThemOff. */
	UFUNCTION(BlueprintCallable, Category = "Engulf")
	void NoteSwing();

	// ---- the knobs ----

	/** How close counts as pressing. 300 cm is arm's length plus a step — the distance
	 *  at which a man is in your way rather than near you. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "50"))
	float EngulfRadius = 300.0f;

	/** Below this, a crowd is scenery. One Architect in a doorway is not a siege, and
	 *  the office has one or two Refusers in it at all times. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "1"))
	int32 PressureFloor = 3;

	/** Each Refuser past the floor takes this fraction of your speed. At 0.06, eight of
	 *  them have you at roughly two thirds; sixteen at about a third. Wading, not glue —
	 *  a player who cannot move at all stops making choices. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0", ClampMax = "0.5"))
	float SlowPerRefuser = 0.06f;

	/** Never fall below this fraction of normal speed, whatever the crowd. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0.05", ClampMax = "1"))
	float MinSpeedFraction = 0.25f;

	/** Shove strength, cm/s per Refuser of net imbalance. Surrounded evenly this cancels
	 *  to nothing and you are simply pinned, which is the correct feeling; pressed from
	 *  one side it moves you off your ground. That behaviour is not special-cased — it
	 *  falls out of summing unit vectors, which is why it is done that way. */
	/** Shove per Refuser of net imbalance, as a FRACTION of walk speed - and the units
	 *  matter, because the first version got them badly wrong.
	 *
	 *  `Away` is a SUM OF UNIT VECTORS, so with 27 Refusers on one side its magnitude is
	 *  about 27. Times the old 45 cm/s that was 1200 cm/s of shove against a 600 cm/s
	 *  walk: the crowd blew Greystone downfield at twice his own top speed while he stood
	 *  in an idle pose. Walt: "he skates away from the Gideons and they keep chasing him."
	 *
	 *  0.04 each, capped below, is a press rather than a catapult. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0", ClampMax = "0.5"))
	float ShovePerRefuser = 0.04f;

	/** However lopsided the crowd, it can never move him faster than this fraction of his
	 *  own walk speed. Being pressed should cost him ground he can still fight for. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxShoveFraction = 0.45f;

	/** Pressure at or above this starts the clock. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "1"))
	int32 OverrulePressure = 12;

	/** How long that must hold. Generous on purpose: this should feel like losing an
	 *  argument slowly, not like a trap closing. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0.5"))
	float OverruleSeconds = 10.0f;

	/** SWINGING IS RESISTING, and this is the whole shape of the mechanic.
	 *
	 *  At 8 Refusers for 6 seconds the first version overruled the player about two
	 *  seconds into any real fight - battle form ended the moment it began, with no way
	 *  to fight back out of it. Raising the numbers alone would only have delayed that.
	 *
	 *  So the clock only runs while he is NOT swinging. Hold them off and you hold your
	 *  ground; stop, and the pile closes over you. Being surrounded becomes a fight
	 *  rather than a timer, it is answered by the one verb the battle is built on, and
	 *  it says the right thing: you are overruled when you stop arguing. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0"))
	float SwingHoldsThemOff = 1.5f;

	/** The clock runs down at this multiple of real time when you break free, so backing
	 *  out of a crowd is genuinely a way out rather than a delay. */
	UPROPERTY(EditAnywhere, Category = "Engulf", meta = (ClampMin = "0.1"))
	float RecoveryRate = 2.0f;

protected:
	virtual void BeginPlay() override;

private:
	/** The overlap is the expensive half and runs at 10 Hz; the shove must be applied
	 *  every frame or it stutters. Split accordingly - see TickComponent. */
	int32 CountCrowd();
	void ApplyShove();

	FVector ShoveDir = FVector::ZeroVector;   // cached between overlaps
	float ShoveMag = 0.0f;
	float SinceCount = 0.0f;
	void ApplySlow();
	void Overrule();

	ACharacter* GetCharacterOwner() const;

	int32 Pressure = 0;
	float OverruleClock = 0.0f;
	float BaseWalkSpeed = -1.0f;   // < 0 until BeginPlay captures the real one
	bool bWarnedThisSiege = false;
	double LastSwingTime = -1000.0;
};
