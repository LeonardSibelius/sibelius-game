// BookRain.cpp — P0/P1 (June 13, 2026). Source/SibeliusGame/ (RUNTIME module).
// Pooled book downpour. Each book is a lightweight ABookRainBook actor (mesh = constructor default
// subobject, so it renders through the normal per-actor path); the pool recycles them, no destroy (SS1).

#include "BookRain.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SauceCauldron.h"
#include "Engine/World.h"

// ============================ ABookRainBook ============================

ABookRainBook::ABookRainBook()
{
	PrimaryActorTick.bCanEverTick = false;   // ABookRain drives the transform

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // cosmetic only
	Mesh->SetCastShadow(false);
}

void ABookRainBook::SetBookMesh(UStaticMesh* InMesh)
{
	if (Mesh && InMesh)
	{
		Mesh->SetStaticMesh(InMesh);
	}
}

void ABookRainBook::Reveal(UStaticMesh* InMesh)
{
	if (Mesh)
	{
		// Order per the render-proxy fix: a valid mesh on a Movable, registered component, made
		// visible, then a render-state kick so the scene proxy actually builds this launch.
		if (InMesh)
		{
			Mesh->SetStaticMesh(InMesh);
			// Explicitly apply the mesh's own materials so the book shows SM_Book_01's material,
			// not a gray default — guards against empty/null component material slots after a
			// runtime SetStaticMesh.
			const int32 NumMats = InMesh->GetStaticMaterials().Num();
			for (int32 m = 0; m < NumMats; ++m)
			{
				Mesh->SetMaterial(m, InMesh->GetMaterial(m));
			}
		}
		Mesh->SetVisibility(true, /*bPropagateToChildren=*/true);
		Mesh->SetHiddenInGame(false);
		Mesh->MarkRenderStateDirty();
	}
	SetActorHiddenInGame(false);
}

void ABookRainBook::Retire()
{
	// Once, on reaching the mouth — never per tick.
	SetActorHiddenInGame(true);
}

// ============================== ABookRain ==============================

ABookRain::ABookRain()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ABookRain::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Build the pool once (SS1: recycle, never destroy). Books are transient + owned by us, so
	// they never enter a save and die with the level (SS3).
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;

	Pool.Reset();
	Pool.Reserve(PoolSize);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		ABookRainBook* BookActor = World->SpawnActor<ABookRainBook>(ABookRainBook::StaticClass(), FTransform::Identity, Params);
		if (!BookActor)
		{
			continue;
		}
		BookActor->SetActorEnableCollision(false);
		BookActor->SetActorHiddenInGame(true);
		if (BookMeshes.Num() > 0 && BookMeshes[0])
		{
			BookActor->SetBookMesh(BookMeshes[0]);
		}
		FFallingBook Book;
		Book.Actor = BookActor;
		Pool.Add(Book);
	}

	UE_LOG(LogTemp, Display, TEXT("[BookRain] %s built pool of %d book actor(s) (sources=%d, meshes=%d)."),
		*GetName(), Pool.Num(), SourceLocations.Num(), BookMeshes.Num());
}

int32 ABookRain::FindFreeIndex()
{
	const int32 N = Pool.Num();
	if (N == 0)
	{
		return INDEX_NONE;
	}
	// Round-robin from the last launched index so a book that just retired isn't relaunched on the
	// very next spawn — that rapid hide/show cycle is what read as the ~1Hz Actor-Hidden flash.
	for (int32 Step = 1; Step <= N; ++Step)
	{
		const int32 i = (LastSpawnIndex + Step) % N;
		if (!Pool[i].bActive)
		{
			LastSpawnIndex = i;
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
	if (!Book.Actor)
	{
		return;
	}

	const FTransform& Xf = GetActorTransform();
	const FVector SourceRel = SourceLocations[FMath::RandRange(0, SourceLocations.Num() - 1)];
	const FVector StartW = Xf.TransformPosition(SourceRel);
	const FVector EndW   = Xf.TransformPosition(MouthLocation);
	const FVector MidW   = (StartW + EndW) * 0.5f + FVector(0, 0, ArcHeight);

	UStaticMesh* Mesh = BookMeshes[FMath::RandRange(0, BookMeshes.Num() - 1)];

	Book.Start    = StartW;
	Book.End      = EndW;
	Book.Control  = MidW;
	Book.Elapsed  = 0.f;
	Book.Duration = FMath::Max(0.1f, FallDuration + FMath::FRandRange(-FallDurationJitter, FallDurationJitter));
	Book.SpinAxis = FRotator(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * SpinRateDeg;
	Book.bActive  = true;

	const float S = FMath::FRandRange(ScaleRange.X, ScaleRange.Y);
	Book.Actor->SetActorScale3D(FVector(S));
	Book.Actor->SetActorLocation(StartW);
	Book.Actor->SetActorRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f));
	Book.Actor->Reveal(Mesh);   // mesh + show + build the scene proxy this launch (the render fix)
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
		if (!Book.bActive || !Book.Actor)
		{
			continue;
		}

		Book.Elapsed += DeltaSeconds;
		const float T = Book.Elapsed / Book.Duration;

		if (T >= 1.f)
		{
			// Arrived: vanish + recycle (+ optional feed).
			Book.bActive = false;
			Book.Actor->Retire();   // hide once, here only — never per tick
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

		Book.Actor->SetActorLocation(Pos);
		Book.Actor->AddActorLocalRotation(Book.SpinAxis * DeltaSeconds);
	}
}
