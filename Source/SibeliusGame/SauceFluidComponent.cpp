// SauceFluidComponent.cpp — v0.9.7.1 experiment 1. See header.

#include "SauceFluidComponent.h"
#include "SauceCauldron.h"
#include "ProgressionSubsystem.h"
#include "SibeliusGame.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

static TAutoConsoleVariable<int32> CVarSauceFluids(
	TEXT("sib.SauceFluids"),
	1,
	TEXT("v0.9.7.1: Niagara Fluids sauce. 0 = meshes/light only (escape hatch). 1 = on."),
	ECVF_Scalability);

namespace SauceFluidPaths
{
	// Engine plugin templates — exist the moment NiagaraFluids is enabled.
	// Hard FObjectFinder so they cook (the v0.7.4 soft-ref miss).
	// 3D gas only. The 2D FLIP hose and shallow-water pool are room-sized
	// WATER demos: they paint a blue volume under the pot rim (Walt's temple
	// screenshot) and overflow the virtual shadow map. Pour uses the same
	// green steam, plus the bowl's cylinder stream for a readable silhouette.
	const TCHAR* PluginSimmer = TEXT("/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_ColoredSmoke.Grid3D_Gas_ColoredSmoke");
	const TCHAR* PluginPour   = TEXT("/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_ColoredSmoke.Grid3D_Gas_ColoredSmoke");
	const TCHAR* PluginPool   = TEXT("/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_Pool.Grid2D_SW_Pool");

	// Optional game copies from Tools/Scripts/build_sauce_fluids.py (retints).
	const TCHAR* GameSimmer = TEXT("/Game/Sauce/NS_SauceSimmer.NS_SauceSimmer");
	const TCHAR* GamePour   = TEXT("/Game/Sauce/NS_SaucePour.NS_SaucePour");
	const TCHAR* GamePool   = TEXT("/Game/Sauce/NS_SaucePool.NS_SaucePool");
}

USauceFluidComponent::USauceFluidComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = false;
	SetIsReplicatedByDefault(false);

	SimmerComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SauceSimmer"));
	SimmerComp->SetupAttachment(this);
	SimmerComp->SetAutoActivate(false);
	SimmerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SimmerComp->SetCastShadow(false);
	SimmerComp->SetMobility(EComponentMobility::Movable);

	PourComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SaucePour"));
	PourComp->SetupAttachment(this);
	PourComp->SetAutoActivate(false);
	PourComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PourComp->SetCastShadow(false);
	PourComp->SetMobility(EComponentMobility::Movable);
	PourComp->SetRelativeLocation(PourOffset);
	PourComp->SetRelativeRotation(PourRotation);

	PoolComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SaucePool"));
	PoolComp->SetupAttachment(this);
	PoolComp->SetAutoActivate(false);
	PoolComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoolComp->SetCastShadow(false);
	PoolComp->SetMobility(EComponentMobility::Movable);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("SauceGlow"));
	Glow->SetupAttachment(this);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	Glow->SetLightColor(SauceColor);
	Glow->SetIntensity(120.f);
	Glow->SetAttenuationRadius(140.f);
	Glow->CastShadows = false;
	Glow->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SimmerFinder(SauceFluidPaths::PluginSimmer);
	if (SimmerFinder.Succeeded())
	{
		SimmerSystem = SimmerFinder.Object;
		SimmerComp->SetAsset(SimmerFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PourFinder(SauceFluidPaths::PluginPour);
	if (PourFinder.Succeeded())
	{
		PourSystem = PourFinder.Object;
		PourComp->SetAsset(PourFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PoolFinder(SauceFluidPaths::PluginPool);
	if (PoolFinder.Succeeded())
	{
		PoolSystem = PoolFinder.Object;
		PoolComp->SetAsset(PoolFinder.Object);
	}
}

void USauceFluidComponent::BeginPlay()
{
	Super::BeginPlay();

	SimmerComp->SetRelativeScale3D(FVector(SimmerScale));
	PourComp->SetRelativeScale3D(FVector(PourScale));
	PourComp->SetRelativeLocation(PourOffset);
	PourComp->SetRelativeRotation(PourRotation);
	PoolComp->SetRelativeScale3D(FVector(PoolScale));
	if (Glow)
	{
		Glow->SetLightColor(SauceColor);
	}

	ResolveGameCopies();
	ApplyLook(SimmerComp);
	ApplyLook(PourComp);
	ApplyLook(PoolComp);

	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		SauceChangedHandle = Progression->OnSauceChanged.AddUObject(
			this, &USauceFluidComponent::HandleSauceChanged);
	}

	if (PoolComp)
	{
		PoolComp->Deactivate();
		PoolComp->SetVisibility(false);
	}

	if (Role == ESauceFluidRole::Bowl)
	{
		SetPouring(false);
		SetFilled(false);
	}
	else
	{
		// Kitchen/temple SHOP cauldron: never run the gas. Default ColoredSmoke
		// is fire-orange — with the pot mesh stripped it looked like a floor fire.
		if (SimmerComp) { SimmerComp->Deactivate(); SimmerComp->SetVisibility(false); }
		if (PourComp) { PourComp->Deactivate(); PourComp->SetVisibility(false); }
		if (PoolComp) { PoolComp->Deactivate(); PoolComp->SetVisibility(false); }
		if (Glow)
		{
			Glow->SetVisibility(false);
			Glow->SetIntensity(0.f);
		}
	}

	RecomputeIntensity();
	RefreshSimmer();
}

void USauceFluidComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SauceChangedHandle.IsValid())
	{
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			Progression->OnSauceChanged.Remove(SauceChangedHandle);
		}
		SauceChangedHandle.Reset();
	}
	Super::EndPlay(Reason);
}

void USauceFluidComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (BoilSpike > 0.f)
	{
		BoilSpike = FMath::Max(0.f, BoilSpike - DeltaTime / FMath::Max(0.1f, BoilDecaySeconds));
	}

	if (Role == ESauceFluidRole::Cauldron)
	{
		if (const ASauceCauldron* Cauldron = Cast<ASauceCauldron>(GetOwner()))
		{
			bShopOpen = Cauldron->IsShopOpen();
		}
	}

	RecomputeIntensity();
	RefreshSimmer();

	if (Glow)
	{
		const bool bShow = ShouldSimmer() || bPouring;
		if (bShow)
		{
			const float Pulse = 0.55f + 0.45f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.4f);
			const float Heat = FMath::Clamp(SimmerIntensity, 0.f, 1.f);
			Glow->SetIntensity((70.f + 180.f * Heat) * Pulse);
			Glow->SetLightColor(SauceColor);
		}
		Glow->SetVisibility(bShow);
	}
}

void USauceFluidComponent::SetBlendHeat(float Blend01)
{
	BlendHeat = FMath::Clamp(Blend01, 0.f, 1.f);
}

void USauceFluidComponent::NotifyBoilOver()
{
	BoilSpike = 1.f;
}

void USauceFluidComponent::SetShopOpen(bool bOpen)
{
	bShopOpen = bOpen;
	if (bOpen)
	{
		BoilSpike = FMath::Max(BoilSpike, 0.7f);
	}
}

void USauceFluidComponent::SetPouring(bool bInPouring)
{
	bPouring = bInPouring;
	if (!PourComp)
	{
		return;
	}

	if (bPouring && FluidsEnabled() && !IsRunningCommandlet() && PourComp->GetAsset())
	{
		ApplyLook(PourComp);
		ApplyIntensity(PourComp, 1.f);
		PourComp->Activate(true);
	}
	else
	{
		PourComp->Deactivate();
	}
}

void USauceFluidComponent::SetFilled(bool bInFilled)
{
	bFilled = bInFilled;
	// Never run the 2D shallow-water pool. It is a swimming-pool template:
	// a blue volume the size of the table, sitting under the pot rim.
	if (PoolComp)
	{
		PoolComp->Deactivate();
		PoolComp->SetVisibility(false);
	}
}

bool USauceFluidComponent::HasSimmerSystem() const
{
	return SimmerComp && SimmerComp->GetAsset() != nullptr;
}

bool USauceFluidComponent::HasPourSystem() const
{
	return PourComp && PourComp->GetAsset() != nullptr;
}

