// NpcChatSmokeTestCommandlet.cpp — the NPC chat gate. See header.
//
// NAMED namespace + function-scoped `using` (unity-build safe), no variable
// shadowing (warnings-as-errors). Editor-closed.

#include "NpcChatSmokeTestCommandlet.h"

#include "DancerAgentComponent.h"
#include "NpcChatSubsystem.h"
#include "NpcChatSaveGame.h"
#include "SibeliusSaveIO.h"
#include "PowerGrant.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "CoreGlobals.h"          // GFrameCounter — see AdvanceTime
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNpcChatSmoke, Log, All);

namespace NpcChatSmokeTestNS
{
	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogNpcChatSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogNpcChatSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	/** What a Send handed back, so an assertion can look at it after the tick. */
	struct FCapturedReply
	{
		bool bFired = false;
		FString Text;
		ENpcChatFailure Failure = ENpcChatFailure::None;
	};

	/**
	 * Push the game instance's clock forward.
	 *
	 * A commandlet has no frame loop, so the mock's timer never fires on its own. Many
	 * small steps rather than one big one, because a single huge delta is exactly the
	 * shape of tick a real game never produces.
	 */
	void AdvanceTime(UGameInstance* GI, float Seconds)
	{
		/* GFrameCounter HAS TO MOVE OR ONLY THE FIRST STEP COUNTS.

		   FTimerManager::Tick early-outs when it has already ticked this frame, and a
		   commandlet has no frame loop to advance the counter — so a plain loop of
		   Tick(0.05) advances the clock by 0.05 seconds total and every timer-backed
		   assertion fails for a reason that looks like a broken service. Cost an
		   afternoon once; costs a line here. */
		const float Step = 0.05f;
		for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += Step)
		{
			++GFrameCounter;
			GI->GetTimerManager().Tick(Step);
		}
	}

	/** Read a console variable's current int, or a sentinel when it does not exist. */
	int32 ReadCVarInt(const TCHAR* Name, int32 Fallback)
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		return Var ? Var->GetInt() : Fallback;
	}

	/**
	 * Does any config file THAT SHIPS set this console variable?
	 *
	 * The question that matters is not "is the switch off on this machine right now" —
	 * a developer who puts sib.Chat.Enable=1 in the engine's own ConsoleVariables.ini
	 * is doing the supported thing, and that file is machine-local and never packaged.
	 * What must never happen is the switch being set in the project's Config/, because
	 * that folder is in the repo and goes into the build. Commented lines do not count.
	 */
	bool ShippedConfigSets(const TCHAR* Token, FString& OutWhere)
	{
		TArray<FString> IniFiles;
		IFileManager::Get().FindFilesRecursive(IniFiles, *FPaths::ProjectConfigDir(),
			TEXT("*.ini"), /*Files=*/true, /*Directories=*/false);

		for (const FString& IniFile : IniFiles)
		{
			TArray<FString> Lines;
			if (!FFileHelper::LoadFileToStringArray(Lines, *IniFile))
			{
				continue;
			}

			for (const FString& RawLine : Lines)
			{
				const FString Line = RawLine.TrimStartAndEnd();
				if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("#")))
				{
					continue;
				}
				if (Line.Contains(Token, ESearchCase::IgnoreCase))
				{
					OutWhere = FPaths::GetCleanFilename(IniFile) + TEXT(" -> ") + Line;
					return true;
				}
			}
		}
		return false;
	}
}

