// Curio.cpp — see header.

#include "Curio.h"
#include "CurioCollectionSubsystem.h"
#include "ElsewhereSubsystem.h"
#include "ProgressionSubsystem.h"   // FUN-5: curios pay Sauce
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ACurio::ACurio()
{
	PrimaryActorTick.bCanEverTick = false;

	// CP3 lesson: explicit root, and a default-subobject mesh so it renders at runtime
	// (the null-proxy lesson). Engine sphere as the placeholder hero shape.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);   // visible to the interact trace
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		Mesh->SetStaticMesh(Sphere);
		Mesh->SetRelativeScale3D(FVector(0.6f));   // reads as treasure alone in the empty hall
	}

	// The glow — reads as "the one collectable" without an authored emissive material.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetIntensity(5000.f);
	Glow->SetAttenuationRadius(800.f);
	Glow->CastShadows = false;   // public UPROPERTY on ULightComponentBase

	// Default emissive base for the glowing-relic look (kit material; referenced by
	// path, graceful fallback if absent). Tinted per-curio in Configure.
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT")));
}

void ACurio::BeginPlay()
{
	Super::BeginPlay();

	// Builder-configured curios (the cathedral) already have an identity — leave them be.
	if (!CurioId.IsNone())
	{
		return;
	}

	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	if (!Elsewhere)
	{
		return;
	}

	// Prefer the staged plan (arrived via the Sauce Door); fall back to the editor-set default
	// (PIE-ing the level directly) so a hand-placed forest curio is always collectable.
	const FElsewherePlan& Plan = Elsewhere->GetStagedPlan();
	const FName WantCurio = Plan.IsValid() ? Plan.CurioId : DefaultCurioId;
	const FName WantPlace = Plan.IsValid() ? Plan.PlaceTypeId : DefaultPlaceTypeId;
	if (WantCurio.IsNone())
	{
		return;
	}

	const FCurioDef* Def = Elsewhere->FindCurio(WantCurio);
	const FPlaceTypeDef* Place = Elsewhere->FindPlace(WantPlace);
	const FLinearColor GlowCol = Place ? Place->CurioGlowColor : FLinearColor(0.9f, 0.8f, 0.4f);

	Configure(WantCurio, WantPlace, GlowCol);
	if (Def && !Def->Mesh.IsNull())
	{
		SetDisplayMesh(Def->Mesh.LoadSynchronous());
	}
}

void ACurio::Configure(FName InCurioId, FName InPlaceTypeId, FLinearColor GlowColor)
{
	CurioId = InCurioId;
	PlaceTypeId = InPlaceTypeId;
	if (Glow)
	{
		Glow->SetLightColor(GlowColor);
	}

	// Make the payoff object read as treasure: an emissive material tinted to the curio
	// color so it glows (and never shows the missing-material checker). If the base
	// material can't load (kit absent), the mesh keeps its valid default material.
	if (Mesh)
	{
		if (UMaterialInterface* Base = GlowMaterial.LoadSynchronous())
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, Base))
			{
				MID->SetVectorParameterValue(TEXT("Emissive"), GlowColor);
				MID->SetVectorParameterValue(TEXT("BaseColor"), GlowColor);
				MID->SetScalarParameterValue(TEXT("Intens"), GlowEmissiveIntensity);
				MID->SetScalarParameterValue(TEXT("TurnOn"), 1.0f);
			}
		}
	}
}

void ACurio::SetDisplayMesh(UStaticMesh* InMesh, float UniformScale)
{
	if (InMesh && Mesh)
	{
		Mesh->SetStaticMesh(InMesh);
		Mesh->SetRelativeScale3D(FVector(UniformScale));
	}
}

bool ACurio::Collect(UObject* WorldContext)
{
	if (bCollected)
	{
		return false;
	}

	UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext ? WorldContext : this);
	UCurioCollectionSubsystem* Collection = GI ? GI->GetSubsystem<UCurioCollectionSubsystem>() : nullptr;
	if (!Collection)
	{
		return false;   // no collection to add to — leave the curio in place
	}

	bCollected = true;
	Collection->CollectCurio(CurioId, PlaceTypeId);   // always succeeds for a known id (§1)

	// FUN-5: the quiet payout. On-screen line only — no tally, no completion bar.
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->GrantSauce(SauceOnCollect);
		Progression->BumpStat(SibeliusStats::CuriosCollected);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Emerald,
				FString::Printf(TEXT("+%d SAUCE  (total %d)"), SauceOnCollect, Progression->GetSauce()));
		}
	}

	Destroy();
	return true;
}

void ACurio::Interact_Implementation(AActor* Interactor)
{
	Collect(Interactor ? static_cast<UObject*>(Interactor) : static_cast<UObject*>(this));
}

FText ACurio::GetInteractionPrompt_Implementation() const
{
	return bCollected ? FText::GetEmpty()
		: NSLOCTEXT("Sibelius", "CurioPrompt", "Take the curio [E]");
}
