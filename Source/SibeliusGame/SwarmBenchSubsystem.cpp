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
#include "Engine/HitResult.h"

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

static FAutoConsoleCommandWithWorldAndArgs GSwarmRidge(
	TEXT("swarm.Ridge"),
	TEXT("Stand N Refusers on the hillside in an arc facing you. "
		 "Args: [count=150] [radiusMetres=300] [arcDegrees=90] [ranks=6]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (USwarmBenchSubsystem* S = Get(World))
			{
				const int32 N       = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 150;
				const float Radius  = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 300.0f;
				const float Arc     = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 90.0f;
				const int32 Ranks   = Args.Num() > 3 ? FCString::Atoi(*Args[3]) : 6;
				const int32 Landed  = S->SpawnRidge(N, Radius, Arc, Ranks);
				UE_LOG(LogSibeliusGame, Display,
					TEXT("[SwarmBench] ridge: %d of %d stood up at %.0f m over %.0f deg in %d ranks."),
					Landed, N, Radius, Arc, Ranks);
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

/* ---------------------------------------------------------------- the composition

   SWARM_PLAN step 3: "put 150 Gideons on the ridge and look at it. Not a bench run; a
   composition." This is that command, and every choice in it is about the picture.

   AN ARC, NOT A RING. An army comes from somewhere. Ringing the player is a fence, and
   a fence reads as a spawn debug rather than a threat - you are surrounded by a test.
   Ninety degrees of horizon, centred on wherever you are already looking, is an army.

   RANKS, BECAUSE DEPTH IS WHAT MAKES A CROWD. 150 in a single line at 300 metres is a
   picket fence: evenly spaced dots with sky between them. The same 150 in six ranks
   overlap from a low camera and become a mass. This is the knob most likely to decide
   the answer, which is why it is an argument.

   FACING THE PLAYER. An army with its back turned is scenery.

   A LINE TRACE DOWN, NOT THE NAVMESH - and this is the exact opposite of the call
   SpawnOne makes fifty lines up, so it is worth saying why. The bench spawns on the flat
   floor and wants the navmesh, because the navmesh IS the floor there. The hills are
   sixty metres tall and steeper than the walkable slope limit, so there is no navmesh on
   them at all: ProjectPointToNavigation would either fail outright or snap every demon
   back down onto the meadow, which is precisely the shot this command exists to avoid.

   DETERMINISTIC JITTER. A fixed seed means re-running at a different radius shows you the
   RADIUS changing, not a fresh random scatter. Comparing two compositions that differ in
   two ways at once tells you nothing, which is the same discipline the bench ladder uses.
*/
int32 USwarmBenchSubsystem::SpawnRidge(int32 HowMany, float RadiusMetres, float ArcDegrees, int32 Ranks)
{
	UWorld* World = GetWorld();
	const TSubclassOf<APawn> Class = FindRefuserClass();
	if (!World || !Class)
	{
		UE_LOG(LogSibeliusGame, Error,
			TEXT("[SwarmBench] no ARefuserSpawner with a RefuserClass in this level - nothing to stand up."));
		return 0;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || HowMany <= 0)
	{
		return 0;
	}

	const FVector Centre = Player->GetActorLocation();
	const float Bearing = Player->GetActorRotation().Yaw;   // point, then type the command
	Ranks = FMath::Clamp(Ranks, 1, HowMany);

	const float RadiusCm = FMath::Max(RadiusMetres, 10.0f) * 100.0f;
	const float RankStepCm = 900.0f;   // about 9 m between ranks: they overlap without merging

	FRandomStream Rand(20260827);
	int32 Landed = 0;

	for (int32 i = 0; i < HowMany; ++i)
	{
		const int32 Rank = i % Ranks;
		const int32 IndexInRank = i / Ranks;
		const int32 PerRank = FMath::Max(1, FMath::DivideAndRoundUp(HowMany, Ranks));

		// Spread across the arc, with each rank offset half a step so the ranks do not
		// line up into columns. Columns read as a parade; offset ranks read as a horde.
		const float T = PerRank > 1 ? static_cast<float>(IndexInRank) / (PerRank - 1) : 0.5f;
		const float Offset = (Rank % 2) ? (0.5f / FMath::Max(1, PerRank - 1)) : 0.0f;
		const float Deg = Bearing + (T + Offset - 0.5f) * ArcDegrees + Rand.FRandRange(-1.5f, 1.5f);
		const float Rad = FMath::DegreesToRadians(Deg);

		const float R = RadiusCm + Rank * RankStepCm + Rand.FRandRange(-250.0f, 250.0f);
		FVector Where = Centre + FVector(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, 0.0f);

		// Find the hillside. 200 m of headroom each way covers a 60 m rim with margin.
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(SwarmRidge), false, Player);
		const FVector From = Where + FVector(0, 0, 20000.0f);
		const FVector To   = Where - FVector(0, 0, 20000.0f);
		if (World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q))
		{
			Where.Z = Hit.ImpactPoint.Z + SpawnZNudge;
		}
		else
		{
			continue;   // off the landscape entirely; a demon in the void helps nobody
		}

		const FRotator Facing = (Centre - Where).GetSafeNormal2D().Rotation();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AActor* Made = World->SpawnActor<AActor>(Class, Where, Facing, Params))
		{
			Spawned.Add(Made);
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
