// FinaleAltar.cpp — the Ch7 Synthesis (FUN-6). See header.

#include "FinaleAltar.h"
#include "ProgressionSubsystem.h"
#include "SibeliusGameCharacter.h"
#include "SibeliusHUD.h"
#include "SibeliusGame.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AFinaleAltar::AFinaleAltar()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// See the header. Refactor is deliberately absent — its key is an Enhanced
	// Input binding, not a C++ BindKey, so guessing it here would be worse than
	// printing nothing.
	VerbKeyHints.Add(EPowerVerb::CodeVision, TEXT("V"));
	VerbKeyHints.Add(EPowerVerb::Compile,    TEXT("B"));
	VerbKeyHints.Add(EPowerVerb::TestDrive,  TEXT("6"));
	VerbKeyHints.Add(EPowerVerb::Deploy,     TEXT("0"));
	VerbKeyHints.Add(EPowerVerb::Generate,   TEXT("G"));
}

void AFinaleAltar::BeginPlay()
{
	Super::BeginPlay();

	// A finished Synthesis stays finished: drop the walls, skip the rite.
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		if (Progression->HasClaimedGrant(GrantKey))
		{
			CompleteSynthesis(/*bAlreadyClaimed=*/true);
			return;
		}
	}

	TryBindToPlayer();
}

void AFinaleAltar::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnPowerVerbUsed.Remove(PowerUsedHandle);
	}
	Super::EndPlay(Reason);
}

void AFinaleAltar::TryBindToPlayer()
{
	ASibeliusGameCharacter* Character =
		Cast<ASibeliusGameCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Character)
	{
		// The pawn may spawn after us — retry briefly (the carousel machine's
		// input-retry pattern). A non-character pawn level just never binds.
		if (++BindAttempts < 40)
		{
			GetWorldTimerManager().SetTimer(BindRetryHandle, this, &AFinaleAltar::TryBindToPlayer, 0.25f, false);
		}
		return;
	}
	BoundCharacter = Character;
	PowerUsedHandle = Character->OnPowerVerbUsed.AddUObject(this, &AFinaleAltar::HandlePowerUsed);
}

void AFinaleAltar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCompleted || !BoundCharacter.IsValid())
	{
		return;
	}

	// Walt's lesson (the fifth of the night): the rite's ask must stay ON SCREEN
	// the whole time the player stands in the circle — a six-second banner that
	// fades leaves them staring at a mute box. Refreshing every tick pins the
	// banner while inside; stepping out lets it fade naturally.
	const bool bInside =
		FVector::DistSquared(BoundCharacter->GetActorLocation(), GetActorLocation())
			<= FMath::Square(ActivationRadius);
	if (bInside)
	{
		Announce(StagePrompt(), 1.5f);
	}
	bPlayerWasInside = bInside;
}

FString AFinaleAltar::StagePrompt() const
{
	// If the rite is asking for a verb the player does NOT own, say so in the
	// prompt itself — the gated-input refusal banner gets stomped by this
	// prompt's every-tick refresh (Walt, stage 5, locked Deploy: "0 does
	// nothing"), so the guidance must live HERE.
	const EPowerVerb Verb = Sequence.CurrentVerb();

	// "  [0]" when we know the key, nothing when we don't. Naming a verb without
	// its key is what sent Walt down the number row into two nested branches.
	const FString* Hint = VerbKeyHints.Find(Verb);
	const FString KeyHint = (Hint && !Hint->IsEmpty())
		? FString::Printf(TEXT("  [%s]"), **Hint)
		: FString();

	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		if (!Progression->IsUnlocked(Verb))
		{
			return FString::Printf(TEXT("THE SYNTHESIS asks for %s%s — a power you do not yet possess. Blend it at a cauldron, or find its shrine."),
				*PowerVerbDisplayName(Verb), *KeyHint);
		}
	}
	return FString::Printf(TEXT("THE SYNTHESIS  —  show me %s%s  (%d/%d)"),
		*PowerVerbDisplayName(Verb), *KeyHint, Sequence.StageIndex + 1, FFinaleSequence::Num());
}

void AFinaleAltar::HandlePowerUsed(EPowerVerb Verb)
{
	if (bCompleted || !bPlayerWasInside)
	{
		return; // the rite only listens at the altar
	}

	if (!Sequence.Submit(Verb))
	{
		// Wrong verb inside the circle: restate what the rite wants. (Re-using an
		// already-shown verb is common while maneuvering — keep the tone gentle.)
		Announce(StagePrompt(), 3.0f);
		return;
	}

	if (Sequence.IsComplete())
	{
		CompleteSynthesis(/*bAlreadyClaimed=*/false);
		return;
	}

	if (StageSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, StageSound, GetActorLocation());
	}
	Announce(StagePrompt());
}

