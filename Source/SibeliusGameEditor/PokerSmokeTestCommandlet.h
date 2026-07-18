// PokerSmokeTestCommandlet.h
//
// SIDE_GAMES G5 — the poker regulator's suite. Verifies the hand evaluator on
// constructed known hands (every rank, the wheel, low-pair-is-nothing), model
// determinism, deck honesty (no duplicate cards through a deal+draw), and
// simulates 200,000 hands under a simple-strategy player to report and band
// the RTP. No map loading — pure model simulation.
//
// Run:
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=PokerSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PokerSmokeTestCommandlet.generated.h"

UCLASS()
class UPokerSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPokerSmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
