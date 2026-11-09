// FilmDirectorSubsystem.cpp — the director. See the header.

#include "FilmDirectorSubsystem.h"

#include "DancerAgentComponent.h"
#include "DancerAgentSubsystem.h"
#include "FilmGenerator.h"
#include "FilmScreenWidget.h"
#include "SibeliusGame.h"                  // LogSibeliusGame
#include "SibeliusHUD.h"

#include "Animation/AnimSequence.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace FilmDirectorNS
{
	/** Seconds of fade either side of a title card, and at the two ends of the film. */
	const float FadeSeconds = 0.45f;

	/** Esc is deaf for this long after the film starts. */
	const float SkipGraceSeconds = 0.5f;

	/** The camera never comes closer to her than this, whatever is in the way. */
	const float MinCameraDistance = 70.0f;

	struct FLens
	{
		float Distance;   // cm from the look-at point
		float Fov;
		float Lift;       // cm the camera sits above the look-at point
		float HeadBias;   // 0 looks at the middle of her, 1 at her head
		float Follow;     // how hard the look-at point chases her; higher is tighter
	};

	FLens LensFor(EFilmFraming Framing)
	{
		switch (Framing)
		{
		case EFilmFraming::Full:    return { 430.0f, 40.0f,  10.0f, 0.15f, 3.0f };
		case EFilmFraming::Medium:  return { 215.0f, 32.0f,   5.0f, 0.70f, 5.0f };
		case EFilmFraming::CloseUp: return {  95.0f, 26.0f,   2.0f, 1.00f, 7.0f };
		case EFilmFraming::Low:     return { 330.0f, 52.0f, -75.0f, 0.25f, 3.0f };
		case EFilmFraming::High:    return { 520.0f, 42.0f, 330.0f, 0.00f, 3.0f };
		case EFilmFraming::Wide:
		default:                    return { 700.0f, 50.0f,  30.0f, 0.00f, 2.5f };
		}
	}

	/* THE FILM ON THE SCREEN, held here and not on the subsystem. A generated film has
	   to live somewhere while it plays, and a new member would change the layout of a
	   UObject that Live Coding patches in place. One film plays at a time, so one is enough. */
	FFilm NowShowing;

	UAnimSequence* FindDance(const UDancerAgentComponent& Agent, const FString& NamePart)
	{
		for (const TObjectPtr<UAnimSequence>& Dance : Agent.Dances)
		{
			if (Dance && Dance->GetName().Contains(NamePart))
			{
				return Dance.Get();
			}
		}
		return nullptr;
	}
}

TStatId UFilmDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFilmDirectorSubsystem, STATGROUP_Tickables);
}

void UFilmDirectorSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

bool UFilmDirectorSubsystem::Play(FName FilmId, UDancerAgentComponent* Star)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const FFilm* Wanted = FilmCatalog::Find(FilmId);
	if (!PC || !Wanted)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Film] cannot play '%s': %s"), *FilmId.ToString(),
			!PC ? TEXT("no player") : TEXT("no such film"));
		return false;
	}
	return PlayFilm(*Wanted, Star);
}

bool UFilmDirectorSubsystem::PlayGenerated(const FString& Wish, UDancerAgentComponent* Star)
{
	FFilm Made;
	const uint32 Take = static_cast<uint32>(FMath::Rand());
	if (!FilmGenerator::Make(Wish, Take, TArray<FString>(), Made))
	{
		return false;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Film] generated \"%s\" from \"%s\" take %u"), *Made.Title, *Wish, Take);
	return PlayFilm(Made, Star);
}

