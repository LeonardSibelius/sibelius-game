// NpcChatSubsystem.cpp — the mock service behind the chat panel (step 4).

#include "NpcChatSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/Guid.h"
#include "SibeliusSaveIO.h"       // the same disk path Progression uses
#include "UObject/Package.h"       // GetTransientPackage
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "SibeliusGame.h"

namespace
{
	/**
	 * Seconds the mock spends "thinking".
	 *
	 * Not zero, and that is deliberate. A reply that lands in the same frame as the
	 * keypress would let the panel ship without ever showing its waiting state, its
	 * duplicate suppression or its cancel path — the three things a real network call
	 * will exercise on the first day. The mock is here to make those reachable.
	 */
	TAutoConsoleVariable<float> CVarChatMockDelay(
		TEXT("sib.Chat.MockDelay"),
		1.1f,
		TEXT("Seconds the mock NPC chat service waits before answering."),
		ECVF_Default);

	/**
	 * Force a failure reason so the failure copy can be walked without a real outage.
	 * 0 off, 1 quota, 2 network, 3 timeout, 4 blocked, 5 service.
	 */
	TAutoConsoleVariable<int32> CVarChatMockFail(
		TEXT("sib.Chat.MockFail"),
		0,
		TEXT("Force the mock chat service to fail: 1 quota, 2 network, 3 timeout, 4 blocked, 5 service."),
		ECVF_Default);

	/**
	 * 0 disabled, 1 mock, 2 proxy.
	 *
	 * DEFAULTS TO MOCK IN DEVELOPMENT AND DISABLED IN SHIPPING, deliberately. A packaged
	 * build that somehow reached this code with no backend configured must say the
	 * feature is unavailable, not serve five canned lines as though a character were
	 * talking. Off is honest; canned is a lie with a friendly face.
	 */
	TAutoConsoleVariable<int32> CVarChatMode(
		TEXT("sib.Chat.Mode"),
#if UE_BUILD_SHIPPING
		0,
#else
		1,
#endif
		TEXT("Chat service: 0 disabled, 1 mock (canned), 2 proxy (real backend)."),
		ECVF_Default);

	/** The backend endpoint. Public by design - the API key lives on the far side. */
	TAutoConsoleVariable<FString> CVarChatProxyUrl(
		TEXT("sib.Chat.ProxyUrl"),
		TEXT(""),
		TEXT("HTTPS endpoint for the NPC chat backend, e.g. https://host/v1/npc-chat"),
		ECVF_Default);

	/** Client deadline. The backend's own is shorter; this catches a dead connection. */
	constexpr float ChatHttpTimeoutSeconds = 25.0f;
}

ENpcChatMode UNpcChatSubsystem::CurrentMode()
{
	const int32 Raw = FMath::Clamp(CVarChatMode.GetValueOnGameThread(), 0, 2);
	const ENpcChatMode Configured = static_cast<ENpcChatMode>(Raw);

	/* PROXY WITHOUT A USABLE URL IS DISABLED, NOT BROKEN. Answering "unavailable" is the
	   right behaviour for an unconfigured build, and it stops a half-configured release
	   from spraying failed requests at nothing. */
	if (Configured == ENpcChatMode::Proxy && !IsUsableProxyUrl(ProxyUrl()))
	{
		return ENpcChatMode::Disabled;
	}
	return Configured;
}

FString UNpcChatSubsystem::ProxyUrl()
{
	return CVarChatProxyUrl.GetValueOnGameThread().TrimStartAndEnd();
}

