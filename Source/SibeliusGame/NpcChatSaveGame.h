// NpcChatSaveGame.h
//
// What the agents remember (docs/NPC_CHAT_REVIEW.md, step 5).
//
// ITS OWN SLOT, AND THAT IS A DECISION. Conversations do not belong in the Progression
// slot: that one holds earned powers and Sauce and is deliberately separated from the
// deploy save so a player who clears deployments does not lose what he won. Memory is
// the opposite kind of thing — losing it costs nothing a player fought for, and mixing
// it in would make every future change to conversation storage a change to the file
// that holds his powers. Same discipline, different slot.
//
// Keyed by STABLE CHAT ID, never by actor name. The finale altar summons a dancer at
// runtime and the city dancers are placed MetaHumans; "BP_Dancer_Kaia_C_2" is a fact
// about one session's object graph, not about who she is.
//
// Save-archive serialization only carries CPF_SaveGame properties, so every persisted
// field here is UPROPERTY(SaveGame) — including the members of the nested structs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NpcChatSaveGame.generated.h"

/** One line of a conversation, as it will be replayed into the panel. */
USTRUCT()
struct FNpcChatTurn
{
	GENERATED_BODY()

	/** True if the player typed it. The agent said it otherwise. */
	UPROPERTY(SaveGame)
	bool bFromPlayer = false;

	UPROPERTY(SaveGame)
	FString Text;
};

/** Everything one agent remembers of one playthrough. */
USTRUCT()
struct FNpcChatHistory
{
	GENERATED_BODY()

	/**
	 * Most recent last, oldest first, and BOUNDED — see UNpcChatSubsystem::MaxTurnsKept.
	 *
	 * Bounded here rather than at send time because the bound is a property of what is
	 * kept, not of what is shown. When the proxy lands it will also be what fits in a
	 * prompt, and a history that grew without limit would quietly grow the cost of
	 * every request until it hit a token ceiling instead of a design decision.
	 */
	UPROPERTY(SaveGame)
	TArray<FNpcChatTurn> Turns;
};

UCLASS()
class SIBELIUSGAME_API UNpcChatSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Bump whenever the persisted shape changes, and migrate on load.
	// v1: per-character bounded turn lists.
	static constexpr int32 CurrentSaveVersion = 1;

	/**
	 * Stamped on write. A freshly constructed object reads 0, which is how a blank or
	 * garbage load is told from a real save — the same rule UProgressionSaveGame uses,
	 * and worth keeping identical so there is one convention to remember.
	 */
	UPROPERTY(SaveGame)
	int32 SaveVersion = 0;

	/** Stable chat id -> that agent's bounded history. */
	UPROPERTY(SaveGame)
	TMap<FName, FNpcChatHistory> Histories;
};
