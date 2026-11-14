// WrestlingMat.cpp - see the header for what the match is, and why it is physics.

#include "WrestlingMat.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"

#include "AnimationRuntime.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"   // FScreenshotRequest

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace WrestlingMatDetail
{
	struct FAgent
	{
		const TCHAR* Name;
		const TCHAR* Class;
	};
	// The plain MetaHuman blueprints, as the hoops and the karts use: no dance logic of their own.
	const FAgent Agents[] = {
		{ TEXT("Kaia"),    TEXT("/Game/MetaHumans/MHC_Kaia/BP_MHC_Kaia.BP_MHC_Kaia_C") },
		{ TEXT("Nyra"),    TEXT("/Game/MetaHumans/MHC_NyraSolmere/BP_MHC_NyraSolmere.BP_MHC_NyraSolmere_C") },
		{ TEXT("Isla"),    TEXT("/Game/MetaHumans/MHC_IslaRowan/BP_MHC_IslaRowan.BP_MHC_IslaRowan_C") },
		{ TEXT("Aisling"), TEXT("/Game/MetaHumans/MHC_Aisling/BP_MHC_Aisling.BP_MHC_Aisling_C") },
		{ TEXT("Elise"),   TEXT("/Game/MetaHumans/MHC_Elise/BP_MHC_Elise.BP_MHC_Elise_C") },
	};
	// Every pairing once, in an order that never puts the same agent on the mat twice running.
	const int32 Pairs[][2] = { {0, 1}, {2, 3}, {4, 0}, {1, 2}, {3, 4}, {0, 2}, {1, 3}, {2, 4}, {3, 0}, {4, 1} };
	constexpr int32 PairCount = UE_ARRAY_COUNT(Pairs);

	constexpr double ShapeSize = 100.0;          // the engine's basic shapes are a metre across
	const FLinearColor MatBlue(0.02f, 0.07f, 0.24f);
	const FLinearColor LineRed(0.60f, 0.05f, 0.02f);
	const FLinearColor LampWarm(1.0f, 0.84f, 0.70f);

	constexpr float SettleKinematic = 0.3f;  // posed but not yet physics: the pose must exist before the bodies do
	constexpr float SettleSeconds = 1.6f;    // on their knees, bodies live, nobody pushing yet
	constexpr float FallSeconds = 3.5f;
	constexpr float WonSeconds = 7.0f;
	constexpr double StartGap = 28.0;        // each from the centre: already in contact, hands on each other's shoulders
	constexpr double KneelHip = 30.0;        // the starting kneel: thighs a little forward, shins flat behind
	constexpr double KneelKnee = 112.0;      // a little short of flat, so the toes clear the mat
	constexpr double KneelLift = 3.0;        // start that much above the mat and settle onto it
	constexpr float BreakApartSeconds = 4.0f; // apart this long and the referee brings them back
	/* NO BODY IS EVER SHOT OUT OF ANOTHER. A body that starts a little inside the mat, or inside
	   the other wrestler, is eased out at this speed (cm/s) instead of the engine's default, which
	   threw both of them a metre into the air at the first whistle (the lab, 2026-11-13). */
	constexpr float EaseOut = 60.0f;

	// The script's measures.
	constexpr double TieGap = 33.0;          // each one's hips from the point between them, in the tie-up
	constexpr float KneelSink = 2.5f;        // how far her hips settle once her knees bear her weight
	constexpr double TieLean = 38.0;         // how far each leans in, degrees from upright
	constexpr double ProneSlide = 12.0;      // flat on her front, her hips are this far forward of where she knelt
	constexpr float TurnSeconds = 0.8f;      // turned from her front to her back: onto her side first, this long
	constexpr float LegTone = 0.6f;          // her legs' muscles against the rest: they hold a shape, they do not lift her
	constexpr double ChestOnChest = 22.0;    // one chest lying on another: spine to spine, cm
	constexpr double LieHeight = 13.0;       // her hips above the mat on her back
	constexpr float ShotLead = 0.35f;        // the level change before the drive
	constexpr float ShotSeconds = 1.9f;      // the whole takedown
	constexpr float KickTurn = 0.6f;         // the bridge, before she sits up
	constexpr float KickSeconds = 2.6f;      // the whole kick-out
	constexpr float RiseSeconds = 2.6f;      // back up to their knees
	constexpr float CircleRate = 5.0f;       // the tie-up's turning, degrees a second at most
	constexpr double MaxCarry = 1800.0;      // the hardest she is ever pulled toward her goal, cm/s^2
	constexpr double TurnRate = 6.0;         // the facing spring's rate, 1/s
	constexpr double MaxTurn = 60.0;         // and the hardest she is ever turned, rad/s^2

	/** The bone itself if it has a physics body, else the nearest one above it that does. */
	FName FirstBody(const USkeletalMeshComponent* Mesh, FName Bone)
	{
		for (FName B = Bone; !B.IsNone(); B = Mesh->GetParentBone(B))
		{
			if (Mesh->GetBodyInstance(B))
			{
				return B;
			}
		}
		return NAME_None;
	}

	/* A RAGDOLL BUILT TO FALL DOWN, NOT TO KNEEL. The MetaHumans' physics assets let a knee bend
	   60 degrees, a hip lift 65 and the spine curl hardly at all, and tie each calf to the pelvis
	   with an extra constraint of their own - fine for a body dropping dead, impossible for one on
	   its knees: the first kneel began outside the limits and the solver threw both wrestlers a
	   metre and a half into the air putting them back (the lab, 2026-11-13). For the mat only, on
	   this instance only, the joints get a wrestler's range and the extra ties go. */
	void LoosenJoints(USkeletalMeshComponent* Body)
	{
		for (FConstraintInstance* Joint : Body->Constraints)
		{
			if (!Joint)
			{
				continue;
			}
			const FString Child = Joint->ConstraintBone1.ToString();
			if (Joint->JointName.ToString().StartsWith(TEXT("UserConstraint")))
			{
				Joint->TermConstraint();
				continue;
			}
			auto Range = [Joint](float Swing1, float Swing2, float Twist)
			{
				Joint->SetAngularSwing1Limit(ACM_Limited, FMath::Max(Swing1, Joint->ProfileInstance.ConeLimit.Swing1LimitDegrees));
				Joint->SetAngularSwing2Limit(ACM_Limited, FMath::Max(Swing2, Joint->ProfileInstance.ConeLimit.Swing2LimitDegrees));
				Joint->SetAngularTwistLimit(ACM_Limited, FMath::Max(Twist, Joint->ProfileInstance.TwistLimit.TwistLimitDegrees));
			};
			if (Child.StartsWith(TEXT("calf_")) || Child.StartsWith(TEXT("lowerarm_")))
			{
				Range(5.0f, 5.0f, 150.0f);       // the hinge is this asset's twist axis
			}
			else if (Child.StartsWith(TEXT("thigh_")))
			{
				Range(125.0f, 70.0f, 45.0f);
			}
			else if (Child.StartsWith(TEXT("upperarm_")))
			{
				Range(100.0f, 100.0f, 60.0f);
			}
			else if (Child.StartsWith(TEXT("spine_")))
			{
				Range(45.0f, 45.0f, 40.0f);
			}
			else if (Child.StartsWith(TEXT("neck_")) || Child == TEXT("head"))
			{
				Range(30.0f, 30.0f, 40.0f);
			}
		}
	}

	const TCHAR* MoveName(int32 Move)
	{
		static const TCHAR* Names[] = { TEXT("kneel"), TEXT("tie"), TEXT("shoot"), TEXT("godown"), TEXT("guard"), TEXT("top"),
			TEXT("flat"), TEXT("bridge"), TEXT("base"), TEXT("back"), TEXT("sit"), TEXT("tall") };
		return Move >= 0 && Move < UE_ARRAY_COUNT(Names) ? Names[Move] : TEXT("?");
	}

	const TCHAR* BeatName(int32 Beat)
	{
		static const TCHAR* Names[] = { TEXT("tie-up"), TEXT("shot"), TEXT("ground"), TEXT("ride"), TEXT("kick-out"), TEXT("rise"), TEXT("over") };
		return Beat >= 0 && Beat < UE_ARRAY_COUNT(Names) ? Names[Beat] : TEXT("?");
	}
}

AWrestlingMat::AWrestlingMat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;   // the pushes go in before the step that uses them

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	SurfaceMaterial = SurfaceFinder.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Mat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mat"));
	Mat->SetupAttachment(SceneRoot);
	Mat->SetStaticMesh(CubeMesh.Object);
	Mat->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Markings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Markings"));
	Markings->SetupAttachment(SceneRoot);
	Markings->SetStaticMesh(CubeMesh.Object);
	Markings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Markings->SetCastShadow(false);

	for (int32 K = 0; K < 3; ++K)
	{
		USpotLightComponent* Lamp = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Lamp%d"), K));
		Lamp->SetupAttachment(SceneRoot);
		Lamp->IntensityUnits = ELightUnits::Candelas;   // set from lux at the mat, as the premiere's are
		Lamps.Add(Lamp);
	}
}

void AWrestlingMat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildMat();
	PaintMat();
}

void AWrestlingMat::BeginPlay()
{
	Super::BeginPlay();
	BuildMat();
	PaintMat();
	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles() & 0x7fffffff));
	for (float& Phase0 : Sway)
	{
		Phase0 = Rng.FRandRange(0.0f, 2.0f * PI);
	}
	// Tuning switches for a test level: -WrestleLog for the line a second, -WrestlePair=N to start
	// on that pairing.
	if (FParse::Param(FCommandLine::Get(), TEXT("WrestleLog")))
	{
		bLog = true;
	}
	int32 FirstPair = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("WrestlePair="), FirstPair))
	{
		PairIndex = FMath::Max(0, FirstPair) % WrestlingMatDetail::PairCount;
	}
	if (bAutoStart)
	{
		AutoStartIn = 1.5f;   // a moment for the level to settle before two MetaHumans arrive
	}
}

void AWrestlingMat::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearPair();
	Super::EndPlay(Reason);
}

