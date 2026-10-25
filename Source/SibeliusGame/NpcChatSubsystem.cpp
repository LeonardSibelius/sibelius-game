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
#include "ProgressionSubsystem.h"   // the facts: powers, sauce, lifetime numbers
#include "SibeliusHUD.h"            // the facts: the gold objective line
#include "GameFramework/PlayerController.h"
#include "SibeliusGameCharacter.h"  // the facts: where the travel keys can take him now
#include "ChatDilemmas.h"           // the facts: what he advised each agent
#include "ChatDeeds.h"              // the facts: what she remembers him doing, and how long he was away

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
	 * DEFAULTS TO PROXY IN DEVELOPMENT AND DISABLED IN SHIPPING. It was mock in
	 * development until the backend existed; it does now, it is deployed, and mock's
	 * job is finished. Mock was built so the panel, the failure copy and the cancel
	 * path could be walked without spending a request - all of that has been walked,
	 * and a default that serves canned lines is now the thing standing between Walt
	 * and the actual feature. `sib.Chat.Mode 1` still gets it back.
	 *
	 * SHIPPING STAYS 0. A packaged store build that reached this code with no backend
	 * configured must say the feature is unavailable rather than serve canned lines as
	 * though a character were talking - off is honest, canned is a lie with a friendly
	 * face. And the store submissions describe a game that generates nothing at
	 * runtime, which is a promise this default keeps. See CVarChatEnable in
	 * InteractorComponent.cpp, which is guarded the same way and for the same reason.
	 *
	 * WHAT THIS COSTS. Every PIE conversation now spends real money on Walt's key
	 * instead of nothing. The ceiling is not here, it is the backend's: $0.75 a day and
	 * $15 a month, refused server-side before Google is called. That is the design
	 * working, but it is a real change from a default that could not cost anything.
	 */
	TAutoConsoleVariable<int32> CVarChatMode(
		TEXT("sib.Chat.Mode"),
#if UE_BUILD_SHIPPING
		0,
#else
		2,
#endif
		TEXT("Chat service: 0 disabled, 1 mock (canned), 2 proxy (real backend)."),
		ECVF_Default);

	/**
	 * The backend endpoint. Public by design - the API key lives on the far side.
	 *
	 * DEFAULTED, AND WITHOUT A SCHEME. Typing this every session was the actual
	 * complaint, and a default is the only fix that survives closing the editor. The
	 * missing "//" is not a typo: it opens a comment in UE's console parser AND in
	 * ConsoleVariables.ini, which is the two-round-of-debugging lesson recorded on
	 * ProxyUrl() below. Loopback gets http, everything else https, so this resolves to
	 * https://sibelius-npc-chat.fly.dev/v1/npc-chat.
	 *
	 * SAFE IN SHIPPING even though it is not guarded: CurrentMode() is 0 there, so
	 * nothing ever dials it. A shipping build would need sib.Chat.Mode 2 typed by hand,
	 * which is the deliberate extra step.
	 */
	TAutoConsoleVariable<FString> CVarChatProxyUrl(
		TEXT("sib.Chat.ProxyUrl"),
		TEXT("sibelius-npc-chat.fly.dev/v1/npc-chat"),
		TEXT("Endpoint for the NPC chat backend. No scheme: '//' opens a comment."),
		ECVF_Default);

	/** Client deadline. The backend's own is shorter; this catches a dead connection. */
	constexpr float ChatHttpTimeoutSeconds = 25.0f;

	/**
	 * The council's deadline. Five agents speak one after another on the server, ~1 s each
	 * on a good day, so the one-to-one 25 s would cut off a slow but healthy table.
	 */
	constexpr float CouncilHttpTimeoutSeconds = 60.0f;

	/** The one path the council URL is derived from, and the one it becomes. */
	const TCHAR* ChatPath = TEXT("/v1/npc-chat");
	const TCHAR* CouncilPath = TEXT("/v1/npc-council");
}

namespace
{
	/** Empty except inside a gate. See SetSlotNameForTesting. */
	FString GChatSlotOverride;
}

FString UNpcChatSubsystem::SlotName()
{
	return GChatSlotOverride.IsEmpty() ? FString(TEXT("NpcChat")) : GChatSlotOverride;
}

