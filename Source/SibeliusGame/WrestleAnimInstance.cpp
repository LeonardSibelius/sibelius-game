// WrestleAnimInstance.cpp - see header.

#include "WrestleAnimInstance.h"
#include "Animation/AnimNodeBase.h"                 // FPoseContext
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "BoneContainer.h"
#include "TwoBoneIK.h"                              // AnimationCore::SolveTwoBoneIK

namespace WrestleAnimNS
{
	/** A bone's transform in component space: its local transform carried up through each parent. */
	FTransform ComponentSpace(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		FTransform T = Pose[Bone];
		for (FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone); Parent.IsValid(); Parent = Bones.GetParentBoneIndex(Parent))
		{
			T = T * Pose[Parent];
		}
		return T;
	}

	/** Put a bone where a component-space transform says, by rewriting its local transform. */
	void SetComponentSpace(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FTransform& Wanted)
	{
		const FCompactPoseBoneIndex Parent = Pose.GetBoneContainer().GetParentBoneIndex(Bone);
		FTransform Local = Parent.IsValid() ? Wanted.GetRelativeTransform(ComponentSpace(Pose, Parent)) : Wanted;
		Local.NormalizeRotation();
		Pose[Bone] = Local;
	}

	constexpr float MaxLookDeg = 55.0f;     // past this the turn stops: a neck, not an owl
	constexpr float BehindDeg = 110.0f;     // the target further round than this: leave the head be
	constexpr float NeckShare = 0.4f;       // the neck takes this much of the turn, the head the rest
}

/* The worker-thread half, as in UDanceRemixAnimInstance: PreUpdate is the game-thread handshake,
   so the match's targets are copied in there and the clip-hand readings copied out. */
struct FWrestleProxy : public FAnimSingleNodeInstanceProxy
{
	explicit FWrestleProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimSingleNodeInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		UWrestleAnimInstance* Owner = CastChecked<UWrestleAnimInstance>(InAnimInstance);

		if (bMeasured)
		{
			Owner->ClipHand[0] = ClipHand[0];
			Owner->ClipHand[1] = ClipHand[1];
			Owner->bClipHands = true;
		}

		// Eased, so a look or a reach never snaps on or off between two frames.
		LookTarget = Owner->LookTarget;
		LookWeight = FMath::FInterpConstantTo(LookWeight, Owner->LookWant, DeltaSeconds, 2.5f);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			ReachTarget[Side] = Owner->ReachTarget[Side];
			ReachWeight[Side] = FMath::FInterpConstantTo(ReachWeight[Side], Owner->ReachWant[Side], DeltaSeconds, 5.0f);
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		FAnimSingleNodeInstanceProxy::Evaluate(Output);

		FCompactPose& Pose = Output.Pose;
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (!bCached || CachedSerial != Bones.GetSerialNumber())
		{
			Cache(Bones);
		}
		const FTransform& ToWorld = GetComponentTransform();

		// The clip's own hands, before anything here moves them.
		bool bAll = true;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (Hand[Side].IsValid())
			{
				ClipHand[Side] = ToWorld.TransformPosition(WrestleAnimNS::ComponentSpace(Pose, Hand[Side]).GetLocation());
			}
			else
			{
				bAll = false;
			}
		}
		bMeasured = bAll;

		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (ReachWeight[Side] > 0.01f)
			{
				Reach(Pose, Side, ToWorld.InverseTransformPosition(ReachTarget[Side]), ReachWeight[Side]);
			}
		}
		if (LookWeight > 0.01f)
		{
			Look(Pose, ToWorld.InverseTransformPosition(LookTarget), LookWeight);
		}
		return true;
	}

