// BasketballCourt.cpp - see the header for what this actor is for and what it must never do.

#include "BasketballCourt.h"

#include "ProgressionSubsystem.h"   // the prize is sauce
#include "SibeliusGame.h"           // LogSibeliusGame
#include "SibeliusHUD.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace BasketballCourtDetail
{
	using namespace BasketballCourt;

	constexpr double ShapeSize = 100.0;          // the engine's basic shapes are a metre across

	// The half-court, NBA, in the model's frame (origin under the rim, +X toward half court).
	constexpr double Baseline = -RimToBaseline;
	constexpr double HalfCourt = Baseline + 1432.6;        // 47 ft from the baseline
	constexpr double Sideline = 762.0;                     // 25 ft either side
	constexpr double KeyHalfWidth = 243.8;                 // the lane is 16 ft wide
	constexpr double FreeThrowLine = Baseline + 579.1;     // 19 ft from the baseline
	constexpr double CircleRadius = 182.9;                 // 6 ft
	constexpr double LineWidth = 5.0;                      // 2 in
	constexpr double LineTop = 0.8;                        // the paint stands this proud of the slab
	// The slab reaches this far down from its top: the park's road under it falls 19 cm across
	// the footprint (place_basketball_court.py), and the top sits just over its highest point.
	constexpr double SlabDepth = 40.0;

	// The arc meets the straight corner lines where it is 22 ft from the middle of the rim sideways.
	inline double ArcCornerX() { return FMath::Sqrt(ArcDistance * ArcDistance - CornerDistance * CornerDistance); }

	constexpr double BoardFace = -RimToBackboard;          // the front of the glass
	constexpr double BoardWidth = 183.0;
	constexpr double BoardHeight = 107.0;
	constexpr double BoardBottom = 290.0;
	constexpr double RimTube = 0.8;                        // the iron's radius
	constexpr double NetDepth = 42.0;
	constexpr double NetBottomRadius = 14.0;
	constexpr double PoleX = Baseline - 90.0;
	constexpr double PoleHeight = 400.0;

	constexpr int32 ArcSegments = 36;
	constexpr int32 CircleSegments = 24;
	constexpr int32 HalfCircleSegments = 12;

	// LINEAR colours, which is what a material multiplies (the Go table's lesson).
	const FLinearColor Concrete(0.16f, 0.16f, 0.15f);
	const FLinearColor CourtBlue(0.025f, 0.07f, 0.20f);
	const FLinearColor KeyRed(0.30f, 0.03f, 0.025f);
	const FLinearColor LinePaint(0.85f, 0.85f, 0.82f);
	const FLinearColor Steel(0.03f, 0.03f, 0.035f);
	const FLinearColor Glass(0.88f, 0.88f, 0.88f);
	const FLinearColor BoardRed(0.60f, 0.02f, 0.01f);
	const FLinearColor RimOrange(0.75f, 0.12f, 0.02f);
	const FLinearColor NetWhite(0.90f, 0.90f, 0.88f);
	const FLinearColor Gilt(1.40f, 1.10f, 0.35f);          // multiplies the leather: a gilded money ball

	// A flat piece of paint from A to B on the court (model X, Y).
	struct FSegment { FVector2D A; FVector2D B; };

	TArray<FSegment> CourtLines()
	{
		TArray<FSegment> Out;
		auto Add = [&Out](double AX, double AY, double BX, double BY) { Out.Add({ FVector2D(AX, AY), FVector2D(BX, BY) }); };
		auto Arc = [&Add](double CX, double CY, double R, double From, double To, int32 Count)
		{
			for (int32 I = 0; I < Count; ++I)
			{
				const double A0 = FMath::DegreesToRadians(From + (To - From) * I / Count);
				const double A1 = FMath::DegreesToRadians(From + (To - From) * (I + 1) / Count);
				Add(CX + R * FMath::Cos(A0), CY + R * FMath::Sin(A0), CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1));
			}
		};
		Add(Baseline, -Sideline, Baseline, Sideline);                 // the baseline
		Add(Baseline, -Sideline, HalfCourt, -Sideline);               // the sidelines
		Add(Baseline, Sideline, HalfCourt, Sideline);
		Add(HalfCourt, -Sideline, HalfCourt, Sideline);               // the half-court line
		Add(Baseline, -KeyHalfWidth, FreeThrowLine, -KeyHalfWidth);   // the lane
		Add(Baseline, KeyHalfWidth, FreeThrowLine, KeyHalfWidth);
		Add(FreeThrowLine, -KeyHalfWidth, FreeThrowLine, KeyHalfWidth);
		Arc(FreeThrowLine, 0.0, CircleRadius, 0.0, 360.0, CircleSegments);
		const double CornerX = ArcCornerX();
		Add(Baseline, -CornerDistance, CornerX, -CornerDistance);     // the straight corner threes
		Add(Baseline, CornerDistance, CornerX, CornerDistance);
		const double ArcEnd = FMath::RadiansToDegrees(FMath::Atan2(CornerDistance, CornerX));
		Arc(0.0, 0.0, ArcDistance, -ArcEnd, ArcEnd, ArcSegments);
		Arc(HalfCourt, 0.0, CircleRadius, 90.0, 270.0, HalfCircleSegments);   // half the centre circle
		return Out;
	}

	// Where rack R's table stands: beside the shooter's spot, to her right as she faces the rim.
	FVector2D RackCentre(int32 Rack, FVector2D& OutAlong)
	{
		const FHoopVec3 Spot = FBasketballContest::RackSpot(Rack);
		const FVector2D Out = FVector2D(Spot.X, Spot.Y).GetSafeNormal();   // away from the rim
		const FVector2D Right(Out.Y, -Out.X);                               // her right, facing in
		OutAlong = Out;
		return FVector2D(Spot.X, Spot.Y) + Right * 75.0;
	}

	constexpr double RackTop = 72.0;
	constexpr double RackLength = 130.0;

	// Her own MetaHuman, by the model's name for her (KartShapes::AgentBlueprint's five).
	const TCHAR* AgentBodyPath(const std::string& Name)
	{
		if (Name == "Kaia")    { return TEXT("/Game/MetaHumans/MHC_Kaia/BP_MHC_Kaia.BP_MHC_Kaia_C"); }
		if (Name == "Nyra")    { return TEXT("/Game/MetaHumans/MHC_NyraSolmere/BP_MHC_NyraSolmere.BP_MHC_NyraSolmere_C"); }
		if (Name == "Isla")    { return TEXT("/Game/MetaHumans/MHC_IslaRowan/BP_MHC_IslaRowan.BP_MHC_IslaRowan_C"); }
		if (Name == "Aisling") { return TEXT("/Game/MetaHumans/MHC_Aisling/BP_MHC_Aisling.BP_MHC_Aisling_C"); }
		if (Name == "Elise")   { return TEXT("/Game/MetaHumans/MHC_Elise/BP_MHC_Elise.BP_MHC_Elise_C"); }
		return nullptr;
	}

	// After her fifth ball the arm comes through before she sets off (the throw's frame 33).
	constexpr double FollowThroughSeconds = 0.35;
	// Mixamo's Dribble bounces the ball three times in its 2.1 s loop.
	constexpr int32 DribbleBounces = 3;

	/* THEY RUN BETWEEN RACKS (Walt, 2026-10-15: "can the players please just run from station
	   to station? the dribble looks funny while the player skates along the ground with legs
	   not moving"). Mixamo "Running" (import_mixamo_basketball.py), its 3 m of travel per
	   loop taken out by bake_basketball_run.py, loaded by path here and cooked by
	   DefaultGame.ini's /Game/Mixamo/Basketball line. Until it is baked, the old dribbling
	   glide stands in. */
	const TCHAR* const RunAgentPath = TEXT("/Game/Mixamo/Basketball/Anim_RunInPlace_MH.Anim_RunInPlace_MH");
	const TCHAR* const RunMannyPath = TEXT("/Game/Mixamo/Basketball/Anim_RunInPlace_Manny.Anim_RunInPlace_Manny");
	// The ground speed the run's stride is made for, cm/s at a play rate of 1 - the travel the
	// bake took out (429 cm/s on the agents, 438 on Manny): the rate is scaled from it so her
	// feet keep up with how far she has to go.
	constexpr double RunStrideSpeed = 430.0;

	double Smooth(double A) { return A * A * (3.0 - 2.0 * A); }

	// A rod (the engine's cylinder) from A to B, D thick - the constructor's Rod, for the net.
	void SetRod(UStaticMeshComponent* Part, const FVector& A, const FVector& B, double D)
	{
		const FVector Axis = B - A;
		const FRotator Turn = FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator();
		Part->SetRelativeTransform(FTransform(Turn, (A + B) * 0.5, FVector(D / ShapeSize, D / ShapeSize, Axis.Size() / ShapeSize)));
	}

	/* THE NET MOVES WITH A MADE BALL (Walt, 2026-10-15: "it is hard to see if the ball goes in
	   the hoop... is it possible for the net to be animated like a ball is going through it").
	   Bulge: how much the net swells round the ball (0..1), at Depth cm below the rim. Swing:
	   the bottom's kick after the ball drops out, a decaying wobble (-1..1). */
	FVector NetPoint(const FVector& Rest, double Bulge, double Depth, double Swing)
	{
		const double D = RimHeight - Rest.Z;
		const double U = FMath::Clamp(D / NetDepth, 0.0, 1.0);
		const double Around = FMath::Exp(-FMath::Square((D - Depth) / 14.0));
		// Tied to the rim at the top (FMath::Min(1, 3U)): only the hanging part gives.
		const double Out = 1.0 + Bulge * 0.55 * Around * FMath::Min(1.0, 3.0 * U) + Swing * 0.18 * U;
		return FVector(Rest.X * Out, Rest.Y * Out, Rest.Z - U * (Bulge * 9.0 + Swing * 11.0));
	}
}