bool USauceFluidComponent::HasPoolSystem() const
{
	return PoolComp && PoolComp->GetAsset() != nullptr;
}

void USauceFluidComponent::ResolveGameCopies()
{
	// Soft-load the optional /Game/Sauce duplicates. Plugin hard-refs already
	// cooked the originals; a missing game copy must never strip the system
	// and must not LogWarning (DoesPackageExist keeps the commandlet quiet).
	auto PreferGameCopy = [](const TCHAR* ObjectPath, TObjectPtr<UNiagaraSystem>& Slot, UNiagaraComponent* Comp)
	{
		FString Pkg, Dummy;
		FString(ObjectPath).Split(TEXT("."), &Pkg, &Dummy, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Pkg.IsEmpty() || !FPackageName::DoesPackageExist(Pkg))
		{
			return;
		}
		if (UNiagaraSystem* Game = LoadObject<UNiagaraSystem>(nullptr, ObjectPath))
		{
			Slot = Game;
			if (Comp)
			{
				Comp->SetAsset(Game);
			}
		}
	};
	PreferGameCopy(SauceFluidPaths::GameSimmer, SimmerSystem, SimmerComp);
	// Do NOT load NS_SaucePour / NS_SaucePool: those copies are the 2D water
	// demos. Pour uses the same 3D gas as simmer (already assigned).
}

void USauceFluidComponent::ApplyLook(UNiagaraComponent* Comp) const
{
	if (!Comp)
	{
		return;
	}

	// Niagara Fluids templates expose color under several names depending on
	// gas vs FLIP vs shallow water. Setting a missing user parameter is a no-op.
	static const FName ColorNames[] = {
		TEXT("User.Color"), TEXT("Color"), TEXT("User.SmokeColor"), TEXT("Smoke Color"),
		TEXT("User.Albedo"), TEXT("Albedo"), TEXT("User.ParticleColor"), TEXT("ParticleColor"),
		TEXT("Grid3D_Gas.Color"), TEXT("User.DensityColor"), TEXT("Density Color")
	};
	for (const FName& Name : ColorNames)
	{
		Comp->SetVariableLinearColor(Name, SauceColor);
	}
}

void USauceFluidComponent::ApplyIntensity(UNiagaraComponent* Comp, float Intensity) const
{
	if (!Comp)
	{
		return;
	}
	const float I = FMath::Clamp(Intensity, 0.f, 1.f);
	static const FName FloatNames[] = {
		TEXT("User.Density"), TEXT("Density"), TEXT("User.SpawnRate"), TEXT("SpawnRate"),
		TEXT("User.SourceStrength"), TEXT("SourceStrength"), TEXT("User.Temperature"),
		TEXT("Temperature"), TEXT("User.Intensity"), TEXT("Intensity"),
		TEXT("User.Buoyancy"), TEXT("Buoyancy")
	};
	for (const FName& Name : FloatNames)
	{
		Comp->SetVariableFloat(Name, I);
	}
	Comp->SetVariableFloat(TEXT("User.DensityScale"), 0.70f + 0.30f * I);
	Comp->SetVariableInt(TEXT("Quality"), 0);
	Comp->SetVariableInt(TEXT("User.Quality"), 0);
}

void USauceFluidComponent::RecomputeIntensity()
{
	float Wallet = 0.12f;
	if (const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		const float T = FMath::Clamp(
			static_cast<float>(Progression->GetSauce()) / static_cast<float>(FMath::Max(1, SauceForFullSimmer)),
			0.f, 1.f);
		Wallet = FMath::Lerp(0.12f, 0.75f, T);
	}

	float Heat = Wallet + BlendHeat * 0.45f + BoilSpike * 0.85f + (bShopOpen ? 0.35f : 0.f);
	if (Role == ESauceFluidRole::Bowl)
	{
		Heat = (bPouring ? 0.9f : 0.f) + (bFilled ? 0.55f + Wallet * 0.25f : 0.f) + BoilSpike * 0.4f;
	}
	SimmerIntensity = FMath::Clamp(Heat, 0.f, 1.f);
}

bool USauceFluidComponent::FluidsEnabled() const
{
	return CVarSauceFluids.GetValueOnGameThread() != 0;
}

