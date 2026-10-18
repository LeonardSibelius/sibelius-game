#include "UniverseCitySmokeTestCommandlet.h"
#include "UniverseForge.h"
#include "UniverseExhibit.h"
#include "AICity.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/InputComponent.h"
#include "ProgressionSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogUniverseCitySmoke, Log, All);

UUniverseCitySmokeTestCommandlet::UUniverseCitySmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

FString UUniverseCitySmokeTestCommandlet::PreviewAction(UWorld* World, const FString& Action, int32 Slot)
{
	FString TestUserDir;
	if (!World || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), TestUserDir) || TestUserDir.IsEmpty())
	{
		return TEXT("REFUSED: visual QA requires an isolated -UserDir.");
	}
	AUniverseForge* Forge = nullptr;
	for (TActorIterator<AUniverseForge> It(World); It; ++It) { Forge = *It; break; }
	if (!Forge) { return TEXT("No forge"); }
	auto* PC = UGameplayStatics::GetPlayerController(World, 0);
	auto* Progress = UProgressionSubsystem::Get(Forge);
	if (!PC || !Progress) { return TEXT("No player/progression"); }
	if (Action == TEXT("open"))
	{
		if (!PC->GetPawn())
		{
			if (auto* Mode = World->GetAuthGameMode()) { Mode->RestartPlayer(PC); }
		}
		Forge->StartPlaying(PC);
	}
	else if (Action == TEXT("grow")) { Forge->BeginGrowing(); }
	else if (Action == TEXT("finish"))
	{
		Forge->Clock = Forge->StageEnd(static_cast<int32>(Forge->History.LastStage)) + Forge->EndingSeconds;
		Forge->State = EForgeState::Ended;
		Forge->Pose(); Forge->CheckMilestones();
	}
	else if (Action == TEXT("keep")) { Forge->OnKeep(); Forge->SelectSlot(Slot); }
	else if (Action == TEXT("confirm")) { Forge->SelectSlot(Slot); }
	else if (Action == TEXT("leave")) { Forge->OnLeave(); }
	else if (Action == TEXT("inspect"))
	{
		const auto* R = Progress->UniverseInSlot(Slot);
		if (!R) { return TEXT("Empty slot"); }
		Forge->InspectSaved(PC, *R);
	}
	else if (Action == TEXT("new")) { Forge->OnNewUniverse(); }
	else if (Action == TEXT("retry")) { Forge->OnAgain(); }
	else if (Action == TEXT("fog")) { Forge->Dials.Gravity = 0.05; Forge->BeginGrowing(); }
	else if (Action != TEXT("status")) { return TEXT("Unknown action"); }
	const auto* R = Progress->UniverseInSlot(Slot);
	return FString::Printf(TEXT("state=%d choosing=%d pending=%d slot=%s id=%s contact=%d ignored=%d"),
		static_cast<int32>(Forge->State), Forge->bChoosingSlot, Forge->PendingReplaceSlot,
		R ? *R->Name : TEXT("EMPTY"), R ? *R->Id.ToString() : TEXT("NONE"),
		Progress->GetStateForRead().bUniverseContactComplete, PC->IsMoveInputIgnored());
}

