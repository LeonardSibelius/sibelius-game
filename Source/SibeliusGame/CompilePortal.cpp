// CompilePortal.cpp — see header.

#include "CompilePortal.h"

#include "SibeliusGame.h"
#include "SibeliusHUD.h"
#include "TravelTransitionSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ACompilePortal::ACompilePortal()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	Effect->SetupAttachment(Root);
	Effect->SetAutoActivate(true);

	PromptVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PromptVolume"));
	PromptVolume->SetupAttachment(Root);
	PromptVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PromptVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PromptVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PromptVolume->SetCanEverAffectNavigation(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetIntensityUnits(ELightUnits::Lumens);
	Glow->SetCastShadows(false);

	PromptText = FText::FromString(TEXT("[C] go through"));

	// HARD-REFERENCED, like the spaceport's portal: Content/PortalVFX is a gitignored pack,
	// and a soft path from C++ is not a package reference - it would work in PIE and be
	// missing from a shipped build.
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HoleFinder(
		TEXT("/Game/PortalVFX/NS/NS_TeleporterHole.NS_TeleporterHole"));
	if (HoleFinder.Succeeded())
	{
		PortalSystem = HoleFinder.Object;
	}
}

void ACompilePortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void ACompilePortal::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

void ACompilePortal::ApplyLook()
{
	if (Effect)
	{
		if (Effect->GetAsset() != PortalSystem)
		{
			Effect->SetAsset(PortalSystem);
		}
		Effect->SetRelativeLocation(EffectOffset);
		Effect->SetRelativeRotation(EffectRotation);
		Effect->SetRelativeScale3D(FVector(EffectScale));
		if (PortalSystem && !Effect->IsActive())
		{
			Effect->Activate(true);
		}
	}
	if (PromptVolume)
	{
		PromptVolume->SetBoxExtent(PromptExtent);
	}
	if (Glow)
	{
		Glow->SetIntensity(GlowLumens);
		Glow->SetLightColor(GlowColor);
		Glow->SetVisibility(GlowLumens > 0.0f);
	}
}

bool ACompilePortal::DestinationExists() const
{
#if WITH_EDITOR
	FString LongName;
	return FPackageName::SearchForPackageOnDisk(TravelTargetLevel.ToString(), &LongName);
#else
	return true;
#endif
}

bool ACompilePortal::TryClaim(APawn* Claimer, bool bLookedAt)
{
	if (!Claimer || (!bLookedAt && FVector::Dist(Claimer->GetActorLocation(), GetActorLocation()) > ClaimRadius))
	{
		return false;
	}

	if (TravelTargetLevel.IsNone() || !DestinationExists())
	{
		ASibeliusHUD::Toast(this, NotReadyText, 3.5f, SibeliusToast::Info);
		UE_LOG(LogSibeliusGame, Display, TEXT("[CompilePortal] %s: no destination yet (TravelTargetLevel '%s')."),
			*GetName(), *TravelTargetLevel.ToString());
		return true;
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[CompilePortal] %s -> %s (arrive at '%s')."),
		*GetName(), *TravelTargetLevel.ToString(), *ArrivalTag.ToString());
	UTravelTransitionSubsystem::Travel(this, TravelTargetLevel, ArrivalTag);
	return true;
}

void ACompilePortal::Interact_Implementation(AActor* Interactor)
{
	ASibeliusHUD::Toast(this, TEXT("[C] - THE COMPILE KEY - TAKES YOU THROUGH"), 2.5f, SibeliusToast::Info);
}

FText ACompilePortal::GetInteractionPrompt_Implementation() const
{
	return PromptText;
}