bool UNpcChatSubsystem::IsUsableProxyUrl(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return false;
	}

	/* THE CONSOLE EATS "//". UE's command parser treats a double slash as the start of a
	   comment, so `sib.Chat.ProxyUrl http://host/path` sets the variable to "http:" and
	   everything after is discarded. libcurl then rejects it - "Port number was not a
	   decimal number" - and the player sees the NETWORK line, which is indistinguishable
	   from the backend being down. That cost a round trip of guessing once.

	   So the shape is checked, and a bad one is named in the log rather than left to
	   fail as a phantom connection error. Set it with quotes, or from
	   ConsoleVariables.ini, which the console tokenizer never touches. */
	const bool bHasScheme = Url.StartsWith(TEXT("http://")) || Url.StartsWith(TEXT("https://"));
	const int32 SchemeEnd = Url.Find(TEXT("://"));
	const bool bHasHost = SchemeEnd != INDEX_NONE && Url.Len() > SchemeEnd + 3;

	if (!bHasScheme || !bHasHost)
	{
		// Once per distinct value: a warning every frame would be its own problem.
		static FString LastComplaint;
		if (LastComplaint != Url)
		{
			LastComplaint = Url;
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Chat] ProxyUrl \"%s\" is not a usable http(s) URL - chat is disabled. ")
				TEXT("The console treats // as a comment: quote the value, or set it in ConsoleVariables.ini."),
				*Url);
		}
		return false;
	}
	return true;
}

bool UNpcChatSubsystem::IsMock()
{
	// One place to answer it, because the panel, the logs and the smoke gate all need
	// the same answer - and the panel's "placeholder replies" notice depends on it.
	return CurrentMode() == ENpcChatMode::Mock;
}

ENpcChatFailure UNpcChatSubsystem::FailureFromReason(const FString& Reason)
{
	// The allowlist from the contract. Anything else is Service on purpose: an
	// unrecognised reason is a backend we do not understand, not a new player-facing
	// category to invent copy for.
	if (Reason.Equals(TEXT("quota"), ESearchCase::IgnoreCase))   { return ENpcChatFailure::Quota; }
	if (Reason.Equals(TEXT("network"), ESearchCase::IgnoreCase)) { return ENpcChatFailure::Network; }
	if (Reason.Equals(TEXT("timeout"), ESearchCase::IgnoreCase)) { return ENpcChatFailure::Timeout; }
	if (Reason.Equals(TEXT("blocked"), ESearchCase::IgnoreCase)) { return ENpcChatFailure::Blocked; }
	return ENpcChatFailure::Service;
}

ENpcChatFailure UNpcChatSubsystem::FailureFromHttpCode(int32 StatusCode)
{
	if (StatusCode == 429) { return ENpcChatFailure::Quota; }
	if (StatusCode == 408 || StatusCode == 504) { return ENpcChatFailure::Timeout; }
	if (StatusCode == 0) { return ENpcChatFailure::Network; }
	return ENpcChatFailure::Service;
}

FString UNpcChatSubsystem::BuildRequestJson(
	const FString& RequestId,
	const FString& CharacterId,
	const FString& Message,
	const FString& LocationId,
	const TArray<FNpcChatTurn>& RecentTurns)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("characterId"), CharacterId);
	Root->SetStringField(TEXT("message"), Message);

	/* storyContext carries a LOCATION and nothing else yet. The allowlisted fact list
	   arrives with the character profiles; sending unfiltered save state would hand the
	   model spoilers the player has not earned, which is the one thing the review is
	   most insistent about. An empty array is the honest placeholder. */
	const TSharedRef<FJsonObject> Story = MakeShared<FJsonObject>();
	Story->SetStringField(TEXT("locationId"), LocationId);
	Story->SetArrayField(TEXT("unlockedFacts"), TArray<TSharedPtr<FJsonValue>>());
	Root->SetObjectField(TEXT("storyContext"), Story);

	TArray<TSharedPtr<FJsonValue>> Turns;
	Turns.Reserve(RecentTurns.Num());
	for (const FNpcChatTurn& Turn : RecentTurns)
	{
		const TSharedRef<FJsonObject> TurnObject = MakeShared<FJsonObject>();
		// "user" and "model" rather than "player" and "agent": the contract speaks the
		// provider's vocabulary so the backend never has to translate roles.
		TurnObject->SetStringField(TEXT("role"), Turn.bFromPlayer ? TEXT("user") : TEXT("model"));
		TurnObject->SetStringField(TEXT("text"), Turn.Text);
		Turns.Add(MakeShared<FJsonValueObject>(TurnObject));
	}
	Root->SetArrayField(TEXT("recentTurns"), Turns);

	/* CONDENSED, NOT PRETTY. Every byte crosses the wire on every turn, and the default
	   writer indents with tabs and spaces a colon. Nobody reads this body by eye except
	   in a gate, and the gate is happier with the shape the server actually receives. */
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

