// GrapplePoseAnimInstance.cpp - see header.

#include "GrapplePoseAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"   // FPoseContext
#include "BoneContainer.h"

FGrapplePosture FGrapplePosture::Blend(const FGrapplePosture& A, const FGrapplePosture& B, float T)
{
	FGrapplePosture Out;
	Out.Curl = FMath::Lerp(A.Curl, B.Curl, T);
	Out.Side = FMath::Lerp(A.Side, B.Side, T);
	Out.Twist = FMath::Lerp(A.Twist, B.Twist, T);
	Out.Nod = FMath::Lerp(A.Nod, B.Nod, T);
	for (int32 S = 0; S < 2; ++S)
	{
		Out.Hip[S] = FMath::Lerp(A.Hip[S], B.Hip[S], T);
		Out.Spread[S] = FMath::Lerp(A.Spread[S], B.Spread[S], T);
		Out.Knee[S] = FMath::Lerp(A.Knee[S], B.Knee[S], T);
		Out.Arm[S] = FMath::Lerp(A.Arm[S], B.Arm[S], T);
		Out.Elbow[S] = FMath::Lerp(A.Elbow[S], B.Elbow[S], T);
		// A reach that is not set yet has nowhere to blend from: the first one is taken whole.
		Out.Reach[S] = A.Reach[S].IsNearlyZero() || B.Reach[S].IsNearlyZero() ? B.Reach[S] : FMath::Lerp(A.Reach[S], B.Reach[S], T);
	}
	return Out;
}

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace GrapplePoseNS
{
	enum class ERole : uint8 { None, Spine, Neck, UpperArm, LowerArm, Thigh, Calf };

	struct FBoneRole
	{
		ERole Role = ERole::None;
		int32 Side = 0;          // 0 left, 1 right (limbs)
		float Share = 0.0f;      // the spine's and neck's share of the whole bend
	};

	/** Which bones move, and by how much of the whole: the five spine bones carry the curl between them. */
	FBoneRole RoleOf(const FName Name)
	{
		static const FName Spine[5] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };
		static const float SpineShare[5] = { 0.12f, 0.18f, 0.22f, 0.24f, 0.24f };
		for (int32 I = 0; I < 5; ++I)
		{
			if (Name == Spine[I])
			{
				return { ERole::Spine, 0, SpineShare[I] };
			}
		}
		if (Name == TEXT("neck_01")) { return { ERole::Neck, 0, 0.4f }; }
		if (Name == TEXT("neck_02")) { return { ERole::Neck, 0, 0.3f }; }
		if (Name == TEXT("head"))    { return { ERole::Neck, 0, 0.3f }; }
		if (Name == TEXT("upperarm_l")) { return { ERole::UpperArm, 0, 0.0f }; }
		if (Name == TEXT("upperarm_r")) { return { ERole::UpperArm, 1, 0.0f }; }
		if (Name == TEXT("lowerarm_l")) { return { ERole::LowerArm, 0, 0.0f }; }
		if (Name == TEXT("lowerarm_r")) { return { ERole::LowerArm, 1, 0.0f }; }
		if (Name == TEXT("thigh_l")) { return { ERole::Thigh, 0, 0.0f }; }
		if (Name == TEXT("thigh_r")) { return { ERole::Thigh, 1, 0.0f }; }
		if (Name == TEXT("calf_l"))  { return { ERole::Calf, 0, 0.0f }; }
		if (Name == TEXT("calf_r"))  { return { ERole::Calf, 1, 0.0f }; }
		return {};
	}

	/** +1 when turning From about Axis by a small positive angle moves it toward Toward, else -1. */
	float SignToward(const FVector& From, const FVector& Axis, const FVector& Toward)
	{
		const FVector Turned = FQuat(Axis, FMath::DegreesToRadians(10.0f)).RotateVector(From);
		return FVector::DotProduct(Turned, Toward) >= FVector::DotProduct(From, Toward) ? 1.0f : -1.0f;
	}

	float Rad(float Degrees) { return FMath::DegreesToRadians(Degrees); }
}

