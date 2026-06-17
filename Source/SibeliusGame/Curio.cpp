// Curio.cpp — see header.

#include "Curio.h"
#include "CurioCollectionSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

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
		Mesh->SetRelativeScale3D(FVector(0.4f));
	}

	// The glow — reads as "the one collectable" without an authored emissive material.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetIntensity(3000.f);
	Glow->SetAttenuationRadius(600.f);
	Glow->CastShadows = false;   // public UPROPERTY on ULightComponentBase
}

void ACurio::Configure(FName InCurioId, FName InPlaceTypeId, FLinearColor GlowColor)
{
	CurioId = InCurioId;
	PlaceTypeId = InPlaceTypeId;
	if (Glow)
	{
		Glow->SetLightColor(GlowColor);
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
