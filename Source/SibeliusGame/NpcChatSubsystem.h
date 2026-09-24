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

struct FNpcStoryFacts
{
	/* WHAT THE AGENTS ARE TOLD ABOUT HIS PLAYTHROUGH (docs/THE_UNWRITTEN.md, "What they talk
	   about"). A short, fixed list read from the save and the HUD — never raw save data.
	   The server allowlists every field again (backend/facts.js) and writes the prompt line
	   itself, so nothing here is ever pasted into an instruction as-is. */
	TArray<FString> Powers;        // display names of the powers he owns, "CODE VISION" ...
	int32 Sauce = 0;               // in his wallet now
	int32 SauceEarned = 0;         // lifetime
	int32 RefusersFought = 0;
	int32 BooksCollected = 0;
	int32 ChaptersCompleted = 0;
	FString Objective;             // the HUD's gold objective line, or empty
	TArray<FString> Destinations;  // where a [Go] button may take him now (OpenTravelDestinations)
};

/**
 * One line said at the council table. SpeakerId is "leonard" or one of the five seats —
 * the server allowlists exactly those, so a history line naming anyone else is a 400.
 */
struct FNpcCouncilLine
{
	FString SpeakerId;
	FString DisplayName;
	FString Text;
};

/**
 * A council turn's answer: the agents who spoke, in the order they spoke. CareReply is set
 * instead when the distress screen stopped the table — then Lines is empty.
 */
DECLARE_DELEGATE_ThreeParams(FOnNpcCouncilReply,
	const TArray<FNpcCouncilLine>& /*Lines*/, const FString& /*CareReply*/, ENpcChatFailure /*Failure*/);

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
	int32 Send(const FString& CharacterId, const FString& Message, FOnNpcChatReply OnReply,
		const FString& StoryEvent = FString());

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
	// THE COUNCIL (docs/THE_UNWRITTEN.md) — Leonard and all five agents at one table,
	// opened from the Tab menu. One player line, five answers, one request.
	//
	// IT SHARES THE ONE-AT-A-TIME SLOT with Send. A council turn is five paid calls on
	// the server; letting it run beside a dancer's reply would be the double-spend the
	// slot exists to prevent. Cancel works on either.
	// -----------------------------------------------------------------------

	/** Put Message to the table. Returns a request id, or 0 if refused (busy or disabled). */
	int32 SendCouncil(const FString& Message, FOnNpcCouncilReply OnReply, const FString& StoryEvent = FString());

	/**
	 * The table's conversation this play session, oldest first.
	 *
	 * NOT SAVED TO DISK, deliberately: the one-to-one store is keyed and bounded per agent
	 * (MaxTurnsKept, and ApplySaveObject trims to it), and a council turn is six lines at
	 * once. Quitting the game clears the table; level travel does not.
	 */
	const TArray<FNpcCouncilLine>& GetCouncilHistory() const { return CouncilHistory; }

	/** Leonard's line and the answers, appended together only once the table answered. */
	void RecordCouncilTurn(const FString& PlayerLine, const TArray<FNpcCouncilLine>& Replies);

	void ForgetCouncil() { CouncilHistory.Reset(); }

	/**
	 * Lines of table history kept and sent — the server's COUNCIL_HISTORY_LINES (30) uses
	 * exactly this many, and its maxTurns (64) refuses past that.
	 */
	static constexpr int32 CouncilLinesKept = 30;

	/** The council endpoint, derived from the chat one so there is one URL to configure. */
	static FString CouncilUrl();

	/** The council POST body: requestId, message, storyContext, recentTurns{speaker,text}. */
	static FString BuildCouncilRequestJson(
		const FString& RequestId,
		const FString& Message,
		const FString& LocationId,
		const TArray<FNpcCouncilLine>& RecentLines,
		const FString& StoryEvent = FString(),
		const FNpcStoryFacts* Facts = nullptr);

	/**
	 * Read a council response. True for an `ok` with at least one line, or with the care
	 * reply. A table where nobody spoke is a failure, never an empty turn.
	 */
	static bool ParseCouncilJson(const FString& Body, TArray<FNpcCouncilLine>& OutLines,
		FString& OutCareReply, ENpcChatFailure& OutFailure);

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
	 *
	 * THE BACKEND HAS ITS OWN NUMBER AND IT IS NOT THIS ONE. backend/server.js keeps the
	 * most recent LIMITS.promptTurns (24) and refuses outright past LIMITS.maxTurns (64).
	 * Raising this to anything up to 24 needs no backend change; past that the extra
	 * turns are trimmed away server-side and the player silently converses with less
	 * history than the panel is showing him. The two were once both 16 - exactly equal,
	 * so a bump here 400'd every request and the player read "unavailable".
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
	static FString SlotName();

	/**
	 * Point the store at a sandbox slot, for gates only.
	 *
	 * EXISTS BECAUSE THE GATE ATE A PLAYER'S CONVERSATIONS. ForgetAll and SaveNow are
	 * the functions under test, and calling them against the live slot deleted real
	 * saved history minutes before a walk-test — the empty "what she remembers" block
	 * was the gate's doing, not a bug in memory. ProgressionSmokeTest had this right
	 * from the start: "a sandbox slot so the gate can never touch a real player save."
	 */
	static void SetSlotNameForTesting(const FString& InSlotName);

	/** True while the service is the canned one. The panel says so, so nobody demos a mock. */
	static bool IsMock();

	/** The mode this process will actually use, after the URL check. */
	static ENpcChatMode CurrentMode();

	/** The configured backend, or empty. Not a secret — the key never leaves the proxy. */
	static FString ProxyUrl();

	/**
	 * Is that string something libcurl could actually dial?
	 *
	 * Exists because a mangled URL and a dead server produce the same player-facing
	 * line, and only one of them is worth debugging. See the note on the console and
	 * double slashes in the implementation.
	 */
	static bool IsUsableProxyUrl(const FString& Url);

	/** Exactly what was configured, before a scheme is supplied. For diagnostics. */
	static FString ConfiguredProxyUrl();

