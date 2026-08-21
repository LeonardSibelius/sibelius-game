// SibeliusSmokeTestCommandlet.cpp - SIB-19 headless smoke test for L_Office_v02 (v0.2 CP1).

#include "SibeliusSmokeTestCommandlet.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Engine/MapBuildDataRegistry.h"
#include "LegacyMachine.h"      // MACHINE_PLAN §8 — the legacy system must exist and be broken

#if WITH_EDITOR
#include "FileHelpers.h" // UEditorLoadingAndSavingUtils (editor-only)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSmokeTest, Log, All);

// Named (not anonymous) namespace so these helpers (FResult / DefaultMapPackage /
// ParseMapArg) don't collide with the identically-named ones in the sibling smoke
// commandlets when unity-build regroups these TUs. Mirrors RefuserSmokeTestNS et al.
namespace SibeliusSmokeTestNS
{
const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");

// World Settings -> GameMode Override = BP_FirstPersonGameMode (SIB-18).
const FString ExpectedGameModeSubstring = TEXT("FirstPerson");

// B12: full QuadArt layout, no cleanup (SIB-17 deferred). ~1,074 actors today.
const int32 MinActorCount = 1000;
const int32 MaxActorCount = 1150;

// House bounding box (world units). Spawn is the living room (Walt 2026-08-19):
// facing the hallway, poker door behind on the sliding glass. Old office spawn
// was ~(-1832, 10365, 414). Living-room known-good is ~(-2050, 9450, 158)
// (carpet Z~62 + capsule half-height). Z=130 fell through the floor.
const FBox OfficeBounds(
FVector(-2600.0, 8900.0, 50.0),
FVector(-1300.0, 11500.0, 800.0));

struct FResult
{
int32 Failures = 0;
void Check(bool bCondition, const FString& Label)
{
if (bCondition)
{
UE_LOG(LogSmokeTest, Display, TEXT("  [PASS] %s"), *Label);
}
else
{
++Failures;
UE_LOG(LogSmokeTest, Error, TEXT("  [FAIL] %s"), *Label);
}
}
void Warn(bool bCondition, const FString& Label)
{
UE_LOG(LogSmokeTest, Display, TEXT("  [%s] %s"),
bCondition ? TEXT("PASS") : TEXT("WARN"), *Label);
}
};

FString ParseMapArg(const FString& Params)
{
FString MapOverride;
if (FParse::Value(*Params, TEXT("map="), MapOverride) && !MapOverride.IsEmpty())
{
return MapOverride;
}
return DefaultMapPackage;
}
}

USibeliusSmokeTestCommandlet::USibeliusSmokeTestCommandlet()
{
IsClient = false;
IsServer = false;
IsEditor = true;
LogToConsole = true;
}

int32 USibeliusSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
UE_LOG(LogSmokeTest, Error, TEXT("SibeliusSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
return 1;
#else
// Scoped here (function body, not file scope) so the namespace doesn't leak into
// other TUs under a unity build and re-introduce the collision we just fixed.
using namespace SibeliusSmokeTestNS;

const FString MapPackage = ParseMapArg(Params);
UE_LOG(LogSmokeTest, Display, TEXT("=== SIB-19 smoke test: %s ==="), *MapPackage);

FResult R;

UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPackage);
R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
if (!World)
{
UE_LOG(LogSmokeTest, Error, TEXT("=== SMOKE TEST FAILED: could not load map. ==="));
return 1;
}

AWorldSettings* WS = World->GetWorldSettings();
R.Check(WS != nullptr, TEXT("World Settings present"));

TSubclassOf<AGameModeBase> GameModeClass = WS ? WS->DefaultGameMode : nullptr;
R.Check(GameModeClass != nullptr, TEXT("GameMode Override is set"));

if (GameModeClass)
{
const FString GameModeName = GameModeClass->GetName();
R.Check(GameModeName.Contains(ExpectedGameModeSubstring),
FString::Printf(TEXT("GameMode Override is the FP GameMode (got %s)"), *GameModeName));

const AGameModeBase* GameModeCDO = GameModeClass->GetDefaultObject<AGameModeBase>();
UClass* PawnClass = GameModeCDO ? GameModeCDO->DefaultPawnClass.Get() : nullptr;
R.Check(PawnClass != nullptr,
FString::Printf(TEXT("GameMode DefaultPawnClass resolves (%s)"),
PawnClass ? *PawnClass->GetName() : TEXT("null")));
}

int32 PlayerStartCount = 0;
FVector PlayerStartLoc = FVector::ZeroVector;
for (TActorIterator<APlayerStart> It(World); It; ++It)
{
++PlayerStartCount;
PlayerStartLoc = It->GetActorLocation();
}
R.Check(PlayerStartCount == 1,
FString::Printf(TEXT("Exactly one PlayerStart (found %d)"), PlayerStartCount));

if (PlayerStartCount == 1)
{
R.Check(OfficeBounds.IsInside(PlayerStartLoc),
FString::Printf(TEXT("PlayerStart inside office bounds (%s)"), *PlayerStartLoc.ToString()));
}

int32 ActorCount = 0;
for (TActorIterator<AActor> It(World); It; ++It)
{
++ActorCount;
}
R.Check(ActorCount >= MinActorCount && ActorCount <= MaxActorCount,
FString::Printf(TEXT("Actor count in [%d, %d] (found %d)"), MinActorCount, MaxActorCount, ActorCount));

/* THE LEGACY SYSTEM (docs/MACHINE_PLAN.md §8 — the one-day test).
   Mrs. Hall's opening ticket is about this machine, so if it is missing the game's
   inciting incident points at nothing again. The self-test is pure state: born broken,
   refactoring the faulty part fixes it, reverting breaks it again — that last one is
   what proves a DISCARDED Test-Drive branch cannot leave the machine wrongly fixed. */
ALegacyMachine* Machine = nullptr;
for (TActorIterator<ALegacyMachine> It(World); It; ++It)
{
	Machine = *It;
	break;
}
R.Check(Machine != nullptr, TEXT("MACHINE: the legacy system is in the office"));
if (Machine)
{
	FString MachineError;
	const bool bMachineOk = Machine->RunMachineSelfTest(MachineError);
	R.Check(bMachineOk, TEXT("MACHINE: broken -> refactor fixes -> revert re-breaks"));
	if (!bMachineOk)
	{
		UE_LOG(LogSmokeTest, Error, TEXT("  machine self-test: %s"), *MachineError);
	}
}

const ULevel* PersistentLevel = World->PersistentLevel;
const bool bHasBuildData = PersistentLevel && PersistentLevel->MapBuildData != nullptr;
R.Warn(bHasBuildData,
FString::Printf(TEXT("Static lighting build data present: %s (Lumen-dynamic => WARN is expected)"),
bHasBuildData ? TEXT("yes") : TEXT("no")));

// The exit-3 lesson (see ElsewhereSmokeTest / CompileSmokeTest): a commandlet
// that initialised a world owes CleanupWorld on every exit path, or engine
// shutdown aborts with exit 3 AFTER the assertions pass. This gate got away
// without it until the FUN_PLAN actors (PowerGrant et al) joined the level.
World->CleanupWorld();

if (R.Failures == 0)
{
UE_LOG(LogSmokeTest, Display, TEXT("=== SMOKE TEST PASSED (CP1 green). ==="));
return 0;
}

UE_LOG(LogSmokeTest, Error, TEXT("=== SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
return R.Failures;
#endif // WITH_EDITOR
}
