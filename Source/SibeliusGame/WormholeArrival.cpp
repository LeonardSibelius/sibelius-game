// WormholeArrival.cpp — see the header for why this is Niagara and not cubes.

#include "WormholeArrival.h"

#include "SibeliusGame.h"   // LogSibeliusGame

#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

namespace
{
	/* THE DEFAULT, HARD-REFERENCED IN THE CONSTRUCTOR SO IT COOKS.

	   Content/PortalVFX/ is a gitignored purchased pack. A soft path from C++ is not a
	   package reference and the cooker does not follow one — the system would work
	   perfectly in PIE and be missing from the shipped build. That is the v0.7.4
	   invisible-spaceport bug, and here it would land in the last scene of the game. */
	const TCHAR* const P_DefaultFX = TEXT("/Game/PortalVFX/NS/NS_DreamLand.NS_DreamLand");

	/* PICK ONE OF FOURTEEN WITHOUT A REBUILD.

	   `sib.WormholeFX NS_HeavenPath` in the Cmd box, then replay. Which of these looks
	   like a passage rather than a special effect is a question about taste, and answering
	   it fourteen times at one editor restart each is not a reasonable way to spend an
	   evening. The boarding lamp learned this the same night.

	   A bare name is resolved under /Game/PortalVFX/NS/; a full path also works. */
	static TAutoConsoleVariable<FString> CVarWormholeFX(
		TEXT("sib.WormholeFX"), TEXT(""),
		TEXT("Niagara system for the Grok arrival. Bare name (NS_HeavenPath) or full path. Empty = the actor's own."),
		ECVF_Cheat);
}

AWormholeArrival::AWormholeArrival()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FXFinder(P_DefaultFX);
	if (FXFinder.Succeeded())
	{
		PassageFX = FXFinder.Object;
	}
}

UNiagaraSystem* AWormholeArrival::ResolveFX() const
{
	const FString Override = CVarWormholeFX.GetValueOnGameThread();
	if (!Override.IsEmpty())
	{
		// A bare name is the useful case at a console; spell out the folder for it.
		FString Path = Override;
		if (!Path.StartsWith(TEXT("/")))
		{
			Path = FString::Printf(TEXT("/Game/PortalVFX/NS/%s.%s"), *Override, *Override);
		}
		if (UNiagaraSystem* Picked = LoadObject<UNiagaraSystem>(nullptr, *Path))
		{
			return Picked;
		}
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Wormhole] sib.WormholeFX '%s' did not load; using the default."), *Override);
	}

	if (UNiagaraSystem* Own = PassageFX.Get())
	{
		return Own;
	}
	return PassageFX.LoadSynchronous();
}

void AWormholeArrival::BeginPlay()
{
	Super::BeginPlay();

	/* HE ARRIVES BLIND, AND THAT IS THE POINT. Black first, then the effect out of it,
	   then the planet out of the effect. Fading up over a good part of the passage rather
	   than snapping means there is never a frame where Grok is simply "on". */
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FMath::Min(2.5f, PassageSeconds),
				FLinearColor::Black, /*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
		}
	}

	/* CENTRED ON HIM, NOT ON THIS ACTOR. The two are placed by the same script and should
	   coincide, but "should" is how a player ends up watching the effect happen somewhere
	   across the valley. Ask the pawn. */
	FVector Where = GetActorLocation();
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Where = Player->GetActorLocation();
	}
	Where += FXOffset;

	if (UNiagaraSystem* System = ResolveFX())
	{
		FXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Where, FRotator::ZeroRotator,
			FVector(FXScale), /*bAutoDestroy=*/false, /*bAutoActivate=*/true);

		/* THE PACK EXPOSES OVERRIDES; NOT EVERY SYSTEM EXPOSES THE SAME ONES. Setting a
		   parameter a system does not have is a no-op, not an error, so these are safe to
		   attempt across all fourteen and there is no need to special-case per system. */
		if (FXComponent)
		{
			FXComponent->SetVariableLinearColor(TEXT("Color"), FXColor);
			FXComponent->SetVariableLinearColor(TEXT("CustomColor"), FXColor);
			FXComponent->SetVariableFloat(TEXT("ScalableSize"), FXScale);
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[Wormhole] Arrival FX '%s' over %.1fs."),
			*System->GetName(), PassageSeconds);
	}
	else
	{
		// Not fatal: he still arrives, just plainly. Say so rather than fail silently -
		// a missing effect and a broken one look identical from the lawn.
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Wormhole] No Niagara system resolved; arriving with no effect."));
	}

	HoldPlayer(true);
	bRunning = true;
	Elapsed = 0.0f;
	SetActorTickEnabled(true);
}

void AWormholeArrival::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	/* GIVE HIM HIS LEGS BACK NO MATTER HOW THIS ENDS. A level torn down mid-passage would
	   otherwise leave IgnoreMoveInput set on a controller that outlives this actor — the
	   player arrives somewhere later unable to move, with nothing on screen to explain it. */
	if (bRunning && !bFinished)
	{
		HoldPlayer(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AWormholeArrival::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRunning)
	{
		return;
	}

	Elapsed += DeltaSeconds;

	// Any key ends it. Read off the controller rather than bound, so it needs no input
	// component of its own and cannot starve another actor of a key (the BindKey trap).
	if (bSkippable)
	{
		if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Enter)
				|| PC->WasInputKeyJustPressed(EKeys::Escape)
				|| PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
			{
				Finish();
				return;
			}
		}
	}

	if (Elapsed >= PassageSeconds)
	{
		Finish();
	}
}

void AWormholeArrival::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	bRunning = false;
	SetActorTickEnabled(false);

	/* DEACTIVATE, DO NOT DESTROY. The pack's own description: "These VFX have Automatic
	   Fade-in, Fade-Out animation for 1 sec once you deactivate them." Destroying the
	   component here would cut the effect off mid-frame and the planet would snap in.
	   Let it fade, then clean up. */
	if (FXComponent)
	{
		FXComponent->Deactivate();

		FTimerHandle Cleanup;
		TWeakObjectPtr<UNiagaraComponent> Weak = FXComponent;
		GetWorldTimerManager().SetTimer(Cleanup, [Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->DestroyComponent();
			}
		}, 1.5f, false);

		FXComponent = nullptr;
	}

	HoldPlayer(false);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Wormhole] Arrived. Controls released."));
}

void AWormholeArrival::HoldPlayer(bool bHold)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(bHold);
		PC->SetIgnoreLookInput(bHold);
	}
}