void UNpcChatSubsystem::SetSlotNameForTesting(const FString& InSlotName)
{
	GChatSlotOverride = InSlotName;
}

ENpcChatMode UNpcChatSubsystem::CurrentMode()
{
	const int32 Raw = FMath::Clamp(CVarChatMode.GetValueOnGameThread(), 0, 2);
	const ENpcChatMode Configured = static_cast<ENpcChatMode>(Raw);

	/* PROXY WITHOUT A USABLE URL IS DISABLED, NOT BROKEN. Answering "unavailable" is the
	   right behaviour for an unconfigured build, and it stops a half-configured release
	   from spraying failed requests at nothing. */
	/* VALIDATE WHAT WAS CONFIGURED, NOT WHAT WE WOULD DIAL. Checking the normalised
	   form let "http:" through: it has no "://", so a scheme was prepended and
	   "https://http:" passed a shape test with "http:" as its host. The truncation
	   signature only survives in the raw value. */
	if (Configured == ENpcChatMode::Proxy && !IsUsableProxyUrl(ConfiguredProxyUrl()))
	{
		return ENpcChatMode::Disabled;
	}
	return Configured;
}

FString UNpcChatSubsystem::ConfiguredProxyUrl()
{
	return CVarChatProxyUrl.GetValueOnGameThread().TrimStartAndEnd();
}

FString UNpcChatSubsystem::ProxyUrl()
{
	/* THE SCHEME IS OPTIONAL, AND THAT IS NOT A CONVENIENCE - IT IS THE ONLY WAY TO
	   CONFIGURE THIS AT ALL.

	   "//" opens a comment in UE's console parser AND in ConsoleVariables.ini, so
	   `sib.Chat.ProxyUrl http://host/path` becomes "http:" in both. That cost two
	   rounds of debugging and a wrong workaround: the ini was recommended as the fix
	   and it fails identically (LogConfig: CVar [[sib.Chat.ProxyUrl:http:]]).

	   So the supported form has no double slash in it:

	       sib.Chat.ProxyUrl=127.0.0.1:8787/v1/npc-chat

	   Loopback gets http, everything else https - a remote backend holding an API key
	   has no business being spoken to in clear text, and defaulting the other way would
	   make a typo into a downgrade. A full URL still works when it can be set somewhere
	   the comment rule does not apply. */
	const FString Configured = ConfiguredProxyUrl();
	if (Configured.IsEmpty() || Configured.Contains(TEXT("://")))
	{
		return Configured;
	}

	const bool bLoopback =
		Configured.StartsWith(TEXT("127.0.0.1")) ||
		Configured.StartsWith(TEXT("localhost")) ||
		Configured.StartsWith(TEXT("[::1]"));

	return (bLoopback ? TEXT("http://") : TEXT("https://")) + Configured;
}

bool UNpcChatSubsystem::IsUsableProxyUrl(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return false;
	}

	/* THE TRUNCATION SIGNATURE. "http:" or "https:" with nothing after it is exactly
	   what both parsers leave behind, and it must never be dialled or silently
	   repaired - repairing it would invent a host nobody configured. */
	if (!Url.Contains(TEXT("://")) &&
		(Url.StartsWith(TEXT("http:")) || Url.StartsWith(TEXT("https:"))))
	{
		static FString LastTruncated;
		if (LastTruncated != Url)
		{
			LastTruncated = Url;
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Chat] ProxyUrl \"%s\" looks truncated - \"//\" opens a comment in both the ")
				TEXT("console and ConsoleVariables.ini. Drop the scheme: sib.Chat.ProxyUrl=127.0.0.1:8787/v1/npc-chat"),
				*Url);
		}
		return false;
	}

	// Scheme-less values are legal: ProxyUrl() supplies http for loopback, https
	// otherwise. Validate the form that will actually be dialled.
	const FString Dialled = Url.Contains(TEXT("://")) ? Url : ProxyUrl();
	return IsWholeUrl(Dialled);
}

