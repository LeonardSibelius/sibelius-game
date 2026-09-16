// NpcChatSubsystem.h
//
// Typed conversations with the AI agents (docs/NPC_CHAT_REVIEW.md, step 4).
//
// THE MOCK IS THE POINT OF THIS FILE. Everything player-facing — the panel, the camera,
// the input handback, the failure copy — has to be finished and walked before a single
// real request is spent, because the real service is a shared free-tier pool and the
// tier has not been chosen yet (review, correction 2). So the subsystem's shape is the
// shape the HTTP version will have: one call per player turn, asynchronous, at most one
// request in flight, cancellable, and answering with a reason rather than a bare failure.
// Swapping the mock for the proxy is then a change of body, not of signature.
//
// The client never decides anything the server should decide. No model choice, no
// character instructions, no retry policy lives here.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NpcChatSaveGame.h"   // FNpcChatTurn / the persisted shape
#include "NpcChatSubsystem.generated.h"

/**
 * Why a reply did not arrive. The panel prints one sentence per reason, and none of
 * them are the provider's own error text — see the review's data-handling note.
 */
UENUM()
enum class ENpcChatFailure : uint8
{
	None,
	Quota,      // the shared allowance is spent; do not retry, do not upgrade
	Network,    // offline, DNS, TLS
	Timeout,    // no answer inside the budget
	Blocked,    // the service refused to answer
	Service     // 5xx, malformed, anything else
};

/**
 * Where replies come from.
 *
 * EXPLICIT, AND NOT INFERRED FROM WHETHER A URL HAPPENS TO BE SET. A packaged build with
 * no configuration must be honestly unavailable rather than quietly serving canned lines
 * as though they were a character talking — that is the difference between a feature that
 * is off and a feature that is lying.
 */
UENUM()
enum class ENpcChatMode : uint8
{
	Disabled,   // say so, call nothing
	Mock,       // canned lines, for walk-tests and gates
	Proxy       // the real backend, which owns the key and the model
};

/** Reply text (empty on failure) and why, if it failed. */
DECLARE_DELEGATE_TwoParams(FOnNpcChatReply, const FString& /*Reply*/, ENpcChatFailure /*Failure*/);

