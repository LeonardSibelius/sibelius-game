// PowerGrant.cpp — placeable power/sauce grant shrine (FUN-1). See header.

#include "PowerGrant.h"
#include "ProgressionSubsystem.h"
#include "SlotScreenWidget.h"
#include "HallAlarmSubsystem.h"   // the trial rings the alarm (temple-pour pattern)
#include "DancerAgentComponent.h" // SPINE: an AI agent can be the way in
#include "InventoryComponent.h"   // attic-key shrine spends books / mints a Key
#include "CompileTypes.h"         // EResourceType::Book / Key
#include "SibeliusHUD.h"          // "need N books" when the alcove is not ready
#include "SibeliusGame.h"         // LogSibeliusGame
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APowerGrant::APowerGrant()
{
	PrimaryActorTick.bCanEverTick = true;

	// CP3 lesson: every placeable actor gets an explicit root. The trigger is the
	// root so the overlap volume sits exactly where Walt places the actor.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->InitSphereRadius(110.0f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // walk-through; the sphere does the work
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	// The shrine glow (Walt's ask, after standing 2 m from an invisible DEPLOY
	// shrine): the curio-beacon recipe at indoor scale — a slim room-height
	// pillar + a colored point light. Hard CDO refs so both always cook.
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
	BeaconMesh->SetupAttachment(Trigger);
	BeaconMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 2.4f));   // 12 cm wide, 2.4 m tall
	BeaconMesh->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconMesh->SetCanEverAffectNavigation(false);
	BeaconMesh->SetCastShadow(false);
	if (Cylinder.Succeeded()) { BeaconMesh->SetStaticMesh(Cylinder.Object); }
	if (Basic.Succeeded()) { BeaconMesh->SetMaterial(0, Basic.Object); }

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Glow->SetIntensity(3000.f);
	Glow->SetAttenuationRadius(600.f);
	Glow->CastShadows = false;
}

void APowerGrant::BeginPlay()
{
	Super::BeginPlay();

	// Already claimed in a previous session -> this shrine is spent; vanish before
	// the player sees it. (The progression save is the authority, not the level.)
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		if (Progression->HasClaimedGrant(EffectiveGrantKey()))
		{
			Destroy();
			return;
		}
	}

	if (Mesh)
	{
		RestLocation = Mesh->GetRelativeLocation();
	}

	// Tint the beacon: cyan = a power waits here, gold = a sauce stash.
	const FLinearColor GlowColor = bGrantsPower
		? FLinearColor(0.35f, 0.9f, 1.0f)
		: FLinearColor(1.0f, 0.85f, 0.3f);
	if (Glow)
	{
		Glow->SetLightColor(GlowColor);
	}
	if (BeaconMesh)
	{
		BeaconMesh->SetVisibility(bShowBeacon);
		if (UMaterialInstanceDynamic* MID = BeaconMesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), GlowColor);
		}
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APowerGrant::OnTriggerOverlap);

	// LAST, deliberately: BindToAgent hides the beacon and stands the trigger down, and
	// the beacon's visibility is set from bShowBeacon a few lines above. Called any
	// earlier and this actor turns its own pole back on.
	BindToAgent();
}

void APowerGrant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Pickup read: slow spin + gentle bob. Cheap, no material or animation needed.
	if (Mesh)
	{
		BobPhase += DeltaSeconds;
		Mesh->SetRelativeLocation(RestLocation + FVector(0, 0, 12.0f * FMath::Sin(BobPhase * 2.0f)));
		Mesh->AddRelativeRotation(FRotator(0.0f, 45.0f * DeltaSeconds, 0.0f));
	}
}

FName APowerGrant::EffectiveGrantKey() const
{
	if (!GrantKey.IsNone())
	{
		return GrantKey;
	}
	// Power shrines key on the verb (one claim per verb, wherever it's placed);
	// sauce-only markers key on their own stable level name.
	return bGrantsPower ? FName(*PowerVerbDisplayName(Power)) : GetFName();
}