bool UNpcChatSubsystem::IsWholeUrl(const FString& Url)
{

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
				TEXT("[Chat] ProxyUrl resolves to \"%s\", which is not a usable http(s) URL - chat is disabled."),
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
	const TArray<FNpcChatTurn>& RecentTurns,
	const FString& StoryEvent,
	const FNpcStoryFacts* Facts)
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
	// Only when there is one: the server allowlists it (backend/events.js), and absent is
	// the ordinary case.
	if (!StoryEvent.IsEmpty())
	{
		Story->SetStringField(TEXT("event"), StoryEvent);
	}
	if (Facts)
	{
		WriteStoryFacts(Story, *Facts);
	}
	/* WHAT THIS BUILD KNOWS HOW TO DO, so the server can add a behaviour without changing it for
	   builds already in players' hands - or the one under Valve's review (backend/curiosity.js).
	   human_questions: she may ask what being human is like, never about his life (2026-10-06).
	   isla_keep: Isla's home is Fafnir's Keep, not the space station older builds have (2026-10-24). */
	TArray<TSharedPtr<FJsonValue>> Features;
	Features.Add(MakeShared<FJsonValueString>(TEXT("human_questions")));
	Features.Add(MakeShared<FJsonValueString>(TEXT("isla_keep")));
	Story->SetArrayField(TEXT("features"), Features);
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

FString UNpcChatSubsystem::ParseTravelOffer(const FString& Body)
{
	TSharedPtr<FJsonObject> Root;
	FString Travel;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) && Root.IsValid())
	{
		Root->TryGetStringField(TEXT("travel"), Travel);
	}
	return Travel;
}

FString UNpcChatSubsystem::ParseAdvice(const FString& Body)
{
	TSharedPtr<FJsonObject> Root;
	FString Advice;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) && Root.IsValid())
	{
		Root->TryGetStringField(TEXT("advice"), Advice);
	}
	return Advice;
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

int32 UNpcChatSubsystem::Send(const FString& CharacterId, const FString& Message, FOnNpcChatReply OnReply,
	const FString& StoryEvent)
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
	LastTravelOffer.Reset();   // an offer belongs to the reply it came with, never the next
	LastAdvice.Reset();        // and so does his advice
	PendingReply = OnReply;
	PendingCharacterId = CharacterId;
	PendingMessage = Message;
	PendingEvent = StoryEvent;

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
	PendingCouncilReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();
	PendingEvent.Reset();
	bNoteOnAnswer = false;   // she never answered: the welcome and the news wait for the next try
	PendingDeeds.Reset();
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

	/* THE MOCK TAKES ADVICE TOO, so a walk-test without the server can finish a dilemma: with
	   her question open on this request, a line naming one of her answers ("keep them") is it. */
	if (const ChatDilemmas::FDilemma* Dilemma = ChatDilemmas::Find(FName(*Character)))
	{
		if (PendingEvent == ChatDilemmas::StoryEvent(*Dilemma))
		{
			LastAdvice = ChatDilemmas::ChoiceNamedIn(*Dilemma, Message);
		}
	}

	InFlightId = 0;
	PendingReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();
	PendingEvent.Reset();

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

void UNpcChatSubsystem::WriteStoryFacts(const TSharedRef<FJsonObject>& Story, const FNpcStoryFacts& Facts)
{
	const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> Powers;
	for (const FString& Power : Facts.Powers)
	{
		Powers.Add(MakeShared<FJsonValueString>(Power));
	}
	Out->SetArrayField(TEXT("powers"), Powers);
	Out->SetNumberField(TEXT("sauce"), Facts.Sauce);
	Out->SetNumberField(TEXT("sauceEarned"), Facts.SauceEarned);
	Out->SetNumberField(TEXT("refusersFought"), Facts.RefusersFought);
	Out->SetNumberField(TEXT("booksCollected"), Facts.BooksCollected);
	Out->SetNumberField(TEXT("chaptersCompleted"), Facts.ChaptersCompleted);
	if (!Facts.Objective.IsEmpty())
	{
		Out->SetStringField(TEXT("objective"), Facts.Objective);
	}
	TArray<TSharedPtr<FJsonValue>> Destinations;
	for (const FString& Destination : Facts.Destinations)
	{
		Destinations.Add(MakeShared<FJsonValueString>(Destination));
	}
	Out->SetArrayField(TEXT("destinations"), Destinations);
	// Only once he has advised someone: until then the facts are exactly what they were.
	if (Facts.Advice.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Advice;
		for (const FString& Entry : Facts.Advice)
		{
			Advice.Add(MakeShared<FJsonValueString>(Entry));
		}
		Out->SetArrayField(TEXT("advice"), Advice);
	}
	/* WHAT SHE REMEMBERS (ChatDeeds.h), only when there is something: the council's facts, and
	   those of a save with nothing done yet, are exactly what they were. */
	const auto Strings = [&Out](const TCHAR* Field, const TArray<FString>& Values)
	{
		if (Values.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Array;
			for (const FString& Value : Values)
			{
				Array.Add(MakeShared<FJsonValueString>(Value));
			}
			Out->SetArrayField(Field, Array);
		}
	};
	Strings(TEXT("deeds"), Facts.Deeds);
	Strings(TEXT("newDeeds"), Facts.NewDeeds);
	if (Facts.HoursAway >= 0)
	{
		Out->SetNumberField(TEXT("hoursAway"), Facts.HoursAway);
	}
	Story->SetObjectField(TEXT("facts"), Out);
}

