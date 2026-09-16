// NpcChatSmokeTestCommandlet.cpp — the NPC chat gate. See header.
//
// NAMED namespace + function-scoped `using` (unity-build safe), no variable
// shadowing (warnings-as-errors). Editor-closed.

#include "NpcChatSmokeTestCommandlet.h"

#include "DancerAgentComponent.h"
#include "NpcChatSubsystem.h"
#include "PowerGrant.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "CoreGlobals.h"          // GFrameCounter — see AdvanceTime
#include "HAL/IConsoleManager.h"
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

		// -1 would mean the variable is missing entirely, which is also a failure:
		// the routing in TryInteract reads it, and a missing switch is not an off one.
		R.Check(ReadCVarInt(TEXT("sib.Chat.Enable"), -1) == 0,
			TEXT("sib.Chat.Enable exists and defaults to 0"));
		R.Check(ReadCVarInt(TEXT("sib.Chat.MockFail"), -1) == 0,
			TEXT("sib.Chat.MockFail defaults to 0 (no forced failures in a normal run)"));
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
		}
	}
	else
	{
		R.Check(false, TEXT("routing: the standalone game instance has a world"));
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
