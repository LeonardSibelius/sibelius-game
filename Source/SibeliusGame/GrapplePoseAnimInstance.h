// GrapplePoseAnimInstance.h
//
// THE WRESTLERS' MUSCLES (Walt, 2026-11-13: "i actually don't want any hitting, just agents
// writhing around with each other"). No animation of two people grappling on a mat exists in the
// project, or on Fab, so the wrestlers' bodies are physics (AWrestlingMat), and this is what they
// try to hold: a pose made up each frame from a handful of numbers - how far the spine curls or
// arches, how the hips and knees bend, where each arm reaches.
//
// The physics is the struggle; this is only the intent. A UPhysicalAnimationComponent pulls each
// simulated body toward the pose made here, the way a muscle pulls toward where the brain wants
// the limb, and the other wrestler, the mat and gravity decide where it actually goes.
//
// NO ASSETS, like UDanceRemixAnimInstance: the pose is built from the skeleton's own reference
// pose. Every bend is a rotation about one of HER axes - forward, left, up, worked out from where
// her shoulders and feet are in the reference pose - and which way is "forward" for a knee or a
// hip is measured once, not assumed, so the same numbers mean the same thing on any MetaHuman.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GrapplePoseAnimInstance.generated.h"

/** What a wrestler is trying to do with her body. Degrees, and two of each limb: [0] left, [1] right. */
struct FGrapplePosture
{
	float Curl = 0.0f;     // the spine bends forward (+, chest toward the knees) or arches back (-)
	float Side = 0.0f;     // the spine bends toward her left (+) or right (-)
	float Twist = 0.0f;    // the shoulders turn toward her left (+) or right (-) over the hips
	float Nod = 0.0f;      // the head and neck, forward (+)
	float Hip[2] = { 0.0f, 0.0f };     // the thigh lifts forward (+)
	float Spread[2] = { 0.0f, 0.0f };  // the thigh swings out to her side (+)
	float Knee[2] = { 0.0f, 0.0f };    // the knee bends (+)
	/** Where each upper arm points, in her chest's frame: X forward, Y out to that arm's side, Z up.
	    Zero leaves the arm as the reference pose has it. */
	FVector Arm[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float Elbow[2] = { 0.0f, 0.0f };   // the elbow bends (+)
	/** Where each HAND should be, in cm from that arm's shoulder, in the same frame as Arm. When it
	    is not zero it is used instead of Arm and Elbow: the shoulder turns and the elbow bends by
	    whatever puts the hand there, so a hand can be sent to a place on the other wrestler. */
	FVector Reach[2] = { FVector::ZeroVector, FVector::ZeroVector };

	static FGrapplePosture Blend(const FGrapplePosture& A, const FGrapplePosture& B, float T);
};

UCLASS(Transient, NotBlueprintable)
class SIBELIUSGAME_API UGrapplePoseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetPosture(const FGrapplePosture& In) { Posture = In; }
	const FGrapplePosture& GetPosture() const { return Posture; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	FGrapplePosture Posture;

	friend struct FGrapplePoseProxy;
};
