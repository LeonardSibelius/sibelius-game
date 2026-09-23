// NpcCouncilSession.h
//
// The council meeting: Leonard and all five agents at one table (docs/THE_UNWRITTEN.md),
// opened from the Tab menu's COUNCIL button. There is no room for it — Walt, 2026-09-23:
// "i really don't think a room is necessary" — so this is a panel and nothing else.
//
// THE PANEL IS THE ONE-TO-ONE CHAT PANEL. UNpcChatWidget already draws a transcript of named
// speakers, a waiting state, the failure line and the Google notice; a council turn is just
// five named lines instead of one. What differs is who answers, so this object is the
// council's counterpart to the dancer's chat half of UDancerAgentComponent: it owns the
// widget, the one request, and the input handback. No camera, no dance, no voice.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NpcChatSubsystem.h"   // FNpcCouncilLine, ENpcChatFailure
#include "NpcCouncilSession.generated.h"

class APlayerController;
class UNpcChatWidget;

UCLASS()
class SIBELIUSGAME_API UNpcCouncilSession : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Whether the menu should offer the council at all.
	 *
	 * The same gate as talking to a dancer — sib.Chat.Enable, which is 0 in Shipping — plus
	 * a chat service that is actually configured. The Steam build describes a game that
	 * generates nothing at runtime, and a button that opens a live AI table would break
	 * that promise from the menu instead of from E.
	 */
	static bool IsAvailable();

	/** Open the panel for this player. Replays the table so far. */
	void Open(APlayerController* PC);

	/** Cancel anything in flight, hide the panel, hand input back to the game. */
	void Close();

	bool IsOpen() const { return bOpen; }

private:
	void HandleSend(const FString& PlayerLine);
	void HandleClose();
	void HandleReply(const TArray<FNpcCouncilLine>& Lines, const FString& CareReply, ENpcChatFailure Failure);

	UNpcChatSubsystem* GetChat() const;

	UPROPERTY()
	TObjectPtr<UNpcChatWidget> Widget;

	TWeakObjectPtr<APlayerController> OwningPC;

	/** The line being answered, recorded with the answers only once they arrive. */
	FString PendingLine;

	int32 RequestId = 0;
	bool bOpen = false;
};
