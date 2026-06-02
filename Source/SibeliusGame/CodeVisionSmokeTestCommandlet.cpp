// CodeVisionSmokeTestCommandlet.cpp
//
// SIB-25 — Ch1 Code Vision headless smoke test.
//
// [HARD] L_CodeVisionTest loads
// [HARD] MPC_CodeVision loads and has the 'Active' scalar parameter
// [HARD] IA_CodeVision loads
// [HARD] IMC_Default loads and maps IA_CodeVision
// [HARD] At least one AHiddenDoor; its DoorMesh uses the reserved stencil (250)
// [HARD] HiddenDoor self-test passes (collision + visibility track state both ways) [CV4]
// [HARD] An end-trigger actor tagged 'CodeVisionEndTrigger' is present
// [SOFT] PP_CodeVision post-process material loads
//
// CP3 lesson #6: helpers live in the NAMED namespace CodeVisionSmokeTestNS,
// NOT an anonymous one, to avoid unity-build redefinition collisions with
// SmokeTestCommandlet.cpp / RefuserSmokeTestCommandlet.cpp (both of which use
// the symbols DefaultMapPackage / FResult / ParseMapArg in anonymous namespaces).
//
// TODO before first run: set the four content paths in the namespace below to
// the real asset paths, and verify the two API calls flagged [API?] against
// your UE 5.7 headers (GetScalarParameterByName, GetMappings) — adjust if the
// signatures differ.

#include "CodeVisionSmokeTestCommandlet.h"
#include "CodeVisionComponent.h"
#include "CodeVisionStencil.h"
#include "HiddenDoor.h"

#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/Material.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/UObjectGlobals.h" // LoadObject

#if WITH_EDITOR
#include "FileHelpers.h"           // UEditorLoadingAndSavingUtils (editor-only)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCodeVisionSmoke, Log, All);

namespace CodeVisionSmokeTestNS
{
	// --- Fill with the real content paths (TODO) ----------------------------
	const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");
	const FString MPCPath  = TEXT("/Game/CodeVision/MPC_CodeVision.MPC_CodeVision");
	const FString IAPath   = TEXT("/Game/Input/IA_CodeVision.IA_CodeVision");
	const FString IMCPath  = TEXT("/Game/Input/IMC_Default.IMC_Default");
	const FString PPPath   = TEXT("/Game/CodeVision/PP_CodeVision.PP_CodeVision");
	const FName   ActiveParam   = TEXT("Active");
	const FName   EndTriggerTag = TEXT("CodeVisionEndTrigger");

	struct FResult
	{
		int32 Failures = 0;

		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogCodeVisionSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogCodeVisionSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}

		void Warn(bool bCondition, const FString& Label)
		{
			UE_LOG(LogCodeVisionSmoke, Display, TEXT("  [%s] %s"),
				bCondition ? TEXT("PASS") : TEXT("WARN"), *Label);
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
}

UCodeVisionSmokeTestCommandlet::UCodeVisionSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCodeVisionSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogCodeVisionSmoke, Error, TEXT("CodeVisionSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	using namespace CodeVisionSmokeTestNS;

	const FString MapPackage = ParseMapArg(Params);
	UE_LOG(LogCodeVisionSmoke, Display, TEXT("=== SIB-25 Code Vision smoke test: %s ==="), *MapPackage);

	FResult R;

	// [HARD] Load the world.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
	if (!World)
	{
		UE_LOG(LogCodeVisionSmoke, Error, TEXT("=== CODE VISION SMOKE TEST FAILED: could not load map. ==="));
		return 1;
	}

	// [HARD] MPC + 'Active' scalar.
	UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, *MPCPath);
	R.Check(MPC != nullptr, FString::Printf(TEXT("MPC_CodeVision loads (%s)"), *MPCPath));
	if (MPC)
	{
		// [API?] UE 5.7: GetScalarParameterByName(FName) -> const FCollectionScalarParameter*
		const bool bHasActive = MPC->GetScalarParameterByName(ActiveParam) != nullptr;
		R.Check(bHasActive, TEXT("MPC has 'Active' scalar parameter"));
	}

	// [HARD] IA_CodeVision.
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *IAPath);
	R.Check(IA != nullptr, FString::Printf(TEXT("IA_CodeVision loads (%s)"), *IAPath));

	// [HARD] IMC_Default maps IA_CodeVision.
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
	R.Check(IMC != nullptr, FString::Printf(TEXT("IMC_Default loads (%s)"), *IMCPath));
	if (IMC && IA)
	{
		bool bMapped = false;
		// [API?] UE 5.7: GetMappings() -> const TArray<FEnhancedActionKeyMapping>&
		for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
		{
			if (M.Action == IA)
			{
				bMapped = true;
				break;
			}
		}
		R.Check(bMapped, TEXT("IMC_Default maps IA_CodeVision"));
	}

	// [HARD] HiddenDoor present + correct stencil + self-test.
	AHiddenDoor* Door = nullptr;
	for (TActorIterator<AHiddenDoor> It(World); It; ++It) { Door = *It; break; }
	R.Check(Door != nullptr, TEXT("At least one AHiddenDoor in the map"));
	if (Door)
	{
		UStaticMeshComponent* Mesh = Door->FindComponentByClass<UStaticMeshComponent>();
		const bool bStencil = Mesh && Mesh->bRenderCustomDepth &&
			Mesh->CustomDepthStencilValue == CODEVISION_STENCIL;
		R.Check(bStencil, FString::Printf(TEXT("Door uses reserved CustomDepth stencil (%d)"), CODEVISION_STENCIL));

		R.Check(Door->RunCollisionSelfTest(),
			TEXT("Door self-test: collision + visibility track state both ways [CV4]"));
	}

	// [HARD] End-trigger tagged actor present (overlapping it ends the chapter).
	bool bEndTrigger = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(EndTriggerTag))
		{
			bEndTrigger = true;
			break;
		}
	}
	R.Check(bEndTrigger, FString::Printf(TEXT("End-trigger actor tagged '%s' present"), *EndTriggerTag.ToString()));

	// [SOFT] Post-process overlay material loads.
	UMaterial* PP = LoadObject<UMaterial>(nullptr, *PPPath);
	R.Warn(PP != nullptr, FString::Printf(TEXT("PP_CodeVision post-process material loads (%s)"), *PPPath));

	// --- Summary ------------------------------------------------------------
	if (R.Failures == 0)
	{
		UE_LOG(LogCodeVisionSmoke, Display, TEXT("=== CODE VISION SMOKE TEST PASSED (Ch1 green). ==="));
		return 0;
	}

	UE_LOG(LogCodeVisionSmoke, Error, TEXT("=== CODE VISION SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
