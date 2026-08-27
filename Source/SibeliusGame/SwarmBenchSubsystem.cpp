// SwarmBenchSubsystem.cpp — see header.

#include "SwarmBenchSubsystem.h"

#include "RefuserSpawner.h"
#include "SibeliusGame.h"

#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"

constexpr int32 USwarmBenchSubsystem::Ladder[];

namespace
{
	/** Demons land in an annulus around the player: close enough to be rendered and
	 *  animated at full cost, far enough not to spawn inside the camera. */
	constexpr float MinSpawnRadius = 350.0f;
	constexpr float MaxSpawnRadius = 2500.0f;
	constexpr float NavProjectExtentZ = 500.0f;
	constexpr float SpawnZNudge = 20.0f;

	USwarmBenchSubsystem* Get(UWorld* World)
	{
		return World ? World->GetSubsystem<USwarmBenchSubsystem>() : nullptr;
	}
}

// ---------------------------------------------------------------- console commands

static FAutoConsoleCommandWithWorldAndArgs GSwarmBench(
	TEXT("swarm.Bench"),
	TEXT("Ramp real Refusers up in rungs and report the frame cost of each. Optional max count."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (USwarmBenchSubsystem* S = Get(World))
			{
				const int32 Max = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 300;
				S->StartBench(Max);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GSwarmSpawn(
	TEXT("swarm.Spawn"),
	TEXT("Spawn N real Refusers around the player, so you can go and look at them."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (USwarmBenchSubsystem* S = Get(World))
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 25;
				const int32 Landed = S->SpawnMore(N);
				UE_LOG(LogSibeliusGame, Display, TEXT("[SwarmBench] spawned %d of %d requested."), Landed, N);
			}
		}));

static FAutoConsoleCommandWithWorld GSwarmClear(
	TEXT("swarm.Clear"),
	TEXT("Destroy every Refuser the bench spawned. Level-placed ones are left alone."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			if (USwarmBenchSubsystem* S = Get(World))
			{
				S->ClearAll();
			}
		}));

// ---------------------------------------------------------------- lifecycle

TStatId USwarmBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USwarmBenchSubsystem, STATGROUP_Tickables);
}

void USwarmBenchSubsystem::Deinitialize()
{
	// A bench that leaks two hundred pawns into the next PIE session is worse than no
	// bench. This runs on world teardown whether or not the ladder finished.
	ClearAll();
	Super::Deinitialize();
}

// ---------------------------------------------------------------- spawning

TSubclassOf<APawn> USwarmBenchSubsystem::FindRefuserClass() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// Read it off the level rather than naming an asset here: the bench then measures
	// whatever this game actually fights, and keeps doing so if the Blueprint changes.
	for (TActorIterator<ARefuserSpawner> It(World); It; ++It)
	{
		if (It->RefuserClass)
		{
			return It->RefuserClass;
		}
	}
	return nullptr;
}

bool USwarmBenchSubsystem::SpawnOne(int32 IndexForSpread, int32 TotalWanted)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const TSubclassOf<APawn> Class = FindRefuserClass();
	if (!Class)
	{
		return false;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player)
	{
		return false;
	}

	/* GOLDEN-ANGLE SPIRAL, not a random scatter. Random points clump, and a clump reads
	   as a cheaper scene than the same count spread out — fewer draw calls survive
	   culling, more of them occlude each other. An even spread is the honest case. */
	const float Golden = 137.508f;
	const float Angle = FMath::DegreesToRadians(Golden * IndexForSpread);
	const float T = TotalWanted > 1 ? FMath::Sqrt(static_cast<float>(IndexForSpread) / TotalWanted) : 0.5f;
	const float Radius = FMath::Lerp(MinSpawnRadius, MaxSpawnRadius, T);

	FVector Where = Player->GetActorLocation()
		+ FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);

	// Same navmesh projection the real spawner uses, for the same reason: without it
	// they spawn at player height and fall out of the sky, and a falling pawn is not
	// the cost of a fighting one.
	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
	{
		FNavLocation Projected;
		const FVector Extent(MaxSpawnRadius, MaxSpawnRadius, NavProjectExtentZ);
		if (Nav->ProjectPointToNavigation(Where, Projected, Extent))
		{
			Where = Projected.Location;
		}
	}
	Where.Z += SpawnZNudge;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* Made = World->SpawnActor<AActor>(Class, Where, FRotator::ZeroRotator, Params);
	if (!Made)
	{
		return false;
	}
	Spawned.Add(Made);
	return true;
}