ABasketballCourt::ABasketballCourt()
{
	using namespace BasketballCourtDetail;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoSurface(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	static ConstructorHelpers::FObjectFinder<UTexture> Leather(TEXT("/Game/Basketball/T_Basketball.T_Basketball"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	FallbackMaterial = BasicMaterial.Succeeded() ? BasicMaterial.Object : nullptr;
	SurfaceMaterial = GoSurface.Succeeded() ? GoSurface.Object : nullptr;
	BallTexture = Leather.Succeeded() ? Leather.Object : nullptr;
	PlayerBodyMesh = MannyMesh.Succeeded() ? MannyMesh.Object : nullptr;
	PlayerBodyAnim = MannyAnim.Succeeded() ? MannyAnim.Class : nullptr;

	// The shooters' clips (import_mixamo_basketball.py), on the MetaHuman skeleton and on Manny's.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ThrowMH(TEXT("/Game/Mixamo/Basketball/Anim_Throw_MH.Anim_Throw_MH"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DribbleMH(TEXT("/Game/Mixamo/Basketball/Anim_Dribble_MH.Anim_Dribble_MH"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> CheerMH(TEXT("/Game/Mixamo/Basketball/Anim_Cheering_MH.Anim_Cheering_MH"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SadMH(TEXT("/Game/Mixamo/Basketball/Anim_Disappointed_MH.Anim_Disappointed_MH"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ThrowManny(TEXT("/Game/Mixamo/Basketball/Anim_Throw_Manny.Anim_Throw_Manny"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DribbleManny(TEXT("/Game/Mixamo/Basketball/Anim_Dribble_Manny.Anim_Dribble_Manny"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> CheerManny(TEXT("/Game/Mixamo/Basketball/Anim_Cheering_Manny.Anim_Cheering_Manny"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SadManny(TEXT("/Game/Mixamo/Basketball/Anim_Disappointed_Manny.Anim_Disappointed_Manny"));
	AgentThrow = ThrowMH.Object;
	AgentDribble = DribbleMH.Object;
	AgentCheer = CheerMH.Object;
	AgentSad = SadMH.Object;
	MannyThrow = ThrowManny.Object;
	MannyDribble = DribbleManny.Object;
	MannyCheer = CheerManny.Object;
	MannySad = SadManny.Object;
	UStaticMesh* Cube = CubeMesh.Object;
	UStaticMesh* Sphere = SphereMesh.Object;
	UStaticMesh* Cylinder = CylinderMesh.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	auto MakePart = [this](const FString& PartName, UStaticMesh* Mesh, bool bShadow) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*PartName);
		Part->SetupAttachment(SceneRoot);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(bShadow);
		// The road under the court has its lane lines as decals, and they would paint
		// themselves across the slab.
		Part->SetReceivesDecals(false);
		Part->SetStaticMesh(Mesh);
		return Part;
	};
	/* SOLID, for what he stands on or walks into: the slab, the pole, the racks. Everything
	   else stays NoCollision. The slab first of all (Walt, 2026-10-15: "manny's feet are sunk
	   into the floor"): without it he stood on the park's road under it, up to 20 cm below
	   its top. Visibility and Camera pass through the pole and the racks, so the E prompt is
	   still the hoop's alone.

	   THE FLOOR BLOCKS VISIBILITY TOO, and is tagged NotInteractive. It had to: his feet were
	   still in it with his capsule on it, because Manny's ABP_Unarmed puts them on the ground
	   with CR_Mannequin_FootIK, and that rig finds the ground with Visibility traces - which
	   went through the slab to the road. The tag makes the interactor treat the floor as a
	   surface, not as the court, so looking down is not "[E] three-point contest". */
	auto Solid = [](UStaticMeshComponent* Part, bool bFloor = false)
	{
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetCollisionObjectType(ECC_WorldStatic);
		Part->SetCollisionResponseToAllChannels(ECR_Block);
		Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		if (bFloor)
		{
			Part->ComponentTags.Add(TEXT("NotInteractive"));   // InteractorDetail::NotInteractiveTag
		}
		else
		{
			Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	};
	// A box from its centre and its size, turned Yaw degrees about the vertical.
	auto Box = [](UStaticMeshComponent* Part, const FVector& Centre, const FVector& Size, double Yaw = 0.0)
	{
		Part->SetRelativeTransform(FTransform(FRotator(0.0, Yaw, 0.0), Centre, Size / ShapeSize));
	};
	// A rod (cylinder) from A to B, D thick.
	auto Rod = [](UStaticMeshComponent* Part, const FVector& A, const FVector& B, double D)
	{
		const FVector Axis = B - A;
		const FRotator Turn = FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator();
		Part->SetRelativeTransform(FTransform(Turn, (A + B) * 0.5, FVector(D / ShapeSize, D / ShapeSize, Axis.Size() / ShapeSize)));
	};

	// The slab, with an apron of concrete round the lines; its top at the origin.
	Slab = MakePart(TEXT("Slab"), Cube, false);
	Box(Slab, FVector((Baseline - 200.0 + HalfCourt + 100.0) * 0.5, 0.0, -SlabDepth * 0.5),
		FVector(HalfCourt + 100.0 - (Baseline - 200.0), 2.0 * (Sideline + 150.0), SlabDepth));
	Solid(Slab, /*bFloor=*/true);
	PlayingSurface = MakePart(TEXT("PlayingSurface"), Cube, false);
	Box(PlayingSurface, FVector((Baseline + HalfCourt) * 0.5, 0.0, 0.15), FVector(HalfCourt - Baseline, 2.0 * Sideline, 0.3));
	KeyPaint = MakePart(TEXT("KeyPaint"), Cube, false);
	Box(KeyPaint, FVector((Baseline + FreeThrowLine) * 0.5, 0.0, 0.4), FVector(FreeThrowLine - Baseline, 2.0 * KeyHalfWidth, 0.2));

	int32 Index = 0;
	for (const FSegment& Seg : CourtLines())
	{
		UStaticMeshComponent* Line = MakePart(FString::Printf(TEXT("Line_%02d"), Index++), Cube, false);
		const FVector2D D = Seg.B - Seg.A;
		Box(Line, FVector((Seg.A.X + Seg.B.X) * 0.5, (Seg.A.Y + Seg.B.Y) * 0.5, LineTop - 0.15),
			FVector(D.Size() + LineWidth * 0.5, LineWidth, 0.3), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
		Lines.Add(Line);
	}

	// The pole behind the baseline, its arm, the glass, and the square on it.
	Pole = MakePart(TEXT("Pole"), Cylinder, true);
	Rod(Pole, FVector(PoleX, 0.0, 0.0), FVector(PoleX, 0.0, PoleHeight), 14.0);
	Solid(Pole);
	Arm = MakePart(TEXT("Arm"), Cube, true);
	Box(Arm, FVector((PoleX + BoardFace - 3.0) * 0.5, 0.0, BoardBottom + 70.0), FVector(BoardFace - 3.0 - PoleX, 12.0, 12.0));
	Backboard = MakePart(TEXT("Backboard"), Cube, true);
	Box(Backboard, FVector(BoardFace - 1.5, 0.0, BoardBottom + BoardHeight * 0.5), FVector(3.0, BoardWidth, BoardHeight));
	{
		// The border (top, bottom, left, right) and the shooter's square: 59 x 45 cm, its bottom at the rim.
		const double X = BoardFace + 0.2;
		const double Top = BoardBottom + BoardHeight;
		const double SquareW = 59.0;
		const double SquareH = 45.0;
		const FVector Centres[8] = {
			FVector(X, 0.0, Top - 2.5), FVector(X, 0.0, BoardBottom + 2.5),
			FVector(X, -BoardWidth * 0.5 + 2.5, BoardBottom + BoardHeight * 0.5), FVector(X, BoardWidth * 0.5 - 2.5, BoardBottom + BoardHeight * 0.5),
			FVector(X, 0.0, RimHeight + SquareH - 2.5), FVector(X, 0.0, RimHeight + 2.5),
			FVector(X, -SquareW * 0.5 + 2.5, RimHeight + SquareH * 0.5), FVector(X, SquareW * 0.5 - 2.5, RimHeight + SquareH * 0.5),
		};
		const FVector Sizes[8] = {
			FVector(0.4, BoardWidth, 5.0), FVector(0.4, BoardWidth, 5.0),
			FVector(0.4, 5.0, BoardHeight), FVector(0.4, 5.0, BoardHeight),
			FVector(0.4, SquareW, 5.0), FVector(0.4, SquareW, 5.0),
			FVector(0.4, 5.0, SquareH), FVector(0.4, 5.0, SquareH),
		};
		for (int32 I = 0; I < 8; ++I)
		{
			UStaticMeshComponent* Mark = MakePart(FString::Printf(TEXT("BoardMark_%d"), I), Cube, false);
			Box(Mark, Centres[I], Sizes[I]);
			BoardMarks.Add(Mark);
		}
	}

	// The rim: short rods round the circle, and the bracket that holds it to the glass.
	const double RimCentreline = RimRadius + RimTube;
	for (int32 I = 0; I < RimSegments; ++I)
	{
		const double A0 = 2.0 * PI * I / RimSegments;
		const double A1 = 2.0 * PI * (I + 1) / RimSegments;
		UStaticMeshComponent* Part = MakePart(FString::Printf(TEXT("Rim_%02d"), I), Cylinder, true);
		const FVector P0(RimCentreline * FMath::Cos(A0), RimCentreline * FMath::Sin(A0), RimHeight);
		const FVector P1(RimCentreline * FMath::Cos(A1), RimCentreline * FMath::Sin(A1), RimHeight);
		Rod(Part, P0 + (P0 - P1) * 0.06, P1 + (P1 - P0) * 0.06, 2.0 * RimTube);
		RimParts.Add(Part);
	}
	{
		UStaticMeshComponent* Bracket = MakePart(TEXT("RimBracket"), Cube, true);
		Box(Bracket, FVector((BoardFace - RimCentreline) * 0.5, 0.0, RimHeight - 1.0), FVector(BoardFace * -1.0 - RimCentreline, 10.0, 3.0));
		RimParts.Add(Bracket);
	}

	// The net: from each tie on the rim one strand down-left and one down-right, crossing, to a
	// smaller ring - the diamond mesh of a real net, in a few rods.
	for (int32 I = 0; I < NetStrands; ++I)
	{
		const double ATop = 2.0 * PI * I / NetStrands;
		const FVector TopPoint(RimRadius * FMath::Cos(ATop), RimRadius * FMath::Sin(ATop), RimHeight - RimTube);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const double ABottom = 2.0 * PI * (I + (Side == 0 ? -0.5 : 0.5)) / NetStrands;
			const FVector Bottom(NetBottomRadius * FMath::Cos(ABottom), NetBottomRadius * FMath::Sin(ABottom), RimHeight - NetDepth);
			UStaticMeshComponent* Strand = MakePart(FString::Printf(TEXT("Net_%02d_%d"), I, Side), Cylinder, false);
			Rod(Strand, TopPoint, Bottom, 0.6);
			NetParts.Add(Strand);
		}
		const double B0 = 2.0 * PI * (I + 0.5) / NetStrands;
		const double B1 = 2.0 * PI * (I + 1.5) / NetStrands;
		UStaticMeshComponent* Ring = MakePart(FString::Printf(TEXT("NetRing_%02d"), I), Cylinder, false);
		Rod(Ring, FVector(NetBottomRadius * FMath::Cos(B0), NetBottomRadius * FMath::Sin(B0), RimHeight - NetDepth),
			FVector(NetBottomRadius * FMath::Cos(B1), NetBottomRadius * FMath::Sin(B1), RimHeight - NetDepth), 0.6);
		NetParts.Add(Ring);
	}

	// Five racks, each a top on two legs, with five balls in a row along it.
	for (int32 Rack = 0; Rack < FBasketballContest::Racks; ++Rack)
	{
		FVector2D Along;
		const FVector2D Centre = RackCentre(Rack, Along);
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
		UStaticMeshComponent* Top = MakePart(FString::Printf(TEXT("RackTop_%d"), Rack), Cube, true);
		Box(Top, FVector(Centre.X, Centre.Y, RackTop - 2.0), FVector(RackLength, 30.0, 4.0), Yaw);
		Solid(Top);
		RackParts.Add(Top);
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			const FVector2D At = Centre + Along * (Leg == 0 ? -0.42 : 0.42) * RackLength;
			UStaticMeshComponent* Part = MakePart(FString::Printf(TEXT("RackLeg_%d_%d"), Rack, Leg), Cube, true);
			Box(Part, FVector(At.X, At.Y, (RackTop - 4.0) * 0.5), FVector(5.0, 24.0, RackTop - 4.0), Yaw);
			Solid(Part);
			RackParts.Add(Part);
		}
		for (int32 Ball = 0; Ball < FBasketballContest::BallsPerRack; ++Ball)
		{
			// The money ball is the last, the one nearest the rim's end of the rack.
			const FVector2D At = Centre + Along * (24.5 * (2 - Ball));
			UStaticMeshComponent* Mesh = MakePart(FString::Printf(TEXT("RackBall_%d_%d"), Rack, Ball), Sphere, true);
			Mesh->SetRelativeTransform(FTransform(FRotator(0.0, 37.0 * (Rack * 5 + Ball), 20.0 * Ball),
				FVector(At.X, At.Y, RackTop + BallRadius), FVector(2.0 * BallRadius / ShapeSize)));
			RackBallMeshes.Add(Mesh);
		}
	}

	for (int32 I = 0; I < FlightBallCount; ++I)
	{
		UStaticMeshComponent* Mesh = MakePart(FString::Printf(TEXT("FlightBall_%d"), I), Sphere, true);
		Mesh->SetRelativeScale3D(FVector(2.0 * BallRadius / ShapeSize));
		Mesh->SetVisibility(false);
		FlightBalls.Add(Mesh);
	}

	// The ball in the shooter's hand (UpdateHandBall), and Leonard's Manny at the rack.
	HandBall = MakePart(TEXT("HandBall"), Sphere, true);
	HandBall->SetRelativeScale3D(FVector(2.0 * BallRadius / ShapeSize));
	HandBall->SetVisibility(false);
	CourtManny = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CourtManny"));
	CourtManny->SetupAttachment(SceneRoot);
	CourtManny->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CourtManny->SetGenerateOverlapEvents(false);
	CourtManny->SetCanEverAffectNavigation(false);
	CourtManny->SetReceivesDecals(false);
	CourtManny->SetSkeletalMeshAsset(PlayerBodyMesh);
	CourtManny->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	CourtManny->SetVisibility(false);

	HoopBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HoopBox"));
	HoopBox->SetupAttachment(SceneRoot);
	HoopBox->SetMobility(EComponentMobility::Movable);
	HoopBox->SetRelativeLocation(FVector((PoleX + RimRadius) * 0.5, 0.0, 230.0));
	HoopBox->SetBoxExtent(FVector((RimRadius - PoleX) * 0.5 + 10.0, 95.0, 190.0));
	HoopBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HoopBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HoopBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HoopBox->SetGenerateOverlapEvents(false);

	CourtCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CourtCamera"));
	CourtCamera->SetupAttachment(SceneRoot);
	CourtCamera->SetRelativeLocation(FVector(1150.0, 0.0, 420.0));
	CourtCamera->SetRelativeRotation(FRotator(-12.0, 180.0, 0.0));
	CourtCamera->SetFieldOfView(70.0f);
}

void ABasketballCourt::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	MakeMaterials();
}

