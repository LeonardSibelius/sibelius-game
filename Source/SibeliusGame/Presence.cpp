// Presence.cpp — THE PRESENCE, Phase 1. See header + docs/THE_PRESENCE.md.

#include "Presence.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "SibeliusHUD.h"

APresence::APresence()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));   // skeletal meshes face +Y raw
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // block the look trace, never the player
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCanEverAffectNavigation(false);

	Hair = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hair"));
	Hair->SetupAttachment(Body);
	Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hair->SetCanEverAffectNavigation(false);

	// Hard CDO refs (the v0.7.4 cook lesson) — Echo, her hair cards, her idle.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(
		TEXT("/Game/EchoContent/Characters/Echo/Meshes/Echo.Echo"));
	if (BodyMesh.Succeeded()) { Body->SetSkeletalMesh(BodyMesh.Object); }

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HairMesh(
		TEXT("/Game/EchoContent/Characters/Echo/Meshes/Echo_Hair.Echo_Hair"));
	if (HairMesh.Succeeded()) { Hair->SetSkeletalMesh(HairMesh.Object); }

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(
		TEXT("/Game/EchoContent/Characters/Echo/Animations/Idle_Long.Idle_Long"));
	if (Idle.Succeeded()) { IdleAnim = Idle.Object; }

	Aura = CreateDefaultSubobject<UPointLightComponent>(TEXT("Aura"));
	Aura->SetupAttachment(SceneRoot);
	Aura->SetRelativeLocation(FVector(0.f, -40.f, 140.f));   // behind and above: a cool rim
	Aura->SetLightColor(FLinearColor(0.55f, 0.85f, 1.0f));
	Aura->SetIntensity(1200.f);
	Aura->SetAttenuationRadius(420.f);
	Aura->CastShadows = false;

	GreetingTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GreetingTrigger"));
	GreetingTrigger->SetupAttachment(SceneRoot);
	GreetingTrigger->InitSphereRadius(450.f);
	GreetingTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GreetingTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	GreetingTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GreetingTrigger->SetGenerateOverlapEvents(true);
}

void APresence::BeginPlay()
{
	Super::BeginPlay();

	if (Body && Hair)
	{
		Hair->SetLeaderPoseComponent(Body);   // the hair rides the body's bones
	}
	if (Body && IdleAnim)
	{
		Body->PlayAnimation(IdleAnim, /*bLooping=*/true);
	}
	if (bHologram)
	{
		ApplyHologram();
	}
	GreetingTrigger->OnComponentBeginOverlap.AddDynamic(this, &APresence::OnGreetingOverlap);
}

void APresence::OnGreetingOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled()) { return; }

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bGreetedThisVisit)
	{
		if (GreetingCooldown <= 0.0f || Now < NextGreetingTime) { return; }
	}
	bGreetedThisVisit = true;
	NextGreetingTime = Now + GreetingCooldown;

	// The greeting rides the HUD ceremony banner — Shipping-safe, and it puts
	// her words in the same visual register as the power ceremonies (they
	// were always hers).
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ShowBanner(GreetingText, GreetingSeconds);
		}
	}
	if (GreetingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GreetingSound, GetActorLocation());
	}
}

void APresence::ApplyHologram()
{
	// The light-being treatment: every slot swapped for the engine basic
	// material tinted her color (the PowerGrant beacon recipe), aura boosted.
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Basic) { return; }

	auto Tint = [&](USkeletalMeshComponent* Comp)
	{
		if (!Comp) { return; }
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Basic, this))
			{
				MID->SetVectorParameterValue(TEXT("Color"), HologramColor);
				Comp->SetMaterial(i, MID);
			}
		}
	};
	Tint(Body);
	Tint(Hair);
	if (Aura)
	{
		Aura->SetLightColor(HologramColor);
		Aura->SetIntensity(3200.f);
	}
}
