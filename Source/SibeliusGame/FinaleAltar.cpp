// FinaleAltar.cpp — the Ch7 Synthesis (FUN-6). See header.

#include "FinaleAltar.h"
#include "ProgressionSubsystem.h"
#include "SibeliusGameCharacter.h"
#include "SibeliusHUD.h"
#include "SibeliusGame.h"
#include "Components/StaticMeshComponent.h"
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

	// Entry prompt, once per approach: tell the player what the rite wants.
	const bool bInside =
		FVector::DistSquared(BoundCharacter->GetActorLocation(), GetActorLocation())
			<= FMath::Square(ActivationRadius);
	if (bInside && !bPlayerWasInside)
	{
		Announce(StagePrompt());
	}
	bPlayerWasInside = bInside;
}

FString AFinaleAltar::StagePrompt() const
{
	return FString::Printf(TEXT("THE SYNTHESIS  —  show me %s  (%d/%d)"),
		*PowerVerbDisplayName(Sequence.CurrentVerb()), Sequence.StageIndex + 1, FFinaleSequence::Num());
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