void AWrestlingMat::BuildMat()
{
	using namespace WrestlingMatDetail;
	Mat->SetRelativeLocation(FVector(0.0, 0.0, MatThick * 0.5));
	Mat->SetRelativeScale3D(FVector(MatHalf * 2.0 / ShapeSize, MatHalf * 2.0 / ShapeSize, MatThick / ShapeSize));

	// The circle and the centre mark: short straight pieces laid round, just proud of the mat.
	TArray<FTransform> Pieces;
	auto Ring = [&](double Radius, int32 Segments, double Width)
	{
		const double Piece = 2.0 * PI * Radius / Segments * 1.04;
		for (int32 I = 0; I < Segments; ++I)
		{
			const double A = 2.0 * PI * (I + 0.5) / Segments;
			const FVector At(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, MatThick + 0.4);
			Pieces.Add(FTransform(FQuat(FVector::UpVector, A + PI * 0.5), At, FVector(Piece, Width, 0.8) / ShapeSize));
		}
	};
	Ring(CircleRadius, 72, 10.0);
	Ring(50.0, 20, 6.0);
	Markings->ClearInstances();
	Markings->AddInstances(Pieces, false);

	// Three warm lamps above the circle, aimed at the middle: a key and two softer.
	const FVector Aim(0.0, 0.0, MatThick + 40.0);
	for (int32 K = 0; K < Lamps.Num(); ++K)
	{
		USpotLightComponent* Lamp = Lamps[K];
		if (!Lamp)
		{
			continue;
		}
		const double A = 2.0 * PI * K / Lamps.Num() + PI / 6.0;
		const FVector At(FMath::Cos(A) * (CircleRadius + 250.0), FMath::Sin(A) * (CircleRadius + 250.0), 420.0);
		const FVector To = Aim - At;
		const double Metres = To.Size() / 100.0;
		Lamp->SetRelativeLocationAndRotation(At, FRotationMatrix::MakeFromX(To).ToQuat());
		Lamp->SetLightColor(LampWarm);
		Lamp->SetIntensity(static_cast<float>(LampLux * (K == 0 ? 1.0 : 0.6) * Metres * Metres));
		Lamp->SetAttenuationRadius(static_cast<float>(To.Size() * 2.0));
		Lamp->SetSourceRadius(40.0f);
		Lamp->SetOuterConeAngle(40.0f);
		Lamp->SetInnerConeAngle(22.0f);
	}
}

void AWrestlingMat::PaintMat()
{
	using namespace WrestlingMatDetail;
	if (!SurfaceMaterial)
	{
		return;
	}
	auto Paint = [this](UMeshComponent* Mesh, const FLinearColor& Color, float Roughness)
	{
		if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SurfaceMaterial, this))
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			M->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
			Mesh->SetMaterial(0, M);
		}
	};
	Paint(Mat, MatBlue, 0.75f);
	Paint(Markings, LineRed, 0.6f);
}

// --- the interaction ----------------------------------------------------------------------

void AWrestlingMat::Interact_Implementation(AActor* Interactor)
{
	if (Phase == EPhase::Idle)
	{
		StartBout();
	}
	else
	{
		StopBout();
	}
}

FText AWrestlingMat::GetInteractionPrompt_Implementation() const
{
	return Phase == EPhase::Idle
		? FText::FromString(FString::Printf(TEXT("Wrestle: %s [E]"), *NextPairText()))
		: FText::FromString(TEXT("Stop the match [E]"));
}

FString AWrestlingMat::NextPairText() const
{
	using namespace WrestlingMatDetail;
	const int32* Pair = Pairs[PairIndex % PairCount];
	return FString::Printf(TEXT("%s vs %s"), Agents[Pair[0]].Name, Agents[Pair[1]].Name);
}

// --- the bout -------------------------------------------------------------------------------

void AWrestlingMat::StartBout()
{
	using namespace WrestlingMatDetail;
	if (Phase != EPhase::Idle)
	{
		return;
	}
	if (!SpawnPair())
	{
		ClearPair();
		Announce(TEXT("The wrestlers are not here"), 3.0f);
		return;
	}
	W[0].Falls = 0;
	W[1].Falls = 0;
	Clock = BoutSeconds;
	WinnerSlot = -1;
	PairIndex = (PairIndex + 1) % PairCount;
	PlaceForStart();
	Announce(FString::Printf(TEXT("%s  VS  %s"), *W[0].Name.ToUpper(), *W[1].Name.ToUpper()), 2.5f);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s vs %s: best of %d falls, %.0f s."), *W[0].Name, *W[1].Name, FallsToWin * 2 - 1, BoutSeconds);
}

void AWrestlingMat::StopBout()
{
	ClearPair();
	Phase = EPhase::Idle;
	PhaseAge = 0.0f;
	PinnedSlot = -1;
	Count = 0;
	Note.Reset();
	NoteFor = 0.0f;
}

bool AWrestlingMat::SpawnPair()
{
	using namespace WrestlingMatDetail;
	ClearPair();
	const int32* Pair = Pairs[PairIndex % PairCount];
	return SpawnWrestler(0, Pair[0]) && SpawnWrestler(1, Pair[1]);
}

bool AWrestlingMat::SpawnWrestler(int32 Slot, int32 Who)
{
	using namespace WrestlingMatDetail;
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// Loaded by path, as the premiere's cast is: the MetaHumans are not in the public repo.
	UClass* Class = LoadClass<AActor>(nullptr, Agents[Who].Class, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Class)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Wrestle] no blueprint for %s."), Agents[Who].Name);
		return false;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(Class, GetActorTransform(), Params);
	if (!Actor)
	{
		return false;
	}
	Actors.Add(Actor);

	FWrestler& Wr = W[Slot];
	Wr = FWrestler();
	Wr.Name = Agents[Who].Name;
	Wr.Actor = Actor;

	// HER BODY is the one skeletal mesh with a physics asset that copies nobody: the face, the
	// clothes and the hair follow it (the Refusers' ragdoll lesson, SlapComponent.cpp).
	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetPhysicsAsset() && !Mesh->LeaderPoseComponent.IsValid()
			&& (!Wr.Body || Mesh->GetName().Contains(TEXT("Body"))))
		{
			Wr.Body = Mesh;
		}
	}
	if (!Wr.Body)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Wrestle] %s has no body to wrestle with."), *Wr.Name);
		return false;
	}
	USkeletalMeshComponent* Body = Wr.Body;

	// Everything else rides along and touches nothing.
	TArray<UPrimitiveComponent*> Parts;
	Actor->GetComponents<UPrimitiveComponent>(Parts);
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part && Part != Body)
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Not a dancer: the dancers' five-second scan adopts any mesh that names a dance, and a dance
	// would take her body back from the muscles.
	Body->AnimationData.AnimToPlay = nullptr;
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UGrapplePoseAnimInstance::StaticClass());
	Wr.Pose = Cast<UGrapplePoseAnimInstance>(Body->GetAnimInstance());
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->bEnableUpdateRateOptimizations = false;

	// A ragdoll that the player walks through and looks through: the mat's [E] stays in reach
	// whoever is lying on it.
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Wr.BodyParent = Body->GetAttachParent();
	Wr.BodyRelative = Body->GetRelativeTransform();

	/* NOTHING ABOUT HER CAN WHIP. Every body is given drag, and a limit on how fast it may turn: a
	   limb that is let go swings to rest instead of flailing, and a wrestler who is pushed leans
	   instead of bouncing. Wrestling is slow strain; the first match was not (the header). */
	for (FBodyInstance* Part : Body->Bodies)
	{
		if (Part)
		{
			Part->SetMaxDepenetrationVelocity(EaseOut);
			Part->LinearDamping = LinearDrag;
			Part->AngularDamping = AngularDrag;
			Part->UpdateDampingProperties();
			Part->SetMaxAngularVelocityInRadians(MaxSpin, false);
		}
	}
	LoosenJoints(Body);

	Wr.Muscles = NewObject<UPhysicalAnimationComponent>(Actor, TEXT("WrestleMuscles"));
	Wr.Muscles->RegisterComponent();
	Wr.Muscles->SetSkeletalMeshComponent(Body);

	Wr.Pelvis = FirstBody(Body, TEXT("pelvis"));
	Wr.Chest = FirstBody(Body, TEXT("spine_05"));
	Wr.Neck = FirstBody(Body, TEXT("neck_01"));
	Wr.Hand[0] = FirstBody(Body, TEXT("hand_l"));
	Wr.Hand[1] = FirstBody(Body, TEXT("hand_r"));
	Wr.UpperArm[0] = FirstBody(Body, TEXT("upperarm_l"));
	Wr.UpperArm[1] = FirstBody(Body, TEXT("upperarm_r"));
	Wr.LowerArm[0] = FirstBody(Body, TEXT("lowerarm_l"));
	Wr.LowerArm[1] = FirstBody(Body, TEXT("lowerarm_r"));
	for (const TCHAR* Part : { TEXT("spine_05"), TEXT("spine_02"), TEXT("pelvis"), TEXT("upperarm_l"), TEXT("upperarm_r"),
		TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("neck_01") })
	{
		const FName Body2 = FirstBody(Body, Part);
		if (!Body2.IsNone())
		{
			Wr.Holdable.AddUnique(Body2);
		}
	}
	if (Wr.Pelvis.IsNone() || Wr.Chest.IsNone() || Wr.Pelvis == Wr.Chest)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Wrestle] %s's physics asset has no separate pelvis and chest."), *Wr.Name);
		return false;
	}
	if (Wr.Neck == Wr.Chest)
	{
		Wr.Neck = NAME_None;   // no neck body of its own: the chest's strength does for it
	}
	// Every body from her hips up, legs apart: what Support takes the weight of (Act).
	for (const FBodyInstance* Part : Body->Bodies)
	{
		if (!Part || Part->InstanceBoneIndex < 0)
		{
			continue;
		}
		const FName Bone = Body->GetBoneName(Part->InstanceBoneIndex);
		const bool bLeg = Bone == TEXT("thigh_l") || Bone == TEXT("thigh_r")
			|| Body->BoneIsChildOf(Bone, TEXT("thigh_l")) || Body->BoneIsChildOf(Bone, TEXT("thigh_r"));
		if (!bLeg && (Bone == Wr.Pelvis || Body->BoneIsChildOf(Bone, Wr.Pelvis)))
		{
			Wr.Upper.Add(Bone);
		}
	}

	// HOW FAR DOWN TO KNEEL: in the starting kneel her knees are a thigh's length (tipped forward)
	// below her hips, so her root - at her feet when she stands - goes that much under the mat.
	if (const USkinnedAsset* Asset = Body->GetSkinnedAsset())
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const int32 Thigh = Ref.FindBoneIndex(TEXT("thigh_l"));
		const int32 Calf = Ref.FindBoneIndex(TEXT("calf_l"));
		if (Thigh != INDEX_NONE && Calf != INDEX_NONE)
		{
			const FVector Hip = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Thigh).GetLocation();
			const FVector Knee = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Calf).GetLocation();
			const double ThighLength = (Knee - Hip).Size();
			Wr.KneelDrop = static_cast<float>(FMath::Max(0.0, Hip.Z - ThighLength * FMath::Cos(FMath::DegreesToRadians(KneelHip)) - 6.0));
		}
	}

	Wr.Heart = Rng.FRandRange(0.85f, 1.2f);
	for (float& R : Wr.Rhythm)
	{
		R = Rng.FRandRange(0.0f, 2.0f * PI);
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s: body %s, pelvis %s, chest %s, hands %s/%s, arms %s/%s, %d bodies carried, kneel drop %.0f cm, heart %.2f."),
		*Wr.Name, *Body->GetName(), *Wr.Pelvis.ToString(), *Wr.Chest.ToString(), *Wr.Hand[0].ToString(), *Wr.Hand[1].ToString(),
		*Wr.UpperArm[0].ToString(), *Wr.UpperArm[1].ToString(), Wr.Upper.Num(), Wr.KneelDrop, Wr.Heart);
	return true;
}

