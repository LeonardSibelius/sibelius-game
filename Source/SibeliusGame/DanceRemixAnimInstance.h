// DanceRemixAnimInstance.h
//
// ONE DANCE MADE FROM TWO, LIVE (Walt, 2026-11-13: "Remix the dances live", after wishing the
// agents could have new animations in real time).
//
// Every agent dance is one UAnimSequence played by Unreal's single-node player, so a dance was
// always exactly one of the 54 in her F menu. This player samples TWO of them each frame and
// stitches them at the waist: the upper body (spine_01 and everything above it - chest, arms,
// head) from one dance, the pelvis and legs from another. On top of that, a speed and a mirror
// (left becomes right). 54 x 54 x speeds x mirror is thousands of dances nobody animated.
//
// NO ASSETS. No Anim Blueprint, no mirror table: the blend is a bone mask built from the
// skeleton, and the mirror uses the engine's own FAnimationRuntime::MirrorPose with the pairs
// worked out by name (_l <-> _r). So nothing here needs the editor, and nothing new cooks.
//
// It is only used when a dance is NOT plain - arms and legs from the same dance at normal speed,
// unmirrored, is still the single-node player, so everything that already pokes that player
// (the talk crawl, films, the greeting) behaves exactly as before for every plain dance.
// UDancerAgentComponent decides which; this class just plays what it is told.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DanceRemixAnimInstance.generated.h"

class UAnimSequence;

UCLASS(Transient, NotBlueprintable)
class SIBELIUSGAME_API UDanceRemixAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** What to play. A sequence that changes restarts in step with the other half. */
	void SetRemix(UAnimSequence* InArms, UAnimSequence* InLegs, float InSpeed, bool bInMirror);

	/** The talk close-up's crawl (UDancerAgentComponent::TalkDanceSpeed). 1 = normal. */
	void SetTalkRate(float InRate) { TalkRate = FMath::Max(0.0f, InRate); }

	UAnimSequence* GetArms() const { return Arms; }
	UAnimSequence* GetLegs() const { return Legs; }

	/** Where the upper body begins. Everything from this bone up comes from the arms dance. */
	static const FName UpperBodyRoot;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Arms;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Legs;

	float Speed = 1.0f;
	float TalkRate = 1.0f;
	bool bMirror = false;

	friend struct FDanceRemixProxy;
};