bool USauceFluidComponent::ShouldSimmer() const
{
	if (!FluidsEnabled() || IsRunningCommandlet())
	{
		return false;
	}
	if (!SimmerComp || !SimmerComp->GetAsset())
	{
		return false;
	}
	if (!IsPlayerNear())
	{
		return false;
	}
	// Kitchen cauldron is the 0.9.7 stove (no extra pot, no aisle cloud).
	// Niagara Fluids live on the temple bowl only.
	if (Role == ESauceFluidRole::Bowl)
	{
		return bFilled || bPouring;
	}
	return false;
}

bool USauceFluidComponent::IsPlayerNear() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		// Headless / no pawn: treat as near so a PIE sit-in-the-kitchen still simmers
		// once possessed, and a commandlet (no pawn) never reaches here (ShouldSimmer
		// already bailed on IsRunningCommandlet).
		return true;
	}
	return FVector::Dist(Pawn->GetActorLocation(), GetComponentLocation()) <= MaxSimDistance;
}

void USauceFluidComponent::RefreshSimmer()
{
	if (!SimmerComp)
	{
		return;
	}

	const bool bWant = ShouldSimmer();
	if (bWant)
	{
		ApplyLook(SimmerComp);
		ApplyIntensity(SimmerComp, SimmerIntensity);
		if (!bSimmerActive)
		{
			SimmerComp->Activate(true);
			bSimmerActive = true;
		}
	}
	else if (bSimmerActive)
	{
		SimmerComp->Deactivate();
		bSimmerActive = false;
	}
}

void USauceFluidComponent::HandleSauceChanged(int32 /*NewTotal*/, int32 Delta)
{
	if (FMath::Abs(Delta) >= 5)
	{
		BoilSpike = FMath::Max(BoilSpike, FMath::Clamp(FMath::Abs(Delta) / 80.f, 0.35f, 1.f));
	}
}

void USauceFluidComponent::DressLiquidMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* SauceMat = nullptr;
	if (FPackageName::DoesPackageExist(TEXT("/Game/Sauce/M_SauceSurface")))
	{
		SauceMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sauce/M_SauceSurface.M_SauceSurface"));
	}
	if (SauceMat)
	{
		Mesh->SetMaterial(0, SauceMat);
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.06f, 0.55f, 0.16f));
		MID->SetScalarParameterValue(TEXT("Glow"), 1.15f);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.88f);
		MID->SetScalarParameterValue(TEXT("WaveHeight"), 0.0f);   // 3.2 made a circular saw
	}
}

void USauceFluidComponent::DressBubbleMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* BubbleMat = nullptr;
	if (FPackageName::DoesPackageExist(TEXT("/Game/Sauce/M_SauceBubble")))
	{
		BubbleMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sauce/M_SauceBubble.M_SauceBubble"));
	}
	if (!BubbleMat && FPackageName::DoesPackageExist(TEXT("/Game/Sauce/M_SauceSurface")))
	{
		BubbleMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sauce/M_SauceSurface.M_SauceSurface"));
	}
	if (BubbleMat)
	{
		Mesh->SetMaterial(0, BubbleMat);
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 1.0f, 0.65f));
		MID->SetScalarParameterValue(TEXT("Glow"), 3.2f);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.28f);
		MID->SetScalarParameterValue(TEXT("WaveHeight"), 0.0f);
	}
}

void USauceFluidComponent::SloshLiquid(UStaticMeshComponent* Mesh, const FVector& BaseScale, const FVector& BaseLoc) const
{
	if (!Mesh || !GetWorld())
	{
		return;
	}
	const float T = GetWorld()->GetTimeSeconds();
	const float Bob = 1.8f * FMath::Sin(T * 2.6f);
	const float Pulse = 1.f + 0.08f * FMath::Sin(T * 3.4f);
	Mesh->SetRelativeLocation(FVector(BaseLoc.X, BaseLoc.Y, BaseLoc.Z + Bob));
	Mesh->SetRelativeScale3D(FVector(BaseScale.X * Pulse, BaseScale.Y / Pulse, BaseScale.Z * (2.f - Pulse)));
}