bool UFilmDirectorSubsystem::PlayFilm(const FFilm& InFilm, UDancerAgentComponent* Star)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const FName FilmId = InFilm.Id;
	if (!PC || InFilm.Shots.Num() == 0)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Film] cannot play '%s': %s"), *InFilm.Title,
			!PC ? TEXT("no player") : TEXT("it has no shots"));
		return false;
	}

	Stop();

	// ---- casting: everyone in the level who can dance, the star first, then nearest ----
	const UDancerAgentSubsystem* Dancers = World->GetSubsystem<UDancerAgentSubsystem>();
	TArray<UDancerAgentComponent*> Found;
	if (Dancers)
	{
		for (const TWeakObjectPtr<UDancerAgentComponent>& Weak : Dancers->GetDancers())
		{
			UDancerAgentComponent* Agent = Weak.Get();
			const AActor* Owner = Agent ? Agent->GetOwner() : nullptr;
			// A hidden guide (SetGuidePresent) is not in the level as far as a camera cares.
			if (Owner && !Owner->IsHidden() && Agent->FindDanceMesh())
			{
				Found.Add(Agent);
			}
		}
	}
	if (Found.Num() == 0)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Film] cannot play '%s': no agent in this level to be in it"),
			*FilmId.ToString());
		return false;
	}

	const APawn* Pawn = PC->GetPawn();
	const FVector Near = Star && Star->GetOwner() ? Star->GetOwner()->GetActorLocation()
		: Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	Found.Sort([Star, Near](const UDancerAgentComponent& A, const UDancerAgentComponent& B)
	{
		if ((&A == Star) != (&B == Star))
		{
			return &A == Star;
		}
		return FVector::DistSquared(A.GetOwner()->GetActorLocation(), Near)
			< FVector::DistSquared(B.GetOwner()->GetActorLocation(), Near);
	});

	Troupe.Reset();
	for (UDancerAgentComponent* Agent : Found)
	{
		USkeletalMeshComponent* Mesh = Agent->FindDanceMesh();
		FCastMember Member;
		Member.Agent = Agent;
		Member.Mesh = Mesh;
		Member.SavedAnim = Mesh->AnimationData.AnimToPlay;
		Member.SavedPosition = Mesh->GetPosition();
		Member.SavedRate = Mesh->GetPlayRate();
		Troupe.Add(Member);
	}
	StarName = Troupe[0].Agent->AgentName;

	// ---- the camera ----
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Near,
		FRotator::ZeroRotator, Params);
	if (!Camera)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Film] camera failed to spawn"));
		Troupe.Reset();
		return false;
	}
	if (UCameraComponent* Lens = Camera->GetCameraComponent())
	{
		// The letterbox is drawn by the screen widget; the camera must not add its own bars.
		Lens->bConstrainAspectRatio = false;
	}

	SavedViewTarget = PC->GetViewTarget();
	if (APawn* Mine = PC->GetPawn())
	{
		if (!Mine->IsHidden())
		{
			Mine->SetActorHiddenInGame(true);
			HiddenPawn = Mine;
		}
	}

	FilmDirectorNS::NowShowing = InFilm;
	Film = &FilmDirectorNS::NowShowing;
	FilmTime = 0.0f;
	StartShot(0);
	UpdateCamera(0.0f);
	PC->SetViewTarget(Camera);

	if (!Screen)
	{
		Screen = CreateWidget<UFilmScreenWidget>(PC, UFilmScreenWidget::StaticClass());
	}
	if (Screen)
	{
		Screen->Open(this);
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[Film] '%s' starring %s, cast of %d, %.0f s"),
		*Film->Title, *StarName, Troupe.Num(), Film->Length());
	return true;
}

void UFilmDirectorSubsystem::Skip()
{
	if (Film && FilmTime >= FilmDirectorNS::SkipGraceSeconds)
	{
		Stop();
	}
}