void ABasketballCourt::BeginPlay()
{
	Super::BeginPlay();
	MakeMaterials();
	ResetRackBalls();
	for (UStaticMeshComponent* Ball : FlightBalls)
	{
		if (Ball)
		{
			Ball->SetVisibility(false);
		}
	}
}

void ABasketballCourt::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnShooterBody();
	BodyLoad.Reset();
	Super::EndPlay(EndPlayReason);
}

UMaterialInstanceDynamic* ABasketballCourt::Paint(const FLinearColor& Color, float Roughness, UTexture* Pattern)
{
	UMaterialInterface* Base = SurfaceMaterial ? SurfaceMaterial.Get() : FallbackMaterial.Get();
	UMaterialInstanceDynamic* M = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("PatternAmount"), Pattern ? 1.0f : 0.0f);
		if (Pattern)
		{
			M->SetTextureParameterValue(TEXT("Pattern"), Pattern);
		}
	}
	return M;
}

void ABasketballCourt::MakeMaterials()
{
	using namespace BasketballCourtDetail;
	auto Wear = [](UStaticMeshComponent* Part, UMaterialInstanceDynamic* M)
	{
		if (Part && M)
		{
			Part->SetMaterial(0, M);
		}
	};
	auto WearAll = [&Wear](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts, UMaterialInstanceDynamic* M)
	{
		for (UStaticMeshComponent* Part : Parts)
		{
			Wear(Part, M);
		}
	};
	Wear(Slab, Paint(Concrete, 0.9f));
	Wear(PlayingSurface, Paint(CourtBlue, 0.7f));
	Wear(KeyPaint, Paint(KeyRed, 0.7f));
	WearAll(Lines, Paint(LinePaint, 0.7f));
	UMaterialInstanceDynamic* SteelPaint = Paint(Steel, 0.4f);
	Wear(Pole, SteelPaint);
	Wear(Arm, SteelPaint);
	Wear(Backboard, Paint(Glass, 0.25f));
	WearAll(BoardMarks, Paint(BoardRed, 0.5f));
	WearAll(RimParts, Paint(RimOrange, 0.35f));
	WearAll(NetParts, Paint(NetWhite, 0.9f));
	WearAll(RackParts, SteelPaint);

	BallMaterial = Paint(FLinearColor::White, 0.6f, BallTexture);
	MoneyBallMaterial = Paint(Gilt, 0.45f, BallTexture);
	for (int32 I = 0; I < RackBallMeshes.Num(); ++I)
	{
		const bool bMoney = I % FBasketballContest::BallsPerRack == FBasketballContest::BallsPerRack - 1;
		Wear(RackBallMeshes[I], bMoney ? MoneyBallMaterial.Get() : BallMaterial.Get());
	}
	WearAll(FlightBalls, BallMaterial);
}

void ABasketballCourt::ResetRackBalls()
{
	for (UStaticMeshComponent* Ball : RackBallMeshes)
	{
		if (Ball)
		{
			Ball->SetVisibility(true);
		}
	}
}

FVector ABasketballCourt::Local(const FHoopVec3& V) const
{
	return FVector(V.X, V.Y, V.Z);
}

FVector ABasketballCourt::World(const FHoopVec3& V) const
{
	return GetActorTransform().TransformPosition(Local(V));
}

// --- playing ---------------------------------------------------------------------------

void ABasketballCourt::Interact_Implementation(AActor* Interactor)
{
	if (bPlaying)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (PC)
	{
		StartPlaying(PC);
		NewContest();
	}
}

FText ABasketballCourt::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Basketball", "Prompt", "[E] three-point contest");
}

void ABasketballCourt::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetInputMode(FInputModeGameOnly());
	EnableInput(PC);
	BindCourtKeys();
	DressPlayer(true);
	LoadAgentBodies();
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Hoops] stepped up to the court (%s)."), *GetActorNameOrLabel());
}

