// WrestlingMatch.cpp - THE MATCH, from the pack (2026-11-15).
//
// Walt, after 2.7.0's physics matches: "I had high hopes for realistic wrestling, but got
// convulsions, seizures and fondling." Real wrestling games do not simulate grappling; they play
// motion that was made in PAIRS, both halves at once, so every grip lands where the other body is.
// He bought RamsterZ's "Wrestler Finishers Volume 2" (Fab) for that, and this file plays it.
//
// HOW A PAIRED MOVE IS PLACED. Each half is authored with its wrestler at her own origin; the pack's
// demo level stands the two meshes a set distance apart, facing each other (the table below, read
// out of that level by Tools/Scripts/probe_wrestling_demo.py). So one wrestler - the anchor - stays
// where she is, and the other is put where the demo would put her. If that is more than a step
// away, she walks there first.
//
// HOW CLIPS JOIN. A clip ends with her root bone (her feet) somewhere; the next clip starts with its
// root bone somewhere else in its own space. Her body is moved so the two coincide: continuity of the
// feet, every clip. Throws carry the thrown wrestler a metre or two, and that is where she lies.
//
// The pack's skeleton lists metahuman_base_skel as compatible (Tools/Scripts/setup_wrestling_pack.py),
// as MorroMotion's does for the gestures, so the clips play straight on the agents' bodies.
//
// WHAT IS NOT HERE, on purpose (Walt: no hitting): the pack's strikes, choke hold, DDT, stunner,
// power bomb and spear; and the hold from behind, which reads as an embrace.

#include "WrestlingMat.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "WrestleAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

namespace WrestlingMatchDetail
{
	const TCHAR* const Pack = TEXT("/Game/Wrestler_Finishers_Volume2/Anim_Seq");
	const TCHAR* const Bow = TEXT("/Game/MorroMotion/Animations/Gestures_MoCap_02/Anim_bow.Anim_bow");

	// HOW THE PACK STANDS EACH PAIR, from its demo level: the attacker's mesh faces down -X (yaw 90),
	// the victim's mesh is Sep cm away down that line, turned VicYaw.
	struct FPairing
	{
		const TCHAR* Move;
		double Sep;
		double VicYaw;
	};
	const FPairing Pairings[] = {
		// 189.8 in the demo, for the mannequin's long arms; the agents' are shorter, and at that
		// distance their hands pushed air; at 172 they passed each other (Walt, 2026-11-15).
		{ TEXT("TestOfStrength_Start"), 181.0, -90.0 },
		{ TEXT("TestOfStrength_Loop"),  181.0, -90.0 },
		{ TEXT("TestOfStrength_End"),   181.0, -90.0 },
		{ TEXT("HeadLock_Start"),       111.9, -90.0 },
		{ TEXT("HeadLock_Loop"),        111.9, -90.0 },
		{ TEXT("HeadLock_End"),         111.9, -90.0 },
		{ TEXT("HeadLockFlipOver"),     111.9, -90.0 },
		{ TEXT("HipThrow"),              88.4, -90.0 },
		{ TEXT("LowScoopSlam"),          88.4, -90.0 },
		{ TEXT("Underarm_FlipToss"),     90.0, -90.0 },
		{ TEXT("LegsToHeadThrow"),      198.8, -90.0 },
		{ TEXT("GrabHeadFlipThrow"),    147.2, -90.0 },
		{ TEXT("BostonCrab_Start"),     133.6, -90.0 },
		{ TEXT("BostonCrab_Loop"),      132.6, -90.0 },
		{ TEXT("BostonCrab_End"),       133.6, -90.0 },
		{ TEXT("Pin"),                  104.1, 180.0 },   // every pin clip: she lies across his line
	};
	enum : int32 { TosStart, TosLoop, TosEnd, HlStart, HlLoop, HlEnd, HlFlip, Hip, Scoop, Toss, Headscissors, FlipThrow, CrabStart, CrabLoop, CrabEnd, PinRow };

	// Clips join at the root, so compare the two poses in that same frame. The counter can
	// finish differently for its two participants. Each should recover from her own landing.
	double RecoveryDistance(const UAnimSequence* Landing, const UAnimSequence* Candidate)
	{
		if (!Landing || !Candidate || !Landing->GetSkeleton() || Landing->GetSkeleton() != Candidate->GetSkeleton())
		{
			return TNumericLimits<double>::Max();
		}
		const FReferenceSkeleton& Skeleton = Landing->GetSkeleton()->GetReferenceSkeleton();
		const FAnimExtractContext End(static_cast<double>(Landing->GetPlayLength()));
		const FAnimExtractContext Start(0.0);
		static const FName Joints[] = { TEXT("pelvis"), TEXT("head"), TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r") };
		double Total = 0.0;
		for (FName Joint : Joints)
		{
			const int32 Index = Skeleton.FindBoneIndex(Joint);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			FTransform From = FTransform::Identity;
			FTransform To = FTransform::Identity;
			for (int32 Bone = Index; Bone > 0; Bone = Skeleton.GetParentIndex(Bone))
			{
				FTransform Local;
				Landing->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), End, false);
				From *= Local;
				Candidate->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), Start, false);
				To *= Local;
			}
			Total += FVector::DistSquared(From.GetLocation(), To.GetLocation());
		}
		return Total;
	}

	constexpr float GapCm = 12.0f;     // further than this from where the move wants her: she walks
	constexpr float GapDeg = 12.0f;
	constexpr float DownHeight = 45.0f;   // hips lower than this above the mat: she is on the mat
	// Walk to the next move's spot; off, they appear there (BeginStep). A console variable, not a
	// constant, so the walk code below it is not "unreachable" to the compiler.
	TAutoConsoleVariable<bool> CVarWalkTo(TEXT("sib.Wrestle.Walk"), false, TEXT("Wrestlers walk to the next move instead of appearing there."));

	// WHO SEES WHOM, AND HANDS THAT MEET (Walt, 2026-11-26). Both on; each can be switched off
	// to compare in the lab.
	TAutoConsoleVariable<bool> CVarLookAt(TEXT("sib.Wrestle.LookAt"), true, TEXT("Wrestlers turn their heads to each other; the referee watches the pair."));
	TAutoConsoleVariable<bool> CVarGrip(TEXT("sib.Wrestle.Grip"), true, TEXT("In the test of strength each hand reaches the other wrestler's hand."));
	constexpr double GripGapCm = 12.0;     // wrist to wrist when two hands press together, palm to palm
	constexpr double GripNearCm = 25.0;    // the clip's hands this close: the reach is full
	constexpr double GripFarCm = 45.0;     // this far apart: no reach (still coming in, or letting go)

	/** The wrestling player on this body, or null while some other player drives it. */
	UWrestleAnimInstance* WrestlePlayer(USkeletalMeshComponent* Body)
	{
		return Body ? Cast<UWrestleAnimInstance>(Body->GetAnimInstance()) : nullptr;
	}

	/** Make the wrestling player the body's player: the single-node player plus look and reach. */
	void UseWrestlePlayer(USkeletalMeshComponent* Body)
	{
		if (Body && !WrestlePlayer(Body))
		{
			Body->SetAnimInstanceClass(UWrestleAnimInstance::StaticClass());
		}
	}

	FTransform RootAt(const UAnimSequence* Anim, double Time)
	{
		FTransform Out = FTransform::Identity;
		if (Anim)
		{
			const double Length = Anim->GetPlayLength();
			const FAnimExtractContext Context(FMath::Clamp(Time, 0.0, Length));
			Anim->GetBoneTransform(Out, FSkeletonPoseBoneIndex(0), Context, false);
		}
		return Out;
	}

	FTransform Yawed(const FVector& At, double Yaw)
	{
		return FTransform(FRotator(0.0, Yaw, 0.0), At);
	}

	double YawOf(const FTransform& T)
	{
		return T.Rotator().Yaw;
	}

	// Feet whose forward - the skeleton's +Y - points along Dir.
	FTransform FeetFacing(const FVector& At, const FVector& Dir)
	{
		return Yawed(At, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) - 90.0);
	}

	double FlatDist(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}
}

