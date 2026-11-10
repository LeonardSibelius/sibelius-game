// TableJuice.cpp — see the header.

#include "TableJuice.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace TableJuiceDetail
{
	// For anyone the shake bothers: sib.TableShake 0 turns it off, 0.5 halves it.
	TAutoConsoleVariable<float> CVarTableShake(
		TEXT("sib.TableShake"), 1.0f,
		TEXT("Scale of the camera shake at the pinball, pool and bowling games (0 = none)."));

	constexpr double ShapeSize = 100.0;       // the engine's basic shapes
	constexpr float StreakSeconds = 0.022f;

	// How brightly the sparks burn. Too high and every colour burns out to white; too low and
	// in a bright room they stop reading as light at all and look like dark seeds.
	TAutoConsoleVariable<float> CVarTableGlow(
		TEXT("sib.TableGlow"), 0.6f,
		TEXT("Brightness of the sparks at the pinball, pool and bowling games."));   // a spark is drawn this much of its own path long
}

void FTableSparks::Build(AActor* Owner, USceneComponent* Parent, const TCHAR* Prefix, int32 Count, bool bInConfetti)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/Genesis/M_GenesisGlow.M_GenesisGlow"));

	bConfetti = bInConfetti;
	Pool.Reset();
	if (!Owner)
	{
		return;
	}
	UStaticMesh* Shape = bConfetti ? CubeMesh.Object : SphereMesh.Object;
	for (int32 I = 0; I < Count; ++I)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s%02d"), Prefix, I));
		if (!Mesh)
		{
			continue;
		}
		if (Parent)
		{
			Mesh->SetupAttachment(Parent);
		}
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bAffectDynamicIndirectLighting = false;
		if (Shape)
		{
			Mesh->SetStaticMesh(Shape);
		}
		if (GlowMaterial.Succeeded())
		{
			Mesh->SetMaterial(0, GlowMaterial.Object);
		}
		Mesh->SetRelativeScale3D(FVector(0.004));
		Mesh->SetVisibility(false);

		FSpark Spark;
		Spark.Mesh = Mesh;
		Pool.Add(Spark);
	}
}

void FTableSparks::Begin()
{
	Next = 0;
	Live = 0;
	for (FSpark& S : Pool)
	{
		S.Life = 0.0f;
		if (UStaticMeshComponent* Mesh = S.Mesh.Get())
		{
			S.Paint = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			Mesh->SetVisibility(false);
		}
	}
}

void FTableSparks::Clear()
{
	Live = 0;
	for (FSpark& S : Pool)
	{
		S.Life = 0.0f;
		if (UStaticMeshComponent* Mesh = S.Mesh.Get())
		{
			Mesh->SetVisibility(false);
		}
	}
}

void FTableSparks::Burst(const FTableBurst& B)
{
	if (Pool.Num() == 0)
	{
		return;
	}
	const FVector Bias = B.Bias.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const int32 Count = FMath::Min(B.Count, Pool.Num());
	for (int32 K = 0; K < Count; ++K)
	{
		// Round and round the pool: when it is full the oldest spark is the one reused.
		FSpark& S = Pool[Next];
		Next = (Next + 1) % Pool.Num();

		FVector Any = FMath::VRand();
		if (FVector::DotProduct(Any, Bias) < 0.0)
		{
			Any = -Any;
		}
		const FVector Dir = FMath::Lerp(Bias, Any, FMath::Clamp(B.Spread, 0.0f, 1.0f)).GetSafeNormal(UE_SMALL_NUMBER, Bias);

		if (S.Life <= 0.0f)
		{
			++Live;
		}
		S.Pos = B.Origin + FMath::VRand() * (B.Scatter * FMath::FRand());
		S.Vel = Dir * (B.Speed * FMath::FRandRange(0.5f, 1.0f));
		S.Axis = FMath::VRand();
		S.Spin = FMath::FRandRange(4.0f, 14.0f);
		S.Age = 0.0f;
		S.Life = FMath::Max(0.05f, B.Life * FMath::FRandRange(0.7f, 1.0f));
		S.Size = B.Size * FMath::FRandRange(0.7f, 1.15f);

		if (UMaterialInstanceDynamic* Paint = S.Paint.Get())
		{
			Paint->SetVectorParameterValue(TEXT("Color"), B.Color);
			// Seen in play (2026-11-10): at the glow the AI city's stars use, every spark burns out
			// to white and the colours are lost. sib.TableGlow scales it.
			Paint->SetScalarParameterValue(TEXT("Intensity"), B.Glow * TableJuiceDetail::CVarTableGlow.GetValueOnGameThread());
		}
	}
}