void UNpcChatSubsystem::FillMemoryFacts(FNpcStoryFacts& Facts, const FNpcChatHistory* History,
	const TArray<FString>& DeedsNow, const FDateTime& NowUtc)
{
	Facts.Deeds = DeedsNow;
	Facts.NewDeeds.Reset();
	Facts.HoursAway = -1;
	if (History && History->LastAnsweredUtc.GetTicks() > 0)
	{
		Facts.HoursAway = ChatDeeds::HoursBetween(History->LastAnsweredUtc, NowUtc);
		Facts.NewDeeds = ChatDeeds::NewSince(DeedsNow, History->DeedsKnown);
	}
}

void UNpcChatSubsystem::NoteAnswered(FName ChatId, const TArray<FString>& DeedsNow, const FDateTime& NowUtc)
{
	if (ChatId.IsNone())
	{
		return;
	}
	FNpcChatHistory& History = Histories.FindOrAdd(ChatId);
	History.LastAnsweredUtc = NowUtc;
	History.DeedsKnown = DeedsNow;
	bDirty = true;   // written with the turns, when the panel closes (SaveNow)
}

FNpcStoryFacts UNpcChatSubsystem::CurrentStoryFacts() const
{
	FNpcStoryFacts Facts;
	const UGameInstance* GI = GetGameInstance();
	const UProgressionSubsystem* Progression = GI ? GI->GetSubsystem<UProgressionSubsystem>() : nullptr;
	if (Progression)
	{
		for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
		{
			const EPowerVerb Verb = static_cast<EPowerVerb>(i);
			if (Progression->IsUnlocked(Verb))
			{
				Facts.Powers.Add(PowerVerbDisplayName(Verb));
			}
		}
		Facts.Sauce = Progression->GetSauce();
		Facts.SauceEarned = Progression->GetStat(SibeliusStats::SauceEarned);
		// The stat KEY is still RefusersSlapped; the game calls it "Refusers fought".
		Facts.RefusersFought = Progression->GetStat(SibeliusStats::RefusersSlapped);
		Facts.BooksCollected = Progression->GetStat(SibeliusStats::BooksCollected);
		Facts.ChaptersCompleted = Progression->GetStat(SibeliusStats::ChaptersCompleted);
		// What he advised the agents; the server tells each only her own (backend/dilemmas.js).
		Facts.Advice = ChatDilemmas::AdviceFacts(Progression->GetStateForRead());
	}

	/* THE SAME LINE THE SCREEN SHOWS. The HUD already derives "the ONE next thing" from live
	   state (ASibeliusHUD::ComputeObjective); asking the agents to work it out would be a
	   second answer free to disagree with the first. */
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (const ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		Facts.Objective = HUD->GetObjectiveLine();
	}
	if (const ASibeliusGameCharacter* Leonard = PC ? Cast<ASibeliusGameCharacter>(PC->GetPawn()) : nullptr)
	{
		for (const FName& Level : Leonard->OpenTravelDestinations())
		{
			Facts.Destinations.Add(Level.ToString());
		}
	}
	return Facts;
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

	FNpcStoryFacts Facts = CurrentStoryFacts();

	/* WHAT SHE REMEMBERS (ChatDeeds.h): everything he has done, what is new to her, and how long
	   since she last answered him. Kept for CompleteRequest, which notes it all as known only
	   once she has actually answered. */
	PendingDeeds.Reset();
	bNoteOnAnswer = false;
	const UGameInstance* GI = GetGameInstance();
	if (const UProgressionSubsystem* Progression = GI ? GI->GetSubsystem<UProgressionSubsystem>() : nullptr)
	{
		PendingDeeds = ChatDeeds::FromState(Progression->GetStateForRead());
		FillMemoryFacts(Facts, Histories.Find(FName(*CharacterId)), PendingDeeds, FDateTime::UtcNow());
		bNoteOnAnswer = true;
	}

	const FString Body = BuildRequestJson(PendingUuid, CharacterId, Message,
		CurrentLocationId(), RecentTurnsFor(CharacterId), PendingEvent, &Facts);

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
				// Only for the request still awaited; CompleteRequest drops any other.
				if (RequestId == Self->InFlightId)
				{
					Self->LastTravelOffer = ParseTravelOffer(Content);
					Self->LastAdvice = ParseAdvice(Content);
				}
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

	// She answered: from now on she knows what he has done, and when they last talked.
	if (bNoteOnAnswer && Failure == ENpcChatFailure::None && !PendingCharacterId.IsEmpty())
	{
		NoteAnswered(FName(*PendingCharacterId), PendingDeeds, FDateTime::UtcNow());
	}
	bNoteOnAnswer = false;
	PendingDeeds.Reset();

	InFlightId = 0;
	PendingReply.Unbind();
	PendingCharacterId.Reset();
	PendingMessage.Reset();
	PendingEvent.Reset();
	PendingHttp.Reset();
	PendingUuid.Reset();

	Callback.ExecuteIfBound(Reply, Failure);
}

