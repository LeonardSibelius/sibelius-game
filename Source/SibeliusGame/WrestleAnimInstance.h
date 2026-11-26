// WrestleAnimInstance.h
//
// THE WRESTLERS SEE EACH OTHER, AND THEIR HANDS MEET (Walt, 2026-11-26: "the hands of the
// opponents don't really grip, the opponents don't really see or react normally to each other").
//
// The match plays the bought pack's mocap on each MetaHuman with Unreal's single-node player,
// and nothing at run time ties one body to the other. This is that same player (a subclass, so
// SetAnimation / Play / SetPosition / GetPosition on the mesh still work exactly as before) with
// two small touches added after the clip's pose is sampled:
//
//   LOOK  - the neck and head turn toward a point the match gives (the other wrestler's head,
//           or for the referee the pair), limited to 55 degrees and eased in and out.
//   REACH - a two-bone reach (upper arm, forearm, hand) pulls a hand toward a point the match
//           gives (in the test of strength: just short of the other wrestler's hand).
//
// Both are weights the match sets every frame; at zero the pose is the clip's, untouched. The
// match also reads back where the clip itself puts each hand (before any reach), so it can judge
// a grip from the animation and not from its own adjustment.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "WrestleAnimInstance.generated.h"

UCLASS(Transient, NotBlueprintable)
class SIBELIUSGAME_API UWrestleAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()

public:
	UWrestleAnimInstance(const FObjectInitializer& ObjectInitializer);

	/** Game thread, every frame. Weight 0 lets the clip's head be. */
	void SetLook(const FVector& WorldTarget, float Weight)
	{
		LookTarget = WorldTarget;
		LookWant = FMath::Clamp(Weight, 0.0f, 1.0f);
	}

	/** Side 0 = hand_l, 1 = hand_r. Weight 0 lets the clip's arm be. */
	void SetReach(int32 Side, const FVector& WorldTarget, float Weight)
	{
		if (Side == 0 || Side == 1)
		{
			ReachTarget[Side] = WorldTarget;
			ReachWant[Side] = FMath::Clamp(Weight, 0.0f, 1.0f);
		}
	}

	/** Where the clip itself had this hand, in the world, a frame or two ago (before any reach). */
	FVector GetClipHand(int32 Side) const { return ClipHand[Side == 1 ? 1 : 0]; }

	/** True once the clip hands have been measured at least once. */
	bool HasClipHands() const { return bClipHands; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	FVector LookTarget = FVector::ZeroVector;
	float LookWant = 0.0f;
	FVector ReachTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float ReachWant[2] = { 0.0f, 0.0f };
	FVector ClipHand[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bClipHands = false;

	friend struct FWrestleProxy;
};