private:
	/** Shape check on a URL that already has its scheme. */
	static bool IsWholeUrl(const FString& Url);

public:

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
		const TArray<FNpcChatTurn>& RecentTurns,
		const FString& StoryEvent = FString(),
		const FNpcStoryFacts* Facts = nullptr);

	/** The facts object, written into storyContext by both request builders. */
	static void WriteStoryFacts(const TSharedRef<class FJsonObject>& Story, const FNpcStoryFacts& Facts);

	/**
	 * Read a backend response. Returns true only for a well-formed `ok` carrying text.
	 *
	 * Anything else — a declared failure, an unknown status, malformed JSON, an empty
	 * reply — comes back false with a reason, because a player must never be shown an
	 * empty speech bubble and told it was an answer.
	 */
	static bool ParseReplyJson(const FString& Body, FString& OutReply, ENpcChatFailure& OutFailure);

	/**
	 * The place an agent's reply offers to take him ("travel", a level name), or empty. Read
	 * from the same body as the reply; the panel re-checks it before showing a button.
	 */
	static FString ParseTravelOffer(const FString& Body);

	/** The last reply's travel offer, handed over once: reading it clears it. */
	FString TakeTravelOffer() { FString Offer = MoveTemp(LastTravelOffer); LastTravelOffer.Reset(); return Offer; }

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

	/** The council's halves of the same three. */
	bool SendCouncilOverHttp(int32 RequestId, const FString& Message);
	void DeliverMockCouncil(int32 RequestId);
	void CompleteCouncil(int32 RequestId, const TArray<FNpcCouncilLine>& Lines,
		const FString& CareReply, ENpcChatFailure Failure);

	/** The turns already in the store for this character — never the one being sent. */
	TArray<FNpcChatTurn> RecentTurnsFor(const FString& CharacterId) const;

	/** The level the player is standing in, for the server to allowlist. */
	FString CurrentLocationId() const;

	/** His playthrough as the agents may know it: the save and the HUD, read now. */
	FNpcStoryFacts CurrentStoryFacts() const;

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
	FOnNpcCouncilReply PendingCouncilReply;   // bound instead of PendingReply for a council turn
	TArray<FNpcCouncilLine> CouncilHistory;
	FString PendingCharacterId;
	FString PendingMessage;

	/** storyContext.event for the request in flight — an Easter egg (ChatEasterEggs.h). */
	FString PendingEvent;

	/** Set when a reply arrives with an offer; taken by the panel in its reply handler. */
	FString LastTravelOffer;

	FTimerHandle MockTimer;

	/** The live HTTP request, so Cancel can actually stop it. */
	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> PendingHttp;

	/** The UUID sent to the backend for this request, for dedup on its side. */
	FString PendingUuid;
};