// ---------------------------------------------------------------------------
// THE COUNCIL
// ---------------------------------------------------------------------------

FString UNpcChatSubsystem::CouncilUrl()
{
	/* DERIVED, NOT A SECOND CVAR. Two endpoints configured separately is two things to get
	   wrong, and pointing chat at a local server while the council quietly kept talking to
	   production would be the worst version of that. A chat URL that is not the standard
	   path has no council, and says so by being empty. */
	const FString Chat = ProxyUrl();
	if (!Chat.EndsWith(ChatPath))
	{
		return FString();
	}
	return Chat.LeftChop(FCString::Strlen(ChatPath)) + CouncilPath;
}

FString UNpcChatSubsystem::BuildCouncilRequestJson(
	const FString& RequestId,
	const FString& Message,
	const FString& LocationId,
	const TArray<FNpcCouncilLine>& RecentLines,
	const FString& StoryEvent,
	const FNpcStoryFacts* Facts)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("message"), Message);

	const TSharedRef<FJsonObject> Story = MakeShared<FJsonObject>();
	Story->SetStringField(TEXT("locationId"), LocationId);
	Story->SetArrayField(TEXT("unlockedFacts"), TArray<TSharedPtr<FJsonValue>>());
	if (!StoryEvent.IsEmpty())
	{
		Story->SetStringField(TEXT("event"), StoryEvent);
	}
	if (Facts)
	{
		WriteStoryFacts(Story, *Facts);
	}
	Root->SetObjectField(TEXT("storyContext"), Story);

	// Only the most recent lines: the server reads 30 and refuses past 64.
	const int32 First = FMath::Max(0, RecentLines.Num() - CouncilLinesKept);
	TArray<TSharedPtr<FJsonValue>> Turns;
	for (int32 i = First; i < RecentLines.Num(); ++i)
	{
		const TSharedRef<FJsonObject> Turn = MakeShared<FJsonObject>();
		// "speaker", not "role": at a table of six, user/model cannot say who spoke.
		Turn->SetStringField(TEXT("speaker"), RecentLines[i].SpeakerId);
		Turn->SetStringField(TEXT("text"), RecentLines[i].Text);
		Turns.Add(MakeShared<FJsonValueObject>(Turn));
	}
	Root->SetArrayField(TEXT("recentTurns"), Turns);

	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