void ABasketballCourt::DressPlayer(bool bOn)
{
	if (bOn)
	{
		const APlayerController* PC = PlayingPC.Get();
		ACharacter* Leonard = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		USkeletalMeshComponent* Body = Leonard ? Leonard->GetMesh() : nullptr;
		if (!Body || DressedBody.IsValid())
		{
			return;
		}
		DressedBody = Body;
		bSavedBodyVisible = Body->IsVisible();
		// Since make_player_manny.py his everyday body IS Manny, and this is a no-op. It
		// swaps only a body that is something else.
		if (PlayerBodyMesh && Body->GetSkeletalMeshAsset() != PlayerBodyMesh)
		{
			SavedBodyMesh = Body->GetSkeletalMeshAsset();
			SavedBodyAnim = Body->GetAnimClass();
			// The old AnimBlueprint off first: another skeleton's cannot drive Manny's.
			Body->SetAnimInstanceClass(nullptr);
			Body->SetSkeletalMeshAsset(PlayerBodyMesh);
			if (PlayerBodyAnim)
			{
				Body->SetAnimInstanceClass(PlayerBodyAnim);
			}
		}
		Body->SetVisibility(true);
		return;
	}

	USkeletalMeshComponent* Body = DressedBody.Get();
	DressedBody.Reset();
	if (!Body)
	{
		return;
	}
	if (SavedBodyMesh)
	{
		Body->SetAnimInstanceClass(nullptr);
		Body->SetSkeletalMeshAsset(SavedBodyMesh);
		if (SavedBodyAnim)
		{
			Body->SetAnimInstanceClass(SavedBodyAnim);
		}
	}
	Body->SetVisibility(bSavedBodyVisible);
	SavedBodyMesh = nullptr;
	SavedBodyAnim = nullptr;
}

void ABasketballCourt::ShowPlayerBody(bool bShow)
{
	if (USkeletalMeshComponent* Body = DressedBody.Get())
	{
		Body->SetVisibility(bShow);
	}
}

void ABasketballCourt::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	State = EHoopState::Idle;
	bMeterHeld = false;
	for (FFlight& F : Flights)
	{
		F.bActive = false;
	}
	for (UStaticMeshComponent* Ball : FlightBalls)
	{
		if (Ball)
		{
			Ball->SetVisibility(false);
		}
	}
	ResetRackBalls();
	DespawnShooterBody();
	BodyLoad.Reset();     // her five Blueprints may go when nothing else holds them
	DressPlayer(false);
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Hoops] walked away from the court."));
}

void ABasketballCourt::BindCourtKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ABasketballCourt::OnShootPressed);
		InputComponent->BindKey(Key, IE_Released, this, &ABasketballCourt::OnShootReleased);
	}
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ABasketballCourt::OnSkipToPlayer);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ABasketballCourt::OnAgain);
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ABasketballCourt::OnLeave);
	}
}

void ABasketballCourt::NewContest()
{
	Order.Reset();
	for (const FShooterStyle& Agent : FBasketballContest::AgentStyles())
	{
		// Nyra alone (Walt, 2026-10-15: "can we restrict the agents to just one, Nyra? It is
		// too long a wait for all five to finish"). The other four stay in the model.
		if (Agent.Name == "Nyra")
		{
			Order.Add(Agent);
		}
	}
	FShooterStyle Leonard;
	Leonard.Name = "Leonard";   // the model's names are std::string
	Leonard.ShotSeconds = 1.3;     // the time to pick up the next ball; his release is the meter
	Leonard.MoveSeconds = 2.0;
	Leonard.ReleaseHeight = 225.0;
	Leonard.ArcDegrees = 52.0;
	Order.Add(Leonard);
	Scores.Init(-1, Order.Num());
	PlayerPlace = -1;
	PrizeGiven = 0;
	ContestSeed = static_cast<uint64>(FDateTime::UtcNow().GetTicks());
	BeginShooter(0);
}

void ABasketballCourt::BeginShooter(int32 IndexIn)
{
	Current = IndexIn;
	const bool bPlayer = Current == Order.Num() - 1;
	Contest.Begin(Order[Current], bPlayer, ContestSeed + 7919ull * static_cast<uint64>(Current + 1));
	Revealed = 0;
	bMeterHeld = false;
	Meter = 0.0f;
	LastShotNote.Reset();
	ResetRackBalls();
	State = bPlayer ? EHoopState::PlayerRound : EHoopState::AgentRound;
	ShowPlayerBody(!bPlayer);
	SpawnShooterBody();
	AimCamera(false);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Hoops] %s steps up."), *FString(UTF8_TO_TCHAR(Order[Current].Name.c_str())));
}

void ABasketballCourt::FinishShooter()
{
	Scores[Current] = Contest.GetScore();
	State = EHoopState::Between;
	BetweenLeft = BetweenSeconds;
	// Hers is the leading body now, so his own comes back for the next round.
	ShowPlayerBody(Current != Order.Num() - 1);
	// A good round, a cheer; a poor one, a shrug - and the pause waits for it.
	if (ShooterMesh)
	{
		const bool bGood = Contest.GetScore() >= CheerScore;
		if (UAnimSequence* React = bGood ? BodyAnim(AgentCheer, MannyCheer) : BodyAnim(AgentSad, MannySad))
		{
			PlayBody(React, false, ReactRate);
			BodyMove = EBodyMove::React;
			BetweenLeft = FMath::Max(BetweenLeft, React->GetPlayLength() / FMath::Max(ReactRate, 0.1f));
		}
		bHandBallLive = false;
		bDribbling = false;
		if (HandBall)
		{
			HandBall->SetVisibility(false);
		}
	}
	AimCamera(false);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Hoops] %s: %d points from %d balls."),
		*FString(UTF8_TO_TCHAR(Order[Current].Name.c_str())), Contest.GetScore(), Contest.GetBallsTaken());
}

void ABasketballCourt::ShowResults()
{
	State = EHoopState::Results;
	const std::vector<int32> Ranked = RankScores(std::vector<int32>(Scores.GetData(), Scores.GetData() + Scores.Num()));
	for (int32 Place = 0; Place < static_cast<int32>(Ranked.size()); ++Place)
	{
		if (Ranked[Place] == Order.Num() - 1)
		{
			PlayerPlace = Place;
		}
	}
	if (PlacePrizes.Num() > 0 && PlayerPlace >= 0)
	{
		PrizeGiven = PlacePrizes[FMath::Min(PlayerPlace, PlacePrizes.Num() - 1)];
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			Progression->GrantSauce(PrizeGiven);
		}
	}
	AimCamera(false);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Hoops] contest over: Leonard placed %d with %d, +%d sauce."),
		PlayerPlace + 1, Scores.Last(), PrizeGiven);
}

void ABasketballCourt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying)
	{
		return;
	}
	const float Speed = State == EHoopState::AgentRound ? AgentSpeed : 1.0f;
	switch (State)
	{
	case EHoopState::AgentRound:
	{
		std::vector<FShot> Shots;
		Contest.Advance(DeltaSeconds * AgentSpeed, Shots);
		for (const FShot& Shot : Shots)
		{
			LaunchBall(Shot);
		}
		if (Contest.IsFinished())
		{
			FinishShooter();
		}
		break;
	}
	case EHoopState::PlayerRound:
	{
		std::vector<FShot> Ignored;
		Contest.Advance(DeltaSeconds, Ignored);
		UpdateMeter(DeltaSeconds);
		if (Contest.IsFinished())
		{
			bMeterHeld = false;
			FinishShooter();
		}
		break;
	}
	case EHoopState::Between:
		BetweenLeft -= DeltaSeconds;
		if (BetweenLeft <= 0.0f)
		{
			if (Current + 1 < Order.Num())
			{
				BeginShooter(Current + 1);
			}
			else
			{
				ShowResults();
			}
		}
		break;
	default:
		break;
	}
	UpdateFlights(DeltaSeconds * Speed);
	if (State == EHoopState::AgentRound || State == EHoopState::PlayerRound)
	{
		UpdateShooterBody(DeltaSeconds);
	}
	UpdateHandBall();

	// The controls, just above "[O] Back to Office" (Walt, 2026-10-15: "can a bit of
	// instructions be placed above the [O] return to office describing the controls").
	if (ASibeliusHUD* Hud = GetHud())
	{
		const FString Throw = TEXT("hold [Left Mouse] or [Space] - the meter fills as you wind up - and let go at the mark\n")
			TEXT("too early falls short, too late goes long");
		switch (State)
		{
		case EHoopState::PlayerRound:
			Hud->ShowAwayHint(FString(TEXT("YOUR TURN: ")) + Throw);
			break;
		case EHoopState::Results:
			Hud->ShowAwayHint(TEXT("[R] play again     [Esc] or [E] leave the court"));
			break;
		default:
			Hud->ShowAwayHint(FString(TEXT("[Space] skip this shooter     [Enter] straight to your turn     [Esc] leave\n"))
				+ TEXT("Your turn: ") + Throw);
			break;
		}
	}

	// The camera eases to where the shooter is, and follows her (or him) to the next rack.
	if ((State == EHoopState::AgentRound || State == EHoopState::PlayerRound) && Contest.GetRack() != CameraRack)
	{
		AimCamera(false);
	}
	if (CourtCamera)
	{
		const FVector Now = CourtCamera->GetComponentLocation();
		const FRotator NowRot = CourtCamera->GetComponentRotation();
		CourtCamera->SetWorldLocationAndRotation(FMath::VInterpTo(Now, CameraGoal, DeltaSeconds, 2.5f),
			FMath::RInterpTo(NowRot, CameraGoalRot, DeltaSeconds, 2.5f));
	}
	RefreshPanel();
}