private:
	void Cache(const FBoneContainer& Bones)
	{
		auto Find = [&Bones](const TCHAR* Name)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
			return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
				: Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		};
		Neck = Find(TEXT("neck_01"));
		Head = Find(TEXT("head"));
		Upper[0] = Find(TEXT("upperarm_l"));
		Lower[0] = Find(TEXT("lowerarm_l"));
		Hand[0] = Find(TEXT("hand_l"));
		Upper[1] = Find(TEXT("upperarm_r"));
		Lower[1] = Find(TEXT("lowerarm_r"));
		Hand[1] = Find(TEXT("hand_r"));

		/* WHICH WAY THE FACE POINTS, in the head bone's own axes. A skeletal mesh faces +Y in its
		   own space, so in the reference pose the face looks along +Y; carried into the head's
		   frame, that is the face's direction whatever pose the clip puts the head in. */
		HeadFace = FVector(0.0, 1.0, 0.0);
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		int32 Index = Ref.FindBoneIndex(TEXT("head"));
		if (Index != INDEX_NONE)
		{
			FTransform HeadRef = FTransform::Identity;
			for (; Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
			{
				HeadRef = HeadRef * Ref.GetRefBonePose()[Index];
			}
			HeadFace = HeadRef.GetRotation().Inverse().RotateVector(FVector(0.0, 1.0, 0.0)).GetSafeNormal();
		}

		CachedSerial = Bones.GetSerialNumber();
		bCached = true;
	}

	void Look(FCompactPose& Pose, const FVector& Target, float Weight)
	{
		using namespace WrestleAnimNS;
		if (!Neck.IsValid() || !Head.IsValid())
		{
			return;
		}
		const FTransform HeadNow = ComponentSpace(Pose, Head);
		const FVector Face = HeadNow.TransformVectorNoScale(HeadFace).GetSafeNormal();
		const FVector Want = (Target - HeadNow.GetLocation()).GetSafeNormal();
		if (Face.IsNearlyZero() || Want.IsNearlyZero())
		{
			return;
		}
		const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Face, Want), -1.0, 1.0)));
		if (Angle < 1.0 || Angle > BehindDeg)
		{
			return;
		}
		const double Share = Weight * FMath::Min(1.0, MaxLookDeg / Angle);
		const FQuat Turn = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Face, Want), Share);
		const FVector Goal = Turn.RotateVector(Face);

		// The neck first, part of the way.
		FTransform NeckCS = ComponentSpace(Pose, Neck);
		NeckCS.SetRotation(FQuat::Slerp(FQuat::Identity, Turn, NeckShare) * NeckCS.GetRotation());
		SetComponentSpace(Pose, Neck, NeckCS);

		// Then the head, the rest of the way to the same goal.
		FTransform HeadCS = ComponentSpace(Pose, Head);
		const FVector FaceNow = HeadCS.TransformVectorNoScale(HeadFace).GetSafeNormal();
		HeadCS.SetRotation(FQuat::FindBetweenNormals(FaceNow, Goal) * HeadCS.GetRotation());
		SetComponentSpace(Pose, Head, HeadCS);
	}

	void Reach(FCompactPose& Pose, int32 Side, const FVector& Target, float Weight)
	{
		using namespace WrestleAnimNS;
		if (!Upper[Side].IsValid() || !Lower[Side].IsValid() || !Hand[Side].IsValid())
		{
			return;
		}
		FTransform UpperCS = ComponentSpace(Pose, Upper[Side]);
		FTransform LowerCS = ComponentSpace(Pose, Lower[Side]);
		FTransform HandCS = ComponentSpace(Pose, Hand[Side]);
		const FVector Effector = FMath::Lerp(HandCS.GetLocation(), Target, static_cast<double>(Weight));

		// The elbow keeps bending the way the clip bends it: aim it further out along its own bend.
		const FVector Elbow = LowerCS.GetLocation();
		const FVector JointTarget = Elbow + (Elbow - 0.5 * (UpperCS.GetLocation() + HandCS.GetLocation()));

		AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, JointTarget, Effector, false, 1.0, 1.0);
		SetComponentSpace(Pose, Upper[Side], UpperCS);
		SetComponentSpace(Pose, Lower[Side], LowerCS);
		SetComponentSpace(Pose, Hand[Side], HandCS);
	}

	FVector LookTarget = FVector::ZeroVector;
	float LookWeight = 0.0f;
	FVector ReachTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float ReachWeight[2] = { 0.0f, 0.0f };
	FVector ClipHand[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bMeasured = false;

	bool bCached = false;
	uint16 CachedSerial = 0;
	FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Upper[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Lower[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Hand[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FVector HeadFace = FVector(0.0, 1.0, 0.0);
};

UWrestleAnimInstance::UWrestleAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FAnimInstanceProxy* UWrestleAnimInstance::CreateAnimInstanceProxy()
{
	return new FWrestleProxy(this);
}