bool UNpcChatSubsystem::ParseCouncilJson(const FString& Body, TArray<FNpcCouncilLine>& OutLines,
	FString& OutCareReply, ENpcChatFailure& OutFailure)
{
	OutLines.Reset();
	OutCareReply.Reset();
	OutFailure = ENpcChatFailure::Service;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const FString Status = Root->GetStringField(TEXT("status"));
	if (Status.Equals(TEXT("unavailable"), ESearchCase::IgnoreCase))
	{
		OutFailure = FailureFromReason(Root->GetStringField(TEXT("reason")));
		return false;
	}
	if (!Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	/* THE CARE REPLY WINS OVER EVERYTHING ELSE IN THE BODY. The server sends it with an
	   empty replies array; if a future server ever sent both, the player in trouble still
	   gets the one authored line and not five jokes. */
	bool bCare = false;
	if (Root->TryGetBoolField(TEXT("care"), bCare) && bCare)
	{
		OutCareReply = Root->GetStringField(TEXT("reply")).TrimStartAndEnd();
		if (OutCareReply.IsEmpty())
		{
			return false;
		}
		OutFailure = ENpcChatFailure::None;
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Replies = nullptr;
	if (Root->TryGetArrayField(TEXT("replies"), Replies))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Replies)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj->IsValid())
			{
				continue;
			}
			FNpcCouncilLine Line;
			Line.SpeakerId = (*Obj)->GetStringField(TEXT("characterId"));
			Line.DisplayName = (*Obj)->GetStringField(TEXT("displayName"));
			Line.Text = (*Obj)->GetStringField(TEXT("text")).TrimStartAndEnd();
			// A line with no speaker or no words is dropped, never drawn as a blank.
			if (!Line.SpeakerId.IsEmpty() && !Line.DisplayName.IsEmpty() && !Line.Text.IsEmpty())
			{
				OutLines.Add(MoveTemp(Line));
			}
		}
	}

	if (OutLines.Num() == 0)
	{
		return false;   // nobody spoke: a failure, not an empty table
	}
	OutFailure = ENpcChatFailure::None;
	return true;
}

void UNpcChatSubsystem::RecordCouncilTurn(const FString& PlayerLine, const TArray<FNpcCouncilLine>& Replies)
{
	FNpcCouncilLine Said;
	Said.SpeakerId = TEXT("leonard");
	Said.DisplayName = TEXT("You");
	Said.Text = PlayerLine;
	CouncilHistory.Add(MoveTemp(Said));
	CouncilHistory.Append(Replies);

	const int32 Excess = CouncilHistory.Num() - CouncilLinesKept;
	if (Excess > 0)
	{
		CouncilHistory.RemoveAt(0, Excess, EAllowShrinking::Yes);
	}
}

int32 UNpcChatSubsystem::SendCouncil(const FString& Message, FOnNpcCouncilReply OnReply, const FString& StoryEvent)
{
	if (InFlightId != 0)
	{
		UE_LOG(LogSibeliusGame, Verbose, TEXT("[Council] send refused, request %d still in flight"), InFlightId);
		return 0;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return 0;
	}

	const ENpcChatMode Mode = CurrentMode();
	if (Mode == ENpcChatMode::Disabled || (Mode == ENpcChatMode::Proxy && CouncilUrl().IsEmpty()))
	{
		UE_LOG(LogSibeliusGame, Verbose, TEXT("[Council] send refused, the council is unavailable"));
		return 0;
	}

	InFlightId = NextRequestId++;
	PendingCouncilReply = OnReply;
	PendingMessage = Message;
	PendingEvent = StoryEvent;

	if (Mode == ENpcChatMode::Proxy)
	{
		if (!SendCouncilOverHttp(InFlightId, Message))
		{
			CompleteCouncil(InFlightId, {}, FString(), ENpcChatFailure::Network);
			return 0;
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[Council] request %d sent (proxy)"), InFlightId);
		return InFlightId;
	}

	const int32 ThisRequest = InFlightId;
	FTimerDelegate Tick;
	Tick.BindUObject(this, &UNpcChatSubsystem::DeliverMockCouncil, ThisRequest);
	GI->GetTimerManager().SetTimer(MockTimer, Tick,
		FMath::Max(0.05f, CVarChatMockDelay.GetValueOnGameThread()), /*bLoop=*/false);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Council] request %d sent (mock)"), ThisRequest);
	return ThisRequest;
}