UNpcChatSmokeTestCommandlet::UNpcChatSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UNpcChatSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace NpcChatSmokeTestNS;

	UE_LOG(LogNpcChatSmoke, Display, TEXT("=== NPC chat smoke test (mock service) ==="));

	FResult R;

	// --- 1) THE SHIPPING PROMISE. Both switches off, by default, everywhere.
	{
		const UDancerAgentComponent* Defaults = GetDefault<UDancerAgentComponent>();
		R.Check(Defaults && !Defaults->bChatEnabled,
			TEXT("chat is OFF on the component default (1.0 ships chat-free)"));

		// -1 means the variable is missing entirely, which is its own failure: the
		// routing in TryInteract reads it, and a switch that does not exist is not an
		// off one.
		R.Check(ReadCVarInt(TEXT("sib.Chat.Enable"), -1) != -1,
			TEXT("sib.Chat.Enable is registered"));

		/* THE RIGHT QUESTION IS WHAT SHIPS, NOT WHAT THIS MACHINE HAS.

		   This assertion used to read the variable's current value, and it went red the
		   first time a developer enabled chat for a walk-test through the engine's
		   machine-local ConsoleVariables.ini — a supported thing to do, with a file that
		   is never packaged. A gate that fails for that is a gate people learn to
		   ignore. What would really break the 1.0 promise is the switch set in the
		   project's own Config/, which is in the repo and goes into the build. */
		/* Searched FIRST, then the label is built. C++ does not order argument
		   evaluation, so passing the search and a message that reads Offender as two
		   arguments to Check printed the empty version even when it had found
		   something — a gate that fails without naming the file it failed on. */
		FString Offender;
		const bool bShippedConfigEnables = ShippedConfigSets(TEXT("sib.Chat.Enable"), Offender);
		R.Check(!bShippedConfigEnables,
			bShippedConfigEnables
				? FString::Printf(TEXT("no shipped config turns chat on (found %s)"), *Offender)
				: FString(TEXT("no shipped config turns chat on")));

		// Reported, not asserted: this one is about the environment this run happens in.
		UE_LOG(LogNpcChatSmoke, Display, TEXT("  [note] sib.Chat.Enable reads %d in this process"),
			ReadCVarInt(TEXT("sib.Chat.Enable"), -1));

		R.Check(ReadCVarInt(TEXT("sib.Chat.MockFail"), -1) == 0,
			TEXT("no forced failure is active in this environment"));
	}

	// --- 2) A standalone game instance, for the subsystem and a world to spawn into.
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	if (!GI)
	{
		UE_LOG(LogNpcChatSmoke, Error, TEXT("  [FAIL] could not create a game instance"));
		return 1;
	}
	GI->InitializeStandalone();

	UNpcChatSubsystem* Chat = GI->GetSubsystem<UNpcChatSubsystem>();
	R.Check(Chat != nullptr, TEXT("UNpcChatSubsystem is registered on the game instance"));

	if (Chat)
	{
		// --- 3) One request in flight, and a reply that actually arrives.
		{
			FCapturedReply First;
			FOnNpcChatReply OnFirst;
			OnFirst.BindLambda([&First](const FString& Text, ENpcChatFailure Failure)
			{
				First.bFired = true;
				First.Text = Text;
				First.Failure = Failure;
			});

			const int32 FirstId = Chat->Send(TEXT("Kaia"), TEXT("hello"), OnFirst);
			R.Check(FirstId != 0, TEXT("mock: Send returns a request id"));
			R.Check(Chat->IsBusy(), TEXT("mock: the service reports itself busy"));

			/* THE RULE THAT PROTECTS A SHARED ALLOWANCE. A player leaning on Enter must
			   not spend two of a daily pool on one thought. Refused, not queued. */
			FOnNpcChatReply OnSecond;
			const int32 SecondId = Chat->Send(TEXT("Kaia"), TEXT("hello again"), OnSecond);
			R.Check(SecondId == 0, TEXT("mock: a second Send while one is in flight is refused"));

			R.Check(!First.bFired, TEXT("mock: no reply before the clock moves"));

			AdvanceTime(GI, 2.5f);

			R.Check(First.bFired, TEXT("mock: the reply arrives once the clock moves"));
			R.Check(First.bFired && First.Failure == ENpcChatFailure::None, TEXT("mock: a normal reply carries no failure"));
			R.Check(First.bFired && !First.Text.IsEmpty(), TEXT("mock: a normal reply carries text"));
			R.Check(!Chat->IsBusy(), TEXT("mock: the service is free again afterwards"));

			// --- 4) Ids are not reused. A dedupe key that repeats is not a key.
			FCapturedReply Second;
			FOnNpcChatReply OnThird;
			OnThird.BindLambda([&Second](const FString& Text, ENpcChatFailure Failure)
			{
				Second.bFired = true;
				Second.Text = Text;
				Second.Failure = Failure;
			});
			const int32 ThirdId = Chat->Send(TEXT("Kaia"), TEXT("and another"), OnThird);
			R.Check(ThirdId != 0 && ThirdId != FirstId, TEXT("mock: request ids are unique"));

			/* --- 5) CANCEL REALLY DROPS IT. This is the acceptance check that a late
			   reply cannot enter another conversation: the panel closes, the id is
			   cancelled, and the answer never lands on anybody. */
			Chat->Cancel(ThirdId);
			R.Check(!Chat->IsBusy(), TEXT("mock: cancelling clears the in-flight request"));
			AdvanceTime(GI, 2.5f);
			R.Check(!Second.bFired, TEXT("mock: a cancelled request never delivers"));
		}

		// --- 6) Failures arrive as reasons, with no text to draw.
		{
			IConsoleVariable* FailVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.MockFail"));
			R.Check(FailVar != nullptr, TEXT("sib.Chat.MockFail is available to force a failure"));

			if (FailVar)
			{
				struct FCase
				{
					int32 Code;
					ENpcChatFailure Expected;
					const TCHAR* Label;
				};
				const FCase Cases[] =
				{
					{ 1, ENpcChatFailure::Quota,   TEXT("quota") },
					{ 2, ENpcChatFailure::Network, TEXT("network") },
					{ 4, ENpcChatFailure::Blocked, TEXT("blocked") },
				};

				for (const FCase& Case : Cases)
				{
					FailVar->Set(Case.Code);

					FCapturedReply Failed;
					FOnNpcChatReply OnFailed;
					OnFailed.BindLambda([&Failed](const FString& Text, ENpcChatFailure Failure)
					{
						Failed.bFired = true;
						Failed.Text = Text;
						Failed.Failure = Failure;
					});

					const int32 FailId = Chat->Send(TEXT("Kaia"), TEXT("will not land"), OnFailed);
					AdvanceTime(GI, 2.5f);

					R.Check(FailId != 0 && Failed.bFired,
						FString::Printf(TEXT("mock: forced %s failure answers"), Case.Label));
					R.Check(Failed.Failure == Case.Expected,
						FString::Printf(TEXT("mock: forced %s failure reports its own reason"), Case.Label));
					// bFired guards it: an empty string is also what a reply that never
					// arrived leaves behind, and that must not read as a pass.
					R.Check(Failed.bFired && Failed.Text.IsEmpty(),
						FString::Printf(TEXT("mock: a %s failure carries no reply text"), Case.Label));
				}

				// Leave the world as it was found: a gate that changes a console
				// variable and walks away poisons every run after it in the session.
				FailVar->Set(0);
				R.Check(ReadCVarInt(TEXT("sib.Chat.MockFail"), -1) == 0,
					TEXT("sib.Chat.MockFail restored to 0"));
			}
		}
	}

	// --- 7) AUTHORED CONTENT KEEPS E. Chat may only have the key once the authored
	// game has nothing left to do at this dancer.
	if (UWorld* World = GI->GetWorld())
	{
		AActor* Host = World->SpawnActor<AActor>();
		R.Check(Host != nullptr, TEXT("routing: a host actor spawns"));

		if (Host)
		{
			UDancerAgentComponent* Agent = NewObject<UDancerAgentComponent>(Host);
			Agent->RegisterComponent();
			Agent->AgentName = TEXT("GateKaia");

			R.Check(!Agent->HasAuthoredBusinessPending(),
				TEXT("routing: a plain dancer has no authored business (chat may take E)"));

			/* SHE IS HOLDING A POWER. E belongs to the slot trial, and a chat window
			   opening in its place would leave the player unable to claim it. */
			if (APowerGrant* Grant = World->SpawnActor<APowerGrant>())
			{
				Agent->SetPowerGrant(Grant, EPowerVerb::Refactor);
				R.Check(Agent->HasAuthoredBusinessPending(),
					TEXT("routing: a dancer holding a power has authored business (chat must not take E)"));

				Grant->Destroy();
				R.Check(!Agent->HasAuthoredBusinessPending(),
					TEXT("routing: handing the power over releases E to chat"));
			}
			else
			{
				R.Check(false, TEXT("routing: a power grant spawns"));
			}

			R.Check(!Agent->IsChatting(), TEXT("routing: no conversation is open on a fresh dancer"));

			/* --- IDENTITY. One id reaches the service and the save file, or a
			   conversation is remembered under a name the provider never saw.
			   AgentName is a DISPLAY name derived from the actor; ChatId is the
			   stable one, and it exists because the finale altar spawns a dancer
			   whose actor name changes every session. */
			R.Check(Agent->ResolveChatId() == FName(TEXT("GateKaia")),
				TEXT("identity: a blank ChatId falls back to AgentName"));

			Agent->ChatId = FName(TEXT("kaia"));
			R.Check(Agent->ResolveChatId() == FName(TEXT("kaia")),
				TEXT("identity: an explicit ChatId wins over the display name"));

			Agent->AgentName.Reset();
			R.Check(Agent->ResolveChatId() == FName(TEXT("kaia")),
				TEXT("identity: the stable id survives a display name going away"));
		}
	}
	else
	{
		R.Check(false, TEXT("routing: the standalone game instance has a world"));
	}

	// --- 8) MEMORY (step 5). Bounded, round-tripped through the real save path, and
	// erasable — against a sandbox slot, never a player's own file.
	if (Chat)
	{
		const FName Kaia(TEXT("GateKaia"));
		const FName Nyra(TEXT("GateNyra"));

		Chat->ForgetAll();
		R.Check(Chat->GetHistory(Kaia).Num() == 0, TEXT("memory: an agent starts with nothing"));

		Chat->RecordTurn(Kaia, true, TEXT("first thing"));
		Chat->RecordTurn(Kaia, false, TEXT("her answer"));
		Chat->RecordTurn(Nyra, true, TEXT("a different agent"));

		R.Check(Chat->GetHistory(Kaia).Num() == 2, TEXT("memory: turns accumulate"));
		R.Check(Chat->GetHistory(Kaia)[0].bFromPlayer && !Chat->GetHistory(Kaia)[1].bFromPlayer,
			TEXT("memory: who said what survives"));
		R.Check(Chat->GetHistory(Nyra).Num() == 1,
			TEXT("memory: agents do not share a history"));

		/* THE BOUND HOLDS AND KEEPS THE RIGHT END. Dropping the newest would be the
		   easy bug and the invisible one: she would answer as if the conversation had
		   stopped several turns ago. */
		for (int32 i = 0; i < UNpcChatSubsystem::MaxTurnsKept + 6; ++i)
		{
			Chat->RecordTurn(Kaia, i % 2 == 0, FString::Printf(TEXT("turn %d"), i));
		}
		const TArray<FNpcChatTurn>& Bounded = Chat->GetHistory(Kaia);
		R.Check(Bounded.Num() == UNpcChatSubsystem::MaxTurnsKept,
			FString::Printf(TEXT("memory: history is bounded to %d turns (got %d)"),
				UNpcChatSubsystem::MaxTurnsKept, Bounded.Num()));
		R.Check(Bounded.Num() > 0 && Bounded.Last().Text == FString::Printf(
				TEXT("turn %d"), UNpcChatSubsystem::MaxTurnsKept + 5),
			TEXT("memory: the bound drops the oldest, never the newest"));
		R.Check(!Bounded.ContainsByPredicate([](const FNpcChatTurn& T){ return T.Text == TEXT("first thing"); }),
			TEXT("memory: the oldest turn really is gone"));

		// --- 9) The real serialization path, on a sandbox slot.
		{
			const FString Sandbox = TEXT("NpcChatSmokeSandbox");
			UNpcChatSaveGame* Written = Chat->BuildSaveObject();
			R.Check(Written && Written->SaveVersion == UNpcChatSaveGame::CurrentSaveVersion,
				TEXT("save: the written object is stamped with the current version"));
			R.Check(FSibeliusSaveIO::Commit(Written, Sandbox), TEXT("save: commits to the sandbox slot"));

			UNpcChatSaveGame* ReadBack = Cast<UNpcChatSaveGame>(FSibeliusSaveIO::Load(Sandbox));
			R.Check(ReadBack != nullptr, TEXT("save: loads back as the right class"));

			if (ReadBack)
			{
				R.Check(ReadBack->SaveVersion == UNpcChatSaveGame::CurrentSaveVersion,
					TEXT("save: the version survives the round trip"));
				R.Check(Chat->ApplySaveObject(ReadBack), TEXT("save: a current-version save is accepted"));
				R.Check(Chat->GetHistory(Kaia).Num() == UNpcChatSubsystem::MaxTurnsKept,
					TEXT("save: the bounded history survives the round trip"));
				R.Check(Chat->GetHistory(Nyra).Num() == 1,
					TEXT("save: a second agent's history survives with it"));
				R.Check(Chat->GetHistory(Nyra)[0].Text == TEXT("a different agent"),
					TEXT("save: the text itself survives (nested struct in a map recurses)"));

				/* A VERSION THIS BUILD CANNOT READ IS REFUSED WHOLE. Half a memory is
				   worse than none: she would answer from a conversation she only
				   partly has. */
				ReadBack->SaveVersion = UNpcChatSaveGame::CurrentSaveVersion + 99;
				R.Check(!Chat->ApplySaveObject(ReadBack), TEXT("save: a future version is refused"));
				R.Check(Chat->GetHistory(Kaia).Num() == 0, TEXT("save: a refused load leaves nothing half-read"));

				ReadBack->SaveVersion = 0;
				R.Check(!Chat->ApplySaveObject(ReadBack), TEXT("save: a blank (version 0) object is refused"));
			}

			R.Check(FSibeliusSaveIO::Delete(Sandbox), TEXT("save: sandbox slot cleaned up"));
			R.Check(!FSibeliusSaveIO::Has(Sandbox), TEXT("save: sandbox slot is really gone"));
		}

		// --- 10) Forgetting, which a player is entitled to ask for.
		Chat->RecordTurn(Kaia, true, TEXT("remember this"));
		Chat->RecordTurn(Nyra, true, TEXT("and this"));
		Chat->ForgetCharacter(Kaia);
		R.Check(Chat->GetHistory(Kaia).Num() == 0, TEXT("forget: one agent forgets"));
		R.Check(Chat->GetHistory(Nyra).Num() == 1, TEXT("forget: the others do not"));

		/* THE SERVICE NEVER WRITES MEMORY. Ordering is the component's business - it
		   sends, then records, so the store holds only prior turns at send time - and
		   that is only true while Send itself stays out of the store. Lock it down
		   here so the proxy cannot quietly start recording as well and double every
		   player line in the next request. */
		Chat->ForgetAll();
		Chat->RecordTurn(Kaia, true, TEXT("a prior turn"));
		FOnNpcChatReply Untouched;
		const int32 QuietId = Chat->Send(TEXT("kaia"), TEXT("this must not be recorded"), Untouched);
		R.Check(Chat->GetHistory(Kaia).Num() == 1,
			TEXT("memory: Send does not write to the store - the component owns the order"));
		Chat->Cancel(QuietId);

		Chat->ForgetAll();
		Chat->RecordTurn(Nyra, true, TEXT("and this"));
		Chat->ForgetAll();
		R.Check(Chat->GetHistory(Nyra).Num() == 0, TEXT("forget: all of them forget"));
		R.Check(!FSibeliusSaveIO::Has(UNpcChatSubsystem::SlotName()),
			TEXT("forget: the real slot is deleted, not emptied"));
	}

	// --- 11) THE CONTRACT (step 6). Proved without a server, a network or a spent
	// request - which is the entire reason BuildRequestJson and ParseReplyJson are
	// pure statics rather than code buried inside an HTTP callback.
	{
		TArray<FNpcChatTurn> Turns;
		FNpcChatTurn Said;
		Said.bFromPlayer = true;
		Said.Text = TEXT("an earlier player line");
		Turns.Add(Said);
		FNpcChatTurn Answered;
		Answered.bFromPlayer = false;
		Answered.Text = TEXT("an earlier reply");
		Turns.Add(Answered);

		const FString Body = UNpcChatSubsystem::BuildRequestJson(
			TEXT("11111111-2222-3333-4444-555555555555"),
			TEXT("kaia"), TEXT("do you remember?"), TEXT("/Game/L_Office_v02"), Turns);

		R.Check(Body.Contains(TEXT("\"requestId\":\"11111111-2222-3333-4444-555555555555\"")),
			TEXT("contract: the request carries its UUID"));
		R.Check(Body.Contains(TEXT("\"characterId\":\"kaia\"")),
			TEXT("contract: the request carries the stable character id"));
		R.Check(Body.Contains(TEXT("\"message\":\"do you remember?\"")),
			TEXT("contract: the request carries the player's message"));
		R.Check(Body.Contains(TEXT("\"locationId\":\"/Game/L_Office_v02\"")),
			TEXT("contract: storyContext carries the location"));

		/* The fact list is EMPTY and present. Spoiler filtering lands with the profiles;
		   until then an empty allowlist is the honest shape, and shipping the field now
		   means the backend contract does not change when it fills. */
		R.Check(Body.Contains(TEXT("\"unlockedFacts\":[]")),
			TEXT("contract: unlockedFacts ships empty until spoiler filtering exists"));

		R.Check(Body.Contains(TEXT("\"role\":\"user\"")) && Body.Contains(TEXT("\"role\":\"model\"")),
			TEXT("contract: prior turns carry provider-vocabulary roles"));
		R.Check(Body.Contains(TEXT("an earlier player line")) && Body.Contains(TEXT("an earlier reply")),
			TEXT("contract: prior turns carry their text"));
		R.Check(!Body.Contains(TEXT("\"recentTurns\":[]")),
			TEXT("contract: recentTurns is populated when history exists"));
	}

	// --- 12) Reading the backend's answer. Every branch a real service will produce.
	{
		FString Reply;
		ENpcChatFailure Failure = ENpcChatFailure::None;

		R.Check(UNpcChatSubsystem::ParseReplyJson(
				TEXT("{\"status\":\"ok\",\"requestId\":\"x\",\"reply\":\"I remember.\"}"), Reply, Failure)
			&& Reply == TEXT("I remember.") && Failure == ENpcChatFailure::None,
			TEXT("parse: a well-formed ok yields the reply"));

		/* AN EMPTY REPLY IS NOT AN ANSWER. It is also the shape an output cap eaten by
		   thinking arrives in, so this branch will earn its keep the first time the
		   token budget is set too low. */
		R.Check(!UNpcChatSubsystem::ParseReplyJson(
				TEXT("{\"status\":\"ok\",\"reply\":\"   \"}"), Reply, Failure),
			TEXT("parse: an empty ok is a failure, not an empty speech bubble"));

		R.Check(!UNpcChatSubsystem::ParseReplyJson(
				TEXT("{\"status\":\"unavailable\",\"reason\":\"quota\"}"), Reply, Failure)
			&& Failure == ENpcChatFailure::Quota,
			TEXT("parse: a declared quota failure keeps its reason"));

		R.Check(!UNpcChatSubsystem::ParseReplyJson(
				TEXT("{\"status\":\"unavailable\",\"reason\":\"blocked\"}"), Reply, Failure)
			&& Failure == ENpcChatFailure::Blocked,
			TEXT("parse: a declared blocked failure keeps its reason"));

		R.Check(!UNpcChatSubsystem::ParseReplyJson(
				TEXT("{\"status\":\"unavailable\",\"reason\":\"something-new\"}"), Reply, Failure)
			&& Failure == ENpcChatFailure::Service,
			TEXT("parse: an unrecognised reason becomes Service, not a new player-facing category"));

		R.Check(!UNpcChatSubsystem::ParseReplyJson(TEXT("not json at all"), Reply, Failure)
			&& Failure == ENpcChatFailure::Service,
			TEXT("parse: malformed JSON is a Service failure"));

		R.Check(!UNpcChatSubsystem::ParseReplyJson(TEXT("{\"status\":\"teapot\"}"), Reply, Failure),
			TEXT("parse: an unknown status is refused"));

		R.Check(UNpcChatSubsystem::FailureFromHttpCode(429) == ENpcChatFailure::Quota
			&& UNpcChatSubsystem::FailureFromHttpCode(504) == ENpcChatFailure::Timeout
			&& UNpcChatSubsystem::FailureFromHttpCode(0) == ENpcChatFailure::Network
			&& UNpcChatSubsystem::FailureFromHttpCode(500) == ENpcChatFailure::Service,
			TEXT("parse: transport status codes map to reasons"));
	}

	// --- 13) MODES. An unconfigured build must be honestly unavailable.
	if (Chat)
	{
		IConsoleVariable* ModeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.Mode"));
		IConsoleVariable* UrlVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.ProxyUrl"));
		R.Check(ModeVar && UrlVar, TEXT("mode: sib.Chat.Mode and sib.Chat.ProxyUrl exist"));

		if (ModeVar && UrlVar)
		{
			const int32 SavedMode = ModeVar->GetInt();
			const FString SavedUrl = UrlVar->GetString();

			ModeVar->Set(1);
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Mock && UNpcChatSubsystem::IsMock(),
				TEXT("mode: 1 is mock, and IsMock agrees"));

			ModeVar->Set(0);
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Disabled && !UNpcChatSubsystem::IsMock(),
				TEXT("mode: 0 is disabled"));

			FOnNpcChatReply Ignored;
			R.Check(Chat->Send(TEXT("kaia"), TEXT("anyone there?"), Ignored) == 0 && !Chat->IsBusy(),
				TEXT("mode: disabled refuses before an id is spent"));

			/* THE ONE THAT PROTECTS A RELEASE. Proxy with no URL is Disabled, not a
			   half-configured build firing requests at nothing - and crucially not a
			   silent fallback to canned lines dressed up as a character talking. */
			ModeVar->Set(2);
			UrlVar->Set(TEXT(""));
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Disabled,
				TEXT("mode: proxy without a URL is disabled, never mock"));
			R.Check(!UNpcChatSubsystem::IsMock(),
				TEXT("mode: an unconfigured proxy build never claims to be the mock"));

			UrlVar->Set(TEXT("https://example.invalid/v1/npc-chat"));
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Proxy,
				TEXT("mode: proxy with a URL is proxy"));

			ModeVar->Set(SavedMode);
			UrlVar->Set(*SavedUrl);
			R.Check(UNpcChatSubsystem::CurrentMode() != ENpcChatMode::Proxy,
				TEXT("mode: restored to the environment's setting"));
		}
	}

	/* TEAR THE WORLD DOWN, NOT JUST THE GAME INSTANCE (the exit-3 lesson).

	   InitializeStandalone creates a world and has no implicit counterpart, so without
	   CleanupWorld its subsystems are destroyed while still initialised. That is a
	   handled ensure, which leaves the process exiting 3 with every assertion green —
	   the worst possible shape for a gate, because the log says PASSED and the CI check
	   says failed. */
	if (UWorld* GateWorld = GI->GetWorld())
	{
		GateWorld->CleanupWorld();
	}
	GI->Shutdown();

	UE_LOG(LogNpcChatSmoke, Display, TEXT("=== NPC chat smoke test: %d failure(s) ==="), R.Failures);
	return R.Failures == 0 ? 0 : 1;
}