void ABasketballCourt::AimCamera(bool bInstant)
{
	FHoopVec3 From;
	FHoopVec3 LookAt(0.0, 0.0, 260.0);
	if (State == EHoopState::Between && Current >= 0 && ShooterMesh)
	{
		// Her cheer or her shrug, from in front: a few metres toward the rim, at her chest.
		const FVector At = GetActorTransform().InverseTransformPosition(ShooterMesh->GetComponentLocation());
		const double Len = FMath::Max(FMath::Sqrt(At.X * At.X + At.Y * At.Y), 1.0);
		From = FHoopVec3(At.X - At.X / Len * 330.0, At.Y - At.Y / Len * 330.0, 175.0);
		LookAt = FHoopVec3(At.X, At.Y, 125.0);
	}
	else if ((State == EHoopState::AgentRound || State == EHoopState::PlayerRound || State == EHoopState::Between)
		&& Current >= 0)
	{
		const FHoopVec3 Spot = FBasketballContest::RackSpot(Contest.GetRack());
		const double Len = FMath::Sqrt(Spot.X * Spot.X + Spot.Y * Spot.Y);
		const double Ox = Spot.X / Len;
		const double Oy = Spot.Y / Len;
		// Behind the shooter - his round too, since he is Manny at the rack now (Walt,
		// 2026-10-15: "will Manny be third person too?"). Low, back and wide enough for all of
		// her, feet and dribble included, and the top of the arc: 3 m up and 4.2 m back cut her
		// off at the waist (Walt, the same day).
		From = FHoopVec3(Spot.X + Ox * 520.0, Spot.Y + Oy * 520.0, 200.0);
		LookAt = FHoopVec3(0.0, 0.0, 200.0);
	}
	if (CourtCamera)
	{
		CourtCamera->SetFieldOfView(80.0f);
	}
	else
	{
		From = FHoopVec3(1150.0, 0.0, 420.0);
		LookAt = FHoopVec3(0.0, 0.0, 200.0);
	}
	CameraGoal = World(From);
	CameraGoalRot = FRotationMatrix::MakeFromX(World(LookAt) - CameraGoal).Rotator();
	CameraRack = Contest.GetRack();
	if (bInstant && CourtCamera)
	{
		CourtCamera->SetWorldLocationAndRotation(CameraGoal, CameraGoalRot);
	}
}

// --- the shooters' bodies --------------------------------------------------------------

void ABasketballCourt::LoadAgentBodies()
{
	using namespace BasketballCourtDetail;
	// In the background as he steps up: the first is fetched at once by SpawnShooterBody if it
	// is not in yet, and the rest arrive during her round. Let go in StopPlaying.
	if (!bShooterBodies || BodyLoad.IsValid())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FShooterStyle& Agent : FBasketballContest::AgentStyles())
	{
		if (const TCHAR* Path = AgentBodyPath(Agent.Name))
		{
			Paths.Add(FSoftObjectPath(Path));
		}
	}
	BodyLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
}

FVector2D ABasketballCourt::SpotXY(int32 Rack) const
{
	const FHoopVec3 Spot = FBasketballContest::RackSpot(Rack);
	return FVector2D(Spot.X, Spot.Y);
}

UAnimSequence* ABasketballCourt::BodyAnim(UAnimSequence* ForAgent, UAnimSequence* ForManny) const
{
	return ShooterActor ? ForAgent : ForManny;
}

void ABasketballCourt::SpawnShooterBody()
{
	using namespace BasketballCourtDetail;
	DespawnShooterBody();
	UWorld* World = GetWorld();
	if (!bShooterBodies || Current < 0 || !World || !World->IsGameWorld())
	{
		return;
	}
	if (Current == Order.Num() - 1)
	{
		if (CourtManny && CourtManny->GetSkeletalMeshAsset() && MannyThrow)
		{
			CourtManny->SetVisibility(true);
			ShooterMesh = CourtManny;
		}
	}
	else if (AgentThrow)
	{
		const TCHAR* Path = AgentBodyPath(Order[Current].Name);
		UClass* Class = Path ? TSoftClassPtr<AActor>(FSoftObjectPath(Path)).LoadSynchronous() : nullptr;
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Agent = Class ? World->SpawnActor<AActor>(Class, GetActorTransform(), Params) : nullptr;
		if (Agent && Agent->GetRootComponent())
		{
			Agent->SetActorEnableCollision(false);
			Agent->GetRootComponent()->SetMobility(EComponentMobility::Movable);
			// The body plays the clip and the face and clothes follow it, as in the kart.
			TArray<USkeletalMeshComponent*> Meshes;
			Agent->GetComponents<USkeletalMeshComponent>(Meshes);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh && Mesh->GetName().Contains(TEXT("Body")))
				{
					ShooterMesh = Mesh;
					break;
				}
			}
			if (ShooterMesh)
			{
				ShooterActor = Agent;
			}
			else
			{
				UE_LOG(LogSibeliusGame, Warning, TEXT("[Hoops] %s has no Body mesh to shoot with"), *Agent->GetName());
				Agent->Destroy();
			}
		}
	}
	if (!ShooterMesh)
	{
		return;
	}
	BodyRack = 0;
	PlaceShooter(SpotXY(0));
	HoldRest();
}

void ABasketballCourt::DespawnShooterBody()
{
	if (ShooterActor)
	{
		ShooterActor->Destroy();
		ShooterActor = nullptr;
	}
	if (CourtManny)
	{
		CourtManny->Stop();
		CourtManny->SetVisibility(false);
	}
	ShooterMesh = nullptr;
	BodyMove = EBodyMove::None;
	ThrowFor = -1.0;
	bHandBallLive = false;
	bDribbling = false;
	if (HandBall)
	{
		HandBall->SetVisibility(false);
	}
}

void ABasketballCourt::PlaceShooter(const FVector2D& Spot)
{
	// Her feet on the spot, facing the rim. A MetaHuman (and Manny) faces her +Y: a quarter
	// turn less than the way she looks.
	const FVector At = World(FHoopVec3(Spot.X, Spot.Y, 0.0));
	const double FaceYaw = GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::Atan2(-Spot.Y, -Spot.X));
	const FRotator Turn(0.0, FaceYaw - 90.0, 0.0);
	if (ShooterActor)
	{
		ShooterActor->SetActorLocationAndRotation(At, Turn);
	}
	else if (ShooterMesh)
	{
		ShooterMesh->SetWorldLocationAndRotation(At, Turn);
	}
}

void ABasketballCourt::PlayBody(UAnimSequence* Anim, bool bLoop, float Rate, float StartAt)
{
	if (!ShooterMesh || !Anim)
	{
		return;
	}
	// AnimationData too, as AKartRace learned: the dancers' 5-second scan adopts anyone whose
	// mesh names a known dance, and her Blueprint names hers.
	ShooterMesh->AnimationData.AnimToPlay = Anim;
	ShooterMesh->AnimationData.bSavedLooping = bLoop;
	ShooterMesh->AnimationData.bSavedPlaying = true;
	ShooterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	ShooterMesh->PlayAnimation(Anim, bLoop);
	ShooterMesh->SetPlayRate(Rate);
	if (StartAt > 0.0f)
	{
		ShooterMesh->SetPosition(FMath::Min(StartAt, Anim->GetPlayLength()), /*bFireNotifies=*/false);
	}
}

float ABasketballCourt::BodyPosition() const
{
	return ShooterMesh ? ShooterMesh->GetPosition() : 0.0f;
}

void ABasketballCourt::HoldRest()
{
	// The throw's first frame is her at rest, and its last is the same.
	PlayBody(BodyAnim(AgentThrow, MannyThrow), false, 1.0f);
	if (ShooterMesh)
	{
		ShooterMesh->Stop();
	}
	BodyMove = EBodyMove::Rest;
	bDribbling = false;
}

void ABasketballCourt::StartThrow(float StartAt, float Rate)
{
	PlayBody(BodyAnim(AgentThrow, MannyThrow), false, Rate, StartAt);
	BodyMove = EBodyMove::Throw;
	ThrowFor = Contest.GetNextBallAt();
	// She takes the next ball off her rack.
	const int32 RackIndex = Contest.GetRack() * FBasketballContest::BallsPerRack + Contest.GetBall();
	UStaticMeshComponent* RackBall = RackBallMeshes.IsValidIndex(RackIndex) ? RackBallMeshes[RackIndex].Get() : nullptr;
	PickupFrom = RackBall ? RackBall->GetComponentLocation() : ShooterMesh->GetComponentLocation() + FVector(0.0, 0.0, 80.0);
	if (RackBall)
	{
		RackBall->SetVisibility(false);
	}
	if (HandBall)
	{
		const bool bMoney = Contest.GetBall() == FBasketballContest::BallsPerRack - 1;
		HandBall->SetMaterial(0, bMoney ? MoneyBallMaterial.Get() : BallMaterial.Get());
	}
	bHandBallLive = true;
}