bool UNpcChatSubsystem::ParseReplyJson(const FString& Body, FString& OutReply, ENpcChatFailure& OutFailure)
{
	OutReply.Reset();
	OutFailure = ENpcChatFailure::Service;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;   // malformed: Service, and never shown verbatim
	}

	const FString Status = Root->GetStringField(TEXT("status"));
	if (Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		const FString Reply = Root->GetStringField(TEXT("reply")).TrimStartAndEnd();
		if (Reply.IsEmpty())
		{
			/* AN EMPTY REPLY IS A FAILURE, NOT AN ANSWER. It is also the shape a
			   thinking budget that ate the whole output allowance arrives in, so this
			   branch will earn its keep the first time the output cap is set too low. */
			return false;
		}
		OutReply = Reply;
		OutFailure = ENpcChatFailure::None;
		return true;
	}

	if (Status.Equals(TEXT("unavailable"), ESearchCase::IgnoreCase))
	{
		OutFailure = FailureFromReason(Root->GetStringField(TEXT("reason")));
		return false;
	}

	return false;   // unknown status
}

int32 UNpcChatSubsystem::Send(const FString& CharacterId, const FString& Message, FOnNpcChatReply OnReply)
{
	if (InFlightId != 0)
	{
		// Refused rather than queued — see the header. The panel keeps the player's text.
		UE_LOG(LogSibeliusGame, Verbose, TEXT("[Chat] %s: send refused, request %d still in flight"),
			*CharacterId, InFlightId);
		return 0;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return 0;
	}

	const ENpcChatMode Mode = CurrentMode();
	if (Mode == ENpcChatMode::Disabled)
	{
		/* REFUSED BEFORE AN ID IS SPENT. The panel shows the unavailable line, and the
		   player never waits on something that was never going to answer. */
		UE_LOG(LogSibeliusGame, Verbose, TEXT("[Chat] %s: send refused, chat is disabled"), *CharacterId);
		return 0;
	}

	InFlightId = NextRequestId++;
	PendingReply = OnReply;
	PendingCharacterId = CharacterId;
	PendingMessage = Message;

	if (Mode == ENpcChatMode::Proxy)
	{
		if (!SendOverHttp(InFlightId, CharacterId, Message))
		{
			// Could not even be dispatched. Answer now rather than leave the panel
			// waiting on a request that does not exist.
			CompleteRequest(InFlightId, FString(), ENpcChatFailure::Network);
			return 0;
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] %s: request %d sent (proxy)"),
			*CharacterId, InFlightId);
		return InFlightId;
	}

	const float Delay = FMath::Max(0.05f, CVarChatMockDelay.GetValueOnGameThread());

	// Captured by value: a lambda holding `this` plus the id is how the timer knows
	// which request it belongs to, and DeliverMockReply drops itself if that id was
	// cancelled while it was waiting.
	const int32 ThisRequest = InFlightId;
	FTimerDelegate Tick;
	Tick.BindUObject(this, &UNpcChatSubsystem::DeliverMockReply, ThisRequest);
	GI->GetTimerManager().SetTimer(MockTimer, Tick, Delay, /*bLoop=*/false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] %s: request %d sent (mock, %.2fs)"),
		*CharacterId, ThisRequest, Delay);
	return ThisRequest;
}

void UNpcChatSubsystem::Cancel(int32 RequestId)
{
	if (RequestId == 0 || RequestId != InFlightId)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(MockTimer);
	}

	if (PendingHttp.IsValid())
	{
		/* CANCELLING THE TRANSPORT DOES NOT UNSPEND THE REQUEST. The backend may already
		   have called the provider and been charged, which is exactly why nothing here
		   retries afterwards: a replay would pay twice for one thought. */
		PendingHttp->CancelRequest();
		PendingHttp.Reset();
	}
	PendingUuid.Reset();

	UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] request %d cancelled"), RequestId);

	InFlightId = 0;
	PendingReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();
}