UAnimSequence* AWrestlingMat::Clip(const TCHAR* Folder, const FString& Name)
{
	using namespace WrestlingMatchDetail;
	const FString Path = Folder ? FString::Printf(TEXT("%s/%s/%s.%s"), Pack, Folder, *Name, *Name) : Name;
	if (UAnimSequence** Found = ClipCache.Find(Path))
	{
		return *Found;
	}
	UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Anim)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Wrestle] no clip %s."), *Path);
	}
	else
	{
		MatchAnims.Add(Anim);
	}
	ClipCache.Add(Path, Anim);
	return Anim;
}

USkeletalMeshComponent* AWrestlingMat::MatchBody(int32 Who) const
{
	return Who < 2 ? W[Who].Body : RefBody;
}

AActor* AWrestlingMat::MatchActor(int32 Who) const
{
	return Who < 2 ? W[Who].Actor : RefActor;
}

FVector AWrestlingMat::ViewDir() const
{
	FVector Eye = GetActorLocation() - GetActorForwardVector() * 550.0;
	// The lab's pictures come from the level's first camera (TickLabShots), not from the player.
	const bool bLab = bLabCamera || FCString::Strifind(FCommandLine::Get(), TEXT("WrestleShots=")) != nullptr;
	if (bLab && GetWorld())
	{
		for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
		{
			Eye = It->GetActorLocation();
			break;
		}
	}
	else if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Eye = Cam->GetCameraLocation();
	}
	FVector V = Eye - MatTop();
	V.Z = 0.0;
	return V.GetSafeNormal(UE_SMALL_NUMBER, -GetActorForwardVector());
}

// Where her feet - her root bone - are in the world now. A clip that has finished holds its last
// frame, so its end is read from the clip itself, not from a pose that may lag a frame.
FTransform AWrestlingMat::FeetNow(int32 Who) const
{
	using namespace WrestlingMatchDetail;
	const USkeletalMeshComponent* Body = MatchBody(Who);
	if (!Body)
	{
		return FTransform::Identity;
	}
	const FPlaying& P = Play[Who];
	double Time = P.Length;
	if (P.bLoop || P.Age < P.Length)
	{
		Time = Body->GetPosition();
	}
	return RootAt(P.Anim, Time) * Body->GetComponentTransform();
}

// Put her body (the skeletal mesh) here, by moving her actor: upright, on the mat.
void AWrestlingMat::PutBody(int32 Who, const FTransform& BodyWorld)
{
	AActor* Actor = MatchActor(Who);
	if (!Actor)
	{
		return;
	}
	const FTransform At = BodyInActor[Who].Inverse() * BodyWorld;
	Actor->SetActorLocationAndRotation(FVector(At.GetLocation().X, At.GetLocation().Y, MatZ + GroundLift[Who]),
		FRotator(0.0, At.Rotator().Yaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
}

// Where the other half of a paired move puts its wrestler's body, given this half's body.
FTransform AWrestlingMat::PairedBody(int32 Pairing, bool bVictim, const FTransform& OtherBody) const
{
	using namespace WrestlingMatchDetail;
	const FPairing& P = Pairings[FMath::Clamp(Pairing, 0, (int32)UE_ARRAY_COUNT(Pairings) - 1)];
	const FTransform Att = Yawed(FVector::ZeroVector, 90.0);
	const FTransform Vic = Yawed(FVector(-P.Sep, 0.0, 0.0), P.VicYaw);
	const FTransform VicInAtt = Vic * Att.Inverse();
	return bVictim ? VicInAtt * OtherBody : VicInAtt.Inverse() * OtherBody;
}

void AWrestlingMat::PlayOn(int32 Who, UAnimSequence* Anim, bool bLoop)
{
	USkeletalMeshComponent* Body = MatchBody(Who);
	if (!Body || !Anim)
	{
		return;
	}
	WrestlingMatchDetail::UseWrestlePlayer(Body);
	if (WrestlingMatchDetail::WrestlePlayer(Body))
	{
		Body->SetAnimation(Anim);   // what PlayAnimation does, without switching back to the plain player
		Body->Play(bLoop);
	}
	else
	{
		Body->PlayAnimation(Anim, bLoop);
	}
	FPlaying& P = Play[Who];
	P = FPlaying();
	P.Anim = Anim;
	P.Length = static_cast<float>(Anim->GetPlayLength());
	P.bLoop = bLoop;
}

bool AWrestlingMat::SpawnReferee(int32 Who)
{
	UWorld* World = GetWorld();
	FString ClassPath;
	AgentInfo(Who, RefName, ClassPath);
	UClass* Class = World ? LoadClass<AActor>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Class)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RefActor = World->SpawnActor<AActor>(Class, GetActorTransform(), Params);
	if (!RefActor)
	{
		return false;
	}
	Actors.Add(RefActor);
	TArray<USkeletalMeshComponent*> Meshes;
	RefActor->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetPhysicsAsset() && !Mesh->LeaderPoseComponent.IsValid()
			&& (!RefBody || Mesh->GetName().Contains(TEXT("Body"))))
		{
			RefBody = Mesh;
		}
	}
	TArray<UPrimitiveComponent*> Parts;
	RefActor->GetComponents<UPrimitiveComponent>(Parts);
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part)
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (RefBody)
	{
		RefBody->AnimationData.AnimToPlay = nullptr;
		RefBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		RefBody->bEnableUpdateRateOptimizations = false;
	}
	return RefBody != nullptr;
}

