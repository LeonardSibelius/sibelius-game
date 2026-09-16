// NpcChatSmokeTestCommandlet.h
//
// Headless gate for the typed NPC conversation (docs/NPC_CHAT_REVIEW.md, step 4).
//
// TWO JOBS, AND THE FIRST ONE MATTERS MOST.
//
//   1. THE SHIPPING PROMISE. Chat is off in every default: the component's
//      bChatEnabled and the sib.Chat.Enable console variable. The Steam submissions
//      in review describe a game that generates nothing at runtime, so a flipped
//      default is a release-grade mistake — and the kind that reviews well and ships
//      anyway. This gate fails loudly instead.
//
//   2. THE SERVICE CONTRACT. One request in flight, cancellation that really drops a
//      reply, ids that never repeat, and failures that arrive as reasons. Those are
//      the behaviours a shared free-tier allowance depends on, and they must be
//      proven against the mock BEFORE any of them is proven with somebody's quota.
//
// What it deliberately does NOT prove: the panel. Focus, cursor, the camera handback
// and Esc need a PIE walk-test (README, verification note) — BeginChat needs a player
// controller and a commandlet has none, so a green run here says nothing about them.
//
// Run editor-closed: -run=NpcChatSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NpcChatSmokeTestCommandlet.generated.h"

UCLASS()
class UNpcChatSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNpcChatSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