void AWrestlingMat::ClearPair()
{
	LetGoAll();   // the joints first: they name the bodies about to go
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
	W[0] = FWrestler();
	W[1] = FWrestler();
}

void AWrestlingMat::PlaceForStart()
{
	using namespace WrestlingMatDetail;
	LetGoAll();   // no hold survives a restart: the bodies are about to be put back
	for (int32 S = 0; S < 2; ++S)
	{
		FWrestler& Wr = W[S];
		if (!Wr.Body || !Wr.Actor)
		{
			continue;
		}
		SetPhysics(Wr, false);
		// Back where her body hangs on her actor, whatever the last fall did with it.
		if (Wr.BodyParent && Wr.Body->GetAttachParent() != Wr.BodyParent)
		{
			Wr.Body->AttachToComponent(Wr.BodyParent, FAttachmentTransformRules::KeepWorldTransform);
		}
		Wr.Body->SetRelativeTransform(Wr.BodyRelative, false, nullptr, ETeleportType::ResetPhysics);

		// On her knees either side of the centre mark, facing each other. A MetaHuman faces her
		// mesh's +Y: a quarter turn less than the way she looks (BasketballCourt.cpp) - and less
		// again whatever turn her blueprint gives the mesh. Isla's and Aisling's are not Kaia's,
		// and they knelt back to back, lunging at nothing (the lab, 2026-11-13).
		const double Side = S == 0 ? -1.0 : 1.0;
		const FVector Spot = GetActorTransform().TransformPosition(FVector(Side * StartGap, 0.0, MatThick - Wr.KneelDrop + KneelLift));
		const double FaceYaw = GetActorRotation().Yaw + (S == 0 ? 0.0 : 180.0);
		const double MeshYaw = Wr.BodyRelative.Rotator().Yaw;
		Wr.Actor->SetActorLocationAndRotation(Spot, FRotator(0.0, FaceYaw - 90.0 - MeshYaw, 0.0), false, nullptr, ETeleportType::ResetPhysics);

		Wr.Stamina = 1.0f;
		Wr.bGoal = false;
		LetGo(Wr, 0);
		LetGo(Wr, 1);
	}
	// The tie-up, round the centre mark - but first the starting kneel, hands on each other, held
	// still until the bodies are live.
	BeginTie(false);
	for (int32 S = 0; S < 2; ++S)
	{
		FWrestler& Wr = W[S];
		if (!Wr.Body)
		{
			continue;
		}
		Wr.Move = EMove::Kneel;
		Wr.MoveAge = 0.0f;
		Wr.Now = PostureFor(Wr);
		AimHands(S, Wr.Now);
		if (Wr.Pose)
		{
			Wr.Pose->SetPosture(Wr.Now);
		}
	}
	Phase = EPhase::Settle;
	PhaseAge = 0.0f;
	ApartFor = 0.0f;
	PinnedSlot = -1;
	PinHeld = 0.0f;
	PinBroken = 0.0f;
	Count = 0;
}

void AWrestlingMat::SetPhysics(FWrestler& Wr, bool bOn)
{
	if (!Wr.Body)
	{
		return;
	}
	FPhysicalAnimationData Muscle;
	Muscle.bIsLocalSimulation = true;   // each body held at its pose RELATIVE to its parent: muscles, not strings
	Muscle.PositionStrength = 0.0f;
	Muscle.VelocityStrength = 0.0f;
	Muscle.MaxLinearForce = 0.0f;
	Muscle.MaxAngularForce = 0.0f;
	if (bOn)
	{
		/* FROM THE PELVIS DOWN, NEVER THE ROOT. These physics assets give the root bone a body of
		   its own, weightless and joined to nothing: simulated, it fell forever and dragged her
		   mesh's frame with it - 55 m under the mat after a few seconds - and Kaia's outfit,
		   drawn in that frame, vanished (the lab, 2026-11-13). The root stays where her actor is. */
		Wr.Body->SetAllBodiesBelowSimulatePhysics(Wr.Pelvis, true, /*bIncludeSelf=*/true);
		Wr.Body->SetAllBodiesBelowPhysicsBlendWeight(Wr.Pelvis, 1.0f, false, /*bIncludeSelf=*/true);
		Wr.Body->WakeAllRigidBodies();
		if (Wr.Muscles)
		{
			Muscle.OrientationStrength = PoseStrength;
			Muscle.AngularVelocityStrength = PoseDamping;
			Wr.Muscles->ApplyPhysicalAnimationSettingsBelow(Wr.Pelvis, Muscle, true);
			/* HER LEGS HOLD A SHAPE; THEY DO NOT LIFT HER. At full strength a leg posed a little
			   straighter than her hips were carried was a stilt, and held her off the mat. */
			Muscle.OrientationStrength = PoseStrength * WrestlingMatDetail::LegTone;
			Muscle.AngularVelocityStrength = PoseDamping * WrestlingMatDetail::LegTone;
			for (const TCHAR* Thigh : { TEXT("thigh_l"), TEXT("thigh_r") })
			{
				const FName Leg = WrestlingMatDetail::FirstBody(Wr.Body, Thigh);
				if (!Leg.IsNone() && Leg != Wr.Pelvis)
				{
					Wr.Muscles->ApplyPhysicalAnimationSettingsBelow(Leg, Muscle, true);
				}
			}
			if (!Wr.Neck.IsNone())
			{
				// Her head is held up and steady: a head that lolls is what makes a body on
				// a mat look hurt.
				Muscle.OrientationStrength = PoseStrength * 1.3f;
				Muscle.AngularVelocityStrength = PoseDamping * 1.3f;
				Wr.Muscles->ApplyPhysicalAnimationSettingsBelow(Wr.Neck, Muscle, true);
			}
		}
	}
	else
	{
		if (Wr.Muscles && Wr.bPhysics)
		{
			Muscle.OrientationStrength = 0.0f;
			Muscle.AngularVelocityStrength = 0.0f;
			Wr.Muscles->ApplyPhysicalAnimationSettingsBelow(Wr.Pelvis, Muscle, true);
		}
		Wr.Body->SetAllBodiesSimulatePhysics(false);
		Wr.Body->SetAllBodiesPhysicsBlendWeight(0.0f);
	}
	Wr.bPhysics = bOn;
}

// --- the script: what the two of them are doing ---------------------------------------------

void AWrestlingMat::SetMove(FWrestler& Wr, EMove Move)
{
	if (Wr.Move != Move)
	{
		Wr.Move = Move;
		Wr.MoveAge = 0.0f;
	}
}

void AWrestlingMat::SetGrips(FWrestler& Wr, EGrip Left, EGrip Right)
{
	const EGrip Want[2] = { Left, Right };
	for (int32 I = 0; I < 2; ++I)
	{
		if (Wr.Grip[I] != Want[I])
		{
			Wr.Grip[I] = Want[I];
			LetGo(Wr, I);   // a hand sent somewhere else lets go of what it had
			Wr.Hold[I].Cool = 0.3f;
		}
	}
}

void AWrestlingMat::SetBeat(EBeat Next)
{
	Beat = Next;
	BeatAge = 0.0f;
	BeatFor = 0.0f;
}

void AWrestlingMat::BeginTie(bool bFromWhereTheyAre)
{
	using namespace WrestlingMatDetail;
	if (bFromWhereTheyAre)
	{
		// Up off the mat. She is on her hands and knees where she knelt before, and the other has
		// been shoved back to where SHE knelt: the tie-up forms again round the same point and line.
		SetBeat(EBeat::Rise);
		BeatFor = RiseSeconds;
	}
	else
	{
		SetBeat(EBeat::Tie);
		BeatFor = Rng.FRandRange(4.0f, 6.5f);
		Axis = GetActorForwardVector();
		Centre = MatTop();
	}
	bGround = false;
	for (FWrestler& Wr : W)
	{
		SetMove(Wr, EMove::Tie);
		SetGrips(Wr, EGrip::Collar, EGrip::Elbow);
		Wr.GripIn = Rng.FRandRange(3.0f, 6.0f);
	}
}

