// WormholeArrival.cpp — see the header for why the passage is not its own level.

#include "WormholeArrival.h"

#include "SibeliusGame.h"   // LogSibeliusGame

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	// Engine basic shapes: always present, no pack to go missing, nothing to gitignore.
	const TCHAR* const P_Shape = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const P_Materialise = TEXT("/Game/AIApparition/M_materialise.M_materialise");

	/** Ease-out cubic — the swarm thins fast at first and lets go slowly. */
	float Settle(float A)
	{
		const float T = 1.0f - FMath::Clamp(A, 0.0f, 1.0f);
		return 1.0f - (T * T * T);
	}
}

AWormholeArrival::AWormholeArrival()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	PassageMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(P_Materialise));
}

void AWormholeArrival::BeginPlay()
{
	Super::BeginPlay();

	/* HE ARRIVES BLIND, AND THAT IS THE POINT. Black first, then the swarm resolves out
	   of it, then the planet resolves out of the swarm. Fading up over the whole passage
	   rather than snapping means there is never a frame where Grok is simply "on". */
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FMath::Min(2.5f, PassageSeconds),
				FLinearColor::Black, /*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
		}
	}

	BuildCloud();
	HoldPlayer(true);

	bRunning = true;
	Elapsed = 0.0f;
	SetActorTickEnabled(true);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Wormhole] Arrival: %d shapes over %.1fs."),
		Shapes.Num(), PassageSeconds);
}

void AWormholeArrival::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	/* GIVE HIM HIS LEGS BACK NO MATTER HOW THIS ENDS. A level torn down mid-passage
	   would otherwise leave IgnoreMoveInput set on a controller that outlives this
	   actor — the player would arrive somewhere later, unable to move, with nothing on
	   screen to explain it. */
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
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(0.5f, PassageSeconds), 0.0f, 1.0f);
	const float Eased = Settle(Alpha);

	// Any key ends it. Checked on the controller rather than bound, so it needs no input
	// component of its own and cannot starve anything else of a key (the BindKey trap).
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

	for (int32 i = 0; i < Shapes.Num(); ++i)
	{
		if (!IsValid(Shapes[i]))
		{
			continue;
		}

		// Opening outward, and turning: a still cloud reads as fog, a drifting one reads
		// as something he is passing through.
		Shapes[i]->AddRelativeLocation(Drifts[i] * DriftSpeed * DeltaSeconds);
		Shapes[i]->AddLocalRotation(FRotator(12.0f * DeltaSeconds, 18.0f * DeltaSeconds, 0.0f));

		if (UMaterialInstanceDynamic* MID = ShapeMIDs.IsValidIndex(i) ? ShapeMIDs[i].Get() : nullptr)
		{
			MID->SetScalarParameterValue(TEXT("Opacity"), 1.0f - Eased);
			MID->SetScalarParameterValue(TEXT("Glow"), PassageGlow * (1.0f - Eased));
		}
	}

	if (Alpha >= 1.0f)
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

	ClearCloud();
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

UMaterialInterface* AWormholeArrival::GetPassageMaterial()
{
	if (UMaterialInterface* Loaded = PassageMaterial.Get())
	{
		return Loaded;
	}
	return PassageMaterial.LoadSynchronous();
}

void AWormholeArrival::BuildCloud()
{
	UWorld* World = GetWorld();
	if (!World || Shapes.Num() > 0)
	{
		return;
	}

	UStaticMesh* Mesh = Cast<UStaticMesh>(
		StaticLoadObject(UStaticMesh::StaticClass(), nullptr, P_Shape));
	UMaterialInterface* Mat = GetPassageMaterial();
	if (!Mesh)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Wormhole] No shape mesh; arriving plainly."));
		return;
	}

	/* CENTRED ON HIM, NOT ON THIS ACTOR. The arrival point and the actor are placed by
	   the same script and should coincide, but "should" is how a player ends up watching
	   the effect happen somewhere across the valley. Ask the pawn. */
	FVector Centre = GetActorLocation();
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Centre = Player->GetActorLocation();
	}

	const float Inner = FMath::Min(CloudInnerRadius, CloudRadius * 0.9f);

	Shapes.Reserve(ShapeCount);
	ShapeMIDs.Reserve(ShapeCount);
	Drifts.Reserve(ShapeCount);

	for (int32 i = 0; i < ShapeCount; ++i)
	{
		const FVector Dir = FMath::VRand();
		const float Dist = FMath::FRandRange(Inner, CloudRadius);

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		if (!Comp)
		{
			continue;
		}
		Comp->SetupAttachment(GetRootComponent());
		Comp->RegisterComponent();
		Comp->SetStaticMesh(Mesh);

		// Never solid. He is standing inside this and must be able to walk out of it the
		// instant it lets go - the spaceport's "invisible solid objects on the lawn"
		// lesson, applied before it can happen rather than after.
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);

		Comp->SetWorldLocation(Centre + Dir * Dist);
		Comp->SetWorldRotation(FRotator(FMath::FRandRange(0.0f, 360.0f),
			FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f)));
		const float S = ShapeSize * FMath::FRandRange(0.4f, 1.6f) / 100.0f;   // cube is 100 cm
		Comp->SetWorldScale3D(FVector(S));

		if (Mat)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this))
			{
				MID->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
				MID->SetScalarParameterValue(TEXT("Glow"), PassageGlow);
				Comp->SetMaterial(0, MID);
				ShapeMIDs.Add(MID);
			}
			else
			{
				ShapeMIDs.Add(nullptr);
			}
		}
		else
		{
			ShapeMIDs.Add(nullptr);
		}

		Shapes.Add(Comp);
		Drifts.Add(Dir);
	}
}

void AWormholeArrival::ClearCloud()
{
	for (UStaticMeshComponent* Comp : Shapes)
	{
		if (IsValid(Comp))
		{
			Comp->DestroyComponent();
		}
	}
	Shapes.Reset();
	ShapeMIDs.Reset();
	Drifts.Reset();
}
