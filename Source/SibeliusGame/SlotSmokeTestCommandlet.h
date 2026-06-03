// SlotSmokeTestCommandlet.h
//
// SIB-34 / S1 — The regulator's suite. Runs USlotGameModel through ~1,000,000
// simulated base spins (free spins played out inline) and asserts the par
// sheet holds: RTP band, hit frequency, bonus trigger rate, max-win exposure,
// determinism, no negative/NaN pays.
//
// No map loading, no editor utilities — pure model simulation.
//
// Run:
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=SlotSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SlotSmokeTestCommandlet.generated.h"

UCLASS()
class USlotSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USlotSmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