void AWrestlingMat::BeginShot()
{
	using namespace WrestlingMatDetail;
	// The fresher one shoots, give or take.
	const float Edge0 = W[0].Stamina * W[0].Heart + Rng.FRandRange(0.0f, 0.35f);
	const float Edge1 = W[1].Stamina * W[1].Heart + Rng.FRandRange(0.0f, 0.35f);
	Top = Edge0 >= Edge1 ? 0 : 1;
	FWrestler& A = W[Top];
	FWrestler& D = W[1 - Top];
	const FVector Up = MatUp();
	const FVector DP = BonePos(D, D.Pelvis);
	FVector Into = Flat(DP - BonePos(A, A.Pelvis)).GetSafeNormal();
	if (Into.IsNearlyZero())
	{
		Into = Top == 0 ? Axis : -Axis;
	}
	/* WHERE SHE GOES DOWN: FORWARD, ONTO HER FRONT - the snap-down, the takedown wrestling has for
	   someone on her knees. Her shins are flat behind her, so every way over backward or sideways
	   sat her on her own heels or in the other's lap, and she never reached the mat (three tries in
	   the lab, 2026-11-13). Forward there is nothing in the way: her head and chest are pulled down
	   past the other's hip onto her hands, her hips sink, her legs go out behind her; the other
	   sprawls on her shoulders, head to head with her the other way round - and from there turns
	   her onto her back for the pin, and still nobody's legs are between them. GroundAlong is the
	   way she lies, hips to head: toward where the other knelt. */
	GroundAlong = -Into;
	GroundSide = FVector::CrossProduct(Up, GroundAlong).GetSafeNormal() * (Rng.FRand() < 0.5f ? -1.0 : 1.0);
	GroundAt = OnMat(DP, 0.0);
	bGround = true;

	SetBeat(EBeat::Shot);
	BeatFor = ShotSeconds;
	A.Stamina = FMath::Max(0.0f, A.Stamina - 0.10f);
	D.Stamina = FMath::Max(0.0f, D.Stamina - 0.12f);
	SetMove(A, EMove::Shoot);
	SetGrips(A, EGrip::Collar, EGrip::Waist);   // a hand on the back of her neck, and one over her back
	SetGrips(D, EGrip::Post, EGrip::Post);      // hers go to the mat
	Note = FString::Printf(TEXT("%s snaps her down"), *A.Name);
	NoteFor = 2.5f;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s shoots (stamina %.2f v %.2f)."), *A.Name, A.Stamina, D.Stamina);
}

void AWrestlingMat::Direct(float Dt)
{
	using namespace WrestlingMatDetail;
	BeatAge += Dt;
	FWrestler& A = W[Top];
	FWrestler& D = W[1 - Top];
	const FVector Up = MatUp();
	auto Spend = [Dt](FWrestler& Wr, float PerSecond)
	{
		Wr.Stamina = FMath::Clamp(Wr.Stamina - PerSecond * Dt / Wr.Heart, 0.0f, 1.0f);
	};

	// AFTER A FALL, OR THE BELL: the one on top kneels up beside her, and the other gets her breath
	// and sits up. Nobody lies still on the mat for long - that is the other kind of scene.
	if (Phase == EPhase::Fall || Phase == EPhase::Won)
	{
		if (Beat != EBeat::Over)
		{
			SetBeat(EBeat::Over);
		}
		for (int32 S = 0; S < 2; ++S)
		{
			FWrestler& Wr = W[S];
			const bool bWonIt = Phase == EPhase::Won && WinnerSlot == S;
			if (bGround && S != Top)
			{
				// On her back a moment, then over onto her front and up onto her knees, head down.
				SetMove(Wr, BeatAge < 1.4f ? EMove::Flat : BeatAge < 3.0f ? EMove::Base : EMove::Sit);
				SetGrips(Wr, BeatAge < 1.4f ? EGrip::Ready : BeatAge < 3.0f ? EGrip::Post : EGrip::Rest,
					BeatAge < 1.4f ? EGrip::Ready : BeatAge < 3.0f ? EGrip::Post : EGrip::Rest);
			}
			else
			{
				SetMove(Wr, bWonIt ? EMove::Tall : EMove::Back);
				SetGrips(Wr, bWonIt ? EGrip::High : EGrip::Rest, bWonIt ? EGrip::High : EGrip::Rest);
			}
		}
		return;
	}
	if (Phase != EPhase::Wrestle)
	{
		return;   // settling on their knees: nothing to decide yet
	}

	switch (Beat)
	{
	case EBeat::Tie:
	{
		// Circling: the line between them turns slowly, one way and then the other; and a tie-up
		// that has wandered is walked back toward the middle on their knees.
		const float T = static_cast<float>(GetWorld()->GetTimeSeconds());
		Axis = Axis.RotateAngleAxis(CircleRate * FMath::Sin(T * 0.37f + Sway[2]) * Dt, Up).GetSafeNormal();
		const FVector Home = Flat(MatTop() - Centre);
		if (Home.Size() > 120.0)
		{
			Centre += Home.GetSafeNormal() * (6.0 * Dt);
		}
		for (FWrestler& Wr : W)
		{
			Spend(Wr, Wr.Stamina < 0.35f ? -0.03f : 0.012f);   // a tired one leans on the other and gets some back
			SetMove(Wr, EMove::Tie);
			Wr.GripIn -= Dt;
			if (Wr.GripIn <= 0.0f)
			{
				// Hand-fighting: a new pair of grips every few seconds.
				static const EGrip Ties[][2] = {
					{ EGrip::Collar, EGrip::Elbow }, { EGrip::Collar, EGrip::Waist }, { EGrip::Shoulder, EGrip::Elbow },
					{ EGrip::Waist, EGrip::Shoulder }, { EGrip::Collar, EGrip::Wrist } };
				const int32 Pick = Rng.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Ties)) - 1);
				SetGrips(Wr, Ties[Pick][0], Ties[Pick][1]);
				Wr.GripIn = Rng.FRandRange(3.5f, 7.0f);
			}
		}
		if (BeatAge >= BeatFor)
		{
			BeginShot();
		}
		break;
	}

	case EBeat::Shot:
		Spend(A, 0.03f);
		Spend(D, 0.04f);
		SetMove(D, BeatAge < ShotLead ? EMove::Tie : EMove::GoDown);
		if (BeatAge >= ShotLead + 0.4f)
		{
			SetMove(A, EMove::Top);   // and she sprawls: hips down, legs out behind
		}
		if (BeatAge >= BeatFor)
		{
			SetBeat(EBeat::Ground);
			BeatFor = Rng.FRandRange(2.5f, 4.5f);
			SetMove(A, EMove::Top);
			SetMove(D, EMove::Guard);
		}
		break;

	case EBeat::Ground:
		Spend(A, 0.02f);
		Spend(D, 0.035f);
		if (BeatAge >= BeatFor)
		{
			// Flattened. Whether she has a kick-out left is decided here, by what each has left;
			// when in the count she makes it is hers.
			SetBeat(EBeat::Ride);
			BeatFor = 7.5f;
			SetGrips(A, EGrip::Arms, EGrip::Arms);           // her arms, held to the mat
			SetGrips(D, EGrip::Shoulder, EGrip::Shoulder);   // pushing up at the one on top
			const float Left = D.Stamina * D.Heart - 0.35f * A.Stamina * A.Heart + Rng.FRandRange(-0.15f, 0.15f);
			bEscape = Left > 0.22f;
			EffortAt = Rng.FRandRange(0.95f, 2.35f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s is flattened: %s (%.2f)."), *D.Name,
				bEscape ? TEXT("she has a kick-out in her") : TEXT("she is spent"), Left);
		}
		break;

	case EBeat::Ride:
	{
		Spend(A, 0.02f);
		Spend(D, 0.035f);
		const bool bCounted = PinnedSlot == 1 - Top;
		// Turned over first - her legs stay out behind her until she is on her side. Then, spent
		// and under the count, she still heaves: a bridge that lifts her hips and not her shoulders.
		SetMove(D, BeatAge < TurnSeconds ? EMove::Guard
			: !bEscape && bCounted && FMath::Fmod(PinHeld, 1.3f) > 0.5f ? EMove::Bridge : EMove::Flat);
		if ((bEscape && bCounted && PinHeld >= EffortAt) || BeatAge >= BeatFor)
		{
			SetBeat(EBeat::KickOut);
			BeatFor = KickSeconds;
			D.Stamina = FMath::Max(0.0f, D.Stamina - 0.18f);
			SetMove(D, EMove::Bridge);
		}
		break;
	}

	case EBeat::KickOut:
		Spend(A, 0.02f);
		Spend(D, 0.03f);
		if (BeatAge >= KickTurn)
		{
			// Over onto her front again, and her knees under her; the one on top is shoved back
			// onto her knees, hands still on her.
			SetMove(D, EMove::Base);
			SetGrips(D, EGrip::Post, EGrip::Post);
			SetMove(A, EMove::Back);
			SetGrips(A, EGrip::Shoulder, EGrip::Shoulder);
		}
		if (BeatAge >= BeatFor)
		{
			BeginTie(true);
		}
		break;

	case EBeat::Rise:
		for (FWrestler& Wr : W)
		{
			Spend(Wr, -0.01f);
		}
		if (BeatAge >= BeatFor)
		{
			SetBeat(EBeat::Tie);   // round the same centre and line the rise formed on
			BeatFor = Rng.FRandRange(4.0f, 7.0f);
		}
		break;

	default:
		break;
	}
}

double AWrestlingMat::TiePress() const
{
	// WHO IS DRIVING in the tie-up, -1..1 (+ is wrestler 0): two slow waves and the edge the
	// fresher one has. It comes in gently after the whistle and after every rise.
	if (Phase != EPhase::Wrestle || Beat != EBeat::Tie || !GetWorld())
	{
		return 0.0;
	}
	const double T = GetWorld()->GetTimeSeconds();
	const double Wave = 0.6 * FMath::Sin(T * 0.52 + Sway[0]) + 0.4 * FMath::Sin(T * 1.17 + Sway[1]);
	const double Edge = 0.6 * (W[0].Stamina * W[0].Heart - W[1].Stamina * W[1].Heart);
	return FMath::Clamp(Wave + Edge, -1.2, 1.2) * FMath::Min(1.0, BeatAge / 2.0);
}

float AWrestlingMat::GoalEase() const
{
	// How slowly the goal itself moves, seconds (and it is eased twice): no beat begins with a jerk.
	switch (Beat)
	{
	case EBeat::Tie:     return 0.22f;
	case EBeat::Shot:    return 0.28f;
	case EBeat::Ground:  return 0.35f;
	case EBeat::Ride:    return 0.30f;
	case EBeat::KickOut: return 0.22f;
	default:             return 0.50f;   // the rise, and after a fall
	}
}

