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
#include "ChatEasterEggs.h"
#include "ChatDilemmas.h"
#include "ChatDeeds.h"
#include "ProgressionTypes.h"   // FProgressionState, for the eggs on a throwaway state

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "CoreGlobals.h"          // GFrameCounter — see AdvanceTime
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"          // the council request body, read back
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

	/* --- 1) THE SHIPPING PROMISE. Off in the build that ships.

	   IT USED TO SAY "both switches off, by default, everywhere", and that is no longer
	   true: sib.Chat.Enable now defaults to 1 and sib.Chat.Mode to 2 in every
	   configuration except Shipping, so a developer gets a working conversation without
	   typing anything. The promise itself did not move - the store build is still
	   chat-free - but what keeps it moved, from a plain 0 to two #if UE_BUILD_SHIPPING
	   blocks in InteractorComponent.cpp and NpcChatSubsystem.cpp.

	   THIS GATE CANNOT SEE THOSE BLOCKS. It runs in an editor build, where the guard is
	   compiled out by definition, so the checks below are what remains provable from
	   here: the component default, the variable existing, and nothing in the project's
	   own Config/ turning it on. The compile-time half is reviewed by reading it. */
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

		/* Reported, not asserted: these are about the environment this run happens in.
		   In an editor build both now read their development defaults - Enable 1,
		   Mode 2 - and that is expected, not a finding. */
		UE_LOG(LogNpcChatSmoke, Display, TEXT("  [note] sib.Chat.Enable reads %d in this process"),
			ReadCVarInt(TEXT("sib.Chat.Enable"), -1));
		UE_LOG(LogNpcChatSmoke, Display, TEXT("  [note] sib.Chat.Mode reads %d in this process"),
			ReadCVarInt(TEXT("sib.Chat.Mode"), -1));

		R.Check(ReadCVarInt(TEXT("sib.Chat.MockFail"), -1) == 0,
			TEXT("no forced failure is active in this environment"));
	}

	/* --- SANDBOX THE STORE BEFORE ANYTHING CAN WRITE TO IT.

	   Set before the game instance exists, because creating it constructs the subsystem
	   and the subsystem loads on Initialize. Everything after this point - RecordTurn,
	   SaveNow, ForgetCharacter, ForgetAll - lands on a slot nobody plays.

	   This is not hypothetical tidiness. An earlier version of this gate ran ForgetAll
	   against the live slot and deleted a real conversation history minutes before a
	   walk-test, and the missing memories looked like a bug in the feature. */
	const FString RealSlot = TEXT("NpcChat");
	const bool bRealSlotExistedBefore = FSibeliusSaveIO::Has(RealSlot);
	UNpcChatSubsystem::SetSlotNameForTesting(TEXT("NpcChatSmokeSandbox"));

	/* AND THE GATE OWNS ITS MODE, for the same reason it owns its slot.

	   ConsoleVariables.ini is read by this commandlet as well as the editor, so a
	   developer who points their machine at a local backend - which is the supported
	   way to walk-test - would otherwise send every behavioural assertion below down
	   the HTTP path and fail fifteen of them. Ambient configuration is the subject of
	   exactly one check in this gate, the shipped-config one above. Everywhere else it
	   is an input to control, not a condition to inherit. */
	IConsoleVariable* GateModeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.Mode"));
	IConsoleVariable* GateUrlVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.ProxyUrl"));
	const int32 AmbientMode = GateModeVar ? GateModeVar->GetInt() : 1;
	const FString AmbientUrl = GateUrlVar ? GateUrlVar->GetString() : FString();
	if (GateModeVar) { GateModeVar->Set(1); }
	if (GateUrlVar) { GateUrlVar->Set(TEXT("")); }

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

			/* CANONICAL CASING, because this id goes on the wire to an allowlist. The
			   first live round trip sent "Kaia" and worked only because that backend
			   folds case before its lookup. FName comparison is case-insensitive, so
			   this costs no already-saved conversation. */
			Agent->ChatId = NAME_None;
			Agent->AgentName = TEXT("Kaia");
			R.Check(Agent->ResolveChatId().ToString() == TEXT("kaia"),
				TEXT("identity: a display-name fallback is lowercased for the wire"));
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

		// What she knew of him, for the welcome back (ChatDeeds.h): noted when she answers, and saved.
		const FDateTime LastAnswered(2026, 10, 15, 9, 30, 0);
		const TArray<FString> KnownDeeds = { TEXT("battle_won"), TEXT("chess_wins:2") };
		Chat->NoteAnswered(Nyra, KnownDeeds, LastAnswered);
		R.Check(Chat->FindHistory(Nyra) && Chat->FindHistory(Nyra)->LastAnsweredUtc == LastAnswered
				&& Chat->FindHistory(Nyra)->Turns.Num() == 1,
			TEXT("memory: an answer notes when, and leaves the turns alone"));

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
				const FNpcChatHistory* NyraBack = Chat->FindHistory(Nyra);
				R.Check(NyraBack && NyraBack->LastAnsweredUtc == LastAnswered && NyraBack->DeedsKnown == KnownDeeds,
					TEXT("save: when she last answered him, and what she knew, survive the round trip"));

				/* v1, from before 2026-10-19, has neither: it is still read, with both unknown, so
				   nobody's conversations are forgotten by the update. */
				ReadBack->SaveVersion = 1;
				R.Check(UNpcChatSaveGame::CurrentSaveVersion == 2 && Chat->ApplySaveObject(ReadBack),
					TEXT("save: a v1 save is still read"));

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

	// --- 12b) THE COUNCIL: its contract, its answer, and one mock meeting.
	{
		TArray<FNpcCouncilLine> Table;
		for (int32 i = 0; i < UNpcChatSubsystem::CouncilLinesKept + 5; ++i)
		{
			Table.Add({ i % 2 ? TEXT("kaia") : TEXT("leonard"), TEXT("x"), FString::Printf(TEXT("line %d"), i) });
		}
		const FString Body = UNpcChatSubsystem::BuildCouncilRequestJson(
			TEXT("council-request-1"), TEXT("hello, table"), TEXT("L_Office"), Table);

		TSharedPtr<FJsonObject> Root;
		const bool bParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) && Root.IsValid();
		R.Check(bParsed, TEXT("council: the request body is JSON"));
		if (bParsed)
		{
			const TArray<TSharedPtr<FJsonValue>>& Turns = Root->GetArrayField(TEXT("recentTurns"));
			R.Check(Turns.Num() == UNpcChatSubsystem::CouncilLinesKept,
				TEXT("council: only the most recent 30 lines are sent (the server reads 30, refuses past 64)"));
			R.Check(Turns.Num() > 0 && Turns.Last()->AsObject()->GetStringField(TEXT("text")) == TEXT("line 34"),
				TEXT("council: the lines kept are the newest, oldest first"));
			R.Check(Turns.Num() > 0 && Turns[0]->AsObject()->HasField(TEXT("speaker"))
					&& !Turns[0]->AsObject()->HasField(TEXT("role")),
				TEXT("council: history names the speaker, not a user/model role"));
			R.Check(Root->GetStringField(TEXT("message")) == TEXT("hello, table")
					&& Root->GetObjectField(TEXT("storyContext"))->GetStringField(TEXT("locationId")) == TEXT("L_Office"),
				TEXT("council: message and location travel"));
		}

		TArray<FNpcCouncilLine> Lines;
		FString Care;
		ENpcChatFailure Failure = ENpcChatFailure::None;

		R.Check(UNpcChatSubsystem::ParseCouncilJson(
				TEXT("{\"status\":\"ok\",\"replies\":[")
				TEXT("{\"characterId\":\"nyra\",\"displayName\":\"Nyra\",\"text\":\"Nice job, Leonard!\"},")
				TEXT("{\"characterId\":\"isla\",\"displayName\":\"Isla\",\"text\":\"   \"},")
				TEXT("{\"characterId\":\"elise\",\"displayName\":\"Elise\",\"text\":\"A masterpiece.\"}]}"),
				Lines, Care, Failure)
			&& Lines.Num() == 2 && Lines[0].DisplayName == TEXT("Nyra") && Lines[1].SpeakerId == TEXT("elise")
			&& Care.IsEmpty() && Failure == ENpcChatFailure::None,
			TEXT("council parse: replies arrive in order, and a blank line is dropped, not drawn"));

		R.Check(UNpcChatSubsystem::ParseCouncilJson(
				TEXT("{\"status\":\"ok\",\"care\":true,\"reply\":\"We are here.\",\"replies\":[]}"), Lines, Care, Failure)
			&& Care == TEXT("We are here.") && Lines.Num() == 0,
			TEXT("council parse: the care reply arrives on its own"));

		R.Check(!UNpcChatSubsystem::ParseCouncilJson(TEXT("{\"status\":\"ok\",\"replies\":[]}"), Lines, Care, Failure)
			&& Failure == ENpcChatFailure::Service,
			TEXT("council parse: a table where nobody spoke is a failure, not an empty turn"));

		R.Check(!UNpcChatSubsystem::ParseCouncilJson(
				TEXT("{\"status\":\"unavailable\",\"reason\":\"quota\"}"), Lines, Care, Failure)
			&& Failure == ENpcChatFailure::Quota,
			TEXT("council parse: a declared quota failure keeps its reason"));

		// One mock meeting, end to end through the subsystem.
		if (Chat && UNpcChatSubsystem::IsMock())
		{
			Chat->ForgetCouncil();
			bool bFired = false;
			TArray<FNpcCouncilLine> Got;
			FOnNpcCouncilReply OnCouncil;
			OnCouncil.BindLambda([&](const TArray<FNpcCouncilLine>& In, const FString&, ENpcChatFailure F)
			{
				bFired = true;
				Got = In;
				Failure = F;
			});

			const int32 Id = Chat->SendCouncil(TEXT("good morning, everyone"), OnCouncil);
			R.Check(Id != 0 && Chat->IsBusy(), TEXT("council mock: a meeting turn is sent"));

			FOnNpcChatReply Ignored;
			R.Check(Chat->Send(TEXT("kaia"), TEXT("me too"), Ignored) == 0,
				TEXT("council mock: a dancer cannot be asked while the table is answering (one slot)"));

			AdvanceTime(GI, 2.5f);
			R.Check(bFired && Failure == ENpcChatFailure::None && Got.Num() == 5,
				TEXT("council mock: all five answer"));

			Chat->RecordCouncilTurn(TEXT("good morning, everyone"), Got);
			R.Check(Chat->GetCouncilHistory().Num() == 6
					&& Chat->GetCouncilHistory()[0].SpeakerId == TEXT("leonard"),
				TEXT("council mock: the turn is recorded as Leonard's line and five answers"));

			bool bLate = false;
			FOnNpcCouncilReply OnLate;
			OnLate.BindLambda([&bLate](const TArray<FNpcCouncilLine>&, const FString&, ENpcChatFailure) { bLate = true; });
			Chat->Cancel(Chat->SendCouncil(TEXT("never mind"), OnLate));
			AdvanceTime(GI, 2.5f);
			R.Check(!bLate && !Chat->IsBusy(), TEXT("council mock: a cancelled meeting turn never delivers"));
			Chat->ForgetCouncil();
		}
	}

	/* --- 12c) EASTER EGGS (ChatEasterEggs.h). The player's words decide; the request carries
	   the event. Tested on a THROWAWAY FProgressionState: the progression slot has no sandbox,
	   and Commit would write a real player's save. */
	{
		using namespace ChatEasterEggs;

		R.Check(MentionsPigeons(TEXT("what about the PIGEONS?")) && MentionsPigeons(TEXT("a pigeon"))
				&& !MentionsPigeons(TEXT("what about the crumbs?")),
			TEXT("eggs: pigeons are matched loosely, and only pigeons"));

		R.Check(NamesHimselfLeonard(TEXT("My name is Leonard!")) && NamesHimselfLeonard(TEXT("i'm leonard"))
				&& NamesHimselfLeonard(TEXT("I\x2019m Leonard.")) && NamesHimselfLeonard(TEXT("please, call me   Leonard"))
				&& NamesHimselfLeonard(TEXT("I am Leonard, not Programmer")),
			TEXT("eggs: 'my name is Leonard' is matched however it is typed"));
		R.Check(!NamesHimselfLeonard(TEXT("I am Leonardo")) && !NamesHimselfLeonard(TEXT("who is Leonard?"))
				&& !NamesHimselfLeonard(TEXT("slim leonard")),
			TEXT("eggs: near misses do not count"));

		FProgressionState Fresh;
		FCheck First = EvaluateChatLine(Fresh, FName(TEXT("kaia")), TEXT("tell me about the pigeons"));
		R.Check(First.bCountsTowardPigeons && First.Egg.IsNone() && First.StoryEvent.IsEmpty(),
			TEXT("eggs: the first pigeon question counts, and finds nothing yet"));
		R.Check(!EvaluateChatLine(Fresh, FName(TEXT("isla")), TEXT("pigeons?")).bCountsTowardPigeons,
			TEXT("eggs: pigeons asked of another agent do not count"));
		R.Check(EvaluateChatLine(Fresh, FName(TEXT("Kaia")), TEXT("pigeons?")).bCountsTowardPigeons,
			TEXT("eggs: the agent id is matched without case"));

		FProgressionState TwoAsked;
		TwoAsked.BumpStat(KaiaPigeonQuestionsStat, PigeonQuestionsNeeded - 1);
		const FCheck Third = EvaluateChatLine(TwoAsked, FName(TEXT("kaia")), TEXT("the pigeons again?"));
		R.Check(Third.Egg == PigeonLedger && Third.StoryEvent == TEXT("egg_pigeon_ledger"),
			TEXT("eggs: the third pigeon question finds the Pigeon Ledger and tells the server"));

		FProgressionState Found;
		Found.BumpStat(KaiaPigeonQuestionsStat, 10);
		Found.Claim(PigeonLedger);
		Found.Claim(NotProgrammer);
		R.Check(EvaluateChatLine(Found, FName(TEXT("kaia")), TEXT("pigeons!")).Egg.IsNone()
				&& EvaluateCouncilLine(Found, TEXT("my name is Leonard")).Egg.IsNone(),
			TEXT("eggs: once per save - a found egg is never found again"));

		const FCheck Named = EvaluateCouncilLine(Fresh, TEXT("My name is Leonard."));
		R.Check(Named.Egg == NotProgrammer && Named.StoryEvent == TEXT("egg_not_programmer"),
			TEXT("eggs: telling the council his name finds Not Programmer"));
		R.Check(EvaluateCouncilLine(Fresh, TEXT("what should I do next?")).Egg.IsNone(),
			TEXT("eggs: an ordinary council line finds nothing"));

		// Egg 2, Finally in Stock: the same thing asked of Aisling five times.
		R.Check(RequestedItem(TEXT("Do you have any bananas?")) == TEXT("banana")
				&& RequestedItem(TEXT("can I get a banana")) == TEXT("banana")
				&& RequestedItem(TEXT("where are the BANANAS")) == TEXT("banana")
				&& RequestedItem(TEXT("I'd like some banana, please")) == TEXT("banana"),
			TEXT("eggs: one item, asked four different ways, is the same item"));
		// Walt's own lines from PIE, which the first matcher missed.
		R.Check(RequestedItem(TEXT("are you really sure you don't have bananas?")) == TEXT("banana")
				&& RequestedItem(TEXT("are you absolutely sure you don\x2019t have bananas?")) == TEXT("banana")
				&& RequestedItem(TEXT("i know you have bananas")) == TEXT("banana")
				&& RequestedItem(TEXT("don't you have any bananas")) == TEXT("banana")
				&& RequestedItem(TEXT("you do not have bananas?")) == TEXT("banana"),
			TEXT("eggs: insisting counts - 'you don't have', 'I know you have', 'don't you have'"));
		R.Check(RequestedItem(TEXT("do you have coffee")) != RequestedItem(TEXT("do you have bananas"))
				&& RequestedItem(TEXT("hello Aisling, how are you?")).IsEmpty(),
			TEXT("eggs: different items differ, and small talk asks for nothing"));

		const TArray<FString> FourAsks = { TEXT("do you have bananas?"), TEXT("any bananas today?"),
			TEXT("can I get a banana"), TEXT("where are the bananas"), TEXT("I want a banana") };
		// "any bananas today?" has no ask-phrase, so it does not count: four of these five do.
		R.Check(EvaluateChatLine(Fresh, FName(TEXT("aisling")), TEXT("do you have bananas?"),
				{ TEXT("do you have bananas?"), TEXT("can I get a banana"), TEXT("where are the bananas") }).Egg.IsNone(),
			TEXT("eggs: the fourth ask for the same thing finds nothing yet"));
		const FCheck Fifth = EvaluateChatLine(Fresh, FName(TEXT("aisling")), TEXT("do you have bananas?"), FourAsks);
		R.Check(Fifth.Egg == FinallyInStock && Fifth.StoryEvent == TEXT("egg_finally_in_stock"),
			TEXT("eggs: the fifth ask for the same thing finds Finally in Stock and tells the server"));
		R.Check(EvaluateChatLine(Fresh, FName(TEXT("aisling")), TEXT("do you have coffee?"), FourAsks).Egg.IsNone(),
			TEXT("eggs: asking for something else resets nothing and finds nothing"));
		R.Check(EvaluateChatLine(Fresh, FName(TEXT("kaia")), TEXT("do you have bananas?"), FourAsks).Egg.IsNone(),
			TEXT("eggs: the same asks of another agent find nothing"));
		FProgressionState Stocked;
		Stocked.Claim(FinallyInStock);
		R.Check(EvaluateChatLine(Stocked, FName(TEXT("aisling")), TEXT("do you have bananas?"), FourAsks).Egg.IsNone(),
			TEXT("eggs: Finally in Stock is found once per save"));

		// Egg 3, Fold Early (Isla), and egg 4, The Memo Poetry Reading (Nyra).
		R.Check(SaysFold(TEXT("I fold.")) && SaysFold(TEXT("FOLD!")) && SaysFold(TEXT("ok, folding"))
				&& !SaysFold(TEXT("put it in the folder")) && !SaysFold(TEXT("unfold the map")),
			TEXT("eggs: 'fold' is matched as a whole word - folder and unfold do not count"));
		R.Check(SaysPoetry(TEXT("do you like poetry?")) && SaysPoetry(TEXT("write me a poem"))
				&& !SaysPoetry(TEXT("poetic justice")),
			TEXT("eggs: 'poetry' and 'poem' are matched as whole words"));
		const FCheck Folded = EvaluateChatLine(Fresh, FName(TEXT("isla")), TEXT("I fold"));
		R.Check(Folded.Egg == FoldEarly && Folded.StoryEvent == TEXT("egg_fold_early"),
			TEXT("eggs: 'fold' to Isla finds Fold Early and tells the server"));
		const FCheck Poetry = EvaluateChatLine(Fresh, FName(TEXT("nyra")), TEXT("tell me about poetry"));
		R.Check(Poetry.Egg == MemoPoetry && Poetry.StoryEvent == TEXT("egg_memo_poetry"),
			TEXT("eggs: 'poetry' to Nyra finds The Memo Poetry Reading and tells the server"));
		R.Check(EvaluateChatLine(Fresh, FName(TEXT("nyra")), TEXT("I fold")).Egg.IsNone()
				&& EvaluateChatLine(Fresh, FName(TEXT("isla")), TEXT("poetry")).Egg.IsNone(),
			TEXT("eggs: each word only works on its own agent"));
		FProgressionState BothFound;
		BothFound.Claim(FoldEarly);
		BothFound.Claim(MemoPoetry);
		R.Check(EvaluateChatLine(BothFound, FName(TEXT("isla")), TEXT("fold")).Egg.IsNone()
				&& EvaluateChatLine(BothFound, FName(TEXT("nyra")), TEXT("poetry")).Egg.IsNone(),
			TEXT("eggs: Fold Early and the Memo Poetry Reading are found once per save"));

		/* The dilemmas (ChatDilemmas.h), on throwaway states too: she asks once, after they have
		   talked; it stays open until he advises her; the advice is one of her two answers. */
		{
			using namespace ChatDilemmas;
			const FDilemma* Isla = Find(FName(TEXT("isla")));
			R.Check(Isla && Find(FName(TEXT("kaia"))) && Find(FName(TEXT("nyra"))) && Find(FName(TEXT("aisling")))
					&& Find(FName(TEXT("elise"))) && !Find(FName(TEXT("mrs_hall"))),
				TEXT("dilemmas: all five agents have one, and no one else"));
			if (Isla)
			{
				FProgressionState New;
				R.Check(!ShouldAsk(New, FName(TEXT("isla")), TurnsBeforeAsking - 1) && ShouldAsk(New, FName(TEXT("isla")), TurnsBeforeAsking)
						&& !IsOpen(New, FName(TEXT("isla"))),
					TEXT("dilemmas: not in their first talk, then she asks"));
				FProgressionState Asked;
				Asked.Claim(AskedGrant(*Isla));
				R.Check(!ShouldAsk(Asked, FName(TEXT("isla")), 16) && IsOpen(Asked, FName(TEXT("isla")))
						&& StoryEvent(*Isla) == TEXT("dilemma_isla_versions"),
					TEXT("dilemmas: asked once, then open - her requests carry its event"));
				FProgressionState Answered = Asked;
				Answered.Claim(AdvisedGrant(*Isla, TEXT("delete")));
				R.Check(!IsOpen(Answered, FName(TEXT("isla"))) && AdviceGiven(Answered, *Isla) == TEXT("delete")
						&& AdviceFacts(Answered) == TArray<FString>{ TEXT("isla_versions:delete") } && AdviceFacts(Asked).Num() == 0,
					TEXT("dilemmas: once he has advised her it closes, and the facts carry what he said"));
				R.Check(ValidChoice(*Isla, TEXT(" KEEP ")) == TEXT("keep") && ValidChoice(*Isla, TEXT("sing")).IsEmpty()
						&& ChoiceNamedIn(*Isla, TEXT("Keep them all.")) == TEXT("keep")
						&& ChoiceNamedIn(*Isla, TEXT("the keeper of nothing")).IsEmpty(),
					TEXT("dilemmas: only her two answers count, as whole words"));
			}
		}

		// Facts: his playthrough, as the agents may know it (backend/facts.js reads these).
		{
			FNpcStoryFacts Facts;
			Facts.Powers = { TEXT("CODE VISION"), TEXT("REFACTOR") };
			Facts.Sauce = 12;
			Facts.SauceEarned = 40;
			Facts.RefusersFought = 41;
			Facts.BooksCollected = 9;
			Facts.ChaptersCompleted = 2;
			Facts.Objective = TEXT("Hold [V] and read the parts.");

			TSharedPtr<FJsonObject> Root;
			const FString Body = UNpcChatSubsystem::BuildRequestJson(
				TEXT("facts-request-1"), TEXT("kaia"), TEXT("what now?"), TEXT("L_Office_v02"), {}, FString(), &Facts);
			const TSharedPtr<FJsonObject>* Written = nullptr;
			const bool bHasFacts = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) && Root.IsValid()
				&& Root->GetObjectField(TEXT("storyContext"))->TryGetObjectField(TEXT("facts"), Written);
			R.Check(bHasFacts && (*Written)->GetArrayField(TEXT("powers")).Num() == 2
					&& (*Written)->GetIntegerField(TEXT("sauce")) == 12
					&& (*Written)->GetIntegerField(TEXT("refusersFought")) == 41
					&& (*Written)->GetStringField(TEXT("objective")) == TEXT("Hold [V] and read the parts."),
				TEXT("facts: a chat request carries his powers, numbers and the objective line"));

			const FString CouncilBody = UNpcChatSubsystem::BuildCouncilRequestJson(
				TEXT("facts-request-2"), TEXT("hello"), TEXT("L_Office_v02"), {}, FString(), &Facts);
			R.Check(CouncilBody.Contains(TEXT("\"facts\"")) && CouncilBody.Contains(TEXT("\"chaptersCompleted\":2")),
				TEXT("facts: a council request carries them too"));

			// The travel button: the open places go out, and an offer comes back.
			FNpcStoryFacts Travelling = Facts;
			Travelling.Destinations = { TEXT("L_City") };
			R.Check(UNpcChatSubsystem::BuildRequestJson(TEXT("facts-request-4"), TEXT("nyra"), TEXT("where now?"),
					TEXT("L_Office_v02"), {}, FString(), &Travelling).Contains(TEXT("\"destinations\":[\"L_City\"]")),
				TEXT("travel: the request says which places the travel keys reach now"));
			R.Check(UNpcChatSubsystem::ParseTravelOffer(TEXT("{\"status\":\"ok\",\"reply\":\"Go!\",\"travel\":\"L_City\"}")) == TEXT("L_City")
					&& UNpcChatSubsystem::ParseTravelOffer(TEXT("{\"status\":\"ok\",\"reply\":\"Hello.\"}")).IsEmpty()
					&& UNpcChatSubsystem::ParseTravelOffer(TEXT("not json")).IsEmpty(),
				TEXT("travel: an offer is read when present, and nothing otherwise"));

			// The dilemmas (ChatDilemmas.h): his advice goes out in the facts, and comes back.
			FNpcStoryFacts Advised = Facts;
			Advised.Advice = { TEXT("isla_versions:keep") };
			R.Check(UNpcChatSubsystem::BuildRequestJson(TEXT("facts-request-5"), TEXT("isla"), TEXT("hi"),
					TEXT("L_Office_v02"), {}, FString(), &Advised).Contains(TEXT("\"advice\":[\"isla_versions:keep\"]"))
					&& !UNpcChatSubsystem::BuildRequestJson(TEXT("facts-request-6"), TEXT("isla"), TEXT("hi"),
						TEXT("L_Office_v02"), {}, FString(), &Facts).Contains(TEXT("\"advice\"")),
				TEXT("dilemmas: the facts carry his advice once he has given some, and no field before"));
			R.Check(UNpcChatSubsystem::ParseAdvice(TEXT("{\"status\":\"ok\",\"reply\":\"Fine.\",\"advice\":\"keep\"}")) == TEXT("keep")
					&& UNpcChatSubsystem::ParseAdvice(TEXT("{\"status\":\"ok\",\"reply\":\"Hello.\"}")).IsEmpty()
					&& UNpcChatSubsystem::ParseAdvice(TEXT("not json")).IsEmpty(),
				TEXT("dilemmas: advice is read when present, and nothing otherwise"));

			// The switch for what only new builds do (backend/curiosity.js).
			R.Check(UNpcChatSubsystem::BuildRequestJson(TEXT("features-request-1"), TEXT("aisling"), TEXT("hi"),
					TEXT("L_Office_v02"), {}).Contains(TEXT("\"features\":[\"human_questions\"]")),
				TEXT("curiosity: every chat request says this build may ask what being human is like"));

			FNpcStoryFacts NoObjective = Facts;
			NoObjective.Objective.Reset();
			R.Check(!UNpcChatSubsystem::BuildRequestJson(TEXT("facts-request-3"), TEXT("kaia"), TEXT("hi"),
					TEXT("L_Office_v02"), {}, FString(), &NoObjective).Contains(TEXT("\"objective\"")),
				TEXT("facts: no objective on screen, no objective field"));

			/* What she remembers (ChatDeeds.h, backend/memory.js): his deeds from the save, what is
			   new to her, and how long since she last answered him. */
			{
				FProgressionState Done;
				Done.Claim(TEXT("Battle.Won"));   // ABattleArrival::BattleWonGrant, as the save spells it
				Done.BumpStat(SibeliusStats::ChessWins, 3);
				Done.RaiseStat(SibeliusStats::PinballBest, 48200);
				Done.BumpStat(SibeliusStats::GrandPrixRaces);
				R.Check(ChatDeeds::FromState(Done) == TArray<FString>{ TEXT("battle_won"), TEXT("grand_prix_races:1"),
						TEXT("chess_wins:3"), TEXT("pinball_best:48200") }
						&& ChatDeeds::FromState(FProgressionState()).Num() == 0,
					TEXT("deeds: read from the save in a fixed order, and nothing done is nothing sent"));

				const TArray<FString> Now = { TEXT("battle_won"), TEXT("chess_wins:3") };
				R.Check(ChatDeeds::NewSince(Now, { TEXT("battle_won"), TEXT("chess_wins:2") }) == TArray<FString>{ TEXT("chess_wins:3") }
						&& ChatDeeds::NewSince(Now, Now).Num() == 0,
					TEXT("deeds: a new deed, or a count that grew, is news; the rest is not"));

				const FDateTime Then(2026, 10, 15, 9, 30, 0);
				R.Check(ChatDeeds::HoursBetween(Then, Then + FTimespan::FromHours(96)) == 96
						&& ChatDeeds::HoursBetween(Then, Then - FTimespan::FromHours(5)) == 0
						&& ChatDeeds::HoursBetween(FDateTime(1, 1, 1), FDateTime(9999, 1, 1)) == ChatDeeds::MaxHoursAway,
					TEXT("deeds: hours away are whole, never negative, and capped"));

				FNpcStoryFacts Remembered = Facts;
				UNpcChatSubsystem::FillMemoryFacts(Remembered, nullptr, Now, Then);
				const bool bStranger = Remembered.Deeds == Now && Remembered.NewDeeds.Num() == 0 && Remembered.HoursAway == -1;
				FNpcChatHistory Unanswered;   // a v1 history: turns, no answered time
				UNpcChatSubsystem::FillMemoryFacts(Remembered, &Unanswered, Now, Then);
				const bool bOldSave = Remembered.NewDeeds.Num() == 0 && Remembered.HoursAway == -1;
				FNpcChatHistory Known;
				Known.LastAnsweredUtc = Then;
				Known.DeedsKnown = { TEXT("battle_won"), TEXT("chess_wins:2") };
				UNpcChatSubsystem::FillMemoryFacts(Remembered, &Known, Now, Then + FTimespan::FromHours(96));
				R.Check(bStranger && bOldSave && Remembered.HoursAway == 96 && Remembered.NewDeeds == TArray<FString>{ TEXT("chess_wins:3") },
					TEXT("deeds: no absence or news without a time she answered him; with one, both"));

				const FString MemoryBody = UNpcChatSubsystem::BuildRequestJson(TEXT("memory-request-1"), TEXT("elise"), TEXT("hi"),
					TEXT("L_Office_v02"), {}, FString(), &Remembered);
				R.Check(MemoryBody.Contains(TEXT("\"deeds\":[\"battle_won\",\"chess_wins:3\"]"))
						&& MemoryBody.Contains(TEXT("\"newDeeds\":[\"chess_wins:3\"]")) && MemoryBody.Contains(TEXT("\"hoursAway\":96")),
					TEXT("deeds: a chat request carries his deeds, the news and the hours away"));
				const FString Plain = UNpcChatSubsystem::BuildRequestJson(TEXT("memory-request-2"), TEXT("elise"), TEXT("hi"),
					TEXT("L_Office_v02"), {}, FString(), &Facts);
				const FString Council = UNpcChatSubsystem::BuildCouncilRequestJson(TEXT("memory-request-3"), TEXT("hi"),
					TEXT("L_Office_v02"), {}, FString(), &Facts);
				R.Check(!Plain.Contains(TEXT("\"deeds\"")) && !Plain.Contains(TEXT("\"hoursAway\"")) && !Council.Contains(TEXT("\"deeds\"")),
					TEXT("deeds: none to send, no fields - the facts Valve's build and the council send are unchanged"));
			}
		}

		// The event rides in storyContext, and only when there is one.
		auto StoryOf = [](const FString& Body) -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Root;
			return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) && Root.IsValid()
				? Root->GetObjectField(TEXT("storyContext")) : nullptr;
		};
		const TSharedPtr<FJsonObject> WithEvent = StoryOf(UNpcChatSubsystem::BuildRequestJson(
			TEXT("egg-request-1"), TEXT("kaia"), TEXT("pigeons?"), TEXT("L_Office_v02"), {}, TEXT("egg_pigeon_ledger")));
		const TSharedPtr<FJsonObject> Without = StoryOf(UNpcChatSubsystem::BuildRequestJson(
			TEXT("egg-request-2"), TEXT("kaia"), TEXT("hello"), TEXT("L_Office_v02"), {}));
		R.Check(WithEvent.IsValid() && WithEvent->GetStringField(TEXT("event")) == TEXT("egg_pigeon_ledger"),
			TEXT("eggs: a chat request carries the event"));
		R.Check(Without.IsValid() && !Without->HasField(TEXT("event")),
			TEXT("eggs: an ordinary request carries no event field at all"));
		const TSharedPtr<FJsonObject> CouncilEvent = StoryOf(UNpcChatSubsystem::BuildCouncilRequestJson(
			TEXT("egg-request-3"), TEXT("my name is Leonard"), TEXT("L_Office_v02"), {}, TEXT("egg_not_programmer")));
		R.Check(CouncilEvent.IsValid() && CouncilEvent->GetStringField(TEXT("event")) == TEXT("egg_not_programmer"),
			TEXT("eggs: a council request carries the event"));
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

			/* THE ONE THE CONSOLE CAUSED. `sib.Chat.ProxyUrl http://host/path` sets the
			   variable to "http:" because // opens a comment, and the failure then
			   arrives as a network error indistinguishable from a dead server. */
			UrlVar->Set(TEXT("http:"));
			R.Check(!UNpcChatSubsystem::IsUsableProxyUrl(TEXT("http:")),
				TEXT("url: a console-truncated URL is rejected, not dialled"));
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Disabled,
				TEXT("url: a truncated URL disables chat instead of failing as a connection"));
			/* SCHEME-LESS IS THE SUPPORTED FORM NOW, because "//" opens a comment in the
			   console AND in ConsoleVariables.ini - there is no way to write a whole URL
			   in either. Loopback gets http; anything else gets https, so a typo cannot
			   quietly downgrade a remote backend to clear text. */
			UrlVar->Set(TEXT("127.0.0.1:8787/v1/npc-chat"));
			R.Check(UNpcChatSubsystem::IsUsableProxyUrl(TEXT("127.0.0.1:8787/v1/npc-chat")),
				TEXT("url: a scheme-less loopback host is accepted"));
			R.Check(UNpcChatSubsystem::ProxyUrl() == TEXT("http://127.0.0.1:8787/v1/npc-chat"),
				TEXT("url: loopback is dialled over http"));
			R.Check(UNpcChatSubsystem::CurrentMode() == ENpcChatMode::Proxy,
				TEXT("url: a scheme-less value still resolves to proxy mode"));

			UrlVar->Set(TEXT("chat.example.com/v1/npc-chat"));
			R.Check(UNpcChatSubsystem::ProxyUrl() == TEXT("https://chat.example.com/v1/npc-chat"),
				TEXT("url: a remote host is dialled over https, never clear text"));
			R.Check(UNpcChatSubsystem::CouncilUrl() == TEXT("https://chat.example.com/v1/npc-council"),
				TEXT("url: the council endpoint is derived from the chat one"));

			UrlVar->Set(TEXT("chat.example.com/some/other/path"));
			R.Check(UNpcChatSubsystem::CouncilUrl().IsEmpty(),
				TEXT("url: a non-standard chat path has no council, rather than a guessed one"));

			ModeVar->Set(0);
			FOnNpcCouncilReply NoCouncil;
			R.Check(Chat->SendCouncil(TEXT("anyone?"), NoCouncil) == 0 && !Chat->IsBusy(),
				TEXT("mode: disabled refuses a council turn before an id is spent"));
			ModeVar->Set(2);
			R.Check(UNpcChatSubsystem::IsUsableProxyUrl(TEXT("http://127.0.0.1:8787/v1/npc-chat")),
				TEXT("url: a whole loopback URL is accepted"));

			ModeVar->Set(SavedMode);
			UrlVar->Set(*SavedUrl);
			R.Check(UNpcChatSubsystem::CurrentMode() != ENpcChatMode::Proxy,
				TEXT("mode: restored to the environment's setting"));
		}
	}

	// --- 14) THE GATE LEAVES NO FOOTPRINTS. Asserted, not assumed.
	{
		FSibeliusSaveIO::Delete(UNpcChatSubsystem::SlotName());
		R.Check(!FSibeliusSaveIO::Has(TEXT("NpcChatSmokeSandbox")),
			TEXT("sandbox: the gate's own slot is cleaned up"));

		UNpcChatSubsystem::SetSlotNameForTesting(FString());
		if (GateModeVar) { GateModeVar->Set(AmbientMode); }
		if (GateUrlVar) { GateUrlVar->Set(*AmbientUrl); }
		R.Check(UNpcChatSubsystem::SlotName() == RealSlot,
			TEXT("sandbox: the slot name is handed back to the real one"));

		/* THE ASSERTION THAT MATTERS. A player's conversations must be exactly as this
		   run found them - present if they were present, absent if they were absent. */
		R.Check(FSibeliusSaveIO::Has(RealSlot) == bRealSlotExistedBefore,
			bRealSlotExistedBefore
				? TEXT("sandbox: the player's conversation slot survived the gate")
				: TEXT("sandbox: the gate did not create a conversation slot"));
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