// --- the match ------------------------------------------------------------------------------

void AWrestlingMat::StartMatch()
{
	using namespace WrestlingMatchDetail;
	RefActor = nullptr;
	RefBody = nullptr;
	// The cast: his choice, once he has made one; else the next pairing, refereed by an agent not in it.
	if (!bChosen)
	{
		PairOf(PairIndex, Lineup[0], Lineup[1]);
		++PairIndex;
		const int32 N = NumAgents();
		for (int32 K = 0; K < N; ++K)
		{
			const int32 Candidate = (Lineup[0] + 2 + K) % N;
			if (Candidate != Lineup[0] && Candidate != Lineup[1])
			{
				Lineup[2] = Candidate;
				break;
			}
		}
	}
	ClearPair();
	if (!SpawnWrestler(0, Lineup[0]) || !SpawnWrestler(1, Lineup[1]))
	{
		ClearPair();
		Announce(TEXT("The wrestlers are not here"), 3.0f);
		return;
	}
	SpawnReferee(Lineup[2]);

	// What his [E] ray hits: a sphere on each of them, seen only by that ray (TickMatch moves them).
	while (Pickers.Num() < 3)
	{
		USphereComponent* Picker = NewObject<USphereComponent>(this);
		Picker->SetupAttachment(GetRootComponent());
		Picker->RegisterComponent();
		Picker->InitSphereRadius(40.0f);
		Picker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Picker->SetCollisionResponseToAllChannels(ECR_Ignore);
		Picker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Picker->SetHiddenInGame(true);
		Pickers.Add(Picker);
	}

	Phase = EPhase::Match;
	PhaseAge = 0.0f;
	StepAge = 0.0f;
	bStepLive = false;
	Steps.Reset();
	Current = FStep();
	Exchange = 0;
	NextAttacker = Rng.RandRange(0, 1);
	MatchWinner = -1;
	MatZ = MatTop().Z;
	GroundLift[0] = GroundLift[1] = GroundLift[2] = 0.0f;

	// Side by side across his view, facing each other; the referee across the mat, facing him.
	const FVector Middle = MatTop();
	const FVector View = ViewDir();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, View).GetSafeNormal();
	const FTransform Start[3] = {
		FeetFacing(Middle - Side * 95.0, Side),
		FeetFacing(Middle + Side * 95.0, -Side),
		FeetFacing(Middle - View * 150.0, View),   // inside his [E] reach (450) from where he lands
	};
	RefPost = Start[2];
	UAnimSequence* Idle = Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01"));
	for (int32 Who = 0; Who < 3; ++Who)
	{
		USkeletalMeshComponent* Body = MatchBody(Who);
		AActor* Actor = MatchActor(Who);
		Play[Who] = FPlaying();
		if (!Body || !Actor)
		{
			continue;
		}
		UseWrestlePlayer(Body);
		Body->AddTickPrerequisiteActor(this);   // she is placed first, then posed: the walk predicts her pose
		BodyInActor[Who] = Body->GetComponentTransform().GetRelativeTransform(Actor->GetActorTransform());
		PutBody(Who, RootAt(Idle, 0.0).Inverse() * Start[Who]);
		PlayOn(Who, Idle, true);
	}

	const FString Names = FString::Printf(TEXT("%s  vs  %s"), *W[0].Name.ToUpper(), *W[1].Name.ToUpper());
	if (Sign)
	{
		Sign->SetText(FText::FromString(Names));
	}
	Announce(Names + (RefBody ? FString::Printf(TEXT("\nReferee: %s"), *RefName) : FString()), 3.0f);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] match: %s vs %s, referee %s."), *W[0].Name, *W[1].Name, RefBody ? *RefName : TEXT("none"));

	// To the crowd first.
	FStep Intro;
	Intro.Act[0] = { EAct::Once, Clip(TEXT("Wrestling_Taunt"), FString::Printf(TEXT("Wrestling_Taunt_%02d"), Rng.RandRange(1, 5))) };
	Intro.Act[1] = { EAct::Once, Clip(TEXT("Wrestling_Taunt"), FString::Printf(TEXT("Wrestling_Taunt_%02d"), Rng.RandRange(1, 5))) };
	Intro.Act[2] = { EAct::Still, Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Referee_End")) };   // she stands as a referee does
	Intro.Say = Names;
	Steps.Add(Intro);
}

