// TableJuice.h
//
// THE SHOW ROUND THE GAMES — sparks, camera kicks and confetti for the living-room pinball
// and pool table and the bowling alley (Walt, 2026-11-10: "it must be more attractive").
//
// The sims decide what happened; the tables already turn that into sound (TableSounds.h).
// This is the same idea for the eye: a table hands an event to one of these and gets a burst
// of light, a nudge of the camera, a fall of confetti. Nothing here changes a score or a ball.
//
// NO PARTICLE ASSETS. Every spark is a small glowing mesh, moved by a few lines of arithmetic,
// so there is no Niagara system to author, license or leave out of a cook. The glow is
// M_GenesisGlow (ours, tracked), the AI city's own light.
//
// EVERY SPARK IS A DEFAULT SUBOBJECT OF ITS TABLE, built in the table's constructor. A mesh
// component made at runtime can sit in the world fully set up and never draw (the SIB-44
// book rain); one made in a constructor always does.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/** One burst: where, how many, how fast, what colour. World space throughout. */
struct FTableBurst
{
	FVector Origin = FVector::ZeroVector;
	FVector Bias = FVector::UpVector;   // the way most of them go
	int32 Count = 8;
	float Speed = 120.0f;               // cm/s; each spark gets 50-100% of it
	float Spread = 1.0f;                // 0 = straight along Bias, 1 = anywhere in the half-space round it
	float Life = 0.45f;                 // seconds
	float Size = 0.6f;                  // cm across
	FLinearColor Color = FLinearColor(1.0f, 0.8f, 0.4f);
	float Glow = 25.0f;
	float Scatter = 0.0f;               // cm: how far from Origin a spark may start
};

/** A fixed pool of sparks (or confetti flakes) belonging to one table. */
class FTableSparks
{
public:
	/** CONSTRUCTOR ONLY. Makes Count hidden mesh components on Owner, under Parent. */
	void Build(AActor* Owner, USceneComponent* Parent, const TCHAR* Prefix, int32 Count, bool bInConfetti);

	/** BeginPlay: gives each spark its own material so each can have its own colour. */
	void Begin();

	void Burst(const FTableBurst& B);
	void Tick(float DeltaSeconds);
	void Clear();

	float Gravity = 500.0f;   // cm/s2
	float Drag = 1.2f;        // per second

private:
	struct FSpark
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Paint;
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		FVector Axis = FVector::UpVector;   // confetti: what it tumbles about
		float Spin = 0.0f;                  // confetti: rad/s
		float Age = 0.0f;
		float Life = 0.0f;                  // 0 = free
		float Size = 0.5f;
	};

	TArray<FSpark> Pool;
	int32 Next = 0;
	int32 Live = 0;
	bool bConfetti = false;
};

/** Camera shake and zoom punch, added on top of wherever a table's camera already is. */
class FTableCameraKick
{
public:
	void Shake(float Amount);      // 0..1; adds, and fades in about half a second
	void Punch(float Degrees);     // field of view; negative leans in
	void Tick(float DeltaSeconds);
	void Reset();

	FVector Offset(float MaxCm) const;
	FRotator Turn(float MaxDegrees) const;
	float Fov() const { return FovNow; }

private:
	float Trauma = 0.0f;
	float FovNow = 0.0f;
	double Clock = 0.0;
};

namespace TableJuice
{
	/** A hue round the wheel, full brightness: for lights that celebrate. */
	FLinearColor Rainbow(double Seconds, float CyclesPerSecond = 0.6f);

	/** The colour of a pool ball, 1..15 (the stripes take their solid's colour). */
	FLinearColor PoolBallColor(int32 Ball);
}
