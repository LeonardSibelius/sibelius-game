// NpcChatSubsystem.cpp — the mock service behind the chat panel (step 4).

#include "NpcChatSubsystem.h"

#include "Engine/GameInstance.h"
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
}

bool UNpcChatSubsystem::IsMock()
{
	// One place to answer it, because the panel, the logs and eventually the smoke gate
	// all need the same answer. When the proxy lands this becomes a config read.
	return true;
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

	InFlightId = NextRequestId++;
	PendingReply = OnReply;
	PendingCharacterId = CharacterId;
	PendingMessage = Message;

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
		TEXT("That is worth remembering. It will not be, yet: nothing here is saved."),
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