UCLASS()
class SIBELIUSGAME_API UNpcChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Ask CharacterId for a reply to Message.
	 *
	 * Returns a request id, or 0 when the call was refused because one is already in
	 * flight. ONE AT A TIME IS A PRODUCT RULE, not a limitation: a player who presses
	 * Enter twice must not spend two of a shared daily allowance on one thought, and a
	 * queue here would only move that spend, not prevent it.
	 */
	int32 Send(const FString& CharacterId, const FString& Message, FOnNpcChatReply OnReply);

	/**
	 * Abandon a request. The reply, if it ever lands, is dropped.
	 *
	 * This is what keeps a late answer out of the next conversation — the panel closes,
	 * the id is cancelled, and the mock's own timer finds nothing to deliver to. The
	 * HTTP version cancels the request the same way and MUST NOT replay it afterwards:
	 * an uncertain timeout may already have cost a request.
	 */
	void Cancel(int32 RequestId);

	bool IsBusy() const { return InFlightId != 0; }

	// -----------------------------------------------------------------------
	// WHAT SHE REMEMBERS (step 5)
	//
	// Held here rather than on the component because a UGameInstanceSubsystem
	// outlives level travel and the dancer does not. Kaia in the office and Kaia
	// in the deli are two actors; the conversation is one.
	// -----------------------------------------------------------------------

	/**
	 * Turns kept per agent — eight exchanges.
	 *
	 * A bound, not a budget: it is what the panel replays today and what will fit in a
	 * prompt tomorrow. Unbounded history would grow the cost of every future request
	 * until a token ceiling chose the limit instead of this line.
	 */
	static constexpr int32 MaxTurnsKept = 16;

	/** That agent's remembered turns, oldest first. Empty when she has never been talked to. */
	const TArray<FNpcChatTurn>& GetHistory(FName ChatId) const;

	/** Append one line and drop the oldest past the bound. Marks the store dirty. */
	void RecordTurn(FName ChatId, bool bFromPlayer, const FString& Text);

	/** Write to disk if anything changed. Cheap and idempotent; safe to call on close. */
	void SaveNow();

	/** Forget one agent, or all of them. Both write through immediately. */
	void ForgetCharacter(FName ChatId);
	void ForgetAll();

	/**
	 * The two halves of save and load, exposed so the gate can exercise the REAL
	 * serialization path against a sandbox slot instead of a player's own file.
	 * BuildSaveObject stamps the current version; ApplySaveObject refuses anything it
	 * does not understand and leaves the store empty rather than half-read.
	 */
	UNpcChatSaveGame* BuildSaveObject() const;
	bool ApplySaveObject(const UNpcChatSaveGame* Save);

	/** The slot conversations live in. Deliberately not the Progression slot. */
	static const TCHAR* SlotName() { return TEXT("NpcChat"); }

	/** True while the service is the canned one. The panel says so, so nobody demos a mock. */
	static bool IsMock();

	/** The mode this process will actually use, after the URL check. */
	static ENpcChatMode CurrentMode();

	/** The configured backend, or empty. Not a secret — the key never leaves the proxy. */
	static FString ProxyUrl();

	// -----------------------------------------------------------------------
	// THE CONTRACT (docs/GEMINI_3_1_FLASH_LITE_HANDOFF.md, step 2)
	//
	// Both halves are pure and static so the gate can prove them without a
	// network, a server or a single spent request. That is the whole reason
	// they are not buried inside the HTTP callback.
	// -----------------------------------------------------------------------

	/** The POST body: requestId, characterId, message, storyContext, recentTurns. */
	static FString BuildRequestJson(
		const FString& RequestId,
		const FString& CharacterId,
		const FString& Message,
		const FString& LocationId,
		const TArray<FNpcChatTurn>& RecentTurns);

	/**
	 * Read a backend response. Returns true only for a well-formed `ok` carrying text.
	 *
	 * Anything else — a declared failure, an unknown status, malformed JSON, an empty
	 * reply — comes back false with a reason, because a player must never be shown an
	 * empty speech bubble and told it was an answer.
	 */
	static bool ParseReplyJson(const FString& Body, FString& OutReply, ENpcChatFailure& OutFailure);

	/** Map the allowlisted reason strings; anything unrecognised is Service. */
	static ENpcChatFailure FailureFromReason(const FString& Reason);

	/** Map a transport-level HTTP status to a reason. */
	static ENpcChatFailure FailureFromHttpCode(int32 StatusCode);

private:
	/** Fired off the mock's timer; drops itself if the id was cancelled meanwhile. */
	void DeliverMockReply(int32 RequestId);

	/** Start a real request. Returns false if it could not even be dispatched. */
	bool SendOverHttp(int32 RequestId, const FString& CharacterId, const FString& Message);

	/** Finish one: match the id, map the outcome, hand it to whoever is still listening. */
	void CompleteRequest(int32 RequestId, const FString& Reply, ENpcChatFailure Failure);

	/** The turns already in the store for this character — never the one being sent. */
	TArray<FNpcChatTurn> RecentTurnsFor(const FString& CharacterId) const;

	/** The level the player is standing in, for the server to allowlist. */
	FString CurrentLocationId() const;

	/** A canned line in the agents' register, folding in what the player actually said. */
	FString ComposeMockReply(const FString& CharacterId, const FString& Message) const;

	/** Initialize loads; nothing else reads disk. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Pull the slot from disk into Histories. Absent or unreadable leaves it empty. */
	void LoadFromDisk();

	UPROPERTY()
	TMap<FName, FNpcChatHistory> Histories;

	/** Nothing is written unless something changed. */
	bool bDirty = false;

	int32 NextRequestId = 1;
	int32 InFlightId = 0;

	FOnNpcChatReply PendingReply;
	FString PendingCharacterId;
	FString PendingMessage;

	FTimerHandle MockTimer;

	/** The live HTTP request, so Cancel can actually stop it. */
	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> PendingHttp;

	/** The UUID sent to the backend for this request, for dedup on its side. */
	FString PendingUuid;
};