int32 USwarmBenchSubsystem::SpawnMore(int32 HowMany)
{
	if (!FindRefuserClass())
	{
		UE_LOG(LogSibeliusGame, Error,
			TEXT("[SwarmBench] no ARefuserSpawner with a RefuserClass in this level - nothing to spawn. "
				 "Run this where the fights are."));
		return 0;
	}
	const int32 Base = Spawned.Num();
	int32 Landed = 0;
	for (int32 i = 0; i < HowMany; ++i)
	{
		if (SpawnOne(Base + i, Base + HowMany))
		{
			++Landed;
		}
	}
	return Landed;
}

void USwarmBenchSubsystem::ClearAll()
{
	for (AActor* A : Spawned)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	const int32 N = Spawned.Num();
	Spawned.Reset();
	if (N > 0)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[SwarmBench] cleared %d spawned Refuser(s)."), N);
	}
}

// ---------------------------------------------------------------- the ladder

void USwarmBenchSubsystem::StartBench(int32 MaxCount)
{
	if (Phase != ESwarmBenchPhase::Idle)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[SwarmBench] already running."));
		return;
	}
	if (!FindRefuserClass())
	{
		UE_LOG(LogSibeliusGame, Error,
			TEXT("[SwarmBench] no ARefuserSpawner with a RefuserClass in this level. "
				 "Load a level that has one - the forests - and stand where a fight happens."));
		return;
	}

	/* TAKE THE FRAME CAP OFF, or the whole exercise is theatre. Capped at 60 every rung
	   reads 16.7ms until the machine finally falls off a cliff, and the bench cheerfully
	   reports that 200 demons are free right up to the frame they are not. */
	if (GEngine)
	{
		bRestoreSmoothFrameRate = GEngine->bSmoothFrameRate;
		GEngine->bSmoothFrameRate = false;
	}
	if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFps->Set(0.0f, ECVF_SetByConsole);
		bCapWasChanged = true;
	}

	ClearAll();
	Rungs.Reset();
	RungIndex = 0;
	MaxRequested = FMath::Max(0, MaxCount);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[SwarmBench] starting. Settle %.0fs + measure %.0fs per rung, budget %.1fms. "
			 "Stand still and do not move the camera."), SettleSeconds, MeasureSeconds, BudgetMs);

	BeginRung();
}

void USwarmBenchSubsystem::BeginRung()
{
	const int32 Target = Ladder[RungIndex];
	const int32 Need = Target - Spawned.Num();
	if (Need > 0)
	{
		SpawnMore(Need);
	}

	Phase = ESwarmBenchPhase::Settling;
	PhaseTime = 0.0f;
	AccumMs = 0.0;
	WorstMs = 0.0;
	FrameCount = 0;

	UE_LOG(LogSibeliusGame, Display, TEXT("[SwarmBench] rung %d: %d live, settling..."),
		RungIndex, Spawned.Num());
}