void AWrestlingMat::PlanExchange()
{
	using namespace WrestlingMatchDetail;
	const int32 Lead = NextAttacker;
	const FString Names = FString::Printf(TEXT("%s  vs  %s"), *W[0].Name.ToUpper(), *W[1].Name.ToUpper());

	auto Paired = [this](int32 Row, const FString& Clips, int32 Attacker, const FString& Say)
	{
		FStep S;
		S.Pairing = Row;
		S.Att = S.Anchor = Attacker;
		S.Act[Attacker] = { EAct::Once, Clip(TEXT("Wrestling_Paired"), TEXT("Wrestling_Paired_") + Clips + TEXT("_Att")) };
		S.Act[1 - Attacker] = { EAct::Once, Clip(TEXT("Wrestling_Paired"), TEXT("Wrestling_Paired_") + Clips + TEXT("_Vic")) };
		S.Say = Say;
		Steps.Add(S);
	};
	// Keep the draw across rematches. On a refill draw from the other moves first.
	int32 Eligible = RemainingThrows.Num();
	if (Eligible == 0)
	{
		RemainingThrows = { Hip, Scoop, Toss };
		for (int32 Row : { Headscissors, FlipThrow })
		{
			const FString Prefix = FString(TEXT("Wrestling_Paired_")) + Pairings[Row].Move;
			if (Clip(TEXT("Wrestling_Paired"), Prefix + TEXT("_Att"))
				&& Clip(TEXT("Wrestling_Paired"), Prefix + TEXT("_Vic")))
			{
				RemainingThrows.Add(Row);
			}
		}
		const int32 Previous = RemainingThrows.Find(LastThrow);
		Eligible = RemainingThrows.Num();
		if (Previous != INDEX_NONE)
		{
			RemainingThrows.Swap(Previous, --Eligible);
		}
	}
	const int32 Draw = Rng.RandRange(0, Eligible - 1);
	LastThrow = RemainingThrows[Draw];
	RemainingThrows.RemoveAtSwap(Draw);
	// In the counter combination the opening grip releases before the other wrestler
	// takes the headlock. The wrestler who wins that second grip owns the throw and cover.
	const bool bGripReversal = LastThrow == Hip && Rng.RandRange(0, 1) == 0;
	const int32 A = bGripReversal ? 1 - Lead : Lead;
	const int32 D = 1 - A;
	const bool bStrength = bGripReversal || Rng.FRand() < 0.5f;
	if (bStrength)
	{
		Paired(TosStart, Pairings[TosStart].Move, Lead, Names);
		Paired(TosLoop, Pairings[TosLoop].Move, Lead, FString());
		Paired(TosEnd, Pairings[TosEnd].Move, Lead, FString());
	}
	if (!bStrength || bGripReversal)
	{
		Paired(HlStart, Pairings[HlStart].Move, A, bGripReversal ? Names + TEXT("\nGRIP REVERSAL") : Names);
		Paired(HlLoop, Pairings[HlLoop].Move, A, FString());
		Paired(HlEnd, Pairings[HlEnd].Move, A, FString());
	}
	const bool bCounter = LastThrow == Headscissors;
	const FString MoveCall = bCounter ? Names + TEXT("\nHEADSCISSORS COUNTER")
		: bGripReversal ? Names + TEXT("\nCOUNTER HIP THROW")
		: LastThrow == FlipThrow ? Names + TEXT("\nFLIP THROW") : FString();
	Paired(LastThrow, Pairings[LastThrow].Move, bCounter ? D : A, MoveCall);
	if (bCounter || LastThrow == FlipThrow)
	{
		const FStep& Throw = Steps.Last();
		UAnimSequence* Idle = Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01"));
		UAnimSequence* Back = Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp02"));
		UAnimSequence* Front = Clip(TEXT("Wrestling_LayOnStomach"), TEXT("Wrestling_LayOnStomach_GetUp02"));
		FStep Reset;
		for (int32 Who = 0; Who < 2; ++Who)
		{
			UAnimSequence* Best = Idle;
			double BestDistance = RecoveryDistance(Throw.Act[Who].Anim, Idle);
			for (UAnimSequence* GetUp : { Back, Front })
			{
				const double Distance = RecoveryDistance(Throw.Act[Who].Anim, GetUp);
				if (Distance < BestDistance)
				{
					Best = GetUp;
					BestDistance = Distance;
				}
			}
			Reset.Act[Who] = { Best == Idle ? EAct::Loop : EAct::Once, Best };
		}
		// A standing attacker can cover a defender who finished on her back.
		// On any other landing, use the recoveries that matched their individual poses.
		if (bCounter || !Idle || !Back || !Front || Reset.Act[A].Anim != Idle || Reset.Act[D].Anim != Back)
		{
			Steps.Add(Reset);
			NextAttacker = D;
			return;
		}
	}

	// One continuous cover per takedown. Let the defender work against it before the count.
	++Exchange;
	const bool bWin = Exchange >= 3 || (Exchange >= 2 && Rng.FRand() < 0.5f);
	auto PinStep = [this, A, D](const TCHAR* Part, const TCHAR* Ref, const FString& Say)
	{
		FStep S;
		S.Pairing = PinRow;
		S.Att = A;
		S.Anchor = D;
		S.Act[A] = { EAct::Once, Clip(TEXT("Wrestling_Pin"), FString::Printf(TEXT("Wrestling_Pin_%s_Attacker"), Part)) };
		S.Act[D] = { EAct::Once, Clip(TEXT("Wrestling_Pin"), FString::Printf(TEXT("Wrestling_Pin_%s_Victim"), Part)) };
		if (Ref)
		{
			S.Act[2] = { EAct::Once, Clip(TEXT("Wrestling_Pin"), FString::Printf(TEXT("Wrestling_Pin_Referee_%s"), Ref)) };
		}
		S.Say = Say;
		Steps.Add(S);
	};
	FStep Setup;
	Setup.bPinSetup = true;
	Setup.Att = A;
	Setup.Act[D] = { EAct::Loop, Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_Idle")) };
	Steps.Add(Setup);
	PinStep(TEXT("Start"), TEXT("Start"), Names);
	const int32 LastCount = bWin ? 3 : (Exchange == 1 ? Rng.RandRange(0, 2) : 2);
	// A flash cover can draw a one-count immediately. The longer covers settle into a struggle.
	if (LastCount > 1)
	{
		PinStep(TEXT("Resist"), nullptr, FString());
		Steps.Last().Act[2] = { EAct::Loop, Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Referee_Loop")) };
	}
	for (int32 Number = 1; Number <= LastCount; ++Number)
	{
		const bool bFinalTap = Number == 3;
		PinStep(Number == 2 ? TEXT("Loop02") : TEXT("Loop01"), bFinalTap ? TEXT("Tap02") : TEXT("Tap01"),
			FString::Printf(TEXT("%s\n%d"), *Names, Number));
		Steps.Last().bCountOnTap = true;
	}
	if (bWin)
	{
		PinStep(TEXT("End"), TEXT("End"), FString::Printf(TEXT("%s WINS"), *W[A].Name.ToUpper()));
		PlanFinish(A);
		return;
	}
	PinStep(TEXT("Failure"), TEXT("End"), LastCount == 0 ? TEXT("ESCAPE BEFORE ONE!") : TEXT("KICK OUT!"));

	// After an escape, the wrestler who got out takes the initiative at the next tie-up.
	NextAttacker = D;
	FStep Recovery;
	UAnimSequence* Rise = Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp02"));
	if (LastCount == 0)
	{
		UAnimSequence* Alternate = Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp03"));
		const UAnimSequence* Escape = Steps.Last().Act[D].Anim;
		if (Alternate && (!Rise || RecoveryDistance(Escape, Alternate) <= RecoveryDistance(Escape, Rise)))
		{
			Rise = Alternate;
		}
	}
	Recovery.Act[D] = { EAct::Once, Rise };
	Recovery.Act[A] = { EAct::Loop, Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01")) };
	Recovery.Act[2] = { EAct::Walk, nullptr, RefPost, 140.0f };
	Steps.Add(Recovery);
}

void AWrestlingMat::PlanFinish(int32 Winner)
{
	MatchWinner = Winner;
	const int32 Loser = 1 - Winner;
	FStep Cheer;
	Cheer.Act[Winner] = { EAct::Once, Clip(TEXT("Wrestling_Crowdwork"), TEXT("Wrestling_Crowdwork_01")) };
	Cheer.Act[Loser] = { EAct::Once, Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp01")) };
	Cheer.Act[2] = { EAct::Walk, nullptr, RefPost, 140.0f };
	Steps.Add(Cheer);
	FStep Face;
	Face.bFaceView = true;
	Steps.Add(Face);
	FStep Bow;
	UAnimSequence* BowClip = Clip(nullptr, WrestlingMatchDetail::Bow);
	for (FAct& Act : Bow.Act)
	{
		Act = { EAct::Once, BowClip };
	}
	Steps.Add(Bow);
	FStep Rest;
	for (FAct& Act : Rest.Act)
	{
		Act = { EAct::Loop, Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01")) };
	}
	Rest.Hold = 4.0f;
	Steps.Add(Rest);
	FStep Next;
	Next.bNextMatch = true;
	Steps.Add(Next);
}

void AWrestlingMat::BeginStep()
{
	using namespace WrestlingMatchDetail;
	Current = Steps[0];
	Steps.RemoveAt(0);
	StepAge = 0.0f;
	bStepLive = true;
	PeakHandHeight[0] = PeakHandHeight[1] = 0.0f;

	FTransform Feet[3];
	for (int32 Who = 0; Who < 3; ++Who)
	{
		Feet[Who] = FeetNow(Who);
	}
	UAnimSequence* Idle = Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01"));

	if (Current.bFaceView)
	{
		for (int32 Who = 0; Who < 3; ++Who)
		{
			Current.Act[Who] = { EAct::Walk, nullptr, FeetFacing(Feet[Who].GetLocation(), ViewDir()), 110.0f };
		}
	}
	if (Current.bPinSetup)
	{
		// The pin is laid on her where she lies; the attacker and the referee walk to it.
		const int32 A = Current.Att;
		const int32 D = 1 - A;
		UAnimSequence* VicStart = Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Start_Victim"));
		UAnimSequence* AttStart = Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Start_Attacker"));
		const FTransform BodyD = RootAt(VicStart, 0.0).Inverse() * Feet[D];
		const FTransform BodyA = PairedBody(PinRow, false, BodyD);
		Current.Act[A] = { EAct::Walk, nullptr, RootAt(AttStart, 0.0) * BodyA, 110.0f };
		// The referee kneels so her slap (a hand 124 cm ahead of where her feet start, 15 to the
		// right) lands by the head: from the far side of her, or from beyond her head - whichever
		// keeps the referee behind the pin as he sees it, not between him and it.
		const FVector View = ViewDir();
		const FVector Slaps[2] = { Feet[D].TransformPosition(FVector(40.0, -70.0, 0.0)), Feet[D].TransformPosition(FVector(5.0, -115.0, 0.0)) };
		const FVector Ways[2] = { Feet[D].TransformVectorNoScale(FVector(-1.0, 0.0, 0.0)), Feet[D].TransformVectorNoScale(FVector(0.0, 1.0, 0.0)) };
		FTransform RefFeet;
		double Best = TNumericLimits<double>::Max();
		for (int32 K = 0; K < 2; ++K)
		{
			FTransform Spot = FeetFacing(Slaps[K], Ways[K]);
			Spot.SetLocation(Slaps[K] - Spot.TransformVectorNoScale(FVector(14.6, 124.4, 0.0)));
			const double Toward = FVector::DotProduct(Spot.GetLocation() - Feet[D].GetLocation(), View);
			if (Toward < Best)
			{
				Best = Toward;
				RefFeet = Spot;
			}
		}
		Current.Act[2] = { EAct::Walk, nullptr, RefFeet, 160.0f };
	}

	// A paired move: the one who is placed against the anchor - is she where the move wants her?
	FTransform AnchorBody = FTransform::Identity;
	if (Current.Pairing >= 0)
	{
		const int32 Anchor = Current.Anchor;
		const int32 Other = 1 - Anchor;
		const USkeletalMeshComponent* AnchorMesh = MatchBody(Anchor);
		const bool bDown = AnchorMesh && AnchorMesh->GetBoneLocation(TEXT("pelvis")).Z - MatZ < DownHeight;
		UAnimSequence* AnchorAnim = Current.Act[Anchor].Anim;
		FTransform AnchorFeet = Feet[Anchor];
		AnchorBody = RootAt(AnchorAnim, 0.0).Inverse() * AnchorFeet;

		const int32 Row = Current.Pairing;
		const bool bFresh = Row == TosStart || Row == HlStart || Row == HlFlip || (Row >= Hip && Row <= FlipThrow);
		bool bMoveAnchor = false;
		if (bFresh && !bDown)
		{
			// Fit the next grip to where the wrestlers ended the previous move. Match the line
			// between their feet, then split the spacing correction equally between them.
			UAnimSequence* OtherAnim = Current.Act[Other].Anim;
			const FTransform AnchorStart = RootAt(AnchorAnim, 0.0);
			const FTransform OtherStart = RootAt(OtherAnim, 0.0);
			FTransform Partner = PairedBody(Row, Other != Current.Att, AnchorBody);
			const FVector Stance = (OtherStart * Partner).GetLocation() - AnchorFeet.GetLocation();
			const FVector Between = Feet[Other].GetLocation() - Feet[Anchor].GetLocation();
			if (Between.SizeSquared2D() > FMath::Square(20.0) && Stance.SizeSquared2D() > 1.0)
			{
				const double Delta = FMath::FindDeltaAngleDegrees(Stance.Rotation().Yaw, Between.Rotation().Yaw);
				AnchorFeet.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(Delta)) * AnchorFeet.GetRotation());
				AnchorBody = AnchorStart.Inverse() * AnchorFeet;
				Partner = PairedBody(Row, Other != Current.Att, AnchorBody);
			}
			FVector Shift = (Feet[Other].GetLocation() - (OtherStart * Partner).GetLocation()) * 0.5;
			Shift.Z = 0.0;

			// Leave room at the edge for the landing. Work in mat coordinates because the AI City
			// pad can face any direction. Keep the preferred placement when the clip already fits.
			const FTransform MatFrame = GetActorTransform();
			FBox Travel(ForceInit);
			auto IncludePath = [&](const UAnimSequence* Anim, const FTransform& BodyWorld)
			{
				const double Length = Anim ? Anim->GetPlayLength() : 0.0;
				const int32 Intervals = FMath::Max(1, FMath::CeilToInt(Length * 6.0));
				for (int32 I = 0; I <= Intervals; ++I)
				{
					const FVector At = (RootAt(Anim, Length * I / Intervals) * BodyWorld).GetLocation() + Shift;
					Travel += MatFrame.InverseTransformPosition(At);
				}
			};
			IncludePath(AnchorAnim, AnchorBody);
			IncludePath(OtherAnim, Partner);
			const double Limit = FMath::Max(0.0, static_cast<double>(MatHalf) - 65.0);
			auto EdgeShift = [Limit](double Min, double Max)
			{
				const double Low = -Limit - Min;
				const double High = Limit - Max;
				return Low <= High ? FMath::Clamp(0.0, Low, High) : (Low + High) * 0.5;
			};
			Shift += MatFrame.TransformVector(FVector(EdgeShift(Travel.Min.X, Travel.Max.X),
				EdgeShift(Travel.Min.Y, Travel.Max.Y), 0.0));
			AnchorFeet.AddToTranslation(Shift);
			AnchorBody = AnchorStart.Inverse() * AnchorFeet;
			bMoveAnchor = Shift.SizeSquared2D() > FMath::Square(GapCm)
				|| FMath::Abs(FMath::FindDeltaAngleDegrees(YawOf(Feet[Anchor]), YawOf(AnchorFeet))) > GapDeg;
		}

		const FTransform OtherBody = PairedBody(Row, Other != Current.Att, AnchorBody);
		const FTransform Want = RootAt(Current.Act[Other].Anim, 0.0) * OtherBody;
		const double Gap = FlatDist(Want.GetLocation(), Feet[Other].GetLocation());
		const double Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(YawOf(Feet[Other]), YawOf(Want)));
		// Loop and release clips are already in contact. Inserting a standing reset here
		// breaks the hold for a frame (or for an entire walk when walking is enabled).
		if (bFresh && CVarWalkTo.GetValueOnGameThread() && (bMoveAnchor || Gap > GapCm || Turn > GapDeg))
		{
			// Walk there first, and come back to this move. The anchor waits (standing, she idles;
			// on the mat, she stays as she lies) unless she is the one being brought to the middle.
			Steps.Insert(Current, 0);
			FStep Walk;
			Walk.Act[Other] = { EAct::Walk, nullptr, Want, 110.0f };
			if (bMoveAnchor)
			{
				Walk.Act[Anchor] = { EAct::Walk, nullptr, AnchorFeet, 110.0f };
			}
			else
			{
				Walk.Act[Anchor] = { bDown ? EAct::None : EAct::Loop, bDown ? nullptr : Idle };
			}
			Current = Walk;
		}
	}

	// Nobody stands frozen: whoever finished a clip on her feet and has nothing to do idles.
	for (int32 Who = 0; Who < 3; ++Who)
	{
		const USkeletalMeshComponent* Mesh = MatchBody(Who);
		const FPlaying& P = Play[Who];
		if (Mesh && Current.Act[Who].Kind == EAct::None && !P.bLoop && P.Age >= P.Length
			&& Mesh->GetBoneLocation(TEXT("pelvis")).Z - MatZ >= DownHeight)
		{
			Current.Act[Who] = Who == 2 ? FAct{ EAct::Still, Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Referee_End")) } : FAct{ EAct::Loop, Idle };
		}
	}

	if (!Current.bCountOnTap && !Current.Say.IsEmpty() && Sign)
	{
		Sign->SetText(FText::FromString(Current.Say));
	}
	if (bLog)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] step: %s | %s | %s%s"),
			Current.Act[0].Anim ? *Current.Act[0].Anim->GetName() : (Current.Act[0].Kind == EAct::Walk ? TEXT("walk") : TEXT("-")),
			Current.Act[1].Anim ? *Current.Act[1].Anim->GetName() : (Current.Act[1].Kind == EAct::Walk ? TEXT("walk") : TEXT("-")),
			Current.Act[2].Anim ? *Current.Act[2].Anim->GetName() : (Current.Act[2].Kind == EAct::Walk ? TEXT("walk") : TEXT("-")),
			Current.Say.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" \"%s\""), *Current.Say.Replace(TEXT("\n"), TEXT(" / "))));
	}

	for (int32 Who = 0; Who < 3; ++Who)
	{
		FAct& Act = Current.Act[Who];
		if (!MatchBody(Who) || Act.Kind == EAct::None)
		{
			continue;
		}
		if (Act.Kind == EAct::Walk)
		{
			// NO WALKING (Walt, 2026-11-15: the pack's walk is a hunched shuffle with clawed hands, at
			// times backwards). She simply appears where the next move starts. WalkTo keeps the
			// walk below, switched off, in case a better walk is ever found.
			if (!CVarWalkTo.GetValueOnGameThread())
			{
				UAnimSequence* Stand = Who == 2 ? Clip(TEXT("Wrestling_Pin"), TEXT("Wrestling_Pin_Referee_End")) : Idle;
				const bool bStill = Who == 2;
				const double At = bStill && Stand ? Stand->GetPlayLength() : 0.0;
				PutBody(Who, RootAt(Stand, At).Inverse() * Act.Feet);
				PlayOn(Who, Stand, !bStill);
				if (bStill && MatchBody(Who))
				{
					MatchBody(Who)->SetPosition(static_cast<float>(At), false);
					Play[Who].Age = Play[Who].Length;
				}
				continue;
			}
			// Turn toward where she is going and walk, then turn to how she must stand there.
			FPlaying& P = Play[Who];
			const FTransform From = Feet[Who];
			const FVector Way = Act.Feet.GetLocation() - From.GetLocation();
			const double Dist = Way.Size2D();
			UAnimSequence* Walking = Clip(TEXT("Wrestling_Walk"), TEXT("Wrestling_Walk_Forward"));
			const bool bWalks = Dist > 20.0 && Walking;
			PlayOn(Who, bWalks ? Walking : Idle, true);
			P.bWalk = true;
			P.From = From;
			P.To = Act.Feet;
			P.WalkYaw = bWalks ? FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X)) - 90.0 : YawOf(From);
			P.WalkFor = bWalks ? static_cast<float>(Dist / FMath::Max(30.0f, Act.Speed)) : 0.0f;
			const double Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(P.WalkYaw, YawOf(Act.Feet)));
			P.TurnFor = FMath::Clamp(static_cast<float>(Turn / 180.0), 0.15f, 1.0f);
			continue;
		}
		if (Act.Kind == EAct::Still)
		{
			// Held at its last frame: the referee standing as a referee does.
			const double End = Act.Anim ? Act.Anim->GetPlayLength() : 0.0;
			PutBody(Who, RootAt(Act.Anim, End).Inverse() * Feet[Who]);
			PlayOn(Who, Act.Anim, false);
			if (USkeletalMeshComponent* Mesh = MatchBody(Who))
			{
				Mesh->SetPosition(static_cast<float>(End), false);
			}
			Play[Who].Age = Play[Who].Length;
			continue;
		}
		// A clip: her feet carry on from where the last one left them; or, the placed half of a
		// paired move, where the pack would stand her.
		FTransform Body = RootAt(Act.Anim, 0.0).Inverse() * Feet[Who];
		if (Current.Pairing >= 0 && Who < 2)
		{
			Body = Who == Current.Anchor ? AnchorBody : PairedBody(Current.Pairing, Who != Current.Att, AnchorBody);
		}
		PutBody(Who, Body);
		PlayOn(Who, Act.Anim, Act.Kind == EAct::Loop);
	}
}