/* THE WORKER-THREAD HALF, as in UDanceRemixAnimInstance: the posture is copied across in
   PreUpdate, and the pose is made in Evaluate. */
struct FGrapplePoseProxy : public FAnimInstanceProxy
{
	explicit FGrapplePoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		Posture = CastChecked<UGrapplePoseAnimInstance>(InAnimInstance)->Posture;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		using namespace GrapplePoseNS;
		Output.ResetToRefPose();
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		if (!bCacheValid || CachedSerial != Bones.GetSerialNumber() || Roles.Num() != Bones.GetCompactPoseNumBones())
		{
			RebuildCaches(Bones);
		}

		/* EACH BEND IS ABOUT HER OWN AXES AS THE BONES ABOVE HAVE ALREADY CARRIED THEM. A delta R
		   given in the reference frame, applied after everything the parents did, keeps the local
		   rotation simple: parent_ref^-1 * R * own_ref - the parents' own deltas cancel out. So a
		   knee bends about the thigh's hinge wherever the hip has swung the thigh, and the arms
		   reach in the chest's frame however the spine has curled. */
		SolveReach();
		const int32 Num = Roles.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FBoneRole& Role = Roles[Index];
			if (Role.Role == ERole::None)
			{
				continue;
			}
			const FQuat R = DeltaFor(Role).GetNormalized();
			const FQuat ParentRef = Parents[Index] >= 0 ? RefRot[Parents[Index]] : FQuat::Identity;
			FTransform& Local = Output.Pose[FCompactPoseBoneIndex(Index)];
			Local.SetRotation((ParentRef.Inverse() * R * RefRot[Index]).GetNormalized());
		}
		return true;
	}

