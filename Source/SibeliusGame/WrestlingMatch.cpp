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

#include "Animation/AnimSequence.h"
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
		{ TEXT("BostonCrab_Start"),     133.6, -90.0 },
		{ TEXT("BostonCrab_Loop"),      132.6, -90.0 },
		{ TEXT("BostonCrab_End"),       133.6, -90.0 },
		{ TEXT("Pin"),                  104.1, 180.0 },   // every pin clip: she lies across his line
	};
	enum : int32 { TosStart, TosLoop, TosEnd, HlStart, HlLoop, HlEnd, HlFlip, Hip, Scoop, Toss, CrabStart, CrabLoop, CrabEnd, PinRow };

	constexpr float GapCm = 12.0f;     // further than this from where the move wants her: she walks
	constexpr float GapDeg = 12.0f;
	constexpr float DownHeight = 45.0f;   // hips lower than this above the mat: she is on the mat
	// Walk to the next move's spot; off, they appear there (BeginStep). A console variable, not a
	// constant, so the walk code below it is not "unreachable" to the compiler.
	TAutoConsoleVariable<bool> CVarWalkTo(TEXT("sib.Wrestle.Walk"), false, TEXT("Wrestlers walk to the next move instead of appearing there."));

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
	if (Body->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	Body->PlayAnimation(Anim, bLoop);
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
	Pins = 0;
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
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
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
	++Exchange;
	const int32 A = Rng.RandRange(0, 1);
	const int32 D = 1 - A;
	const FString Names = FString::Printf(TEXT("%s  vs  %s"), *W[0].Name.ToUpper(), *W[1].Name.ToUpper());

	auto Paired = [this, A](int32 Row, const FString& Clips, int32 Anchor, const FString& Say)
	{
		FStep S;
		S.Pairing = Row;
		S.Att = A;
		S.Anchor = Anchor;
		S.Act[A] = { EAct::Once, Clip(TEXT("Wrestling_Paired"), TEXT("Wrestling_Paired_") + Clips + TEXT("_Att")) };
		S.Act[1 - A] = { EAct::Once, Clip(TEXT("Wrestling_Paired"), TEXT("Wrestling_Paired_") + Clips + TEXT("_Vic")) };
		S.Say = Say;
		Steps.Add(S);
	};
	auto Solo = [this](UAnimSequence* Zero, UAnimSequence* One)
	{
		FStep S;
		S.Act[0] = { Zero ? EAct::Once : EAct::None, Zero };
		S.Act[1] = { One ? EAct::Once : EAct::None, One };
		Steps.Add(S);
	};
	auto Taunt = [this]() { return Clip(TEXT("Wrestling_Taunt"), FString::Printf(TEXT("Wrestling_Taunt_%02d"), Rng.RandRange(1, 5))); };

	// THE MATCH IS ON THE MAT (Walt, 2026-11-15: "more grappling on the ground and less throwing").
	// A short tie-up, one takedown to get there, then a long ground game: a pin, a struggle and a
	// kick-out, a Boston crab before she can rise, and only then up again.

	// 1. The tie-up: a test of strength, or a headlock.
	if (Rng.FRand() < 0.5f)
	{
		Paired(TosStart, TEXT("TestOfStrength_Start"), A, Names);
		Paired(TosLoop, TEXT("TestOfStrength_Loop"), A, FString());
		Paired(TosEnd, TEXT("TestOfStrength_End"), A, FString());
	}
	else
	{
		Paired(HlStart, TEXT("HeadLock_Start"), A, Names);
		Paired(HlLoop, TEXT("HeadLock_Loop"), A, FString());
		Paired(HlEnd, TEXT("HeadLock_End"), A, FString());
	}

	// 2. One takedown, landing her on her back. (Not the flip-over: it lands her face down, and
	// that is the end of the ground game.)
	static const TCHAR* const Throws[] = { TEXT("HipThrow"), TEXT("LowScoopSlam"), TEXT("Underarm_FlipToss") };
	const int32 Which = Rng.RandRange(0, 2);
	Paired(Hip + Which, Throws[Which], A, FString());

	// 3. Two pins, back to back, while she is down (Walt, 2026-11-15: "still too much throwing" -
	// so each throw buys more time on the mat). The first is always kicked out; the second wins
	// from the second exchange on, sometimes, and from the third always.
	++Pins;
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
	PinStep(TEXT("Loop01"), TEXT("Tap01"), Names + TEXT("\n1"));
	PinStep(TEXT("Resist"), TEXT("Tap01"), Names + TEXT("\n2"));
	PinStep(TEXT("Failure"), TEXT("End"), TEXT("KICK OUT!"));
	// She is still on her back: covered again at once.
	Steps.Add(Setup);
	PinStep(TEXT("Start"), TEXT("Start"), Names);
	PinStep(TEXT("Loop02"), TEXT("Tap01"), Names + TEXT("\n1"));
	if (bWin)
	{
		PinStep(TEXT("Loop02"), TEXT("Tap01"), Names + TEXT("\n2"));
		PinStep(TEXT("Loop01"), TEXT("Tap02"), Names + TEXT("\n3"));
		PinStep(TEXT("End"), TEXT("End"), FString::Printf(TEXT("%s WINS"), *W[A].Name.ToUpper()));
		PlanFinish(A);
		return;
	}
	PinStep(TEXT("Resist"), TEXT("Tap01"), Names + TEXT("\n2"));
	PinStep(TEXT("Failure"), TEXT("End"), TEXT("KICK OUT!"));

	// 4. Up she gets, while the other plays to the crowd; the referee back to her post. (A Boston
	// crab stood here until Walt saw it: "this move is too violent i don't like it", 2026-11-15.)
	Solo(A == 0 ? Taunt() : Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp02")),
		 A == 1 ? Taunt() : Clip(TEXT("Wrestling_LayOnBack"), TEXT("Wrestling_LayOnBack_GetUp02")));
	Steps.Last().Act[2] = { EAct::Walk, nullptr, RefPost, 140.0f };
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

		// Before a new move on their feet (never in the middle of a hold), the pair is brought back to
		// the middle of the mat and turned side-on to him, so he sees both of them and the grips.
		const int32 Row = Current.Pairing;
		const bool bFresh = Row == TosStart || Row == HlStart || Row == HlFlip || Row == Hip || Row == Scoop || Row == Toss;
		bool bMoveAnchor = false;
		// The middle of the move: both starts and both ends - a throw carries her a metre or two,
		// and the pin happens where she lands.
		UAnimSequence* OtherAnim = Current.Act[Other].Anim;
		auto MoveMiddle = [&](const FTransform& ABody, const FTransform& OBody)
		{
			const FVector Sum = ABody.GetLocation() + OBody.GetLocation()
				+ (RootAt(AnchorAnim, AnchorAnim ? AnchorAnim->GetPlayLength() : 0.0) * ABody).GetLocation()
				+ (RootAt(OtherAnim, OtherAnim ? OtherAnim->GetPlayLength() : 0.0) * OBody).GetLocation();
			return Sum * 0.25;
		};
		if (bFresh && !bDown)
		{
			const FVector Side = FVector::CrossProduct(FVector::UpVector, ViewDir()).GetSafeNormal();
			const FVector Fwd = AnchorFeet.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal2D();
			const FVector WantFwd = FVector::DotProduct(Fwd, Side) >= 0.0 ? Side : -Side;
			const double TurnBy = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Fwd, WantFwd), -1.0, 1.0)));
			const FTransform OtherNow = PairedBody(Row, Other != Current.Att, AnchorBody);
			const double OffCentre = FlatDist(MoveMiddle(AnchorBody, OtherNow), MatTop());
			if (TurnBy > 35.0 || OffCentre > 60.0)
			{
				FTransform Turned = FeetFacing(AnchorFeet.GetLocation(), WantFwd);
				const FTransform TurnedBody = RootAt(AnchorAnim, 0.0).Inverse() * Turned;
				const FTransform OtherTurned = PairedBody(Row, Other != Current.Att, TurnedBody);
				FVector Shift = MatTop() - MoveMiddle(TurnedBody, OtherTurned);
				Shift.Z = 0.0;
				Turned.AddToTranslation(Shift);
				AnchorFeet = Turned;
				AnchorBody = RootAt(AnchorAnim, 0.0).Inverse() * AnchorFeet;
				bMoveAnchor = true;
			}
		}

		const FTransform OtherBody = PairedBody(Row, Other != Current.Att, AnchorBody);
		const FTransform Want = RootAt(Current.Act[Other].Anim, 0.0) * OtherBody;
		const double Gap = FlatDist(Want.GetLocation(), Feet[Other].GetLocation());
		const double Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(YawOf(Feet[Other]), YawOf(Want)));
		if (bMoveAnchor || Gap > GapCm || Turn > GapDeg)
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

	if (!Current.Say.IsEmpty() && Sign)
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
			Body->PlayAnimation(Idle, true);
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