void UNpcChatSubsystem::DeliverMockReply(int32 RequestId)
{
	/* A LATE REPLY MUST NOT ENTER ANOTHER CONVERSATION (review, acceptance checks).

	   The panel cancels on close, travel and NPC destruction, which clears InFlightId.
	   If this fires anyway — a timer that was already scheduled, a cancel that raced —
	   the id no longer matches and the answer is dropped on the floor, which is the
	   correct outcome and the cheap one. */
	if (RequestId != InFlightId)
	{
		return;
	}

	// Copy before clearing: the delegate may open a new request from inside itself.
	FOnNpcChatReply Reply = PendingReply;
	const FString Character = PendingCharacterId;
	const FString Message = PendingMessage;

	InFlightId = 0;
	PendingReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();

	const int32 ForcedFail = CVarChatMockFail.GetValueOnGameThread();
	if (ForcedFail > 0)
	{
		const ENpcChatFailure Failure = static_cast<ENpcChatFailure>(
			FMath::Clamp(ForcedFail, 1, static_cast<int32>(ENpcChatFailure::Service)));
		UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] request %d forced failure %d"),
			RequestId, ForcedFail);
		Reply.ExecuteIfBound(FString(), Failure);
		return;
	}

	Reply.ExecuteIfBound(ComposeMockReply(Character, Message), ENpcChatFailure::None);
}

FString UNpcChatSubsystem::ComposeMockReply(const FString& CharacterId, const FString& Message) const
{
	/* The canned lines are in the agents' register on purpose — an obviously fake
	   placeholder ("Lorem ipsum", "REPLY HERE") makes the panel impossible to judge in
	   a walk-test, and judging the panel is the entire reason the mock exists. They are
	   still plainly canned: short, deflecting, and they never claim to know anything. */
	static const TCHAR* Lines[] =
	{
		TEXT("You said: \"{0}\". I heard it. I am not allowed to say much yet."),
		TEXT("\"{0}\" - ask me again when I am wired to something that can think."),
		TEXT("I am a placeholder standing where {1} will answer you about \"{0}\"."),
		TEXT("That is worth remembering, and now it is — the last few exchanges, on this machine."),
		TEXT("I granted you a power. Conversation is the part I am still learning."),
	};

	const FString Trimmed = Message.Left(120).TrimStartAndEnd();
	const uint32 Pick = GetTypeHash(Trimmed) % UE_ARRAY_COUNT(Lines);

	/* FString::Format, not Printf. UE 5.7 checks Printf's format string at compile time
	   and will only take a literal, so a table of format strings cannot go through it.
	   Format's {0} placeholders are resolved at runtime, and the two lines that use no
	   placeholder at all pass through untouched. */
	FStringFormatOrderedArguments Args;
	Args.Add(FStringFormatArg(Trimmed));
	Args.Add(FStringFormatArg(CharacterId));
	return FString::Format(Lines[Pick], Args);
}

// ---------------------------------------------------------------------------
// THE PROXY TRANSPORT (step 6)
// ---------------------------------------------------------------------------

TArray<FNpcChatTurn> UNpcChatSubsystem::RecentTurnsFor(const FString& CharacterId) const
{
	/* The store holds ONLY PRIOR TURNS at this moment - the component sends before it
	   records, deliberately (DancerAgentComponent::HandleChatSend). Reverse that order
	   and the current message ships twice, and she starts echoing herself. */
	return GetHistory(FName(*CharacterId));
}

FString UNpcChatSubsystem::CurrentLocationId() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString();
}