void AWrestlingMat::TickWalk(int32 Who, float Dt)
{
	using namespace WrestlingMatchDetail;
	FPlaying& P = Play[Who];
	USkeletalMeshComponent* Body = MatchBody(Who);
	if (!Body || !P.bWalk)
	{
		return;
	}
	const double FromYaw = YawOf(P.From);
	FVector At;
	double Yaw;
	if (P.Age < P.WalkFor)
	{
		const double A = P.Age / P.WalkFor;
		At = FMath::Lerp(P.From.GetLocation(), P.To.GetLocation(), A);
		Yaw = FromYaw + FMath::FindDeltaAngleDegrees(FromYaw, P.WalkYaw) * FMath::Min(1.0, P.Age / 0.3);
	}
	else
	{
		UAnimSequence* Idle = Clip(TEXT("Wrestling_Idle"), TEXT("Wrestling_Idle01"));
		if (P.Anim != Idle && Idle)
		{
			Body->SetAnimation(Idle);
			Body->Play(true);
			P.Anim = Idle;
			P.Length = static_cast<float>(Idle->GetPlayLength());
		}
		const double A = FMath::Min(1.0, (P.Age - P.WalkFor) / FMath::Max(0.01f, P.TurnFor));
		At = P.To.GetLocation();
		Yaw = P.WalkYaw + FMath::FindDeltaAngleDegrees(P.WalkYaw, YawOf(P.To)) * A;
	}
	// Pin her feet to that: the pose she is about to be given is the clip a tick on.
	double Time = Body->GetPosition() + Dt;
	if (P.Length > 0.0f)
	{
		Time = FMath::Fmod(Time, static_cast<double>(P.Length));
	}
	PutBody(Who, RootAt(P.Anim, Time).Inverse() * Yawed(At, Yaw));
}