void ABasketballCourt::UpdateShooterBody(float DeltaSeconds)
{
	using namespace BasketballCourtDetail;
	if (!ShooterMesh || Contest.IsFinished())
	{
		return;
	}
	const bool bPlayerRound = State == EHoopState::PlayerRound;
	const float Rate = bPlayerRound ? 1.0f : AgentSpeed;
	const double Clock = Contest.GetClock();
	const double Next = Contest.GetNextBallAt();

	// Her fifth ball is away: the arm comes through, then she dribbles along the arc to the
	// next rack, there in time to set for its first ball.
	if (Contest.GetRack() != BodyRack && BodyMove != EBodyMove::Glide)
	{
		const std::vector<FShot>& Shots = Contest.GetShots();
		GlideFrom = SpotXY(BodyRack);
		GlideTo = SpotXY(Contest.GetRack());
		GlideStart = (Shots.empty() ? Clock : Shots.back().Clock) + FollowThroughSeconds;
		GlideEnd = Next - (bPlayerRound ? 0.1 : ReleaseSeconds + 0.1);
		GlideEnd = FMath::Max(GlideEnd, GlideStart + 0.3);
		BodyRack = Contest.GetRack();
		BodyMove = EBodyMove::Glide;
		bDribbling = false;
	}
	if (BodyMove == EBodyMove::Glide)
	{
		const double R0 = GlideFrom.Size();
		const double R1 = GlideTo.Size();
		const double Th0 = FMath::Atan2(GlideFrom.Y, GlideFrom.X);
		const double Th1 = FMath::Atan2(GlideTo.Y, GlideTo.X);
		// Round the arc, not across it: the angle and the distance from the rim each eased.
		auto PathAt = [&](double S)
		{
			const double R = FMath::Lerp(R0, R1, S);
			const double Th = FMath::Lerp(Th0, Th1, S);
			return FVector2D(R * FMath::Cos(Th), R * FMath::Sin(Th));
		};
		UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr, ShooterActor ? RunAgentPath : RunMannyPath,
			nullptr, LOAD_NoWarn | LOAD_Quiet);   // quietly absent until it is imported
		if (Clock >= GlideStart && !bDribbling)
		{
			if (Run)
			{
				// Her stride matched to the ground she covers (the eased path's mean speed).
				const double Length = FMath::Abs(Th1 - Th0) * 0.5 * (R0 + R1) + FMath::Abs(R1 - R0);
				const double Speed = Length / FMath::Max(GlideEnd - GlideStart, 0.1);
				PlayBody(Run, true, Rate * static_cast<float>(FMath::Clamp(Speed / RunStrideSpeed, 0.6, 1.6)));
			}
			else
			{
				PlayBody(BodyAnim(AgentDribble, MannyDribble), true, Rate);
			}
			bDribbling = true;
		}
		const double A = FMath::Clamp((Clock - GlideStart) / FMath::Max(GlideEnd - GlideStart, 0.01), 0.0, 1.0);
		const double S = Smooth(A);
		const FVector2D Spot = PathAt(S);
		if (Run && bDribbling)
		{
			// Running, she faces the way she goes; she turns from the rim as she sets off and
			// back to it as she arrives. (A MetaHuman, and Manny, faces her +Y.)
			const FVector2D Ahead = (PathAt(FMath::Min(S + 0.02, 1.0)) - PathAt(FMath::Max(S - 0.02, 0.0))).GetSafeNormal();
			const double ToRim = FMath::RadiansToDegrees(FMath::Atan2(-Spot.Y, -Spot.X));
			const double Along = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
			const double Turn = FMath::Min(Smooth(FMath::Clamp(A / 0.15, 0.0, 1.0)), Smooth(FMath::Clamp((1.0 - A) / 0.2, 0.0, 1.0)));
			const FQuat Facing = FQuat::Slerp(FRotator(0.0, ToRim, 0.0).Quaternion(), FRotator(0.0, Along, 0.0).Quaternion(), Turn);
			const FRotator Yaw(0.0, GetActorRotation().Yaw + Facing.Rotator().Yaw - 90.0, 0.0);
			const FVector At = World(FHoopVec3(Spot.X, Spot.Y, 0.0));
			if (ShooterActor)
			{
				ShooterActor->SetActorLocationAndRotation(At, Yaw);
			}
			else
			{
				ShooterMesh->SetWorldLocationAndRotation(At, Yaw);
			}
		}
		else
		{
			PlaceShooter(Spot);
		}
		if (A >= 1.0)
		{
			HoldRest();
		}
		return;
	}

	// He let go early and his arm has caught up: now the ball goes (ReleaseMeter).
	if (bPlayerRound && BodyMove == EBodyMove::Throw && bHandBallLive && !bMeterHeld
		&& BodyPosition() >= ReleaseSeconds)
	{
		ReleaseMeter();
	}

	// An agent's next throw, started so that her hand lets go as the model shoots the ball.
	// Leonard's starts when he presses (OnShootPressed).
	if (!bPlayerRound && Next != ThrowFor && Next <= FBasketballContest::TimeLimit
		&& Clock >= Next - ReleaseSeconds)
	{
		// Her clip and the model's clock both run at AgentSpeed: a model second is a clip second.
		StartThrow(static_cast<float>(Clock - (Next - ReleaseSeconds)), Rate);
	}
}

bool ABasketballCourt::PalmBallAt(FVector& OutWorld) const
{
	using namespace BasketballCourtDetail;
	static const FName Hand(TEXT("hand_r"));
	static const FName Middle(TEXT("middle_01_r"));
	static const FName Index(TEXT("index_01_r"));
	static const FName Pinky(TEXT("pinky_01_r"));
	const USkeletalMeshComponent* M = ShooterMesh;
	if (!M || M->GetBoneIndex(Hand) == INDEX_NONE || M->GetBoneIndex(Middle) == INDEX_NONE
		|| M->GetBoneIndex(Index) == INDEX_NONE || M->GetBoneIndex(Pinky) == INDEX_NONE)
	{
		return false;
	}
	/* The palm's side, measured on both bodies through the whole throw and the dribble
	   (import_mixamo_basketball.py's palm check, 2026-10-15): it is the side AGAINST
	   (index - pinky) x (middle - hand). That side faces forward as she lets go, down as she
	   dribbles, and into her thigh at rest - which is why the ball is only hers from
	   PickupSeconds to the release. */
	const FVector H = M->GetSocketLocation(Hand);
	const FVector Mid = M->GetSocketLocation(Middle);
	const FVector N = FVector::CrossProduct(M->GetSocketLocation(Index) - M->GetSocketLocation(Pinky), Mid - H).GetSafeNormal();
	OutWorld = H + (Mid - H) * 0.7 - N * (BallRadius + 1.5);
	return true;
}

void ABasketballCourt::UpdateHandBall()
{
	using namespace BasketballCourtDetail;
	if (!HandBall)
	{
		return;
	}
	FVector Palm;
	if (!ShooterMesh || !PalmBallAt(Palm))
	{
		HandBall->SetVisibility(false);
		return;
	}
	if (BodyMove == EBodyMove::Throw && bHandBallLive)
	{
		// Off the rack and into her hand as she winds up, with a little lift on the way.
		const double A = FMath::Clamp(BodyPosition() / FMath::Max(PickupSeconds, 0.05f), 0.0f, 1.0f);
		const FVector At = FMath::Lerp(PickupFrom, Palm, Smooth(A)) + FVector(0.0, 0.0, 20.0 * FMath::Sin(PI * A));
		HandBall->SetWorldLocation(At);
		HandBall->SetVisibility(true);
		return;
	}
	// The dribbling glide only: running, she carries nothing - her next ball is on the rack.
	if (BodyMove == EBodyMove::Glide && bDribbling && ShooterMesh->AnimationData.AnimToPlay == BodyAnim(AgentDribble, MannyDribble))
	{
		// From her palm to the court and back, three times to the loop, in step with her hand.
		UAnimSequence* Dribble = BodyAnim(AgentDribble, MannyDribble);
		const double Period = Dribble ? Dribble->GetPlayLength() / DribbleBounces : 0.7;
		const double Phase = FMath::Fmod(static_cast<double>(BodyPosition()), Period) / Period;
		const double Floor = GetActorLocation().Z + BallRadius;
		const double Z = Floor + (Palm.Z - Floor) * FMath::Abs(FMath::Cos(PI * Phase));
		HandBall->SetMaterial(0, BallMaterial.Get());
		HandBall->SetWorldLocation(FVector(Palm.X, Palm.Y, Z));
		HandBall->SetVisibility(true);
		return;
	}
	HandBall->SetVisibility(false);
}

// --- the balls -------------------------------------------------------------------------

void ABasketballCourt::LaunchBall(const FShot& Shot)
{
	const int32 RackIndex = Shot.Rack * FBasketballContest::BallsPerRack + Shot.Ball;
	if (RackBallMeshes.IsValidIndex(RackIndex) && RackBallMeshes[RackIndex])
	{
		RackBallMeshes[RackIndex]->SetVisibility(false);
	}
	// The next free ball in the air; if all are busy the oldest gives way.
	int32 Slot = 0;
	float Oldest = -1.0f;
	for (int32 I = 0; I < FlightBallCount; ++I)
	{
		if (!Flights[I].bActive)
		{
			Slot = I;
			Oldest = -1.0f;
			break;
		}
		const float Age = Flights[I].T + Flights[I].AfterT;
		if (Age > Oldest)
		{
			Oldest = Age;
			Slot = I;
		}
	}
	if (Flights[Slot].bActive && !Flights[Slot].bAfter)
	{
		++Revealed;   // the one given way never reached the rim on screen - count it all the same
	}
	FFlight& F = Flights[Slot];
	F = FFlight();
	F.bActive = true;
	F.Shot = Shot;
	/* FROM HER HAND. The model put the release a step in front of her feet at a jump shot's
	   height; her throw lets go at its own height, a little to her right. Same target, same
	   result - the model decided both - but the arc starts where the ball is. */
	if (bHandBallLive && HandBall && HandBall->IsVisible())
	{
		const FVector From = GetActorTransform().InverseTransformPosition(HandBall->GetComponentLocation());
		FHoopVec3 Velocity;
		double Seconds = 0.0;
		const FHoopVec3 Release(From.X, From.Y, From.Z);
		if (SolveLaunch(Release, Shot.Target, Contest.GetStyle().ArcDegrees, Velocity, Seconds))
		{
			F.Shot.Release = Release;
			F.Shot.Velocity = Velocity;
			F.Shot.FlightSeconds = Seconds;
		}
	}
	bHandBallLive = false;
	if (HandBall)
	{
		HandBall->SetVisibility(false);
	}
	if (UStaticMeshComponent* Mesh = FlightBalls.IsValidIndex(Slot) ? FlightBalls[Slot].Get() : nullptr)
	{
		Mesh->SetMaterial(0, Shot.bMoney ? MoneyBallMaterial.Get() : BallMaterial.Get());
		Mesh->SetWorldLocation(World(F.Shot.Release));
		Mesh->SetVisibility(true);
	}
	if (F.Shot.FlightSeconds <= 0.0)
	{
		F.T = 0.0f;
		F.bAfter = true;
		F.AfterPos = Local(F.Shot.Release);
		F.AfterVel = FVector::ZeroVector;
		++Revealed;
	}
}