int32 UUniverseCitySmokeTestCommandlet::Main(const FString& Params)
{
	int32 Failures = 0;
	auto Check = [&Failures](bool Condition, const TCHAR* Label)
	{
		if (Condition) { UE_LOG(LogUniverseCitySmoke, Display, TEXT("PASS %s"), Label); }
		else { ++Failures; UE_LOG(LogUniverseCitySmoke, Error, TEXT("FAIL %s"), Label); }
	};
	// Editor world has no game instance: these tests cannot touch the player's progression slot.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Maps/L_Genesis"));
	if (!World) { return 1; }
	Check(World->GetGameInstance() == nullptr, TEXT("isolated world has no player save subsystem"));
	if (World->GetGameInstance()) { return 1; }
	AUniverseForge* Forge = nullptr;
	AAICity* City = nullptr;
	for (TActorIterator<AUniverseForge> It(World); It; ++It) { Forge = *It; break; }
	for (TActorIterator<AAICity> It(World); It; ++It) { City = *It; break; }
	Check(Forge && City, TEXT("real Genesis map contains city and forge"));
	if (!Forge || !City) { return 1; }
	Check(GetDefault<AUniverseForge>()->Dials.Time == 0.75, TEXT("welcoming first preset"));
	Forge->Dials = { 0.5, 0.5, 0.75 };
	Forge->BeginGrowing();
	const auto First = Forge->History;
	Forge->State = EForgeState::Ended;
	Forge->OnAgain();
	Forge->BeginGrowing();
	Check(Forge->History.Seed == First.Seed && Forge->History.Chronicle == First.Chronicle, TEXT("retry retains seed and identical outcome"));
	Forge->State = EForgeState::Ended;
	Forge->OnNewUniverse();
	Forge->BeginGrowing();
	Check(Forge->History.Seed != First.Seed, TEXT("new universe obtains new seed"));
	Forge->Clock = 1;
	Forge->SeenBeforeRun = 0;
	Forge->OnSkip();
	Check(Forge->Clock == 1, TEXT("unseen stage cannot be skipped"));
	Forge->SeenBeforeRun = 1;
	Forge->OnSkip();
	Check(Forge->Clock == Forge->StageEnd(0), TEXT("seen stage skips to next boundary"));
	Check(!Forge->bContactSent && !City->IsAnswering(), TEXT("early skip never sends contact"));
	Forge->Clock = Forge->StageEnd(5) + 2;
	Forge->CheckMilestones();
	Check(Forge->bContactSent && City->IsAnswering(), TEXT("visible machines trigger city response"));
	City->Tick(13);
	Check(!City->IsAnswering(), TEXT("response completes in bounded time"));
	Forge->CheckMilestones();
	Check(!City->IsAnswering(), TEXT("same run cannot trigger twice"));
	Forge->Dials.Gravity = 0.05;
	Forge->BeginGrowing();
	Forge->Clock = 1000;
	Forge->CheckMilestones();
	Check(!Forge->bContactSent && !City->IsAnswering(), TEXT("early-ending universe never triggers contact"));
	Forge->State = EForgeState::Ended;
	Forge->bChoosingSlot = true;
	Forge->PendingReplaceSlot = 1;
	Forge->OnLeave();
	Check(!Forge->bChoosingSlot && Forge->PendingReplaceSlot == INDEX_NONE && Forge->State == EForgeState::Ended,
		TEXT("cancel replacement retains result without leaving forge"));
	APlayerController* PC = World->SpawnActor<APlayerController>();
	APawn* Pawn = World->SpawnActor<APawn>();
	if (PC && Pawn)
	{
		PC->Possess(Pawn);
		const auto Record = FUniverseRecord::Capture(First, FGuid::NewGuid());
		Forge->InspectSaved(PC, Record);
		Check(Forge->bInspecting && Forge->History.Seed == First.Seed && PC->IsMoveInputIgnored(), TEXT("saved exhibit opens original history with inspection controls"));
		Forge->OnNewUniverse();
		Check(Forge->State == EForgeState::Ended, TEXT("inspection cannot accidentally create a replacement"));
		Forge->StopPlaying();
		Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && Forge->State == EForgeState::Idle,
			TEXT("exit restores camera/input state"));
		PC->Destroy(); Pawn->Destroy();
	}
	else { Check(false, TEXT("spawn inspection controller and pawn")); }
	Check(World->SpawnActor<AUniverseExhibit>() != nullptr, TEXT("personal exhibit components instantiate"));
	UE_LOG(LogUniverseCitySmoke, Display, TEXT("AI City smoke: %d failures"), Failures);
	return Failures == 0 ? 0 : 1;
}