// The pack was made on the mannequin; on the agents' bodies the same pose can put a heel, a knee
// or a back a few centimetres into the mat (Walt: "their feet and backs sink into the mat"). Each
// tick her body is raised just enough that every part that touches the mat sits on it.
void AWrestlingMat::TickGround(int32 Who, float Dt)
{
	USkeletalMeshComponent* Body = MatchBody(Who);
	AActor* Actor = MatchActor(Who);
	if (!Body || !Actor)
	{
		return;
	}
	struct FContact { const TCHAR* Bone; float Clear; };   // joint to skin, cm
	static const FContact Contacts[] = {
		{ TEXT("ball_l"), 2.5f }, { TEXT("ball_r"), 2.5f }, { TEXT("foot_l"), 7.0f }, { TEXT("foot_r"), 7.0f },
		{ TEXT("calf_l"), 5.0f }, { TEXT("calf_r"), 5.0f }, { TEXT("pelvis"), 9.0f }, { TEXT("spine_03"), 9.0f },
		{ TEXT("spine_05"), 9.0f }, { TEXT("head"), 9.0f }, { TEXT("hand_l"), 3.0f }, { TEXT("hand_r"), 3.0f },
	};
	double Worst = -1.0e9;
	for (const FContact& C : Contacts)
	{
		if (Body->GetBoneIndex(C.Bone) != INDEX_NONE)
		{
			Worst = FMath::Max(Worst, C.Clear - (Body->GetBoneLocation(C.Bone).Z - MatZ));
		}
	}
	const float Want = static_cast<float>(FMath::Max(0.0, GroundLift[Who] + Worst));
	GroundLift[Who] = FMath::FInterpTo(GroundLift[Who], Want, Dt, 12.0f);
	const FVector At = Actor->GetActorLocation();
	Actor->SetActorLocation(FVector(At.X, At.Y, MatZ + GroundLift[Who]), false, nullptr, ETeleportType::TeleportPhysics);
}

