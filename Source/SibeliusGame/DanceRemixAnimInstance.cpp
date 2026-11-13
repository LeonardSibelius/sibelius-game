// DanceRemixAnimInstance.cpp - see header.

#include "DanceRemixAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"          // FPoseContext
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"                // FAnimationRuntime::MirrorPose
#include "BoneContainer.h"

const FName UDanceRemixAnimInstance::UpperBodyRoot(TEXT("spine_01"));

namespace DanceRemixNS
{
	/* A bone's partner on the other side, by the MetaHuman naming rule: a trailing _l or _r
	   (upperarm_l, thigh_r, index_01_l, upperarm_twist_01_l...). NAME_None for a bone on the
	   centre line - spine, neck, head, pelvis, root - which mirrors onto itself. */
	FName MirrorNameOf(FName Bone)
	{
		FString S = Bone.ToString();
		if (S.Len() < 3 || S[S.Len() - 2] != TEXT('_'))
		{
			return NAME_None;
		}
		const TCHAR Side = S[S.Len() - 1];
		TCHAR Other = 0;
		switch (Side)
		{
		case TEXT('l'): Other = TEXT('r'); break;
		case TEXT('r'): Other = TEXT('l'); break;
		case TEXT('L'): Other = TEXT('R'); break;
		case TEXT('R'): Other = TEXT('L'); break;
		default: return NAME_None;
		}
		S[S.Len() - 1] = Other;
		return FName(*S);
	}

	/** Keep a playhead inside its loop. */
	void Wrap(float& Time, const UAnimSequence* Seq)
	{
		const float Len = Seq ? Seq->GetPlayLength() : 0.0f;
		if (Len <= KINDA_SMALL_NUMBER)
		{
			Time = 0.0f;
			return;
		}
		Time = FMath::Fmod(Time, Len);
		if (Time < 0.0f)
		{
			Time += Len;
		}
	}
}

/* THE WORKER-THREAD HALF. Unreal updates and poses an anim instance through its proxy, off the
   game thread; PreUpdate is the one game-thread handshake, so the settings are copied there and
   the playheads live here. The sequences themselves are kept alive by the instance's
   UPROPERTYs - the proxy only borrows them. */
struct FDanceRemixProxy : public FAnimInstanceProxy
{
	explicit FDanceRemixProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		const UDanceRemixAnimInstance* Owner = CastChecked<UDanceRemixAnimInstance>(InAnimInstance);

		/* A NEW HALF JOINS IN STEP. Swapping the arms while the legs keep going starts the new
		   arms at the legs' playhead (and the other way round), so the two halves stay on the
		   same count instead of the new one always starting from its first frame. */
		if (Owner->Legs != Legs)
		{
			Legs = Owner->Legs;
			LegsTime = ArmsTime;
			DanceRemixNS::Wrap(LegsTime, Legs);
		}
		if (Owner->Arms != Arms)
		{
			Arms = Owner->Arms;
			ArmsTime = LegsTime;
			DanceRemixNS::Wrap(ArmsTime, Arms);
		}
		Rate = Owner->Speed * Owner->TalkRate;
		bMirror = Owner->bMirror;
	}

	virtual void Update(float DeltaSeconds) override
	{
		LegsTime += DeltaSeconds * Rate;
		DanceRemixNS::Wrap(LegsTime, Legs);
		if (Arms == Legs)
		{
			ArmsTime = LegsTime;   // one dance: one playhead, so the halves cannot drift apart
		}
		else
		{
			ArmsTime += DeltaSeconds * Rate;
			DanceRemixNS::Wrap(ArmsTime, Arms);
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (!Legs)
		{
			Output.ResetToRefPose();
			return true;
		}

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		if (!bCacheValid || CachedSerial != Bones.GetSerialNumber()
			|| UpperMask.Num() != Bones.GetCompactPoseNumBones())
		{
			RebuildCaches(Bones);
		}

		// The legs dance is the whole pose to start with: root, pelvis, legs - and, for now,
		// the upper body too, which the arms dance then replaces bone by bone.
		{
			FAnimationPoseData LegsData(Output);
			Legs->GetAnimationPose(LegsData, FAnimExtractContext(static_cast<double>(LegsTime), false, {}, true));
		}

		if (Arms && Arms != Legs)
		{
			FPoseContext ArmsPose(this);
			FAnimationPoseData ArmsData(ArmsPose);
			Arms->GetAnimationPose(ArmsData, FAnimExtractContext(static_cast<double>(ArmsTime), false, {}, true));
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				if (UpperMask[Bone.GetInt()])
				{
					Output.Pose[Bone] = ArmsPose.Pose[Bone];
				}
			}
		}

		if (bMirror && MirrorBones.Num() == Bones.GetCompactPoseNumBones())
		{
			/* X IS SIDEWAYS. An Unreal skeletal mesh faces +Y in its own space (the film
			   director relies on the same fact), so left and right lie along X. */
			FAnimationRuntime::MirrorPose(Output.Pose, EAxis::X, MirrorBones, RefRotations);
		}
		return true;
	}

private:
	/* Built once per set of required bones (a LOD change makes a new set): which bones are
	   upper body, each bone's mirror partner, and the reference rotations the engine's mirror
	   needs - the same three things a UMirrorDataTable would supply, worked out here instead. */
	void RebuildCaches(const FBoneContainer& Bones)
	{
		const int32 Num = Bones.GetCompactPoseNumBones();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();

		UpperMask.Init(false, Num);
		MirrorBones.Reset(Num);
		RefRotations.SetNumUninitialized(Num);

		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			const FName Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt());

			// Compact order puts every parent before its children, so one pass is enough.
			UpperMask[Index] = Name == UDanceRemixAnimInstance::UpperBodyRoot
				|| (Parent.IsValid() && UpperMask[Parent.GetInt()]);

			FCompactPoseBoneIndex Partner = Bone;   // centre-line bones mirror in place
			const FName PartnerName = DanceRemixNS::MirrorNameOf(Name);
			if (!PartnerName.IsNone())
			{
				const int32 PartnerMesh = Ref.FindBoneIndex(PartnerName);
				if (PartnerMesh != INDEX_NONE)
				{
					const FCompactPoseBoneIndex PartnerCompact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(PartnerMesh));
					if (PartnerCompact.IsValid())
					{
						Partner = PartnerCompact;
					}
				}
			}
			MirrorBones.Add(Partner);

			const FQuat Local = Bones.GetRefPoseTransform(Bone).GetRotation();
			RefRotations[Bone] = Parent.IsValid() ? RefRotations[Parent] * Local : Local;
		}

		CachedSerial = Bones.GetSerialNumber();
		bCacheValid = true;
	}

	UAnimSequence* Arms = nullptr;
	UAnimSequence* Legs = nullptr;
	float ArmsTime = 0.0f;
	float LegsTime = 0.0f;
	float Rate = 1.0f;
	bool bMirror = false;

	bool bCacheValid = false;
	uint16 CachedSerial = 0;
	TArray<bool> UpperMask;
	TArray<FCompactPoseBoneIndex> MirrorBones;
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> RefRotations;
};

void UDanceRemixAnimInstance::SetRemix(UAnimSequence* InArms, UAnimSequence* InLegs, float InSpeed, bool bInMirror)
{
	Legs = InLegs;
	Arms = InArms ? InArms : InLegs;
	Speed = FMath::Clamp(InSpeed, 0.1f, 4.0f);
	bMirror = bInMirror;
}

FAnimInstanceProxy* UDanceRemixAnimInstance::CreateAnimInstanceProxy()
{
	return new FDanceRemixProxy(this);
}

void UDanceRemixAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