void ABasketballCourt::UpdateFlights(float DeltaSeconds)
{
	using namespace BasketballCourtDetail;
	const double G = Gravity;
	for (int32 Slot = 0; Slot < FlightBallCount; ++Slot)
	{
		FFlight& F = Flights[Slot];
		UStaticMeshComponent* Mesh = FlightBalls.IsValidIndex(Slot) ? FlightBalls[Slot].Get() : nullptr;
		if (!F.bActive || !Mesh)
		{
			continue;
		}
		FVector Pos;
		if (!F.bAfter)
		{
			F.T += DeltaSeconds;
			if (F.T < F.Shot.FlightSeconds)
			{
				const FHoopVec3 At = BallAt(F.Shot.Release, F.Shot.Velocity, F.T);
				Pos = Local(At);
			}
			else
			{
				// At the rim: what happens next is what the model already decided.
				F.bAfter = true;
				++Revealed;
				const double Tf = F.Shot.FlightSeconds;
				const FVector In(F.Shot.Velocity.X, F.Shot.Velocity.Y, F.Shot.Velocity.Z - G * Tf);
				F.AfterPos = Local(F.Shot.Target);
				const FVector Off = FVector(F.Shot.Target.X, F.Shot.Target.Y, 0.0);
				const FVector Away = Off.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Off.GetSafeNormal();
				switch (F.Shot.Result)
				{
				case EShotResult::Swish:
				case EShotResult::RimIn:
					// Through the net: nearly straight down, a little of its run kept.
					F.AfterPos = FVector(F.AfterPos.X * 0.3, F.AfterPos.Y * 0.3, F.AfterPos.Z);
					F.AfterVel = FVector(In.X * 0.08, In.Y * 0.08, -160.0);
					break;
				case EShotResult::RimOut:
					// Off the iron, away from the middle and up.
					F.AfterVel = Away * 230.0 + FVector(0.0, 0.0, 170.0);
					break;
				default:
					// Short, long or wide: it misses everything and carries on.
					F.AfterVel = In;
					break;
				}
				Pos = F.AfterPos;
			}
		}
		else
		{
			F.AfterT += DeltaSeconds;
			F.AfterVel.Z -= G * DeltaSeconds;
			F.AfterPos += F.AfterVel * DeltaSeconds;
			// The glass: a long ball comes back off it.
			if (F.AfterVel.X < 0.0 && F.AfterPos.X - BallRadius < BoardFace
				&& F.AfterPos.Z > BoardBottom && F.AfterPos.Z < BoardBottom + BoardHeight
				&& FMath::Abs(F.AfterPos.Y) < BoardWidth * 0.5)
			{
				F.AfterPos.X = BoardFace + BallRadius;
				F.AfterVel.X = -0.5 * F.AfterVel.X;
			}
			// The court: it bounces, lower each time.
			if (F.AfterPos.Z < BallRadius)
			{
				F.AfterPos.Z = BallRadius;
				F.AfterVel.Z = FMath::Abs(F.AfterVel.Z) * 0.55;
				F.AfterVel.X *= 0.8;
				F.AfterVel.Y *= 0.8;
			}
			Pos = F.AfterPos;
			if (F.AfterT > 2.2f)
			{
				F.bActive = false;
				Mesh->SetVisibility(false);
				continue;
			}
		}
		const float Spin = (F.T + F.AfterT) * -540.0f;   // backspin, as a shooter puts on it
		Mesh->SetWorldLocationAndRotation(GetActorTransform().TransformPosition(Pos),
			GetActorQuat() * FQuat(FRotator(Spin, 0.0f, 0.0f)));
	}

	/* A MADE BALL SHOWS: the net swells round it as it drops through and kicks after it, and
	   the iron and the net flash green - gold for a money ball - fading over a second. All from
	   the made flights themselves, so a net nobody scored on stays exactly as built. */
	double Bulge = 0.0;
	double Depth = 0.0;
	double Swing = 0.0;
	double SinceExit = 1e9;
	double Flash = 0.0;
	bool bMoneyFlash = false;
	bool bNetMoving = false;
	// The made ball leaves the rim at 160 cm/s down (see above) and falls: it is out of the
	// net's bottom, NetDepth + a radius below, this long after the rim.
	const double Out = NetDepth + BallRadius;
	const double ExitT = (-160.0 + FMath::Sqrt(160.0 * 160.0 + 2.0 * G * Out)) / G;
	for (const FFlight& F : Flights)
	{
		if (!F.bActive || !F.bAfter || !IsMade(F.Shot.Result))
		{
			continue;
		}
		bNetMoving = true;
		const double Below = RimHeight - F.AfterPos.Z;
		const double In = FMath::Clamp((Below + BallRadius) / BallRadius, 0.0, 1.0)
			* FMath::Clamp(1.0 - (Below - NetDepth) / BallRadius, 0.0, 1.0);
		if (In > Bulge)
		{
			Bulge = In;
			Depth = Below;
		}
		// The kick is the newest ball's to drop out: the least time since it left the bottom.
		if (F.AfterT > ExitT && F.AfterT - ExitT < SinceExit)
		{
			SinceExit = F.AfterT - ExitT;
			Swing = FMath::Exp(-6.0 * SinceExit) * FMath::Cos(20.0 * SinceExit);
		}
		const double Fade = FMath::Exp(-3.0 * F.AfterT);
		if (Fade > Flash)
		{
			Flash = Fade;
			bMoneyFlash = F.Shot.bMoney;
		}
	}
	if (bNetMoving)
	{
		for (int32 I = 0; I < NetStrands; ++I)
		{
			const double ATop = 2.0 * PI * I / NetStrands;
			const FVector Top = NetPoint(FVector(RimRadius * FMath::Cos(ATop), RimRadius * FMath::Sin(ATop), RimHeight - RimTube), Bulge, Depth, Swing);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double ABottom = 2.0 * PI * (I + (Side == 0 ? -0.5 : 0.5)) / NetStrands;
				const FVector Bottom = NetPoint(FVector(NetBottomRadius * FMath::Cos(ABottom), NetBottomRadius * FMath::Sin(ABottom), RimHeight - NetDepth), Bulge, Depth, Swing);
				if (UStaticMeshComponent* Strand = NetParts.IsValidIndex(I * 3 + Side) ? NetParts[I * 3 + Side].Get() : nullptr)
				{
					SetRod(Strand, Top, Bottom, 0.6);
				}
			}
			const double B0 = 2.0 * PI * (I + 0.5) / NetStrands;
			const double B1 = 2.0 * PI * (I + 1.5) / NetStrands;
			if (UStaticMeshComponent* Ring = NetParts.IsValidIndex(I * 3 + 2) ? NetParts[I * 3 + 2].Get() : nullptr)
			{
				SetRod(Ring,
					NetPoint(FVector(NetBottomRadius * FMath::Cos(B0), NetBottomRadius * FMath::Sin(B0), RimHeight - NetDepth), Bulge, Depth, Swing),
					NetPoint(FVector(NetBottomRadius * FMath::Cos(B1), NetBottomRadius * FMath::Sin(B1), RimHeight - NetDepth), Bulge, Depth, Swing), 0.6);
			}
		}
	}
	const FLinearColor Lit = bMoneyFlash ? FLinearColor(1.6f, 1.15f, 0.2f) : FLinearColor(0.15f, 1.4f, 0.2f);
	const float A = static_cast<float>(Flash);
	auto Glow = [A, &Lit](UStaticMeshComponent* Part, const FLinearColor& Base)
	{
		if (UMaterialInstanceDynamic* M = Part ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr)
		{
			M->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Base, Lit, A));
		}
	};
	// Each list wears one paint (MakeMaterials), so the first part's is all of them.
	Glow(RimParts.Num() > 0 ? RimParts[0].Get() : nullptr, RimOrange);
	Glow(NetParts.Num() > 0 ? NetParts[0].Get() : nullptr, NetWhite);
}

int32 ABasketballCourt::RevealedScore() const
{
	int32 Total = 0;
	const std::vector<FShot>& Shots = Contest.GetShots();
	for (int32 I = 0; I < Revealed && I < static_cast<int32>(Shots.size()); ++I)
	{
		Total += Shots[I].Points;
	}
	return Total;
}

// --- Leonard's meter -------------------------------------------------------------------

void ABasketballCourt::UpdateMeter(float DeltaSeconds)
{
	if (!bMeterHeld)
	{
		return;
	}
	Meter += DeltaSeconds / FMath::Max(MeterSeconds, 0.1f);
	if (Meter >= 1.0f)
	{
		Meter = 1.0f;
		ReleaseMeter();   // held too long: it goes, late
	}
}

