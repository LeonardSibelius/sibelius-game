// BookRain.cpp — P0/P1 (June 13, 2026). Source/SibeliusGame/ (RUNTIME module).
// Pooled book downpour. No per-book spawn/destroy (SS1) — components are created once in BeginPlay and recycled.

#include "BookRain.h"
#include "Components/StaticMeshComponent.h"
#include "SauceCauldron.h"

ABookRain::ABookRain()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ABookRain::BeginPlay()
{
	Super::BeginPlay();

	// Build the pool once (SS1: recycle, never destroy).
	Pool.Reset();
	Pool.Reserve(PoolSize);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		FFallingBook Book;
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetupAttachment(SceneRoot);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // cosmetic only
		Comp->SetCastShadow(false);
		Comp->RegisterComponent();
		Comp->SetVisibility(false);
		if (BookMeshes.Num() > 0 && BookMeshes[0])
		{
			Comp->SetStaticMesh(BookMeshes[0]);
		}
		Book.Comp = Comp;
		Pool.Add(Book);
	}
}

int32 ABookRain::FindFreeIndex() const
{
	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		if (!Pool[i].bActive)
		{
			return i;
		}
	}
	return INDEX_NONE;   // cap reached; skip this spawn
}

void ABookRain::SpawnOne()
{
	if (SourceLocations.Num() == 0 || BookMeshes.Num() == 0)
	{
		return;
	}
	const int32 Idx = FindFreeIndex();
	if (Idx == INDEX_NONE)
	{
		return;
	}

	FFallingBook& Book = Pool[Idx];
	if (!Book.Comp)
	{
		return;
	}

	const FTransform& Xf = GetActorTransform();
	const FVector SourceRel = SourceLocations[FMath::RandRange(0, SourceLocations.Num() - 1)];
	const FVector StartW = Xf.TransformPosition(SourceRel);
	const FVector EndW   = Xf.TransformPosition(MouthLocation);
	const FVector MidW   = (StartW + EndW) * 0.5f + FVector(0, 0, ArcHeight);

	UStaticMesh* Mesh = BookMeshes[FMath::RandRange(0, BookMeshes.Num() - 1)];
	if (Mesh)
	{
		Book.Comp->SetStaticMesh(Mesh);
	}

	Book.Start    = StartW;
	Book.End      = EndW;
	Book.Control  = MidW;
	Book.Elapsed  = 0.f;
	Book.Duration = FMath::Max(0.1f, FallDuration + FMath::FRandRange(-FallDurationJitter, FallDurationJitter));
	Book.SpinAxis = FRotator(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * SpinRateDeg;
	Book.bActive  = true;

	const float S = FMath::FRandRange(ScaleRange.X, ScaleRange.Y);
	Book.Comp->SetWorldScale3D(FVector(S));
	Book.Comp->SetWorldLocation(StartW);
	Book.Comp->SetWorldRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f));
	Book.Comp->SetVisibility(true);
}

void ABookRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Throttled spawning.
	SpawnAccum += DeltaSeconds;
	while (SpawnAccum >= SpawnInterval)
	{
		SpawnAccum -= SpawnInterval;
		SpawnOne();
	}

	// Advance active books along a quadratic arc; recycle at the mouth.
	for (FFallingBook& Book : Pool)
	{
		if (!Book.bActive || !Book.Comp)
		{
			continue;
		}

		Book.Elapsed += DeltaSeconds;
		const float T = Book.Elapsed / Book.Duration;

		if (T >= 1.f)
		{
			// Arrived: vanish + recycle (+ optional feed).
			Book.bActive = false;
			Book.Comp->SetVisibility(false);
			if (Cauldron && FeedPerBook > 0.f)
			{
				Cauldron->FeedSauce(FeedPerBook);
			}
			continue;
		}

		// Quadratic Bezier: (1-t)^2*Start + 2(1-t)t*Control + t^2*End
		const float OneMinusT = 1.f - T;
		const FVector Pos =
			(OneMinusT * OneMinusT) * Book.Start +
			(2.f * OneMinusT * T)   * Book.Control +
			(T * T)                 * Book.End;

		Book.Comp->SetWorldLocation(Pos);
		Book.Comp->AddLocalRotation(Book.SpinAxis * DeltaSeconds);
	}
}