void UNpcChatSubsystem::DeliverMockCouncil(int32 RequestId)
{
	const int32 ForcedFail = CVarChatMockFail.GetValueOnGameThread();
	if (ForcedFail > 0)
	{
		CompleteCouncil(RequestId, {}, FString(), static_cast<ENpcChatFailure>(
			FMath::Clamp(ForcedFail, 1, static_cast<int32>(ENpcChatFailure::Service))));
		return;
	}

	// Plainly canned, and in the server's seat order, so a walk-test can judge the panel.
	static const TCHAR* Seats[][2] =
	{
		{ TEXT("kaia"), TEXT("Kaia") }, { TEXT("isla"), TEXT("Isla") }, { TEXT("aisling"), TEXT("Aisling") },
		{ TEXT("nyra"), TEXT("Nyra") }, { TEXT("elise"), TEXT("Elise") },
	};
	TArray<FNpcCouncilLine> Lines;
	for (const auto& Seat : Seats)
	{
		Lines.Add({ Seat[0], Seat[1],
			FString::Printf(TEXT("A placeholder where %s will answer \"%s\"."), Seat[1], *PendingMessage.Left(60)) });
	}
	CompleteCouncil(RequestId, Lines, FString(), ENpcChatFailure::None);
}

bool UNpcChatSubsystem::SendCouncilOverHttp(int32 RequestId, const FString& Message)
{
	const FString Url = CouncilUrl();
	if (Url.IsEmpty())
	{
		return false;
	}

	PendingUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const FNpcStoryFacts Facts = CurrentStoryFacts();
	const FString Body = BuildCouncilRequestJson(PendingUuid, Message, CurrentLocationId(), CouncilHistory,
		PendingEvent, &Facts);

	PendingHttp = FHttpModule::Get().CreateRequest();
	PendingHttp->SetURL(Url);
	PendingHttp->SetVerb(TEXT("POST"));
	PendingHttp->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	PendingHttp->SetTimeout(CouncilHttpTimeoutSeconds);
	PendingHttp->SetContentAsString(Body);

	// Weak and matched by id, for the same reasons as SendOverHttp.
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
				Self->CompleteCouncil(RequestId, {}, FString(), ENpcChatFailure::Network);
				return;
			}

			const int32 Code = Response->GetResponseCode();
			TArray<FNpcCouncilLine> Lines;
			FString Care;
			ENpcChatFailure Failure = ENpcChatFailure::Service;
			const bool bOk = ParseCouncilJson(Response->GetContentAsString(), Lines, Care, Failure);

			if (Code < 200 || Code > 299)
			{
				// Prefer a declared reason in the body to the bare status.
				if (Failure == ENpcChatFailure::Service)
				{
					Failure = FailureFromHttpCode(Code);
				}
				UE_LOG(LogSibeliusGame, Warning, TEXT("[Council] request %d failed, HTTP %d"), RequestId, Code);
				Self->CompleteCouncil(RequestId, {}, FString(), Failure);
				return;
			}

			Self->CompleteCouncil(RequestId, Lines, Care, bOk ? ENpcChatFailure::None : Failure);
		});

	if (!PendingHttp->ProcessRequest())
	{
		PendingHttp.Reset();
		PendingUuid.Reset();
		return false;
	}
	return true;
}

void UNpcChatSubsystem::CompleteCouncil(int32 RequestId, const TArray<FNpcCouncilLine>& Lines,
	const FString& CareReply, ENpcChatFailure Failure)
{
	if (RequestId != InFlightId)
	{
		return;
	}

	FOnNpcCouncilReply Callback = PendingCouncilReply;

	InFlightId = 0;
	PendingCouncilReply.Unbind();
	PendingMessage.Reset();
	PendingEvent.Reset();
	PendingHttp.Reset();
	PendingUuid.Reset();

	Callback.ExecuteIfBound(Lines, CareReply, Failure);
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