AWrestlingMat::FGoal AWrestlingMat::GoalFor(int32 Me) const
{
	using namespace WrestlingMatDetail;
	const FWrestler& Wr = W[Me];
	const FVector Up = MatUp();
	FGoal G;

	/* ON HER KNEES, FACING THE OTHER: hips TieGap from the point between them, leaning in. The one
	   driving (Press toward her side) leans deeper and lower, and the pair is pushed that way; the
	   one giving ground comes more upright. Each is a little to one side, so their heads pass. */
	auto Kneeling = [&](double Press, double ExtraLean)
	{
		const FVector To = Me == 0 ? Axis : -Axis;
		const double Mine = Me == 0 ? Press : -Press;
		const double Lean = FMath::DegreesToRadians(FMath::Max(2.0, TieLean + 8.0 * Mine + ExtraLean));
		const FVector Aside = FVector::CrossProduct(Up, To);   // opposite for the two: each has her head by the other's ear
		G.Pelvis = OnMat(Centre + Axis * (Press * 9.0) - To * TieGap + Aside * 3.0, Wr.KneelHeight - 1.5 * FMath::Max(0.0, Mine));
		G.Torso = (Up * FMath::Cos(Lean) + To * FMath::Sin(Lean) + Aside * 0.28).GetSafeNormal();
		G.Face = To;
		G.Support = 0.85f;
	};
	if (!bGround || Beat == EBeat::Tie || Beat == EBeat::Rise)
	{
		Kneeling(TiePress(), Beat == EBeat::Over ? -28.0 : 0.0);
		return G;
	}

	const bool bTop = Me == Top;
	const FWrestler& Over = W[Top];
	const FWrestler& Under = W[1 - Top];
	const double ShotPress = Top == 0 ? 1.2 : -1.2;

	/* THE ONE BENEATH, ON HER FRONT. Raised is how far she is up off the mat: 0 flat on her front,
	   hips slid forward and legs out behind; 1 folded on her knees with her chest down in front of
	   them (where she goes first, and where she gets up from). */
	FGoal Low;
	auto OnFront = [&](double Raised)
	{
		Low.Pelvis = OnMat(GroundAt + GroundAlong * (ProneSlide * (1.0 - Raised) - 2.0 * Raised), 12.0 + 18.0 * Raised);
		Low.Torso = (GroundAlong + Up * (0.12 - 0.42 * Raised)).GetSafeNormal();
		Low.Face = -Up;
		Low.Support = static_cast<float>(0.35 * Raised);
	};
	// AND ON HER BACK, where she was flat on her front: turned over where she lay.
	auto OnBack = [&]()
	{
		Low.Pelvis = OnMat(GroundAt + GroundAlong * ProneSlide, LieHeight);
		Low.Torso = (GroundAlong + Up * 0.28).GetSafeNormal();
		Low.Face = Up;
		Low.Support = 0.0f;
	};
	/* THE ONE ON TOP is sprawled on her from the head end: chest on the other's shoulders (or
	   chest, once she is turned), a little to one side so the other's head is by her hip and not
	   under it, hips low and back toward where she knelt, legs out behind. Placed from where the
	   one beneath is MEANT to be, not where she is, so the two cannot chase each other. */
	auto OnTop = [&](double Lift)
	{
		const FVector Chest = Low.Pelvis + Low.Torso * Under.SpineLength + GroundAlong * 3.0 + GroundSide * 10.0 + Up * (ChestOnChest + Lift);
		G.Torso = (-GroundAlong + Up * 0.35).GetSafeNormal();
		G.Pelvis = Chest - G.Torso * Over.SpineLength;
		G.Face = (-Up - GroundAlong * 0.3).GetSafeNormal();
		G.Support = 0.45f;
	};

	switch (Beat)
	{
	case EBeat::Shot:
		// The snap first - she sits back and pulls, and the other is bent forward over her own
		// knees - and then down.
		if (BeatAge < ShotLead)
		{
			Kneeling(-ShotPress, bTop ? -6.0 : 16.0);
			return G;
		}
		// Pulled down onto her hands in front of her knees, and then flattened out.
		OnFront(BeatAge < ShotLead + 0.7f ? 1.0 : 0.4);
		OnTop(0.0);
		break;

	case EBeat::Ground:
		// Fighting up off her front: her knees under her, and pressed flat again, slowly.
		OnFront(0.35 + 0.35 * FMath::Sin(BeatAge * 1.6f));
		OnTop(0.0);
		break;

	case EBeat::Ride:
		if (BeatAge < TurnSeconds)
		{
			// Turned over: onto her side first, the one on top easing up to let her come round.
			OnFront(0.0);
			Low.Face = GroundSide;
			OnTop(8.0);
			break;
		}
		OnBack();
		if (Under.Move == EMove::Bridge)
		{
			// The spent one's heave: hips up a hand's breadth, shoulders still down.
			Low.Pelvis += Up * 8.0;
			Low.Torso = (GroundAlong + Up * 0.05).GetSafeNormal();
		}
		OnTop(0.0);
		break;

	case EBeat::KickOut:
		if (BeatAge < KickTurn)
		{
			// The bridge: hips up and a shoulder coming off the mat, the one on top lifted with it.
			OnBack();
			Low.Pelvis += Up * 10.0;
			Low.Torso = (GroundAlong - Up * 0.1).GetSafeNormal();
			Low.Face = (Up + GroundSide * 0.9).GetSafeNormal();
			OnTop(4.0);
		}
		else if (bTop)
		{
			// Shoved back onto her knees, where she knelt before.
			Kneeling(0.0, -8.0);
			return G;
		}
		else
		{
			// Over onto her front, by the side she bridged to, and her knees under her.
			OnFront(1.0);
			if (BeatAge < KickTurn + 0.7f)
			{
				Low.Face = (GroundSide - Up * 0.5).GetSafeNormal();
			}
		}
		break;

	default:   // after a fall, or the bell
		if (bTop)
		{
			Kneeling(0.0, -28.0);
			return G;
		}
		if (BeatAge < 1.4f)
		{
			OnBack();
		}
		else if (BeatAge < 3.0f)
		{
			// Over onto her front and her knees under her - by her side, never straight through.
			OnFront(BeatAge < 2.1f ? 0.3 : 1.0);
			if (BeatAge < 2.1f)
			{
				Low.Face = GroundSide;
			}
		}
		else
		{
			Kneeling(0.0, -22.0);
			return G;
		}
		break;
	}
	return bTop ? G : Low;
}

// --- each wrestler's body -------------------------------------------------------------------

FGrapplePosture AWrestlingMat::PostureFor(const FWrestler& Wr) const
{
	using namespace WrestlingMatDetail;
	const float T = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	auto Wave = [&](int32 K, float Amount, float Speed) { return Amount * FMath::Sin(T * Speed + Wr.Rhythm[K % 6]); };
	FGrapplePosture P;
	auto Legs = [&P](float Hip, float Knee, float Spread)
	{
		P.Hip[0] = P.Hip[1] = Hip;
		P.Knee[0] = P.Knee[1] = Knee;
		P.Spread[0] = P.Spread[1] = Spread;
	};
	/* THE SHAPE SHE HOLDS in each move: spine, head and legs. Her hips and chest are carried to
	   where the beat puts them (Act) and her hands are sent to their grips (AimHands), so these are
	   only what her body does round that - and they change slowly, a breath and a shift of weight,
	   never a writhe. */
	switch (Wr.Move)
	{
	case EMove::Kneel:
		// The starting kneel, before the bodies are live: thighs a little forward, shins flat behind.
		P.Curl = 15.0f;
		Legs(static_cast<float>(KneelHip), static_cast<float>(KneelKnee), 8.0f);
		break;
	case EMove::Tie:
		// Leaning in on her knees, base wide, head up; her weight going from knee to knee.
		P.Curl = 18.0f;
		P.Nod = -22.0f;
		P.Side = Wave(1, 4.0f, 0.7f);
		P.Twist = Wave(2, 6.0f, 0.5f);
		Legs(32.0f, 112.0f, 14.0f);
		break;
	case EMove::Shoot:
		// Driving in low off her knees - knees still under her, not up onto her feet.
		P.Curl = 26.0f;
		P.Nod = -14.0f;
		Legs(40.0f, 105.0f, 16.0f);
		break;
	case EMove::GoDown:
		// Pulled down in front of her knees: folded over them, head up off the mat.
		P.Curl = 16.0f;
		P.Nod = -14.0f;
		Legs(100.0f, 125.0f, 14.0f);
		break;
	case EMove::Guard:
	{
		// On her front. In the ground work she works her knees up under her and is pressed flat
		// again, in time with where her hips are carried (GoalFor); otherwise flat, legs out behind.
		const float Raised = Beat == EBeat::Ground ? 0.35f + 0.35f * FMath::Sin(BeatAge * 1.6f) : Beat == EBeat::Shot ? 0.4f : 0.0f;
		P.Curl = 6.0f + 10.0f * Raised;
		P.Nod = -16.0f;
		P.Twist = Wave(2, 6.0f, 0.9f);
		Legs(8.0f + 90.0f * Raised, 14.0f + 108.0f * Raised, 14.0f);
		break;
	}
	case EMove::Top:
		// Sprawled on her: hips low, legs out behind and wide for a base, shifting; head up.
		P.Curl = 10.0f;
		P.Nod = -14.0f;
		Legs(12.0f, 35.0f, 24.0f);
		P.Hip[0] += Wave(3, 8.0f, 0.6f);
		P.Hip[1] -= Wave(3, 8.0f, 0.6f);
		break;
	case EMove::Flat:
		P.Curl = 2.0f;
		P.Nod = 8.0f;
		Legs(60.0f, 110.0f, 18.0f);   // knees up, feet planted
		break;
	case EMove::Bridge:
		// Feet planted, driving the hips up - a heave, not a buck: a hand's breadth of it.
		P.Curl = -10.0f;
		P.Nod = -8.0f;
		Legs(42.0f, 105.0f, 14.0f);
		break;
	case EMove::Base:
		// On her front with her knees under her, chest still down: where she gets up from.
		P.Curl = 16.0f;
		P.Nod = -12.0f;
		Legs(100.0f, 125.0f, 14.0f);
		break;
	case EMove::Back:
		P.Curl = 8.0f;
		Legs(30.0f, 112.0f, 12.0f);
		break;
	case EMove::Sit:
		// Beaten: on her knees, sat back, head down, getting her breath.
		P.Curl = 28.0f;
		P.Nod = 18.0f;
		Legs(40.0f, 120.0f, 10.0f);
		break;
	case EMove::Tall:
	default:
		P.Curl = 0.0f;
		P.Nod = -6.0f;
		Legs(30.0f, 112.0f, 10.0f);
		break;
	}
	// Breathing, harder as she tires.
	P.Curl += Wave(0, 2.0f + 3.0f * (1.0f - Wr.Stamina), 2.1f);
	return P;
}