bool UNpcChatSubsystem::SendOverHttp(int32 RequestId, const FString& CharacterId, const FString& Message)
{
	const FString Url = ProxyUrl();
	if (Url.IsEmpty())
	{
		return false;   // CurrentMode already guards this; belt and braces
	}

	// A fresh UUID per attempt, for the backend to dedup on. Deliberately NOT the local
	// request id: one is our bookkeeping, the other belongs to the contract.
	PendingUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	const FString Body = BuildRequestJson(PendingUuid, CharacterId, Message,
		CurrentLocationId(), RecentTurnsFor(CharacterId));

	PendingHttp = FHttpModule::Get().CreateRequest();
	PendingHttp->SetURL(Url);
	PendingHttp->SetVerb(TEXT("POST"));
	PendingHttp->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	PendingHttp->SetTimeout(ChatHttpTimeoutSeconds);
	PendingHttp->SetContentAsString(Body);

	/* WEAK, AND MATCHED BY ID. A reply can land after the panel closed, after travel, or
	   during teardown. A weak pointer plus the id check makes every one of those a no-op
	   instead of a crash or a line arriving in somebody else's conversation. */
	TWeakObjectPtr<UNpcChatSubsystem> WeakThis(this);
	PendingHttp->OnProcessRequestComplete().BindLambda(
		[WeakThis, RequestId](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			UNpcChatSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			if (!bConnectedOk || !Response.IsValid())
			{
				Self->CompleteRequest(RequestId, FString(), ENpcChatFailure::Network);
				return;
			}

			const int32 Code = Response->GetResponseCode();
			const FString Content = Response->GetContentAsString();

			if (Code < 200 || Code > 299)
			{
				// The body may still carry a declared reason; prefer it to the status.
				ENpcChatFailure Failure = FailureFromHttpCode(Code);
				FString Unused;
				ENpcChatFailure Declared = ENpcChatFailure::Service;
				if (!ParseReplyJson(Content, Unused, Declared) && Declared != ENpcChatFailure::Service)
				{
					Failure = Declared;
				}
				UE_LOG(LogSibeliusGame, Warning, TEXT("[Chat] request %d failed, HTTP %d"),
					RequestId, Code);
				Self->CompleteRequest(RequestId, FString(), Failure);
				return;
			}

			FString Reply;
			ENpcChatFailure Failure = ENpcChatFailure::None;
			if (ParseReplyJson(Content, Reply, Failure))
			{
				Self->CompleteRequest(RequestId, Reply, ENpcChatFailure::None);
			}
			else
			{
				Self->CompleteRequest(RequestId, FString(), Failure);
			}
		});

	if (!PendingHttp->ProcessRequest())
	{
		PendingHttp.Reset();
		PendingUuid.Reset();
		return false;
	}
	return true;
}

void UNpcChatSubsystem::CompleteRequest(int32 RequestId, const FString& Reply, ENpcChatFailure Failure)
{
	// The mock's rule, shared: an answer for a request nobody is waiting on is dropped,
	// not handed to whoever happens to be listening now.
	if (RequestId != InFlightId)
	{
		return;
	}

	FOnNpcChatReply Callback = PendingReply;

	InFlightId = 0;
	PendingReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();
	PendingHttp.Reset();
	PendingUuid.Reset();

	Callback.ExecuteIfBound(Reply, Failure);
}

// ---------------------------------------------------------------------------
// WHAT SHE REMEMBERS (step 5)
// ---------------------------------------------------------------------------

void UNpcChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromDisk();
}

void UNpcChatSubsystem::LoadFromDisk()
{
	Histories.Reset();
	bDirty = false;

	if (!FSibeliusSaveIO::Has(SlotName()))
	{
		return;   // never talked to anybody. Not an error.
	}

	const UNpcChatSaveGame* Save = Cast<UNpcChatSaveGame>(FSibeliusSaveIO::Load(SlotName()));
	if (!ApplySaveObject(Save))
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Chat] conversation slot could not be read; starting with no memories"));
	}
}