void AFinaleAltar::CompleteSynthesis(bool bAlreadyClaimed)
{
	bCompleted = true;
	DropWalls();
	SummonDancer();   // before the revisit early-out: the apse should look the
	                  // same when you come back, just without the fanfare

	if (bAlreadyClaimed)
	{
		return; // silent on revisit — the open apse says it all
	}

	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		if (Progression->ClaimOneTimeGrant(GrantKey))
		{
			Progression->GrantSauce(SauceReward);
		}
	}
	if (CompleteSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CompleteSound, GetActorLocation());
	}
	Announce(TEXT("THE THREE-PART SYNTHESIS IS COMPLETE"), 10.0f);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Finale] Synthesis complete — walls down, +%d sauce"), SauceReward);
}

void AFinaleAltar::SummonDancer()
{
	// Nothing assigned is a valid setup (a level without a dancer just skips it),
	// and a revisit must not stack a second one on top of the first.
	if (!DancerClass || SpawnedDancer.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A placed marker beats coordinates every time — if one is set, it wins.
	// Otherwise the offset is expressed in the altar's own space, so rotating the
	// altar carries her round with it rather than leaving her behind a pillar.
	const FRotator AltarRotation = GetActorRotation();
	FVector SpawnLocation;
	FRotator SpawnRotation;
	if (DancerSpawnPoint)
	{
		SpawnLocation = DancerSpawnPoint->GetActorLocation();
		SpawnRotation = FRotator(0.0f, DancerSpawnPoint->GetActorRotation().Yaw, 0.0f);
	}
	else
	{
		SpawnLocation = GetActorLocation() + AltarRotation.RotateVector(DancerSpawnOffset);
		SpawnRotation = FRotator(0.0f, AltarRotation.Yaw + DancerSpawnYaw, 0.0f);
	}

	// Put her feet on the floor. See the header for why SpawnCollisionHandling
	// cannot do this for a MetaHuman.
	if (bSnapDancerToFloor)
	{
		const FVector TraceStart = SpawnLocation + FVector(0.0f, 0.0f, FloorTraceHeight);
		const FVector TraceEnd = SpawnLocation - FVector(0.0f, 0.0f, FloorTraceDepth);

		// bTraceComplex MUST be true. Architectural meshes (the altar steps here)
		// routinely ship with no simple collision hull, only per-triangle. A simple
		// trace passes straight through them and finds the ground plane far below —
		// which is how she first landed at Z=-0.5, buried to the neck on steps that
		// are visibly 150 cm up.
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(FinaleDancerFloor), /*bTraceComplex=*/true, this);
		TraceParams.AddIgnoredActor(this);
		if (DancerSpawnPoint)
		{
			TraceParams.AddIgnoredActor(DancerSpawnPoint);
		}

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			SpawnLocation.Z = Hit.ImpactPoint.Z;
			// Name the surface she landed on: if she is ever sunk or floating again,
			// this line says immediately whether the trace found the right thing.
			UE_LOG(LogSibeliusGame, Display, TEXT("[Finale] dancer floor = '%s' at Z=%.1f"),
				*GetNameSafe(Hit.GetActor()), Hit.ImpactPoint.Z);
		}
		else
		{
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Finale] no floor found under the dancer point %s — spawning at the given Z"),
				*SpawnLocation.ToCompactString());
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Dancer = World->SpawnActor<AActor>(DancerClass, SpawnLocation, SpawnRotation, SpawnParams);
	SpawnedDancer = Dancer;

	if (Dancer)
	{
		// She needs no start call: the dancer Blueprints drive their Body component
		// from Anim to Play with Looping set, so she is dancing the moment she exists.
		UE_LOG(LogSibeliusGame, Display, TEXT("[Finale] summoned dancer '%s' at %s"),
			*Dancer->GetName(), *SpawnLocation.ToCompactString());
	}
	else
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Finale] DancerClass set but the spawn failed at %s"),
			*SpawnLocation.ToCompactString());
	}
}

void AFinaleAltar::DropWalls()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	int32 Dropped = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (*It != this && It->ActorHasTag(WallTag))
		{
			It->Destroy();
			++Dropped;
		}
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Finale] dropped %d '%s' wall actor(s)"), Dropped, *WallTag.ToString());
}

void AFinaleAltar::Announce(const FString& Text, float Seconds) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowBanner(Text, Seconds);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0xF17A1E, Seconds, FColor::Cyan, Text);
	}
}
