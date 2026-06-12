// RefactorSmokeTestCommandlet.cpp
//
// SIB-26 — Ch2 Refactor headless smoke test.
//
// [HARD] L_Office_v02 loads
// [HARD] IA_Refactor loads
// [HARD] IMC_Default loads and maps IA_Refactor
// [HARD] URefactorComponent exists on the player character class (CDO)
// [HARD] At least one URefactorableComponent in the map; its EditType is in the enum (R5)
// [HARD] That refactorable's self-test passes (apply->apply->revert restores
//        material + scale + collision exactly, and the first apply changed
//        something) [R1/R2/R6/R9]
//
// CP3 lesson #6: helpers live in the NAMED namespace RefactorSmokeTestNS, not an
// anonymous one, to avoid unity-build redefinition collisions with the other
// commandlets (DefaultMapPackage / FResult / ParseMapArg).
//
// TODO before first run: set the paths below to the real asset paths, and
// verify the two [API?] calls against your UE 5.7 headers.

#include "RefactorSmokeTestCommandlet.h"
#include "RefactorComponent.h"
#include "RefactorableComponent.h"
#include "RefactorTypes.h"

#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/UObjectGlobals.h" // LoadObject

#if WITH_EDITOR
#include "FileHelpers.h"           // UEditorLoadingAndSavingUtils (editor-only)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRefactorSmoke, Log, All);

namespace RefactorSmokeTestNS
{
	// --- Fill with the real content paths (TODO) ----------------------------
	const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");
	const FString IAPath  = TEXT("/Game/Input/IA_Refactor.IA_Refactor");
	const FString IMCPath = TEXT("/Game/Input/IMC_Default.IMC_Default");

	struct FResult
	{
		int32 Failures = 0;

		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogRefactorSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogRefactorSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	FString ParseMapArg(const FString& Params)
	{
		TArray<FString> Tokens, Switches;
		TMap<FString, FString> SwitchPairs;
		UCommandlet::ParseCommandLine(*Params, Tokens, Switches, SwitchPairs);
		if (const FString* Found = SwitchPairs.Find(TEXT("map")))
		{
			return *Found;
		}
		return DefaultMapPackage;
	}

	// Find the player character class via the map's GameMode override, so we can
	// CDO-check for URefactorComponent without spawning a pawn.
	UClass* ResolvePawnClass(UWorld* World)
	{
		if (!World) return nullptr;
		AWorldSettings* WS = World->GetWorldSettings();
		TSubclassOf<AGameModeBase> GM = WS ? WS->DefaultGameMode : nullptr;
		if (!GM) return nullptr;
		const AGameModeBase* GMCDO = GM->GetDefaultObject<AGameModeBase>();
		return GMCDO ? GMCDO->DefaultPawnClass.Get() : nullptr;
	}
}

URefactorSmokeTestCommandlet::URefactorSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 URefactorSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogRefactorSmoke, Error, TEXT("RefactorSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	using namespace RefactorSmokeTestNS;

	const FString MapPackage = ParseMapArg(Params);
	UE_LOG(LogRefactorSmoke, Display, TEXT("=== SIB-26 Refactor smoke test: %s ==="), *MapPackage);

	FResult R;

	// [HARD] Load the world.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
	if (!World)
	{
		UE_LOG(LogRefactorSmoke, Error, TEXT("=== REFACTOR SMOKE TEST FAILED: could not load map. ==="));
		return 1;
	}

	// [HARD] IA_Refactor + IMC mapping.
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *IAPath);
	R.Check(IA != nullptr, FString::Printf(TEXT("IA_Refactor loads (%s)"), *IAPath));

	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
	R.Check(IMC != nullptr, FString::Printf(TEXT("IMC_Default loads (%s)"), *IMCPath));
	if (IMC && IA)
	{
		bool bMapped = false;
		// [API?] UE 5.7: GetMappings() -> const TArray<FEnhancedActionKeyMapping>&
		for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
		{
			if (M.Action == IA) { bMapped = true; break; }
		}
		R.Check(bMapped, TEXT("IMC_Default maps IA_Refactor"));
	}

	// [HARD] URefactorComponent on the player character CDO.
	if (UClass* PawnClass = ResolvePawnClass(World))
	{
		const AActor* PawnCDO = PawnClass->GetDefaultObject<AActor>();
		const bool bHasComp = PawnCDO && PawnCDO->FindComponentByClass<URefactorComponent>() != nullptr;
		// Note: native subobjects resolve on the CDO; a BP-added component may not.
		// If this reads false but the component is BP-added, treat as soft and rely
		// on the in-editor check. Kept HARD here assuming a native subobject.
		R.Check(bHasComp, TEXT("URefactorComponent present on player character"));
	}
	else
	{
		R.Check(false, TEXT("Could resolve player pawn class from GameMode"));
	}

	// [HARD] A refactorable exists; EditType valid; self-test passes.
	URefactorableComponent* Refactorable = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (URefactorableComponent* RC = It->FindComponentByClass<URefactorableComponent>())
		{
			Refactorable = RC;
			break;
		}
	}
	R.Check(Refactorable != nullptr, TEXT("At least one URefactorableComponent in the map"));
	if (Refactorable)
	{
		const ERefactorEditType T = Refactorable->GetEditType();
		const bool bValidType = (T == ERefactorEditType::Material) || (T == ERefactorEditType::Scale);
		R.Check(bValidType, TEXT("Refactorable EditType is in the allowed enum (R5)"));

		R.Check(Refactorable->RunRefactorSelfTest(),
			TEXT("Refactor self-test: apply->apply->revert restores material+scale+collision exactly [R1/R2/R6/R9]"));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogRefactorSmoke, Display, TEXT("=== REFACTOR SMOKE TEST PASSED (Ch2 green). ==="));
		return 0;
	}
	UE_LOG(LogRefactorSmoke, Error, TEXT("=== REFACTOR SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif
}