bool UNpcChatSubsystem::ApplySaveObject(const UNpcChatSaveGame* Save)
{
	Histories.Reset();
	bDirty = false;

	if (!Save)
	{
		return false;
	}

	/* A VERSION THIS BUILD DOES NOT KNOW IS NOT A VERSION TO GUESS AT.

	   0 is a blank or garbage object; anything above CurrentSaveVersion was written by
	   a later build, and reading it half-way would mean an agent who remembers some of
	   a conversation. Forgetting is the honest failure here, and the save is left on
	   disk untouched so a newer build can still read it. */
	if (Save->SaveVersion <= 0 || Save->SaveVersion > UNpcChatSaveGame::CurrentSaveVersion)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Chat] conversation save version %d is not readable by this build (expects 1..%d)"),
			Save->SaveVersion, UNpcChatSaveGame::CurrentSaveVersion);
		return false;
	}

	Histories = Save->Histories;

	// Trust nothing about the bound: an older save, a hand-edited file or a future
	// change to MaxTurnsKept all arrive here, and the panel should not be handed
	// forty turns because the file had them.
	for (TPair<FName, FNpcChatHistory>& Pair : Histories)
	{
		const int32 Excess = Pair.Value.Turns.Num() - MaxTurnsKept;
		if (Excess > 0)
		{
			Pair.Value.Turns.RemoveAt(0, Excess, EAllowShrinking::Yes);
			bDirty = true;
		}
	}

	return true;
}

UNpcChatSaveGame* UNpcChatSubsystem::BuildSaveObject() const
{
	UNpcChatSaveGame* Save = NewObject<UNpcChatSaveGame>(GetTransientPackage());
	Save->SaveVersion = UNpcChatSaveGame::CurrentSaveVersion;
	Save->Histories = Histories;
	return Save;
}

const TArray<FNpcChatTurn>& UNpcChatSubsystem::GetHistory(FName ChatId) const
{
	if (const FNpcChatHistory* Found = Histories.Find(ChatId))
	{
		return Found->Turns;
	}

	static const TArray<FNpcChatTurn> Empty;
	return Empty;
}

void UNpcChatSubsystem::RecordTurn(FName ChatId, bool bFromPlayer, const FString& Text)
{
	if (ChatId.IsNone() || Text.IsEmpty())
	{
		return;
	}

	FNpcChatHistory& History = Histories.FindOrAdd(ChatId);

	FNpcChatTurn Turn;
	Turn.bFromPlayer = bFromPlayer;
	Turn.Text = Text;
	History.Turns.Add(MoveTemp(Turn));

	const int32 Excess = History.Turns.Num() - MaxTurnsKept;
	if (Excess > 0)
	{
		// Oldest out of the front. The most recent exchange is the one she needs.
		History.Turns.RemoveAt(0, Excess, EAllowShrinking::Yes);
	}

	bDirty = true;
}

void UNpcChatSubsystem::SaveNow()
{
	if (!bDirty)
	{
		return;   // walking up to a dancer and away again is no reason to touch disk
	}

	if (FSibeliusSaveIO::Commit(BuildSaveObject(), SlotName()))
	{
		bDirty = false;
	}
	else
	{
		// Kept dirty on purpose: the next close tries again rather than silently
		// dropping what she was told.
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Chat] could not write the conversation slot"));
	}
}

void UNpcChatSubsystem::ForgetCharacter(FName ChatId)
{
	if (Histories.Remove(ChatId) > 0)
	{
		bDirty = true;
		SaveNow();
		UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] %s forgets everything"), *ChatId.ToString());
	}
}

void UNpcChatSubsystem::ForgetAll()
{
	Histories.Reset();
	bDirty = false;

	/* DELETE THE SLOT RATHER THAN WRITE AN EMPTY ONE. "Forget everything" should leave
	   nothing on disk to recover, which is the answer a player asking for it expects,
	   and it is also what makes a new game start genuinely fresh. */
	FSibeliusSaveIO::Delete(SlotName());
	UE_LOG(LogSibeliusGame, Display, TEXT("[Chat] every agent forgets everything"));
}

namespace
{
	/** Player-facing erasure. The panel will grow a button; the console is the start. */
	FAutoConsoleCommand ChatForgetCommand(
		TEXT("sib.Chat.Forget"),
		TEXT("Make the AI agents forget every saved conversation. No argument, no undo."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			if (UNpcChatSubsystem* Chat = GI ? GI->GetSubsystem<UNpcChatSubsystem>() : nullptr)
			{
				Chat->ForgetAll();
			}
		}));
}