bool AWrestlingMat::ChestFrame(const FWrestler& Wr, FVector& Fwd, FVector& Wide, FVector& Tall) const
{
	if (!Wr.Body || Wr.UpperArm[0].IsNone() || Wr.UpperArm[1].IsNone())
	{
		return false;
	}
	// Her chest's own axes, from where her bones are now: toward her left shoulder, the way she
	// faces, and up her spine.
	Wide = (BonePos(Wr, Wr.UpperArm[0]) - BonePos(Wr, Wr.UpperArm[1])).GetSafeNormal();
	Fwd = Facing(Wr);
	const FVector Spine = BonePos(Wr, Wr.Chest) - BonePos(Wr, Wr.Pelvis);
	Tall = (Spine - Wide * FVector::DotProduct(Spine, Wide)).GetSafeNormal();
	return !Wide.IsNearlyZero() && !Fwd.IsNearlyZero() && !Tall.IsNearlyZero();
}

FVector AWrestlingMat::GripPoint(int32 Me, int32 Hand) const
{
	const FWrestler& A = W[Me];
	const FWrestler& B = W[1 - Me];
	static const FName Shoulders[2] = { TEXT("upperarm_l"), TEXT("upperarm_r") };
	static const FName Elbows[2] = { TEXT("lowerarm_l"), TEXT("lowerarm_r") };
	static const FName Hands[2] = { TEXT("hand_l"), TEXT("hand_r") };
	FVector Fwd, Wide, Tall;
	if (!ChestFrame(A, Fwd, Wide, Tall) || !B.Body)
	{
		return BonePos(A, A.Chest);
	}
	const FVector Sh = A.Body->GetBoneLocation(Shoulders[Hand]);
	const FVector Out = Hand == 0 ? Wide : -Wide;
	auto Her = [&B](FName Bone) { return B.Body->GetBoneLocation(Bone); };
	// Face to face, her left hand is by the other's right side.
	const int32 Near = 1 - Hand;
	const FVector HerLeft = (Her(Shoulders[0]) - Her(Shoulders[1])).GetSafeNormal();
	const FVector HerSide = Near == 0 ? HerLeft : -HerLeft;
	const FVector HerFront = Facing(B);
	switch (A.Grip[Hand])
	{
	case EGrip::Collar:   return Her(TEXT("neck_01")) + HerSide * 6.0 - HerFront * 4.0;   // the back of her neck
	case EGrip::Elbow:    return Her(Elbows[Near]);
	case EGrip::Shoulder: return Her(Shoulders[Near]) + HerFront * 4.0;
	case EGrip::Waist:    return Her(TEXT("spine_02")) + HerSide * 13.0 - HerFront * 6.0;   // round her, under the arm
	case EGrip::Wrist:    return Her(Hands[Near]);
	case EGrip::Arms:
	{
		// Whichever of her arms is nearer this hand: held down at the elbow.
		const FVector E0 = Her(Elbows[0]);
		const FVector E1 = Her(Elbows[1]);
		return (E0 - Sh).SizeSquared() <= (E1 - Sh).SizeSquared() ? E0 : E1;
	}
	case EGrip::Post:     return OnMat(Sh + Out * 10.0, 3.0);                    // a hand on the mat under her shoulder
	case EGrip::Rest:     return Sh + Fwd * 14.0 - Tall * 36.0 - Out * 2.0;      // hands on her own thighs
	case EGrip::High:     return Sh + Tall * 48.0 + Out * 14.0 + Fwd * 6.0;      // both arms up: she has won
	case EGrip::Ready:
	default:              return Sh + Fwd * 26.0 - Out * 9.0 - Tall * 10.0;      // hands up in front of her chest
	}
}

void AWrestlingMat::AimHands(int32 Me, FGrapplePosture& P) const
{
	// Each hand's place, turned into her chest's own frame from that arm's shoulder: the pose
	// bends the elbow and turns the shoulder to put the hand there (UGrapplePoseAnimInstance).
	const FWrestler& A = W[Me];
	static const FName Shoulders[2] = { TEXT("upperarm_l"), TEXT("upperarm_r") };
	FVector Fwd, Wide, Tall;
	if (!ChestFrame(A, Fwd, Wide, Tall))
	{
		return;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const FVector To = GripPoint(Me, I) - A.Body->GetBoneLocation(Shoulders[I]);
		const FVector Out = I == 0 ? Wide : -Wide;
		P.Reach[I] = FVector(FVector::DotProduct(To, Fwd), FVector::DotProduct(To, Out), FVector::DotProduct(To, Tall));
	}
}

void AWrestlingMat::Steer(FWrestler& Wr, FName Bone, const FVector& Target, double Rate)
{
	using namespace WrestlingMatDetail;
	if (!Wr.Body || !Wr.bPhysics || Bone.IsNone())
	{
		return;
	}
	/* CARRIED, NOT SHOVED: a critically damped spring from where this part of her is to where it
	   should be. It slows as it arrives, it cannot overshoot, and it can never pull harder than
	   MaxCarry - so whatever is in the way (the other wrestler, the mat) is leaned on, not rammed. */
	const FVector Off = Target - BonePos(Wr, Bone);
	const FVector Vel = Wr.Body->GetPhysicsLinearVelocity(Bone);
	Push(Wr, Bone, (Off * (Rate * Rate) - Vel * (2.0 * Rate)).GetClampedToMaxSize(MaxCarry));
}

void AWrestlingMat::Act(int32 Me, float Dt)
{
	using namespace WrestlingMatDetail;
	FWrestler& A = W[Me];
	FWrestler& B = W[1 - Me];
	if (!A.Body || !B.Body)
	{
		return;
	}
	A.MoveAge += Dt;

	// The muscles' target: the move's shape with her hands sent to their grips, eased toward so a
	// change of move is not a jerk.
	FGrapplePosture Shape = PostureFor(A);
	AimHands(Me, Shape);
	/* Her spine and legs change shape at the pace her body is being carried (GoalEase): legs that
	   took up the next move's shape before her hips had got there straightened under her and
	   stood her up (the lab, 2026-11-13). Her hands are quicker - what they reach for moves. */
	const FVector Reached[2] = { A.Now.Reach[0], A.Now.Reach[1] };
	A.Now = FGrapplePosture::Blend(A.Now, Shape, 1.0f - FMath::Exp(-Dt / (2.0f * FMath::Max(0.05f, GoalEase()))));
	for (int32 I = 0; I < 2; ++I)
	{
		A.Now.Reach[I] = Reached[I].IsNearlyZero() ? Shape.Reach[I] : FMath::Lerp(Reached[I], Shape.Reach[I], 1.0f - FMath::Exp(-Dt * 5.0f));
	}
	if (A.Pose)
	{
		A.Pose->SetPosture(A.Now);
	}
	const float Tone = A.Move == EMove::Flat || A.Move == EMove::Sit ? 0.7f : 1.0f;
	if (A.Muscles && Tone != A.Tone)
	{
		A.Muscles->SetStrengthMultiplyer(Tone);
		A.Tone = Tone;
	}

	if (!A.bPhysics)
	{
		A.bGoal = false;
		return;
	}

	const FVector Up = MatUp();
	const FVector AP = BonePos(A, A.Pelvis);
	const FVector AC = BonePos(A, A.Chest);
	const FVector Spine = (AC - AP).GetSafeNormal();

	// WHERE THE BEAT PUTS HER, eased twice: the goal she is carried to starts from where she is
	// and never jumps, whatever the script does.
	const FGoal Want = GoalFor(Me);
	if (!A.bGoal)
	{
		A.Goal1.Pelvis = AP;
		A.Goal1.Torso = Spine.IsNearlyZero() ? Up : Spine;
		A.Goal1.Face = Facing(A).IsNearlyZero() ? Want.Face : Facing(A);
		A.Goal1.Support = Want.Support;
		A.Goal2 = A.Goal1;
		A.bGoal = true;
	}
	const float Ease = 1.0f - FMath::Exp(-Dt / FMath::Max(0.05f, GoalEase()));
	auto Toward = [Ease](FGoal& Now, const FGoal& To)
	{
		Now.Pelvis = FMath::Lerp(Now.Pelvis, To.Pelvis, Ease);
		const FVector Torso = FMath::Lerp(Now.Torso, To.Torso, Ease).GetSafeNormal();
		const FVector Face = FMath::Lerp(Now.Face, To.Face, Ease).GetSafeNormal();
		Now.Torso = Torso.IsNearlyZero() ? To.Torso : Torso;
		Now.Face = Face.IsNearlyZero() ? To.Face : Face;
		Now.Support = FMath::Lerp(Now.Support, To.Support, Ease);
	};
	Toward(A.Goal1, Want);
	Toward(A.Goal2, A.Goal1);
	const FGoal& G = A.Goal2;

	// A tired wrestler is carried a little less firmly: the other's weight tells on her.
	const double Power = Drive * (0.75 + 0.25 * A.Stamina);
	const double Rate = Carry * FMath::Sqrt(FMath::Max(0.0, Power));
	Steer(A, A.Pelvis, G.Pelvis, Rate);
	Steer(A, A.Chest, G.Pelvis + G.Torso * A.SpineLength, Rate);

	// Her upper body's weight, as much of it as the move says her legs and arms are bearing. Her
	// legs keep all of theirs, so knees and feet stay on the mat.
	const double Lift = -GetWorld()->GetGravityZ() * G.Support;
	if (Lift > 1.0)
	{
		for (const FName Bone : A.Upper)
		{
			Push(A, Bone, Up * Lift);
		}
	}

	// Which way her chest faces: turned about her own spine, the same damped way.
	const FVector Front = Facing(A);
	FVector Aim = G.Face - Spine * FVector::DotProduct(G.Face, Spine);
	if (!Front.IsNearlyZero() && !Spine.IsNearlyZero() && Aim.Size() > 0.2)
	{
		Aim.Normalize();
		const double Off = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Front, Aim), Spine), FVector::DotProduct(Front, Aim));
		for (const FName Bone : { A.Chest, A.Pelvis })
		{
			const double Spin = FVector::DotProduct(A.Body->GetPhysicsAngularVelocityInRadians(Bone), Spine);
			const double Turn = FMath::Clamp(Off * TurnRate * TurnRate * Power - Spin * 2.0 * TurnRate, -MaxTurn, MaxTurn);
			A.Body->AddTorqueInRadians(Spine * Turn, Bone, true);
		}
	}

	TickHolds(Me, Dt);
}