int32 AWrestlingMat::AimedAt() const
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return -1;
	}
	const FVector Eye = Cam->GetCameraLocation();
	const FVector Fwd = Cam->GetCameraRotation().Vector();
	int32 Best = -1;
	double BestMiss = 70.0;   // cm off the line of sight, at most
	for (int32 Who = 0; Who < 3; ++Who)
	{
		const USkeletalMeshComponent* Body = MatchBody(Who);
		if (!Body)
		{
			continue;
		}
		const FVector At = Body->GetBoneLocation(TEXT("spine_03"));
		const double Along = FVector::DotProduct(At - Eye, Fwd);
		if (Along <= 0.0 || Along > 700.0)
		{
			continue;
		}
		const double Miss = FVector::Dist(At, Eye + Fwd * Along);
		if (Miss < BestMiss)
		{
			BestMiss = Miss;
			Best = Who;
		}
	}
	return Best;
}

// [E] on one of the three: the next agent who is not already on the mat takes her place, and the
// match starts again with the new cast - which then stays for the rematches.
void AWrestlingMat::SwapAgent(int32 Who)
{
	const int32 N = NumAgents();
	int32 Next = Lineup[Who];
	for (int32 K = 0; K < N; ++K)
	{
		Next = (Next + 1) % N;
		if (Next != Lineup[(Who + 1) % 3] && Next != Lineup[(Who + 2) % 3])
		{
			break;
		}
	}
	Lineup[Who] = Next;
	bChosen = true;
	ClearPair();
	Phase = EPhase::Idle;
	StartMatch();
}

bool AWrestlingMat::StepDone() const
{
	if (StepAge < Current.Hold)
	{
		return false;
	}
	for (int32 Who = 0; Who < 3; ++Who)
	{
		if (!MatchBody(Who))
		{
			continue;
		}
		const FPlaying& P = Play[Who];
		switch (Current.Act[Who].Kind)
		{
		case EAct::Once:
			if (P.Age < P.Length)
			{
				return false;
			}
			break;
		case EAct::Walk:
			if (P.bWalk && P.Age < P.WalkFor + P.TurnFor)
			{
				return false;
			}
			break;
		default:
			break;
		}
	}
	return true;
}