void ABasketballCourt::ReleaseMeter()
{
	bMeterHeld = false;
	/* THE BALL GOES WITH HIS HAND (Walt, 2026-10-15: "the ball flies off towards the hoop, and
	   then he throws afterwards"). Let go before Manny's arm is up and the arm hurries - what
	   is left of the wind-up in a fifth of a second - and UpdateShooterBody calls this again
	   when the hand gets there. Meter keeps where he let go, so the timing is his. */
	if (ShooterMesh && BodyMove == EBodyMove::Throw && bHandBallLive && BodyPosition() < ReleaseSeconds)
	{
		const float Left = ReleaseSeconds - BodyPosition();
		ShooterMesh->SetPlayRate(FMath::Max(ShooterMesh->GetPlayRate(), Left / 0.2f));
		return;
	}
	// -1 at an empty meter, 0 on the sweet spot, +1 at a full one.
	const float Timing = Meter < MeterSweetSpot
		? (Meter - MeterSweetSpot) / FMath::Max(MeterSweetSpot, 0.01f)
		: (Meter - MeterSweetSpot) / FMath::Max(1.0f - MeterSweetSpot, 0.01f);
	FShot Shot;
	if (Contest.ShootPlayer(Timing, Shot))
	{
		LaunchBall(Shot);
		LastShotNote = FMath::Abs(Timing) < 0.1f ? TEXT("A perfect release.")
			: Timing < 0.0f ? (Timing < -0.4f ? TEXT("Let go far too early - short.") : TEXT("A touch early."))
			: (Timing > 0.4f ? TEXT("Held it far too long - long.") : TEXT("A touch late."));
	}
	else
	{
		bHandBallLive = false;   // no ball to throw: nothing waits on his hand
	}
	Meter = 0.0f;
}

void ABasketballCourt::OnShootPressed()
{
	switch (State)
	{
	case EHoopState::PlayerRound:
		// Not while the last ball is still on its way out of his hand.
		if (Contest.IsBallReady() && !bMeterHeld && !(bHandBallLive && BodyMove == EBodyMove::Throw))
		{
			bMeterHeld = true;
			Meter = 0.0f;
			// Holding is Manny's wind-up: his hand reaches the release as the meter reaches
			// the sweet spot, and lets go wherever it is when Space does.
			if (ShooterMesh && BodyMove != EBodyMove::Glide)
			{
				StartThrow(0.0f, ReleaseSeconds / FMath::Max(MeterSweetSpot * MeterSeconds, 0.1f));
			}
		}
		break;
	case EHoopState::AgentRound:
	case EHoopState::Between:
		OnSkipShooter();
		break;
	default:
		break;
	}
}

void ABasketballCourt::OnShootReleased()
{
	if (State == EHoopState::PlayerRound && bMeterHeld)
	{
		ReleaseMeter();
	}
}

void ABasketballCourt::OnSkipShooter()
{
	if (State == EHoopState::AgentRound)
	{
		// The rest of her sixty seconds at once: the same model, no balls in the air.
		std::vector<FShot> Shots;
		Contest.Advance(FBasketballContest::TimeLimit, Shots);
		Revealed = static_cast<int32>(Contest.GetShots().size());
		for (const FShot& Shot : Contest.GetShots())
		{
			const int32 RackIndex = Shot.Rack * FBasketballContest::BallsPerRack + Shot.Ball;
			if (RackBallMeshes.IsValidIndex(RackIndex) && RackBallMeshes[RackIndex])
			{
				RackBallMeshes[RackIndex]->SetVisibility(false);
			}
		}
		FinishShooter();
		BetweenLeft = FMath::Min(BetweenLeft, 1.2f);
	}
	else if (State == EHoopState::Between)
	{
		BetweenLeft = 0.0f;
	}
}

void ABasketballCourt::OnSkipToPlayer()
{
	if (State != EHoopState::AgentRound && State != EHoopState::Between)
	{
		return;
	}
	if (State == EHoopState::AgentRound)
	{
		OnSkipShooter();
	}
	// Every agent still to shoot shoots now, unseen, from the same seeds a watched round uses.
	for (int32 I = Current + 1; I < Order.Num() - 1; ++I)
	{
		FBasketballContest Unseen;
		Unseen.Begin(Order[I], false, ContestSeed + 7919ull * static_cast<uint64>(I + 1));
		std::vector<FShot> Shots;
		Unseen.Advance(FBasketballContest::TimeLimit, Shots);
		Scores[I] = Unseen.GetScore();
	}
	for (FFlight& F : Flights)
	{
		F.bActive = false;
	}
	for (UStaticMeshComponent* Ball : FlightBalls)
	{
		if (Ball)
		{
			Ball->SetVisibility(false);
		}
	}
	BeginShooter(Order.Num() - 1);
}

void ABasketballCourt::OnAgain()
{
	if (State == EHoopState::Results)
	{
		NewContest();
	}
}

void ABasketballCourt::OnLeave()
{
	StopPlaying();
}

// --- the panel -------------------------------------------------------------------------

FString ABasketballCourt::RacksLine() const
{
	// O made, $ a made money ball, x missed, . still to come - only what he has seen land.
	FString Out;
	const std::vector<FShot>& Shots = Contest.GetShots();
	for (int32 Rack = 0; Rack < FBasketballContest::Racks; ++Rack)
	{
		Out += Rack == 0 ? TEXT("") : TEXT("   ");
		for (int32 Ball = 0; Ball < FBasketballContest::BallsPerRack; ++Ball)
		{
			const int32 I = Rack * FBasketballContest::BallsPerRack + Ball;
			if (I < Revealed && I < static_cast<int32>(Shots.size()))
			{
				Out += IsMade(Shots[I].Result) ? (Shots[I].bMoney ? TEXT("$") : TEXT("O")) : TEXT("x");
			}
			else
			{
				Out += TEXT(".");
			}
		}
	}
	return Out;
}

FString ABasketballCourt::StandingsLine() const
{
	FString Out;
	for (int32 I = 0; I < Order.Num(); ++I)
	{
		const FString Name = UTF8_TO_TCHAR(Order[I].Name.c_str());
		const FString Score = Scores.IsValidIndex(I) && Scores[I] >= 0 ? FString::FromInt(Scores[I]) : FString(TEXT("-"));
		Out += FString::Printf(TEXT("%s%s %s"), I == 0 ? TEXT("") : TEXT("    "), *Name, *Score);
	}
	return Out;
}

void ABasketballCourt::RefreshPanel()
{
	ASibeliusHUD* Hud = GetHud();
	if (!Hud || Current < 0 || !Order.IsValidIndex(Current))
	{
		return;
	}
	const FString Name = UTF8_TO_TCHAR(Order[Current].Name.c_str());
	const int32 Left = FMath::CeilToInt32(Contest.GetTimeLeft());
	const FString Clock = FString::Printf(TEXT("0:%02d"), FMath::Clamp(Left, 0, 60));
	FString Status;
	FString Controls;
	switch (State)
	{
	case EHoopState::AgentRound:
		Status = FString::Printf(TEXT("THREE-POINT CONTEST     %s shooting     %s     %d points"), *Name, *Clock, RevealedScore());
		Controls = TEXT("[Space] skip her round   [Enter] skip to your turn   [ESC] walk away");
		break;
	case EHoopState::PlayerRound:
	{
		Status = FString::Printf(TEXT("THREE-POINT CONTEST     Your round     %s     %d points"), *Clock, RevealedScore());
		FString Bar;
		constexpr int32 Width = 32;
		const int32 Filled = FMath::RoundToInt32(Meter * Width);
		for (int32 I = 0; I < Width; ++I)
		{
			const float Here = (I + 0.5f) / Width;
			const bool bSweet = FMath::Abs(Here - MeterSweetSpot) < 0.05f;
			Bar += I < Filled ? (bSweet ? TEXT("@") : TEXT("#")) : (bSweet ? TEXT("+") : TEXT("-"));
		}
		const FString Ready = Contest.IsBallReady() ? FString(TEXT("Ball ready."))
			: Contest.GetBall() == 0 && Contest.GetBallsTaken() > 0 ? FString(TEXT("Walking to the next rack..."))
			: FString(TEXT("Picking up the next ball..."));
		Status += FString::Printf(TEXT("\n%s   [%s]   %s"), *Ready, *Bar, *LastShotNote);
		Controls = TEXT("Hold [Space] or the left mouse button and let go on the ++ (the sweet spot)   [ESC] walk away");
		break;
	}
	case EHoopState::Between:
		Status = FString::Printf(TEXT("THREE-POINT CONTEST     %s scores %d."), *Name, Scores.IsValidIndex(Current) ? Scores[Current] : 0);
		Controls = TEXT("[Space] next shooter   [Enter] skip to your turn   [ESC] walk away");
		break;
	case EHoopState::Results:
	{
		const std::vector<int32> Ranked = RankScores(std::vector<int32>(Scores.GetData(), Scores.GetData() + Scores.Num()));
		FString Places;
		for (int32 Place = 0; Place < static_cast<int32>(Ranked.size()); ++Place)
		{
			const int32 Who = Ranked[Place];
			Places += FString::Printf(TEXT("%s%d. %s %d"), Place == 0 ? TEXT("") : TEXT("    "), Place + 1,
				UTF8_TO_TCHAR(Order[Who].Name.c_str()), Scores[Who]);
		}
		Status = FString::Printf(TEXT("THREE-POINT CONTEST - FINAL     %s\n%s"), *Places,
			PlayerPlace == 0 ? *FString::Printf(TEXT("You win! +%d sauce."), PrizeGiven)
				: *FString::Printf(TEXT("You came %d of %d. +%d sauce."), PlayerPlace + 1, Order.Num(), PrizeGiven));
		Controls = TEXT("[R] another contest   [ESC] walk away");
		Hud->ShowTablePanel(Status, Controls, 0.25f);
		return;
	}
	default:
		return;
	}
	Status += FString::Printf(TEXT("\n%s\n%s"), *RacksLine(), *StandingsLine());
	Hud->ShowTablePanel(Status, Controls, 0.25f);
}

ASibeliusHUD* ABasketballCourt::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