void UFilmDirectorSubsystem::Stop()
{
	if (!Film)
	{
		return;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Film] '%s' ended at %.1f s"), *Film->Title, FilmTime);
	Film = nullptr;
	Caption.Reset();

	// Everyone back to the dance she was doing, where she was in it.
	for (const FCastMember& Member : Troupe)
	{
		USkeletalMeshComponent* Mesh = Member.Mesh.Get();
		UAnimationAsset* Anim = Member.SavedAnim.Get();
		if (Mesh)
		{
			if (Anim)
			{
				Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				Mesh->PlayAnimation(Anim, /*bLooping=*/true);
				Mesh->SetPosition(Member.SavedPosition);
			}
			Mesh->SetPlayRate(Member.SavedRate);
		}
	}
	Troupe.Reset();

	if (APawn* Mine = HiddenPawn.Get())
	{
		Mine->SetActorHiddenInGame(false);
	}
	HiddenPawn = nullptr;

	if (Screen)
	{
		Screen->Close();
	}

	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		AActor* Restore = SavedViewTarget.Get();
		if (!Restore)
		{
			Restore = PC->GetPawn();
		}
		if (Restore)
		{
			PC->SetViewTargetWithBlend(Restore, 0.5f, VTBlend_Cubic);
		}
		// Keep the HUD away until the camera is home, then the lease lapses by itself.
		if (ASibeliusHUD* Hud = PC->GetHUD<ASibeliusHUD>())
		{
			Hud->HoldCinematic(0.5f);
		}
	}
	SavedViewTarget = nullptr;

	if (Camera)
	{
		// Not destroyed this instant: the blend home is still reading it. It has no mesh
		// and nothing else refers to it, so half a second of lifetime is the whole cost.
		Camera->SetLifeSpan(1.0f);
		Camera = nullptr;
	}
}

const FFilmShot* UFilmDirectorSubsystem::CurrentShot() const
{
	return Film && Film->Shots.IsValidIndex(ShotIndex) ? &Film->Shots[ShotIndex] : nullptr;
}

UFilmDirectorSubsystem::FCastMember* UFilmDirectorSubsystem::LeadOf(const FFilmShot& Shot)
{
	if (Troupe.Num() == 0)
	{
		return nullptr;
	}
	FCastMember& Member = Troupe[FMath::Abs(Shot.Lead) % Troupe.Num()];
	return Member.Mesh.IsValid() ? &Member : nullptr;
}

bool UFilmDirectorSubsystem::IsCard() const
{
	const FFilmShot* Shot = CurrentShot();
	return Shot && Shot->Framing == EFilmFraming::Card;
}

float UFilmDirectorSubsystem::GetBlackAlpha() const
{
	using namespace FilmDirectorNS;

	const FFilmShot* Shot = CurrentShot();
	if (!Shot)
	{
		return 0.0f;
	}
	if (Shot->Framing == EFilmFraming::Card)
	{
		return 1.0f;
	}

	// A shot fades only where it meets black: the ends of the film, and a card. Between
	// two shots it is a straight cut.
	const auto IsBlackAt = [this](int32 Index)
	{
		return !Film->Shots.IsValidIndex(Index) || Film->Shots[Index].Framing == EFilmFraming::Card;
	};
	float Alpha = 0.0f;
	if (IsBlackAt(ShotIndex - 1))
	{
		Alpha = FMath::Max(Alpha, 1.0f - ShotTime / FadeSeconds);
	}
	if (IsBlackAt(ShotIndex + 1))
	{
		Alpha = FMath::Max(Alpha, 1.0f - (Shot->Seconds - ShotTime) / FadeSeconds);
	}
	return FMath::Clamp(Alpha, 0.0f, 1.0f);
}