void APowerGrant::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (bConsumed || bTrialOpen || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	/* YOU HAVE TO BE ON THE SAME FLOOR.

	   A 110cm trigger on a shrine standing a metre off the ground reaches DOWN through the
	   floor it stands on. PowerGrant_Compile sits in the library at Z=420; the staircase
	   passes underneath it, and a player climbing at Z=213 has a capsule whose top reaches
	   about 309 — against a trigger whose underside is at 310. It clipped, and the library's
	   reward opened on the stairs, several rooms before the player was ever meant to meet
	   it. That is the machine Walt kept hitting: not the one nearest his spawn, and not a
	   binding failure. A sphere poking through a ceiling.

	   Cheaper and more honest than shrinking every trigger by hand: a shrine belongs to the
	   floor it stands on. On the same storey the pawn's centre sits within a few units of
	   the shrine's; a storey below it is 200-odd out. */
	const float FloorGap = FMath::Abs(Pawn->GetActorLocation().Z - GetActorLocation().Z);
	if (FloorGap > SameFloorTolerance)
	{
		UE_LOG(LogSibeliusGame, Verbose,
			TEXT("[PowerGrant] %s ignored an overlap from %.0f units below/above — not this floor."),
			*GetActorLabelSafe(), FloorGap);
		return;
	}

	// The attic-key sphere spends books and mints a Key. It is not a power
	// trial — walking in with enough books is the whole beat.
	if (bGrantsKey)
	{
		ClaimNow(Pawn);
		return;
	}

	// Walt's trial: a POWER must be won at the machine, not collected like
	// loose change. Sauce-only markers keep the instant walk-in grant.
	if (bSlotTrial && bGrantsPower)
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			OpenTrial(PC);
		}
		return;
	}

	ClaimNow(Pawn);
}

void APowerGrant::BindToAgent()
{
	/* When an AI agent hands this power over, the pole stops being the interface: it hides
	   itself and switches its trigger off, so the player can never be ambushed by walking
	   past. Everything else about the shrine — the trial, the stake, the claim key, the
	   Refuser alarm, the sauce — is unchanged and still lives here. Only the way it is
	   ASKED FOR moves to the dancer. */
	if (!GrantedByAgent)
	{
		return;
	}

	/* STAND THE POLE DOWN FIRST, BEFORE looking for her component.
	   The first version did this only AFTER a successful bind, and returned early when the
	   component was missing — but UDancerAgentComponent is attached by
	   DancerAgentSubsystem's scan, which runs on a FIVE SECOND cycle. So for the first
	   seconds of every session the trigger was still live, and Walt kept meeting Deploy's
	   slot machine on the staircase even though the binding worked and Isla's prompt read
	   correctly a moment later. Designating an agent is enough on its own to retire the
	   sphere; whether she has woken up yet is a separate question. */
	StandDown();

	UDancerAgentComponent* Agent = GrantedByAgent->FindComponentByClass<UDancerAgentComponent>();
	if (!Agent)
	{
		/* The component is attached at runtime by DancerAgentSubsystem's scan, which may
		   not have reached her yet. Retry rather than fail silently — a missed bind leaves
		   an invisible, un-triggerable shrine and a power that cannot be obtained at all,
		   with nothing in the log to say why. */
		if (++AgentBindAttempts <= 20)
		{
			GetWorldTimerManager().SetTimer(AgentBindTimer, this, &APowerGrant::BindToAgent, 0.5f, false);
		}
		else
		{
			UE_LOG(LogSibeliusGame, Error,
				TEXT("[PowerGrant] %s: '%s' never grew a UDancerAgentComponent — %s is UNREACHABLE."),
				*GetActorLabelSafe(), *GrantedByAgent->GetName(), *PowerVerbDisplayName(Power));
		}
		return;
	}

	Agent->SetPowerGrant(this, Power);

	UE_LOG(LogSibeliusGame, Display, TEXT("[PowerGrant] %s is now given by %s"),
		*PowerVerbDisplayName(Power), *GrantedByAgent->GetName());
}