void FTableSparks::Tick(float DeltaSeconds)
{
	if (Live <= 0 || DeltaSeconds <= 0.0f)
	{
		return;
	}
	const float Keep = FMath::Exp(-Drag * DeltaSeconds);
	for (FSpark& S : Pool)
	{
		if (S.Life <= 0.0f)
		{
			continue;
		}
		UStaticMeshComponent* Mesh = S.Mesh.Get();
		S.Age += DeltaSeconds;
		if (S.Age >= S.Life || !Mesh)
		{
			S.Life = 0.0f;
			--Live;
			if (Mesh)
			{
				Mesh->SetVisibility(false);
			}
			continue;
		}

		S.Vel.Z -= Gravity * DeltaSeconds;
		S.Vel *= Keep;
		S.Pos += S.Vel * DeltaSeconds;

		const float Left = 1.0f - S.Age / S.Life;   // 1 new, 0 gone
		FQuat Rotation;
		FVector Scale;
		if (bConfetti)
		{
			// A flake: thin, tumbling, and sliding sideways as it falls.
			Rotation = FQuat(S.Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector), S.Age * S.Spin);
			S.Pos += Rotation.GetRightVector() * (FMath::Sin(S.Age * S.Spin * 0.6f) * 14.0f * DeltaSeconds);
			const float Grow = FMath::Min(1.0f, Left * 5.0f);
			Scale = FVector(S.Size, S.Size * 0.6, S.Size * 0.06) * Grow;
		}
		else
		{
			// A spark: a streak along the way it is going, thinning as it dies.
			const float Speed = static_cast<float>(S.Vel.Size());
			Rotation = Speed > 1.0f ? S.Vel.ToOrientationQuat() : FQuat::Identity;
			const float Thick = S.Size * FMath::Max(0.4f, Left);
			const float Long = FMath::Clamp(Thick + Speed * TableJuiceDetail::StreakSeconds, Thick, S.Size * 4.0f);
			Scale = FVector(Long, Thick, Thick);
		}
		Mesh->SetWorldLocationAndRotation(S.Pos, Rotation);
		Mesh->SetWorldScale3D(Scale / TableJuiceDetail::ShapeSize);
		Mesh->SetVisibility(true);
	}
}

void FTableCameraKick::Shake(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.0f, 1.0f);
}

void FTableCameraKick::Punch(float Degrees)
{
	// The bigger of what is there and what is asked, so a run of bumpers does not pile up.
	FovNow = FMath::Abs(Degrees) > FMath::Abs(FovNow) ? Degrees : FovNow;
	FovNow = FMath::Clamp(FovNow, -8.0f, 8.0f);
}

void FTableCameraKick::Tick(float DeltaSeconds)
{
	Clock += DeltaSeconds;
	Trauma = FMath::Max(0.0f, Trauma - DeltaSeconds * 2.2f);
	FovNow = FMath::FInterpTo(FovNow, 0.0f, DeltaSeconds, 5.0f);
}

void FTableCameraKick::Reset()
{
	Trauma = 0.0f;
	FovNow = 0.0f;
}

FVector FTableCameraKick::Offset(float MaxCm) const
{
	// Squared: a small knock barely shows, a big one does.
	const double A = static_cast<double>(Trauma) * Trauma * MaxCm * FMath::Max(0.0f, TableJuiceDetail::CVarTableShake.GetValueOnGameThread());
	return FVector(FMath::Sin(Clock * 61.0), FMath::Sin(Clock * 47.0 + 1.3), FMath::Sin(Clock * 53.0 + 2.1)) * A;
}

FRotator FTableCameraKick::Turn(float MaxDegrees) const
{
	const double A = static_cast<double>(Trauma) * Trauma * MaxDegrees * FMath::Max(0.0f, TableJuiceDetail::CVarTableShake.GetValueOnGameThread());
	return FRotator(FMath::Sin(Clock * 43.0 + 0.7) * A, FMath::Sin(Clock * 57.0 + 2.9) * A, FMath::Sin(Clock * 39.0) * A * 0.5);
}

FLinearColor TableJuice::Rainbow(double Seconds, float CyclesPerSecond)
{
	const float Hue = static_cast<float>(FMath::Fmod(Seconds * CyclesPerSecond, 1.0)) * 360.0f;
	return FLinearColor(Hue, 0.85f, 1.0f).HSVToLinearRGB();
}

FLinearColor TableJuice::PoolBallColor(int32 Ball)
{
	static const FLinearColor Colors[8] =
	{
		FLinearColor(1.00f, 0.80f, 0.05f),   // 1, 9   yellow
		FLinearColor(0.10f, 0.30f, 1.00f),   // 2, 10  blue
		FLinearColor(1.00f, 0.08f, 0.05f),   // 3, 11  red
		FLinearColor(0.55f, 0.15f, 0.90f),   // 4, 12  purple
		FLinearColor(1.00f, 0.42f, 0.05f),   // 5, 13  orange
		FLinearColor(0.10f, 0.85f, 0.30f),   // 6, 14  green
		FLinearColor(0.80f, 0.12f, 0.20f),   // 7, 15  maroon
		FLinearColor(0.90f, 0.90f, 1.00f),   // 8      black: its sparks are white
	};
	if (Ball <= 0)
	{
		return FLinearColor::White;
	}
	return Colors[(Ball - 1) % 8];   // 8 lands on white; 9..15 on their solids' colours
}