private:
	/* A HAND SENT SOMEWHERE: the elbow bends until the hand is as far from the shoulder as the
	   place is, and the whole arm then turns, by the shortest way, to point the hand at it. */
	void SolveReach()
	{
		using namespace GrapplePoseNS;
		for (int32 S = 0; S < 2; ++S)
		{
			bReach[S] = !Posture.Reach[S].IsNearlyZero() && UpperLen[S] > 1.0f && ForeLen[S] > 1.0f;
			if (!bReach[S])
			{
				continue;
			}
			const FVector Out = S == 0 ? Left : -Left;
			const FVector To = Forward * Posture.Reach[S].X + Out * Posture.Reach[S].Y + Up * Posture.Reach[S].Z;
			const double Full = UpperLen[S] + ForeLen[S];
			const double Far = FMath::Clamp(To.Size(), Full * 0.35, Full * 0.985);
			const double Inside = FMath::Acos(FMath::Clamp(
				(UpperLen[S] * UpperLen[S] + ForeLen[S] * ForeLen[S] - Far * Far) / (2.0 * UpperLen[S] * ForeLen[S]), -1.0, 1.0));
			const double Bend = FMath::Max(0.0, (UE_DOUBLE_PI - Inside) - RefBend[S]);
			ReachElbow[S] = FQuat(ElbowAxis[S], ElbowSign[S] * Bend);
			const FVector Hand = ArmDir[S] * UpperLen[S] + ReachElbow[S].RotateVector(ForeDir[S]) * ForeLen[S];
			ReachArm[S] = FQuat::FindBetweenNormals(Hand.GetSafeNormal(), To.GetSafeNormal());
		}
	}

	FQuat DeltaFor(const GrapplePoseNS::FBoneRole& Role) const
	{
		using namespace GrapplePoseNS;
		const int32 S = Role.Side;
		switch (Role.Role)
		{
		case ERole::Spine:
			return FQuat(Left, CurlSign * Rad(Posture.Curl * Role.Share))
				* FQuat(Forward, SideSign * Rad(Posture.Side * Role.Share))
				* FQuat(Up, TwistSign * Rad(Posture.Twist * Role.Share));
		case ERole::Neck:
			return FQuat(Left, CurlSign * Rad(Posture.Nod * Role.Share));
		case ERole::Thigh:
			return FQuat(Left, HipSign[S] * Rad(Posture.Hip[S]))
				* FQuat(Forward, SpreadSign[S] * Rad(Posture.Spread[S]));
		case ERole::Calf:
			return FQuat(Left, KneeSign[S] * Rad(Posture.Knee[S]));
		case ERole::UpperArm:
		{
			if (bReach[S])
			{
				return ReachArm[S];
			}
			const FVector& A = Posture.Arm[S];
			if (A.IsNearlyZero())
			{
				return FQuat::Identity;
			}
			const FVector Out = S == 0 ? Left : -Left;
			const FVector Want = (Forward * A.X + Out * A.Y + Up * A.Z).GetSafeNormal();
			return Want.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(ArmDir[S], Want);
		}
		case ERole::LowerArm:
			return bReach[S] ? ReachElbow[S] : FQuat(ElbowAxis[S], ElbowSign[S] * Rad(Posture.Elbow[S]));
		default:
			return FQuat::Identity;
		}
	}

	/* Once per set of required bones: the reference pose in component space, each bone's role, her
	   axes, and which way each joint's positive bend goes - measured on her, not assumed. */
	void RebuildCaches(const FBoneContainer& Bones)
	{
		using namespace GrapplePoseNS;
		const int32 Num = Bones.GetCompactPoseNumBones();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		Roles.SetNum(Num);
		Parents.SetNum(Num);
		RefRot.SetNum(Num);
		TArray<FVector> RefPos;
		RefPos.SetNum(Num);
		TMap<FName, int32> ByName;

		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			const FName Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt());
			const FTransform& Local = Bones.GetRefPoseTransform(Bone);
			Parents[Index] = Parent.IsValid() ? Parent.GetInt() : -1;
			if (Parent.IsValid())
			{
				RefRot[Index] = RefRot[Parent.GetInt()] * Local.GetRotation();
				RefPos[Index] = RefPos[Parent.GetInt()] + RefRot[Parent.GetInt()].RotateVector(Local.GetTranslation());
			}
			else
			{
				RefRot[Index] = Local.GetRotation();
				RefPos[Index] = Local.GetTranslation();
			}
			Roles[Index] = RoleOf(Name);
			ByName.Add(Name, Index);
		}

		auto Pos = [&](const TCHAR* Name, FVector& Out) -> bool
		{
			const int32* Found = ByName.Find(FName(Name));
			if (Found)
			{
				Out = RefPos[*Found];
			}
			return Found != nullptr;
		};

		/* HER AXES. Up is the mesh's Z. Forward is the way her toes point (a MetaHuman faces +Y in
		   her own space, which is the fallback). Left runs from her right shoulder to her left. */
		Up = FVector::UpVector;
		Forward = FVector(0.0, 1.0, 0.0);
		FVector FootL, BallL;
		if (Pos(TEXT("foot_l"), FootL) && Pos(TEXT("ball_l"), BallL))
		{
			const FVector Toes = FVector(BallL.X - FootL.X, BallL.Y - FootL.Y, 0.0).GetSafeNormal();
			if (!Toes.IsNearlyZero())
			{
				Forward = Toes;
			}
		}
		Left = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		FVector ShoulderL, ShoulderR;
		if (Pos(TEXT("upperarm_l"), ShoulderL) && Pos(TEXT("upperarm_r"), ShoulderR)
			&& FVector::DotProduct(ShoulderL - ShoulderR, Left) < 0.0)
		{
			Left = -Left;
		}

		CurlSign = SignToward(Up, Left, Forward);
		SideSign = SignToward(Up, Forward, Left);
		TwistSign = SignToward(Forward, Up, Left);

		static const TCHAR* Thigh[2] = { TEXT("thigh_l"), TEXT("thigh_r") };
		static const TCHAR* Calf[2] = { TEXT("calf_l"), TEXT("calf_r") };
		static const TCHAR* Foot[2] = { TEXT("foot_l"), TEXT("foot_r") };
		static const TCHAR* Upper[2] = { TEXT("upperarm_l"), TEXT("upperarm_r") };
		static const TCHAR* Lower[2] = { TEXT("lowerarm_l"), TEXT("lowerarm_r") };
		static const TCHAR* Hand[2] = { TEXT("hand_l"), TEXT("hand_r") };
		for (int32 S = 0; S < 2; ++S)
		{
			const FVector Out = S == 0 ? Left : -Left;
			FVector A, B, C;
			FVector ThighDir = -Up;
			FVector CalfDir = -Up;
			if (Pos(Thigh[S], A) && Pos(Calf[S], B)) { ThighDir = (B - A).GetSafeNormal(); }
			if (Pos(Calf[S], B) && Pos(Foot[S], C)) { CalfDir = (C - B).GetSafeNormal(); }
			HipSign[S] = SignToward(ThighDir, Left, Forward);
			KneeSign[S] = SignToward(CalfDir, Left, -Forward);
			SpreadSign[S] = SignToward(ThighDir, Forward, Out);

			ArmDir[S] = Out;
			ForeDir[S] = Out;
			UpperLen[S] = 0.0f;
			ForeLen[S] = 0.0f;
			if (Pos(Upper[S], A) && Pos(Lower[S], B))
			{
				ArmDir[S] = (B - A).GetSafeNormal();
				UpperLen[S] = static_cast<float>((B - A).Size());
			}
			if (Pos(Lower[S], B) && Pos(Hand[S], C))
			{
				ForeDir[S] = (C - B).GetSafeNormal();
				ForeLen[S] = static_cast<float>((C - B).Size());
			}
			RefBend[S] = FMath::Acos(FMath::Clamp(FVector::DotProduct(ArmDir[S], ForeDir[S]), -1.0, 1.0));
			ElbowAxis[S] = FVector::CrossProduct(ArmDir[S], Forward).GetSafeNormal();
			if (ElbowAxis[S].IsNearlyZero())
			{
				ElbowAxis[S] = Left;
			}
			ElbowSign[S] = SignToward(ForeDir[S], ElbowAxis[S], Forward);
		}

		CachedSerial = Bones.GetSerialNumber();
		bCacheValid = true;
	}

	FGrapplePosture Posture;

	bool bCacheValid = false;
	uint16 CachedSerial = 0;
	TArray<GrapplePoseNS::FBoneRole> Roles;
	TArray<int32> Parents;
	TArray<FQuat> RefRot;

	FVector Up = FVector::UpVector;
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Left = FVector(1.0, 0.0, 0.0);
	float CurlSign = 1.0f;
	float SideSign = 1.0f;
	float TwistSign = 1.0f;
	float HipSign[2] = { 1.0f, 1.0f };
	float KneeSign[2] = { 1.0f, 1.0f };
	float SpreadSign[2] = { 1.0f, 1.0f };
	FVector ArmDir[2] = { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0) };
	FVector ElbowAxis[2] = { FVector(0.0, 1.0, 0.0), FVector(0.0, 1.0, 0.0) };
	float ElbowSign[2] = { 1.0f, 1.0f };
	FVector ForeDir[2] = { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0) };
	float UpperLen[2] = { 0.0f, 0.0f };
	float ForeLen[2] = { 0.0f, 0.0f };
	double RefBend[2] = { 0.0, 0.0 };     // the bend the reference pose's elbow already has, radians

	// This frame's reach, worked out once before the bones are turned.
	bool bReach[2] = { false, false };
	FQuat ReachArm[2] = { FQuat::Identity, FQuat::Identity };
	FQuat ReachElbow[2] = { FQuat::Identity, FQuat::Identity };
};

FAnimInstanceProxy* UGrapplePoseAnimInstance::CreateAnimInstanceProxy()
{
	return new FGrapplePoseProxy(this);
}

void UGrapplePoseAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
