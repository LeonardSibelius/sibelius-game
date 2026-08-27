// SwarmBenchSubsystem.h — how many real Refusers can this game actually afford?
//
// THE QUESTION THIS EXISTS TO ANSWER. The Swarm power wants armies of demons, and the
// plan for it is a HYBRID: a cheap instanced horde in the distance, and real Actor
// Refusers near the player that the existing verbs still work on — slap them, refactor
// them into a fox, see their code. That split only works if we know where the Actor
// half runs out of road, and nobody knows that number. Guessing it would mean designing
// the whole feature around a fiction.
//
// So this spawns real Refusers in rungs, measures the frame cost at each, and stops
// when the frame time crosses a budget. The output is one number: how many the scene
// carries before it hurts. Everything about the hybrid's design follows from it.
//
// THE FRAME CAP WOULD MAKE THIS LIE. With vsync or t.MaxFPS on, every rung reads 16.7ms
// until the machine finally falls off a cliff, and the bench would report "200 demons
// are free" right up to the moment they are not. StartBench turns the cap and frame
// smoothing off for the duration and puts them back afterwards; the report records
// whether it managed it. A bench that cannot control for this is worse than no bench,
// because it produces a confident wrong number.
//
// USAGE — console, in PIE or a packaged build, standing where a fight would happen:
//
//     swarm.Bench            run the ladder, write Saved/swarm_bench.json
//     swarm.Spawn 50         add 50 by hand and go look at them
//     swarm.Clear            remove everything the bench spawned
//
// IT USES THE LEVEL'S OWN REFUSER. The pawn class is read off any ARefuserSpawner in
// the level rather than named here, so the bench measures whatever the game actually
// fights, and it keeps measuring the right thing if that Blueprint is ever swapped.
// Run it in a level that has a spawner - the forests - or it has nothing to spawn.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SwarmBenchSubsystem.generated.h"

/** One rung of the ladder: N demons, and what they cost. */
USTRUCT()
struct FSwarmBenchRung
{
	GENERATED_BODY()

	UPROPERTY() int32 Count = 0;
	UPROPERTY() double AvgMs = 0.0;
	UPROPERTY() double WorstMs = 0.0;
	UPROPERTY() int32 Frames = 0;
};

UENUM()
enum class ESwarmBenchPhase : uint8
{
	Idle,
	Settling,    // spawned, but AI and animation have not caught up yet
	Measuring,
};

UCLASS()
class SIBELIUSGAME_API USwarmBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/** Run the ladder from an empty scene up to MaxCount or the frame budget. */
	void StartBench(int32 MaxCount);

	/** Add HowMany Refusers around the player. Returns how many actually landed. */
	int32 SpawnMore(int32 HowMany);

	/** Destroy everything this subsystem spawned. Never touches level-placed Refusers. */
	void ClearAll();

private:
	/** Ring the player rather than a fixed point: the cost that matters is the cost of
	 *  demons you can SEE, and a pile behind the camera measures nothing. */
	bool SpawnOne(int32 IndexForSpread, int32 TotalWanted);

	TSubclassOf<APawn> FindRefuserClass() const;
	void BeginRung();
	void FinishRung();
	void FinishBench(const TCHAR* Why);
	void WriteReport(const TCHAR* Why) const;

	/** Only what we made. Level-placed Refusers are somebody else's actors. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Spawned;

	UPROPERTY()
	TArray<FSwarmBenchRung> Rungs;

	ESwarmBenchPhase Phase = ESwarmBenchPhase::Idle;

	/* THE LADDER. Starts at 0 so the report carries a baseline — a rung is only
	   meaningful against the empty scene it was added to, and "50 demons cost 4ms" is a
	   different sentence depending on whether the room was already at 8 or at 30. */
	static constexpr int32 Ladder[] = { 0, 10, 25, 50, 100, 150, 200, 300 };
	int32 RungIndex = 0;
	int32 MaxRequested = 300;

	/** Seconds of grace before measuring. Navmesh queries, controller possession and
	 *  animation initialisation all land in the first second and none of it is the
	 *  steady-state cost we are asking about. */
	static constexpr float SettleSeconds = 2.0f;
	static constexpr float MeasureSeconds = 4.0f;

	/** Stop when a rung's average crosses this. 33.3ms is 30fps — past there the answer
	 *  is already "too many" and the next rung only tells us how much too many. */
	static constexpr double BudgetMs = 33.3;

	float PhaseTime = 0.0f;
	double AccumMs = 0.0;
	double WorstMs = 0.0;
	int32 FrameCount = 0;

	/** Frame-cap state to put back when we are done. */
	bool bRestoreSmoothFrameRate = false;
	bool bCapWasChanged = false;
};