void USwarmBenchSubsystem::FinishRung()
{
	FSwarmBenchRung R;
	R.Count = Spawned.Num();
	R.Frames = FrameCount;
	R.AvgMs = FrameCount > 0 ? AccumMs / FrameCount : 0.0;
	R.WorstMs = WorstMs;
	Rungs.Add(R);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[SwarmBench]   %4d demons -> avg %6.2f ms (%5.1f fps), worst %6.2f ms, %d frames"),
		R.Count, R.AvgMs, R.AvgMs > 0.0 ? 1000.0 / R.AvgMs : 0.0, R.WorstMs, R.Frames);

	if (R.AvgMs > BudgetMs)
	{
		FinishBench(TEXT("frame budget exceeded"));
		return;
	}
	if (Spawned.Num() >= MaxRequested)
	{
		FinishBench(TEXT("reached the requested maximum"));
		return;
	}
	if (++RungIndex >= UE_ARRAY_COUNT(Ladder))
	{
		FinishBench(TEXT("ladder complete - the scene carried every rung"));
		return;
	}
	BeginRung();
}

void USwarmBenchSubsystem::FinishBench(const TCHAR* Why)
{
	Phase = ESwarmBenchPhase::Idle;

	// Put the frame cap back before anything else, so a player who runs this mid-session
	// does not silently keep an uncapped frame rate.
	if (GEngine)
	{
		GEngine->bSmoothFrameRate = bRestoreSmoothFrameRate;
	}
	if (bCapWasChanged)
	{
		if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
		{
			MaxFps->Set(0.0f, ECVF_SetByConsole);   // engine default is uncapped
		}
		bCapWasChanged = false;
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[SwarmBench] done: %s."), Why);
	WriteReport(Why);
	ClearAll();
}

void USwarmBenchSubsystem::WriteReport(const TCHAR* Why) const
{
	// JSON into Saved/, the same shape every other tool in this project reports in, so
	// the numbers can be read back without anyone transcribing them out of a log.
	FString J = TEXT("{\n");
	J += FString::Printf(TEXT("  \"stopped_because\": \"%s\",\n"), Why);
	J += FString::Printf(TEXT("  \"settle_seconds\": %.1f,\n"), SettleSeconds);
	J += FString::Printf(TEXT("  \"measure_seconds\": %.1f,\n"), MeasureSeconds);
	J += FString::Printf(TEXT("  \"budget_ms\": %.1f,\n"), BudgetMs);
	J += FString::Printf(TEXT("  \"frame_cap_removed\": %s,\n"), bCapWasChanged ? TEXT("false") : TEXT("true"));
	J += TEXT("  \"rungs\": [\n");
	for (int32 i = 0; i < Rungs.Num(); ++i)
	{
		const FSwarmBenchRung& R = Rungs[i];
		J += FString::Printf(
			TEXT("    { \"count\": %d, \"avg_ms\": %.3f, \"fps\": %.1f, \"worst_ms\": %.3f, \"frames\": %d }%s\n"),
			R.Count, R.AvgMs, R.AvgMs > 0.0 ? 1000.0 / R.AvgMs : 0.0, R.WorstMs, R.Frames,
			i + 1 < Rungs.Num() ? TEXT(",") : TEXT(""));
	}
	J += TEXT("  ]\n}\n");

	const FString Out = FPaths::ProjectSavedDir() / TEXT("swarm_bench.json");
	if (FFileHelper::SaveStringToFile(J, *Out))
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[SwarmBench] wrote %s"), *Out);
	}
	else
	{
		UE_LOG(LogSibeliusGame, Error, TEXT("[SwarmBench] could not write %s"), *Out);
	}
}

void USwarmBenchSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Phase == ESwarmBenchPhase::Idle)
	{
		return;
	}

	PhaseTime += DeltaSeconds;

	if (Phase == ESwarmBenchPhase::Settling)
	{
		if (PhaseTime >= SettleSeconds)
		{
			Phase = ESwarmBenchPhase::Measuring;
			PhaseTime = 0.0f;
		}
		return;
	}

	// Measuring. DeltaSeconds is the honest wall-clock cost of the last frame, which is
	// the number that decides whether this is affordable.
	const double Ms = DeltaSeconds * 1000.0;
	AccumMs += Ms;
	WorstMs = FMath::Max(WorstMs, Ms);
	++FrameCount;

	if (PhaseTime >= MeasureSeconds)
	{
		FinishRung();
	}
}
