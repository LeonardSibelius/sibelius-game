// CarouselMachine.cpp — SIB-46 grey-box cabinet. See header.

#include "CarouselMachine.h"
#include "CarouselRunSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ACarouselMachine::ACarouselMachine()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Lever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lever"));
	Lever->SetupAttachment(Body);
	Lever->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACarouselMachine::BeginPlay()
{
	Super::BeginPlay();

	// Placeholder cube meshes (engine assets — no project art dependency).
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		if (Body)  { Body->SetStaticMesh(Cube);  Body->SetRelativeScale3D(FVector(1.0f, 1.5f, 2.0f)); }
		if (Lever)
		{
			Lever->SetStaticMesh(Cube);
			Lever->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.8f));
			Lever->SetRelativeLocation(FVector(80.0f, 90.0f, 120.0f));   // jutting out the right side
		}
	}

	// Auto-start a run so PIE is immediately playable.
	if (UCarouselRunSubsystem* RunSub = GetRun())
	{
		RunSub->OnSpinResolved.AddDynamic(this, &ACarouselMachine::HandleSpinResolved);
		if (RunSub->GetPhase() == ECarouselRunPhase::NotStarted)
		{
			RunSub->StartRun(/*Seed*/ 1);
		}
	}

	TryEnableInput();
}

UCarouselRunSubsystem* ACarouselMachine::GetRun() const
{
	const UWorld* W = GetWorld();
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCarouselRunSubsystem>() : nullptr;
}

void ACarouselMachine::TryEnableInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		if (++InputAttempts < 40)
		{
			GetWorldTimerManager().SetTimer(InputRetryHandle, this, &ACarouselMachine::TryEnableInput, 0.25f, false);
		}
		return;
	}

	EnableInput(PC);
	if (InputComponent)
	{
		// Direct key binds — no Input Mapping Context needed (portable).
		InputComponent->BindKey(EKeys::E,         IE_Pressed, this, &ACarouselMachine::OnPullLever);
		InputComponent->BindKey(EKeys::One,       IE_Pressed, this, &ACarouselMachine::OnBuy0);
		InputComponent->BindKey(EKeys::Two,       IE_Pressed, this, &ACarouselMachine::OnBuy1);
		InputComponent->BindKey(EKeys::Three,     IE_Pressed, this, &ACarouselMachine::OnBuy2);
		InputComponent->BindKey(EKeys::R,         IE_Pressed, this, &ACarouselMachine::OnReroll);
		InputComponent->BindKey(EKeys::Enter,     IE_Pressed, this, &ACarouselMachine::OnContinue);
		InputComponent->BindKey(EKeys::N,         IE_Pressed, this, &ACarouselMachine::OnNewRun);
	}
}

void ACarouselMachine::OnPullLever()
{
	if (UCarouselRunSubsystem* RunSub = GetRun())
	{
		if (RunSub->Spin()) { LeverPull = 1.0f; }   // the sim resolves it; we just animate the pull
	}
}

void ACarouselMachine::OnBuy0()    { if (UCarouselRunSubsystem* R = GetRun()) { R->BuyOffering(0); } }
void ACarouselMachine::OnBuy1()    { if (UCarouselRunSubsystem* R = GetRun()) { R->BuyOffering(1); } }
void ACarouselMachine::OnBuy2()    { if (UCarouselRunSubsystem* R = GetRun()) { R->BuyOffering(2); } }
void ACarouselMachine::OnReroll()  { if (UCarouselRunSubsystem* R = GetRun()) { R->Reroll(); } }
void ACarouselMachine::OnContinue(){ if (UCarouselRunSubsystem* R = GetRun()) { R->AdvanceToNextRound(); } }

void ACarouselMachine::OnNewRun()
{
	if (UCarouselRunSubsystem* R = GetRun()) { R->StartRun(FMath::Rand()); }
}

void ACarouselMachine::HandleSpinResolved(const FSpinResult& Result)
{
	// Presentation reaction only — scale the flash with the payout magnitude (the spec's "scalable").
	if (Result.SpinPayout >= BigWinThreshold)
	{
		const float Mag = FMath::Clamp(static_cast<float>(Result.SpinPayout) / 1000.0f, 0.0f, 1.0f);
		BigWinFlash = FMath::Max(BigWinFlash, 0.35f + 0.65f * Mag);
	}
}

void ACarouselMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Lever pull springs back.
	if (LeverPull > 0.0f)
	{
		LeverPull = FMath::Max(0.0f, LeverPull - DeltaSeconds * 3.0f);
		if (Lever) { Lever->SetRelativeRotation(FRotator(0.0f, 0.0f, -60.0f * LeverPull)); }
	}

	// Big-win pulse: body swells with the flash, then decays.
	if (BigWinFlash > 0.0f)
	{
		BigWinFlash = FMath::Max(0.0f, BigWinFlash - DeltaSeconds * 1.2f);
		if (Body) { Body->SetRelativeScale3D(FVector(1.0f, 1.5f, 2.0f) * (1.0f + 0.15f * BigWinFlash)); }
	}
}
