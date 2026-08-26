// SibeliusProgressSubsystem.h
//
// SIB-43 — session progress that must SURVIVE LEVEL TRAVEL (CL1/CL2).
// Actors die with their world on OpenLevel; the GameInstance does not.
// Session-only by design — no save-file coupling (consistent with the
// free-play slot decision). The clue chain reads these flags:
//
//   bIntroPlayed — the office apparition auto-bang fires once per session.
//   bSlotPlayed  — set when the cathedral cabinet opens its screen; flips
//                  the oracle computers from clue 1 to clue 2.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SibeliusProgressSubsystem.generated.h"

UCLASS()
class SIBELIUSGAME_API USibeliusProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	bool bIntroPlayed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	bool bSlotPlayed = false;

	/**
	 * AVideoCue ids already shown this session (docs/CINEMATICS.md).
	 *
	 * A SET, not a bool, because there will be more than one cutscene and each needs
	 * its own memory. Session-only like everything else here - travelling back to the
	 * office must not replay Kaia's introduction, but a fresh run of the game should
	 * show it again.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	TSet<FName> PlayedVideoCues;
};