void UFilmDirectorSubsystem::StartShot(int32 Index)
{
	ShotIndex = Index;
	ShotTime = 0.0f;

	const FFilmShot* Shot = CurrentShot();
	if (!Shot)
	{
		return;
	}
	Caption = Shot->Caption.Replace(TEXT("{0}"), *StarName);

	if (Shot->Framing == EFilmFraming::Card)
	{
		return;
	}
	FCastMember* Lead = LeadOf(*Shot);
	if (!Lead)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Lead->Mesh.Get();

	if (!Shot->Dance.IsEmpty())
	{
		// Straight onto the mesh. UDancerAgentComponent::PlayDance would SAVE this as the
		// dance he chose for her, and a film must not rewrite his choices.
		if (UAnimSequence* Dance = FilmDirectorNS::FindDance(*Lead->Agent, Shot->Dance))
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Mesh->PlayAnimation(Dance, /*bLooping=*/true);
		}
		else
		{
			UE_LOG(LogSibeliusGame, Warning, TEXT("[Film] no dance matching '%s'; she keeps the one she is on"),
				*Shot->Dance);
		}
	}
	Mesh->SetPlayRate(Shot->Rate);

	// A skeletal mesh faces +Y in its own space, whatever its actor is doing.
	ShotFacing = Mesh->GetComponentTransform().GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	if (ShotFacing.IsNearlyZero())
	{
		ShotFacing = FVector::ForwardVector;
	}
	SmoothedTarget = TargetPoint(*Mesh, FilmDirectorNS::LensFor(Shot->Framing).HeadBias);
}

FVector UFilmDirectorSubsystem::TargetPoint(const USkeletalMeshComponent& Mesh, float HeadBias)
{
	// Bounds follow the pose; the actor origin and the capsule do not (GetAimPoint's lesson).
	const FVector Middle = Mesh.Bounds.Origin;
	static const FName HeadBone(TEXT("head"));
	const FVector Head = Mesh.GetBoneIndex(HeadBone) != INDEX_NONE
		? Mesh.GetSocketLocation(HeadBone)
		: Middle + FVector(0.0f, 0.0f, Mesh.Bounds.BoxExtent.Z * 0.8f);
	return FMath::Lerp(Middle, Head, HeadBias);
}

void UFilmDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const FFilmShot* Shot = CurrentShot();
	if (!PC || !Shot || !Camera)
	{
		Stop();
		return;
	}

	// The lease, renewed: if this subsystem dies mid-film the HUD comes back unaided.
	if (ASibeliusHUD* Hud = PC->GetHUD<ASibeliusHUD>())
	{
		Hud->HoldCinematic(0.25f);
	}

	ShotTime += DeltaTime;
	FilmTime += DeltaTime;
	if (ShotTime >= Shot->Seconds)
	{
		if (!Film->Shots.IsValidIndex(ShotIndex + 1))
		{
			Stop();
			return;
		}
		StartShot(ShotIndex + 1);
	}
	UpdateCamera(DeltaTime);
}