void AWrestlingMat::TickAttention()
{
	using namespace WrestlingMatchDetail;
	USkeletalMeshComponent* Body[3] = { MatchBody(0), MatchBody(1), MatchBody(2) };
	UWrestleAnimInstance* Player[3] = { WrestlePlayer(Body[0]), WrestlePlayer(Body[1]), WrestlePlayer(Body[2]) };
	const int32 Row = bStepLive ? Current.Pairing : INDEX_NONE;
	const bool bTos = Row == TosStart || Row == TosLoop || Row == TosEnd;
	const bool bFace = bStepLive && Current.bFaceView;
	const bool bLook = CVarLookAt.GetValueOnGameThread();
	auto Standing = [this, &Body](int32 Who)
	{
		return Body[Who] && Body[Who]->GetBoneLocation(TEXT("pelvis")).Z - MatZ >= DownHeight;
	};

	// WHO LOOKS AT WHOM. Standing between moves and in the test of strength, each wrestler watches
	// the other's face. In a hold or a throw the clip's own head is the move, so it is left alone.
	const bool bWatch = bLook && !bFace && (Row == INDEX_NONE || bTos) && Standing(0) && Standing(1);
	for (int32 Who = 0; Who < 2; ++Who)
	{
		const int32 Other = 1 - Who;
		if (Player[Who])
		{
			const FVector Face = Body[Other] ? Body[Other]->GetBoneLocation(TEXT("head")) : FVector::ZeroVector;
			Player[Who]->SetLook(Face, bWatch && Body[Other] ? 1.0f : 0.0f);
		}
	}
	// The referee watches the pair - and the shoulders she is counting - except when all face him.
	if (Player[2] && Body[0] && Body[1])
	{
		const FVector Pair = 0.5 * (Body[0]->GetBoneLocation(TEXT("spine_03")) + Body[1]->GetBoneLocation(TEXT("spine_03")));
		Player[2]->SetLook(Pair, bLook && !bFace ? 1.0f : 0.0f);
	}

	// HANDS THAT MEET. In the test of strength each wrestler's hand reaches to just short of the
	// other's, palm to palm. It is judged from where the clip itself puts the hands, so the reach
	// comes in as the clip's hands close and lets go when the clip lets go.
	for (int32 Who = 0; Who < 2; ++Who)
	{
		UWrestleAnimInstance* Mine = Player[Who];
		UWrestleAnimInstance* Theirs = Player[1 - Who];
		if (!Mine)
		{
			continue;
		}
		if (!bTos || !Theirs || !CVarGrip.GetValueOnGameThread() || !Mine->HasClipHands() || !Theirs->HasClipHands())
		{
			Mine->SetReach(0, FVector::ZeroVector, 0.0f);
			Mine->SetReach(1, FVector::ZeroVector, 0.0f);
			continue;
		}
		const FVector A[2] = { Mine->GetClipHand(0), Mine->GetClipHand(1) };
		const FVector B[2] = { Theirs->GetClipHand(0), Theirs->GetClipHand(1) };
		// Face to face a left hand meets a right one - unless the clip crosses them.
		const bool bSameSide = FVector::Dist(A[0], B[0]) + FVector::Dist(A[1], B[1])
			< FVector::Dist(A[0], B[1]) + FVector::Dist(A[1], B[0]);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FVector& Hand = A[Side];
			const FVector& Partner = B[bSameSide ? Side : 1 - Side];
			const double Weight = 1.0 - FMath::SmoothStep(GripNearCm, GripFarCm, FVector::Dist(Hand, Partner));
			const FVector Target = 0.5 * (Hand + Partner) + (Hand - Partner).GetSafeNormal() * (0.5 * GripGapCm);
			Mine->SetReach(Side, Target, static_cast<float>(Weight));
		}

		// The lab's proof (-WrestleLog): wrist to wrist as the clip has it, and as drawn.
		static double NextLog = 0.0;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (bLog && Who == 0 && Body[0] && Body[1] && (Now >= NextLog || Now < NextLog - 10.0))
		{
			NextLog = Now + 0.5;
			static const FName Hands[] = { TEXT("hand_l"), TEXT("hand_r") };
			FString Line;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 Other = bSameSide ? Side : 1 - Side;
				Line += FString::Printf(TEXT(" %s: clip %.0f cm, drawn %.0f cm;"), *Hands[Side].ToString(),
					FVector::Dist(A[Side], B[Other]),
					FVector::Dist(Body[0]->GetBoneLocation(Hands[Side]), Body[1]->GetBoneLocation(Hands[Other])));
			}
			UE_LOG(LogSibeliusGame, Display, TEXT("[Wrestle] grip%s"), *Line);
		}
	}
}

void AWrestlingMat::TickMatch(float Dt)
{
	PhaseAge += Dt;
	StepAge += Dt;
	for (int32 Who = 0; Who < 3; ++Who)
	{
		if (MatchBody(Who))
		{
			Play[Who].Age += Dt;
			TickWalk(Who, Dt);
			TickGround(Who, Dt);
			if (Pickers.IsValidIndex(Who) && Pickers[Who])
			{
				Pickers[Who]->SetWorldLocation(MatchBody(Who)->GetBoneLocation(TEXT("spine_03")));
			}
		}
	}
	TickAttention();
	if (bStepLive && Current.bCountOnTap && Sign)
	{
		// Either hand can slap. A planted supporting hand never makes the downward stroke.
		bool bTap = false;
		static const FName Hands[] = { TEXT("hand_l"), TEXT("hand_r") };
		for (int32 Hand = 0; RefBody && Hand < 2; ++Hand)
		{
			const float AboveMat = static_cast<float>(RefBody->GetBoneLocation(Hands[Hand]).Z - MatZ);
			PeakHandHeight[Hand] = FMath::Max(PeakHandHeight[Hand], AboveMat);
			bTap |= AboveMat < 12.0f && PeakHandHeight[Hand] - AboveMat > 18.0f;
		}
		// Keep the count legible if the referee is absent or a particular rig misses the threshold.
		const float Fallback = RefBody && Current.Act[2].Anim ? Play[2].Length * 0.85f : 0.45f;
		if (bTap || StepAge >= Fallback)
		{
			Sign->SetText(FText::FromString(Current.Say));
			Current.bCountOnTap = false;
		}
	}
	if (bStepLive && !StepDone())
	{
		return;
	}
	for (FPlaying& P : Play)
	{
		P.bWalk = false;   // a finished walk leaves her idling where it ended
	}
	if (bStepLive && Current.bNextMatch)
	{
		ClearPair();
		Phase = EPhase::Idle;
		StartMatch();
		return;
	}
	if (Steps.Num() == 0)
	{
		PlanExchange();
	}
	BeginStep();
}
