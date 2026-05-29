// SibeliusSmokeTestCommandlet.cpp - SIB-19 headless smoke test for L_Office_v02 (v0.2 CP1).

#include "SibeliusSmokeTestCommandlet.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Engine/MapBuildDataRegistry.h"

#if WITH_EDITOR
#include "FileHelpers.h" // UEditorLoadingAndSavingUtils (editor-only)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSmokeTest, Log, All);

namespace
{
const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");

// World Settings -> GameMode Override = BP_FirstPersonGameMode (SIB-18).
const FString ExpectedGameModeSubstring = TEXT("FirstPerson");

// B12: full QuadArt layout, no cleanup (SIB-17 deferred). ~1,074 actors today.
const int32 MinActorCount = 1000;
const int32 MaxActorCount = 1150;

// Office bounding box (world units). Generous: catches spawn-in-field / under-world
// without tripping on a small PlayerStart nudge. Known-good is ~(-1832, 10500, 400).
const FBox OfficeBounds(
FVector(-2300.0, 9500.0, 100.0),
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

const ULevel* PersistentLevel = World->PersistentLevel;
const bool bHasBuildData = PersistentLevel && PersistentLevel->MapBuildData != nullptr;
R.Warn(bHasBuildData,
FString::Printf(TEXT("Static lighting build data present: %s (Lumen-dynamic => WARN is expected)"),
bHasBuildData ? TEXT("yes") : TEXT("no")));

if (R.Failures == 0)
{
UE_LOG(LogSmokeTest, Display, TEXT("=== SMOKE TEST PASSED (CP1 green). ==="));
return 0;
}

UE_LOG(LogSmokeTest, Error, TEXT("=== SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
return R.Failures;
#endif // WITH_EDITOR
}