// --- the holds ------------------------------------------------------------------------------

void AWrestlingMat::TickHolds(int32 Me, float Dt)
{
	FWrestler& A = W[Me];
	FWrestler& B = W[1 - Me];
	for (int32 I = 0; I < 2; ++I)
	{
		FHold& H = A.Hold[I];
		if (A.Hand[I].IsNone())
		{
			continue;
		}
		const bool bOnHer = A.Grip[I] <= EGrip::Arms;   // this hand is meant to be on the other wrestler
		if (H.Joint)
		{
			// Held - until it tears loose, her hand is pulled far from what it held, or she lets go
			// to take a better hold.
			H.Age += Dt;
			const bool bTorn = H.Joint->IsBroken();
			const bool bFlung = (BonePos(A, A.Hand[I]) - BonePos(B, H.Target)).Size() > 60.0;
			if (bTorn || bFlung || !bOnHer || H.Age > H.Keep)
			{
				LetGo(A, I);
				A.Hold[I].Cool = bTorn ? 0.8f : 0.4f;
			}
			continue;
		}
		H.Cool -= Dt;
		if (H.Cool > 0.0f || !bOnHer || Phase != EPhase::Wrestle)
		{
			continue;
		}
		// A TOUCH IS A HOLD: the nearest part of her within reach of the hand. The hand gets there
		// by the arm reaching for its grip, not by being pulled.
		const FVector Hand = BonePos(A, A.Hand[I]);
		FName Best;
		double BestGap = GrabReach;
		for (const FName Part : B.Holdable)
		{
			if (const FBodyInstance* Body = B.Body->GetBodyInstance(Part))
			{
				const double Gap = FMath::Sqrt(Body->GetBodyBounds().ComputeSquaredDistanceToPoint(Hand));
				if (Gap < BestGap)
				{
					BestGap = Gap;
					Best = Part;
				}
			}
		}
		if (!Best.IsNone())
		{
			Grab(Me, I, Best);
		}
	}
}

void AWrestlingMat::Grab(int32 Me, int32 HandIndex, FName Target)
{
	FWrestler& A = W[Me];
	FWrestler& B = W[1 - Me];
	UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(this);
	if (!Joint || !A.Body || !B.Body)
	{
		return;
	}
	Joint->RegisterComponent();
	// The joint is where her hand is now: it keeps her hand on that spot of the other's body.
	Joint->SetWorldLocation(BonePos(A, A.Hand[HandIndex]));
	Joint->SetLinearXLimit(LCM_Locked, 0.0f);
	Joint->SetLinearYLimit(LCM_Locked, 0.0f);
	Joint->SetLinearZLimit(LCM_Locked, 0.0f);
	Joint->SetAngularSwing1Limit(ACM_Free, 0.0f);
	Joint->SetAngularSwing2Limit(ACM_Free, 0.0f);
	Joint->SetAngularTwistLimit(ACM_Free, 0.0f);
	Joint->SetDisableCollision(true);
	Joint->SetLinearBreakable(true, HoldBreak);
	Joint->SetConstrainedComponents(A.Body, A.Hand[HandIndex], B.Body, Target);
	Joints.Add(Joint);
	FHold& H = A.Hold[HandIndex];
	H.Joint = Joint;
	H.Target = Target;
	H.Age = 0.0f;
	H.Keep = Rng.FRandRange(6.0f, 12.0f);
}

void AWrestlingMat::LetGo(FWrestler& Wr, int32 HandIndex)
{
	FHold& H = Wr.Hold[HandIndex];
	if (H.Joint)
	{
		H.Joint->BreakConstraint();
		H.Joint->DestroyComponent();
		Joints.Remove(H.Joint);
	}
	H = FHold();
}

void AWrestlingMat::LetGoAll()
{
	for (FWrestler& Wr : W)
	{
		LetGo(Wr, 0);
		LetGo(Wr, 1);
	}
	for (UPhysicsConstraintComponent* Joint : Joints)
	{
		if (Joint)
		{
			Joint->DestroyComponent();
		}
	}
	Joints.Reset();
}

int32 AWrestlingMat::HoldCount(const FWrestler& Wr) const
{
	return (Wr.Hold[0].Joint ? 1 : 0) + (Wr.Hold[1].Joint ? 1 : 0);
}

void AWrestlingMat::Push(FWrestler& Wr, FName Bone, const FVector& Accel)
{
	if (Wr.Body && Wr.bPhysics && !Bone.IsNone())
	{
		Wr.Body->AddForce(Accel, Bone, /*bAccelChange=*/true);
	}
}

// --- the referee ----------------------------------------------------------------------------

void AWrestlingMat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AutoStartIn > 0.0f)
	{
		AutoStartIn -= DeltaSeconds;
		if (AutoStartIn <= 0.0f)
		{
			StartBout();
		}
	}
	if (Phase == EPhase::Idle)
	{
		return;
	}
	TickBout(FMath::Min(DeltaSeconds, 0.05f));
	TickLabShots(DeltaSeconds);
	if (Phase == EPhase::Idle)
	{
		return;
	}
	ShowStatus();
	if (bLog)
	{
		LogAge += DeltaSeconds;
		if (LogAge >= 1.0f)
		{
			LogAge = 0.0f;
			LogState();
		}
	}
}

void AWrestlingMat::TickLabShots(float Dt)
{
	/* THE LAB'S EYES: -WrestleShots=N on the command line takes a picture every N seconds of a
	   match, from the level's first CameraActor - so a match run offscreen (-RenderOffscreen) can
	   still be looked at. Nothing happens without the switch. */
	if (LabShotEvery < 0.0f)
	{
		LabShotEvery = 0.0f;
		float Every = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("WrestleShots="), Every) && Every > 0.0f)
		{
			LabShotEvery = Every;
		}
	}
	if (LabShotEvery <= 0.0f || Phase == EPhase::Idle)
	{
		return;
	}
	if (!bLabCamera)
	{
		bLabCamera = true;
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		for (TActorIterator<ACameraActor> It(GetWorld()); It && PC; ++It)
		{
			PC->SetViewTargetWithBlend(*It, 0.0f);
			break;
		}
		if (PC && PC->GetHUD())
		{
			PC->GetHUD()->bShowHUD = false;   // the pictures are of the match, not the screen
		}
	}
	LabShotIn -= Dt;
	if (LabShotIn <= 0.0f)
	{
		LabShotIn = LabShotEvery;
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Wrestle_%03d.png"), LabShotIndex++), /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	}
}

void AWrestlingMat::TickBout(float Dt)
{
	using namespace WrestlingMatDetail;
	PhaseAge += Dt;
	if (NoteFor > 0.0f)
	{
		NoteFor -= Dt;
	}
	switch (Phase)
	{
	case EPhase::Settle:
		if (!W[0].bPhysics && PhaseAge >= SettleKinematic)
		{
			for (int32 S = 0; S < 2; ++S)
			{
				// Which way her chest faces, measured now while she kneels facing the other.
				const FVector Toward = BonePos(W[1 - S], W[1 - S].Chest) - BonePos(W[S], W[S].Chest);
				W[S].FacingSign = 1.0f;
				if (FVector::DotProduct(Facing(W[S]), Toward) < 0.0)
				{
					W[S].FacingSign = -1.0f;
				}
				// And her own measures, while she kneels as posed: how high her hips are on her
				// knees, and how far it is from hips to chest. She is posed KneelLift above the mat
				// and sinks a little more as her knees take her weight: without both she was
				// carried too high, and knelt on air (the lab, 2026-11-13).
				W[S].KneelHeight = FMath::Clamp(Height(BonePos(W[S], W[S].Pelvis)) - static_cast<float>(KneelLift) - KneelSink, 35.0f, 65.0f);
				W[S].SpineLength = FMath::Clamp(static_cast<float>((BonePos(W[S], W[S].Chest) - BonePos(W[S], W[S].Pelvis)).Size()), 12.0f, 45.0f);
				SetPhysics(W[S], true);
				SetMove(W[S], EMove::Tie);
			}
		}
		Direct(Dt);
		Act(0, Dt);
		Act(1, Dt);
		if (PhaseAge >= SettleSeconds)
		{
			Phase = EPhase::Wrestle;
			PhaseAge = 0.0f;
			BeatAge = 0.0f;
		}
		break;

	case EPhase::Wrestle:
	{
		Clock -= Dt;
		Direct(Dt);
		Act(0, Dt);
		Act(1, Dt);
		for (int32 S = 0; S < 2; ++S)
		{
			if (LooksBroken(S))
			{
				Restart(TEXT("The referee stops it: back to the centre"));
				return;
			}
		}
		// Out of bounds: both chests over the circle, or anyone off the mat.
		const FVector Middle = MatTop();
		const bool bOutA = Flat(BonePos(W[0], W[0].Chest) - Middle).Size() > CircleRadius + 40.0;
		const bool bOutB = Flat(BonePos(W[1], W[1].Chest) - Middle).Size() > CircleRadius + 40.0;
		const bool bOffA = Flat(BonePos(W[0], W[0].Pelvis) - Middle).Size() > MatHalf - 20.0;
		const bool bOffB = Flat(BonePos(W[1], W[1].Pelvis) - Middle).Size() > MatHalf - 20.0;
		if ((bOutA && bOutB) || bOffA || bOffB)
		{
			Restart(TEXT("Out of the circle: back to the centre"));
			return;
		}
		// Apart and getting nowhere: back to their knees in the middle.
		if (Flat(BonePos(W[0], W[0].Chest) - BonePos(W[1], W[1].Chest)).Size() > 110.0)
		{
			ApartFor += Dt;
			if (ApartFor > BreakApartSeconds)
			{
				Restart(TEXT("They break apart: back to the centre"));
				return;
			}
		}
		else
		{
			ApartFor = 0.0f;
		}
		TickPin(Dt);
		if (Phase == EPhase::Wrestle && Clock <= 0.0f)
		{
			Clock = 0.0f;
			if (W[0].Falls == W[1].Falls)
			{
				WinnerSlot = -1;
				Announce(TEXT("TIME!  A DRAW"), 5.0f);
			}
			else
			{
				WinnerSlot = W[0].Falls > W[1].Falls ? 0 : 1;
				Announce(FString::Printf(TEXT("TIME!  %s WINS ON FALLS"), *W[WinnerSlot].Name.ToUpper()), 5.0f);
			}
			Phase = EPhase::Won;
			PhaseAge = 0.0f;
		}
		break;
	}

	case EPhase::Fall:
		Direct(Dt);
		Act(0, Dt);
		Act(1, Dt);
		if (PhaseAge >= FallSeconds)
		{
			if (WinnerSlot >= 0 && W[WinnerSlot].Falls >= FallsToWin)
			{
				Phase = EPhase::Won;
				PhaseAge = 0.0f;
				Announce(FString::Printf(TEXT("%s WINS THE MATCH"), *W[WinnerSlot].Name.ToUpper()), 6.0f);
				UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s wins %d-%d."), *W[WinnerSlot].Name, W[WinnerSlot].Falls, W[1 - WinnerSlot].Falls);
			}
			else
			{
				PlaceForStart();
			}
		}
		break;

	case EPhase::Won:
		Direct(Dt);
		Act(0, Dt);
		Act(1, Dt);
		if (PhaseAge >= WonSeconds)
		{
			StopBout();
			if (bAutoStart)
			{
				AutoStartIn = 1.0f;
			}
		}
		break;

	default:
		break;
	}
}