void UFilmDirectorSubsystem::UpdateCamera(float DeltaTime)
{
	using namespace FilmDirectorNS;

	const FFilmShot* Shot = CurrentShot();
	if (!Shot || !Camera || Shot->Framing == EFilmFraming::Card)
	{
		return;   // a card is black; the camera stays wherever the last shot left it
	}
	FCastMember* Lead = LeadOf(*Shot);
	if (!Lead)
	{
		return;
	}
	const USkeletalMeshComponent& Mesh = *Lead->Mesh;
	const FLens Lens = LensFor(Shot->Framing);

	// 0..1 across the shot, eased at both ends so no move starts or stops with a jolt.
	const float Ease = FMath::SmoothStep(0.0f, 1.0f, Shot->Seconds > 0.0f ? ShotTime / Shot->Seconds : 1.0f);

	float Azimuth = Shot->Azimuth;
	float Distance = Lens.Distance;
	float Lift = Lens.Lift;
	switch (Shot->Move)
	{
	case EFilmMove::Orbit:     Azimuth += Shot->Sweep * Ease; break;
	case EFilmMove::PushIn:    Distance *= FMath::Lerp(1.0f, 0.6f, Ease); break;
	case EFilmMove::PullOut:   Distance *= FMath::Lerp(0.6f, 1.0f, Ease); break;
	case EFilmMove::CraneUp:   Lift += FMath::Lerp(0.0f, 260.0f, Ease); break;
	case EFilmMove::CraneDown: Lift += FMath::Lerp(0.0f, -260.0f, Ease); break;
	case EFilmMove::Hold:
	default:                   Azimuth += 4.0f * Ease; break;
	}

	const FVector Target = TargetPoint(Mesh, Lens.HeadBias);
	SmoothedTarget = DeltaTime > 0.0f
		? FMath::VInterpTo(SmoothedTarget, Target, DeltaTime, Lens.Follow)
		: Target;

	const FVector Direction = ShotFacing.RotateAngleAxis(Azimuth, FVector::UpVector);
	FVector CameraAt = SmoothedTarget + Direction * Distance + FVector(0.0f, 0.0f, Lift);

	// Never under the floor she is standing on, however low the shot asks to go.
	const float FloorZ = Mesh.Bounds.Origin.Z - Mesh.Bounds.BoxExtent.Z;
	CameraAt.Z = FMath::Max(CameraAt.Z, FloorZ + 20.0f);

	/* THE ROOM IS NOT KNOWN. A film plays in any level, so the place the shot wants the
	   camera may be inside a wall. Trace from her to it and stop short of whatever is in
	   the way: the shot gets tighter, which is what an operator backed against a wall
	   does too. */
	FCollisionQueryParams Query(SCENE_QUERY_STAT(FilmCamera), /*bTraceComplex=*/false);
	for (const FCastMember& Member : Troupe)
	{
		if (const UDancerAgentComponent* Agent = Member.Agent.Get())
		{
			Query.AddIgnoredActor(Agent->GetOwner());
		}
	}
	if (const APawn* Mine = HiddenPawn.Get())
	{
		Query.AddIgnoredActor(Mine);
	}
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, SmoothedTarget, CameraAt, ECC_Visibility, Query))
	{
		const FVector Back = (SmoothedTarget - CameraAt).GetSafeNormal();
		const float Reach = FMath::Max((Hit.Location - SmoothedTarget).Size() - 25.0f, MinCameraDistance);
		CameraAt = SmoothedTarget - Back * Reach;
	}

	Camera->SetActorLocationAndRotation(CameraAt, (SmoothedTarget - CameraAt).Rotation());
	if (UCameraComponent* Glass = Camera->GetCameraComponent())
	{
		Glass->SetFieldOfView(Lens.Fov);
	}
}

// ---------------------------------------------------------------------------------------
// Console, so a film can be tried without walking up to anyone.

static FAutoConsoleCommandWithWorldAndArgs GFilmPlay(
	TEXT("sib.Film.Play"),
	TEXT("Play a film in this level: sib.Film.Play [id]. No id plays the first."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UFilmDirectorSubsystem* Director = World ? World->GetSubsystem<UFilmDirectorSubsystem>() : nullptr;
		const TArray<FFilm>& Films = FilmCatalog::All();
		if (Director && Films.Num() > 0)
		{
			Director->Play(Args.Num() > 0 ? FName(*Args[0]) : Films[0].Id);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFilmGenerate(
	TEXT("sib.Film.Generate"),
	TEXT("Write a new film and play it: sib.Film.Generate [what it is about]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UFilmDirectorSubsystem* Director = World ? World->GetSubsystem<UFilmDirectorSubsystem>() : nullptr)
		{
			Director->PlayGenerated(FString::Join(Args, TEXT(" ")));
		}
	}));

static FAutoConsoleCommandWithWorld GFilmStop(
	TEXT("sib.Film.Stop"),
	TEXT("Stop the film that is playing."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UFilmDirectorSubsystem* Director = World ? World->GetSubsystem<UFilmDirectorSubsystem>() : nullptr)
		{
			Director->Stop();
		}
	}));

static FAutoConsoleCommand GFilmList(
	TEXT("sib.Film.List"),
	TEXT("List the films."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (const FFilm& Film : FilmCatalog::All())
		{
			UE_LOG(LogSibeliusGame, Display, TEXT("[Film] %s  \"%s\"  %d shots, %.0f s"),
				*Film.Id.ToString(), *Film.Title, Film.Shots.Num(), Film.Length());
		}
	}));
