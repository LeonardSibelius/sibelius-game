// FateCarousel.cpp — SIB-34, the Carousel of Fates. See header.

#include "FateCarousel.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogFateCarousel, Log, All);

namespace
{
	// Orbit order tells a quiet story: the seven leads, fate (scatter) closes.
	const TCHAR* CardIds[] = {
		TEXT("seven"), TEXT("star"), TEXT("crown"), TEXT("saturn"), TEXT("galaxy"),
		TEXT("moon"), TEXT("mars"), TEXT("wild"), TEXT("scatter")
	};
	constexpr int32 NumCards = UE_ARRAY_COUNT(CardIds);
}

AFateCarousel::AFateCarousel()
{
	PrimaryActorTick.bCanEverTick = true;

	Hub = CreateDefaultSubobject<USceneComponent>(TEXT("Hub"));
	SetRootComponent(Hub);
}

void AFateCarousel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildCards();
}

void AFateCarousel::BuildCards()
{
	// Tear down previous construction-script components (OnConstruction reruns
	// on every property edit).
	for (UStaticMeshComponent* Old : Cards)
	{
		if (Old) { Old->DestroyComponent(); }
	}
	Cards.Reset();
	CardBaseZ.Reset();

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SlotFactory/Materials/M_fate_base.M_fate_base"));
	if (!Plane || !Base)
	{
		UE_LOG(LogFateCarousel, Error, TEXT("[Fate] missing Plane mesh or M_fate_base — run Tools/Scripts/build_fate_altar.py first."));
		return;
	}

	const float Scale = CardSize / 100.0f;   // engine Plane is 100cm
	for (int32 i = 0; i < NumCards; ++i)
	{
		const float AngleDeg = 360.0f * i / NumCards;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Pos(OrbitRadius * FMath::Cos(Rad), OrbitRadius * FMath::Sin(Rad), 0.0f);

		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("FateCard_%s"), CardIds[i]));
		Card->SetupAttachment(Hub);
		Card->SetStaticMesh(Plane);
		Card->SetRelativeLocation(Pos);
		// C++ FRotator is (Pitch, Yaw, Roll). Knob-driven — see header note.
		Card->SetRelativeRotation(FRotator(CardPitch, AngleDeg + CardYawOffset, 0.0f));
		Card->SetRelativeScale3D(FVector(Scale, Scale, Scale));
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never block the E-trace to the plinth
		Card->SetCastShadow(false);
		Card->RegisterComponent();

		const FString TexPath = FString::Printf(TEXT("/Game/SlotFactory/SymbolSprites/T_sym_%s.T_sym_%s"), CardIds[i], CardIds[i]);
		if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexPath))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetTextureParameterValue(TEXT("Sprite"), Tex);
			MID->SetScalarParameterValue(TEXT("Glow"), CardGlow);   // per-instance brightness (Walt's library ring)
			Card->SetMaterial(0, MID);
		}
		else
		{
			UE_LOG(LogFateCarousel, Error, TEXT("[Fate] sprite missing: %s"), *TexPath);
		}

		Cards.Add(Card);
		CardBaseZ.Add(Pos.Z);
	}
	UE_LOG(LogFateCarousel, Display, TEXT("[Fate] carousel built: %d cards, radius %.0f"), Cards.Num(), OrbitRadius);
}

void AFateCarousel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RunningTime += DeltaSeconds;

	// Ceremonial orbit...
	AddActorWorldRotation(FRotator(0.0f, RotationDegPerSec * DeltaSeconds, 0.0f));

	// ...with a gentle per-card breath (phase-offset bob).
	const float TwoPi = 2.0f * PI;
	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		if (UStaticMeshComponent* Card = Cards[i])
		{
			const float Phase = TwoPi * i / FMath::Max(Cards.Num(), 1);
			FVector Loc = Card->GetRelativeLocation();
			Loc.Z = CardBaseZ.IsValidIndex(i) ? CardBaseZ[i] : 0.0f;
			Loc.Z += BobAmplitude * FMath::Sin(TwoPi * RunningTime / FMath::Max(BobPeriodSeconds, 0.1f) + Phase);
			Card->SetRelativeLocation(Loc);
		}
	}
}