void AWrestlingMat::TickPin(float Dt)
{
	if (PinnedSlot < 0)
	{
		for (int32 S = 0; S < 2; ++S)
		{
			if (IsPinned(S))
			{
				PinnedSlot = S;
				PinHeld = 0.0f;
				PinBroken = 0.0f;
				Count = 0;
				break;
			}
		}
		return;
	}
	if (IsPinned(PinnedSlot))
	{
		PinHeld += Dt;
		PinBroken = 0.0f;
		const int32 Reached = FMath::FloorToInt(PinHeld / CountSeconds);
		if (Reached > Count)
		{
			Count = Reached;
			if (Count >= 3)
			{
				EndFall(1 - PinnedSlot, FString::Printf(TEXT("%s pinned"), *W[PinnedSlot].Name));
				return;
			}
			Announce(Count == 1 ? TEXT("ONE") : TEXT("TWO"), CountSeconds * 0.9f);
			Note = FString::Printf(TEXT("%s is pinned: %s..."), *W[PinnedSlot].Name, Count == 1 ? TEXT("one") : TEXT("two"));
			NoteFor = 2.0f;
		}
	}
	else
	{
		PinBroken += Dt;
		if (PinBroken > 0.35f)
		{
			if (Count >= 1)
			{
				Note = FString::Printf(TEXT("%s kicks out at %s!"), *W[PinnedSlot].Name, Count == 1 ? TEXT("one") : TEXT("two"));
				NoteFor = 3.0f;
				UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] %s kicks out at %d."), *W[PinnedSlot].Name, Count);
			}
			PinnedSlot = -1;
			Count = 0;
			PinHeld = 0.0f;
		}
	}
}

bool AWrestlingMat::IsPinned(int32 Slot) const
{
	const FWrestler& A = W[Slot];
	const FWrestler& B = W[1 - Slot];
	if (!A.Body || !B.Body || A.UpperArm[0].IsNone() || A.UpperArm[1].IsNone())
	{
		return false;
	}
	// Once she bridges out the count is broken, however long her shoulders take to leave the mat.
	if (Beat == EBeat::KickOut && Slot != Top)
	{
		return false;
	}
	// On her back, both shoulders down, and the other one over her chest.
	if (FVector::DotProduct(Facing(A), MatUp()) < 0.4)
	{
		return false;
	}
	if (Height(BonePos(A, A.UpperArm[0])) > PinHeight || Height(BonePos(A, A.UpperArm[1])) > PinHeight)
	{
		return false;
	}
	const FVector AC = BonePos(A, A.Chest);
	const FVector BC = BonePos(B, B.Chest);
	return Flat(BC - AC).Size() < 65.0 && Height(BC) > Height(AC) + 4.0;
}

void AWrestlingMat::EndFall(int32 Winner, const FString& Why)
{
	FWrestler& Victor = W[Winner];
	++Victor.Falls;
	WinnerSlot = Winner;
	Announce(FString::Printf(TEXT("THREE!  %s TAKES THE FALL"), *Victor.Name.ToUpper()), WrestlingMatDetail::FallSeconds);
	Note = FString::Printf(TEXT("%s: %d fall%s"), *Victor.Name, Victor.Falls, Victor.Falls == 1 ? TEXT("") : TEXT("s"));
	NoteFor = WrestlingMatDetail::FallSeconds;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] fall to %s (%s), %d-%d."), *Victor.Name, *Why, W[0].Falls, W[1].Falls);
	Phase = EPhase::Fall;
	PhaseAge = 0.0f;
	PinnedSlot = -1;
	Count = 0;
	PinHeld = 0.0f;
	if (bGround)
	{
		Top = Winner;   // whoever took the fall is the one who kneels up beside the other (Direct)
	}
	for (FWrestler& Wr : W)
	{
		LetGo(Wr, 0);
		LetGo(Wr, 1);
	}
}

void AWrestlingMat::Restart(const FString& Why)
{
	Note = Why;
	NoteFor = 3.0f;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] restart: %s"), *Why);
	PlaceForStart();
}

bool AWrestlingMat::LooksBroken(int32 Slot) const
{
	const FWrestler& Wr = W[Slot];
	if (!Wr.Body)
	{
		return false;
	}
	// A body flung faster than anyone moves, or through the mat, or into the air: the physics
	// has come apart, and the referee calls it before anyone sees much of it.
	for (const FName Bone : { Wr.Pelvis, Wr.Chest })
	{
		if (Wr.Body->GetPhysicsLinearVelocity(Bone).Size() > 2500.0)
		{
			return true;
		}
		const double H = Height(BonePos(Wr, Bone));
		if (H < -40.0 || H > 350.0)
		{
			return true;
		}
	}
	return false;
}

// --- where things are -----------------------------------------------------------------------

FVector AWrestlingMat::BonePos(const FWrestler& Wr, FName Bone) const
{
	return Wr.Body && !Bone.IsNone() ? Wr.Body->GetBoneLocation(Bone) : GetActorLocation();
}

FVector AWrestlingMat::MatUp() const
{
	return GetActorUpVector();
}

FVector AWrestlingMat::MatTop() const
{
	return GetActorLocation() + MatUp() * MatThick;
}

float AWrestlingMat::Height(const FVector& P) const
{
	return static_cast<float>(FVector::DotProduct(P - MatTop(), MatUp()));
}

FVector AWrestlingMat::OnMat(const FVector& P, double AtHeight) const
{
	// The point above (or below) P that is AtHeight over the mat.
	return P + MatUp() * (AtHeight - Height(P));
}

FVector AWrestlingMat::Flat(const FVector& V) const
{
	const FVector Up = MatUp();
	return V - Up * FVector::DotProduct(V, Up);
}

FVector AWrestlingMat::Facing(const FWrestler& Wr) const
{
	// The way her chest faces: across her shoulders, crossed with up her spine.
	const FVector Spine = BonePos(Wr, Wr.Chest) - BonePos(Wr, Wr.Pelvis);
	const FVector Across = BonePos(Wr, Wr.UpperArm[0]) - BonePos(Wr, Wr.UpperArm[1]);
	return FVector::CrossProduct(Spine, Across).GetSafeNormal() * Wr.FacingSign;
}

// --- what he sees ---------------------------------------------------------------------------

void AWrestlingMat::Announce(const FString& Text, float Seconds)
{
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] \"%s\""), *Text);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ShowBanner(Text, Seconds);
		}
	}
}

void AWrestlingMat::ShowStatus()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Hud || !Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > 3000.0)
	{
		return;
	}
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Clock));
	const FString Status = FString::Printf(TEXT("%s  %d    -    %d  %s          %d:%02d"),
		*W[0].Name.ToUpper(), W[0].Falls, W[1].Falls, *W[1].Name.ToUpper(), Seconds / 60, Seconds % 60);
	Hud->ShowTablePanel(Status, TEXT("[E] at the mat: stop the match"), 0.25f, NoteFor > 0.0f ? Note : FString());
}

void AWrestlingMat::LogState()
{
	using namespace WrestlingMatDetail;
	static const TCHAR* PhaseNames[] = { TEXT("idle"), TEXT("settle"), TEXT("wrestle"), TEXT("fall"), TEXT("won") };
	FString Line = FString::Printf(TEXT("[Wrestle] %s %.0fs clock %.0f %s %.1fs |"), PhaseNames[static_cast<int32>(Phase)], PhaseAge, Clock,
		BeatName(static_cast<int32>(Beat)), BeatAge);
	for (int32 S = 0; S < 2; ++S)
	{
		const FWrestler& Wr = W[S];
		if (!Wr.Body)
		{
			continue;
		}
		// "moving" is how fast her chest is going, cm/s: a strain is tens, a convulsion is hundreds.
		Line += FString::Printf(TEXT(" %s %s chest %.0f pelvis %.0f (goal %.0f) moving %.0f faceup %.2f stam %.2f grips %d%d |"),
			*Wr.Name, MoveName(static_cast<int32>(Wr.Move)),
			Height(BonePos(Wr, Wr.Chest)), Height(BonePos(Wr, Wr.Pelvis)), Height(Wr.Goal2.Pelvis),
			Wr.Body->GetPhysicsLinearVelocity(Wr.Chest).Size(),
			FVector::DotProduct(Facing(Wr), MatUp()), Wr.Stamina,
			Wr.Hold[0].Joint ? 1 : 0, Wr.Hold[1].Joint ? 1 : 0);
	}
	if (PinnedSlot >= 0)
	{
		Line += FString::Printf(TEXT(" PIN %s %d"), *W[PinnedSlot].Name, Count);
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("%s"), *Line);
}