void APowerGrant::StandDown()
{
	// The pole is decoration for a job someone else does now: invisible, and unable to
	// take the screen off anyone who walks past. Idempotent — BindToAgent may retry.
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
	}
	if (BeaconMesh)
	{
		BeaconMesh->SetVisibility(false);
		BeaconMesh->SetHiddenInGame(true);
	}
	if (Glow)
	{
		Glow->SetVisibility(false);
		Glow->SetHiddenInGame(true);
	}
	if (Trigger)
	{
		Trigger->SetGenerateOverlapEvents(false);
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

FString APowerGrant::GetActorLabelSafe() const
{
#if WITH_EDITOR
	return GetActorNameOrLabel();
#else
	return GetName();
#endif
}

void APowerGrant::RequestTrial(APlayerController* PC)
{
	if (bConsumed || bTrialOpen || !PC)
	{
		return;
	}
	OpenTrial(PC);
}

void APowerGrant::OpenTrial(APlayerController* PC)
{
	/* SAY WHICH SHRINE THIS IS, AND WHERE.

	   Walt has now met a trial on the staircase four times, and three separate inferences
	   about which grant it was have been wrong — Refactor by chapter order, then Deploy by
	   distance to the PlayerStart, then a bind-timing race. Guessing from level geometry
	   has cost more than the bug. This makes the game name the culprit: label, verb, its
	   own location, the player's location, and whether an agent was supposed to have
	   retired it. One line in the log ends the argument. */
	const FVector Here = GetActorLocation();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerAt = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	UE_LOG(LogSibeliusGame, Warning,
		TEXT("[PowerGrant] TRIAL OPENED by '%s' (%s) at (%.0f, %.0f, %.0f); player at "
		     "(%.0f, %.0f, %.0f); GrantedByAgent=%s"),
		*GetActorLabelSafe(), *PowerVerbDisplayName(Power),
		Here.X, Here.Y, Here.Z, PlayerAt.X, PlayerAt.Y, PlayerAt.Z,
		GrantedByAgent ? *GrantedByAgent->GetName() : TEXT("NONE"));

	// Fresh widget per entry = fresh stake per entry, no state to reset.
	TrialWidget = CreateWidget<USlotScreenWidget>(PC, USlotScreenWidget::StaticClass());
	if (!TrialWidget)
	{
		return;
	}
	TrialWidget->InitModel(FMath::Rand());
	TrialWidget->SetTrial(TrialStartCredits, TrialTargetCredits);
	TrialWidget->OnTrialWon.BindUObject(this, &APowerGrant::HandleTrialWon);
	TrialWidget->OnClosed.BindUObject(this, &APowerGrant::HandleTrialClosed);
	TrialWidget->AddToViewport(80);

	// SC1's pattern: UIOnly + focus so Space reaches the reels and WASD stops.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TrialWidget->TakeWidget());
	PC->SetInputMode(Mode);
	TrialWidget->SetFocus();
	bTrialOpen = true;

	// The machine draws a crowd (Walt's design, the temple pour's pattern):
	// the alarm rings while you spin, and Gideon is waiting when you stand up.
	if (bSummonRefusersOnTrial)
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UHallAlarmSubsystem* Alarm = GI ? GI->GetSubsystem<UHallAlarmSubsystem>() : nullptr)
		{
			Alarm->TriggerAlarm();
		}
	}
}

void APowerGrant::HandleTrialWon()
{
	// Hold the screen 1.6 s so the widget's THE MACHINE YIELDS banner and
	// fanfare land before the claim ceremony takes over. If the player Escs
	// during the hold, the close is a no-op and the claim still pays — a won
	// trial is won.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &APowerGrant::FinishTrialClaim), 1.6f, false);
}

void APowerGrant::FinishTrialClaim()
{
	CloseTrialWidget();
	ClaimNow();
}

void APowerGrant::HandleTrialClosed()
{
	// Retreat or bust: put the world back; step out and in again to retry.
	CloseTrialWidget();
}

void APowerGrant::CloseTrialWidget()
{
	if (TrialWidget && TrialWidget->IsInViewport())
	{
		TrialWidget->RemoveFromParent();
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	TrialWidget = nullptr;
	bTrialOpen = false;
}

void APowerGrant::ClaimNow(APawn* Pawn)
{
	UInventoryComponent* Inv = Pawn
		? Pawn->FindComponentByClass<UInventoryComponent>()
		: nullptr;

	if (bGrantsKey && BookCost > 0)
	{
		const int32 Have = Inv ? Inv->GetCount(EResourceType::Book) : 0;
		if (Have < BookCost)
		{
			const FString Line = FString::Printf(
				TEXT("THE ATTIC KEY NEEDS %d BOOKS — YOU HAVE %d"), BookCost, Have);
			APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
			{
				HUD->ShowBanner(Line, 3.5f);
			}
			return;   // shrine stays; come back with the books
		}
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression || !Progression->ClaimOneTimeGrant(EffectiveGrantKey()))
	{
		return; // no subsystem (headless) or already claimed — nothing to give
	}
	bConsumed = true;

	// The grant moment: the HUD's ceremony banner (OnPowerUnlocked) and the
	// sauce delta flash (OnSauceChanged) fire off these two calls — FUN-7.
	if (bGrantsPower)
	{
		Progression->UnlockPower(Power);
	}
	if (bGrantsKey && Inv)
	{
		if (BookCost > 0)
		{
			Inv->Spend(EResourceType::Book, BookCost);
		}
		Inv->Add(EResourceType::Key, 1);
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
		{
			HUD->ShowBanner(TEXT("THE ATTIC KEY"), 4.0f);
		}
	}
	Progression->GrantSauce(SauceReward);

	if (GrantSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GrantSound, GetActorLocation());
	}

	Destroy();
}
