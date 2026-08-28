// BattleArrival.h — what happens when he walks through the door.
//
// WHY THIS EXISTS. Walt, having played to the meadow: "why must I press the 5 and 4?"
// He should not. Those are debug keys, made so the fight could be iterated on without
// playing the whole game to reach it. A player walks through a door and the battle is
// already happening; nobody presses anything.
//
// ---------------------------------------------------------------------------
// THE GRANT IS THE POINT, AND IT IS NOT DECORATION.
//
// BattleFormComponent.h is explicit about this and it is worth repeating where the beat
// is actually staged: a 71-year-old programmer who turns into a Paragon sword hero has
// quietly stopped being a memoir. An AI that GIVES him a body which can fight is the
// same move Kaia made in the opening when she gave him his name — his employer calls him
// "Programmer" and refuses the name; the agents hand him both.
//
// That distinction lives entirely in framing and costs nothing to get right. So the body
// does not simply appear: it is announced, by them, before it arrives. Skip the line and
// it is a costume change.
//
// ---------------------------------------------------------------------------
// THE ORDER MATTERS, AND IT IS DELIBERATELY SLOW.
//
//   0.0s  he arrives. Empty meadow, quiet. Let him look.
//   1.5s  the army is already there, on the ridge, facing him. Seen before it is fought.
//   4.0s  the agents speak. The grant.
//   6.0s  the body. Hard cut, no blend — a transformation is a cut.
//   7.5s  they come.
//
// Six seconds of standing in a field looking at an army is not dead time; it is the only
// moment in the game where he sees what he is walking into before it starts. The fight
// is louder for having waited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleArrival.generated.h"

/**
 * Place ONE in L_Meadow. Runs the arrival on BeginPlay and then does nothing forever.
 */
UCLASS()
class SIBELIUSGAME_API ABattleArrival : public AActor
{
	GENERATED_BODY()

public:
	ABattleArrival();

	/** How many Architects are waiting. 150 reads as an army at this range and costs
	 *  about 7 ms; 400 is a wall and still runs. See docs/SWARM_PLAN.md. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival", meta = (ClampMin = "1"))
	int32 ArmyCount = 150;

	/** Metres out. 120 is where an army READS — at 300 a man is nine pixels tall and the
	 *  ridge line reads as gravel. Measured, not chosen. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival", meta = (ClampMin = "10"))
	float ArmyDistanceMetres = 120.0f;

	/** Degrees of horizon they occupy. A front, not a ring: being surrounded on arrival
	 *  is a trap, and a trap is a different feeling from a battle. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival", meta = (ClampMin = "10", ClampMax = "360"))
	float ArmyArcDegrees = 60.0f;

	/** Ranks deep. Depth is what turns a line of dots into a mass. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival", meta = (ClampMin = "1"))
	int32 ArmyRanks = 12;

	/** They stand and are looked at before they move. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival")
	float SeeThemAtSeconds = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Battle Arrival")
	float GrantLineAtSeconds = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Battle Arrival")
	float GrantBodyAtSeconds = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Battle Arrival")
	float TheyComeAtSeconds = 7.5f;

	/* THE END OF IT, IN WALT'S OWN WORDS.

	   He killed the last of them and asked for this: "can there be a message something
	   like 'AI has set you free. More adventures coming soon.'"

	   Kept close to verbatim, because it is the thesis of the entire game said plainly by
	   the man whose forty years it is about. Every AI in this story has given him
	   something - Kaia gave him his name in the opening when his employer would only call
	   him Programmer, the agents gave him the powers, and at the meadow they gave him a
	   body that could reach the men who were never on call. This is the receipt.

	   Editable properties, because the wording of an ending belongs to its author. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival|Victory")
	FText VictoryLine = NSLOCTEXT("Sibelius", "BattleWon", "AI has set you free.");

	UPROPERTY(EditAnywhere, Category = "Battle Arrival|Victory")
	FText ComingSoonLine = NSLOCTEXT("Sibelius", "BattleSoon", "More adventures coming soon.");

	/** A breath after the last one falls, so the line lands on a quiet field rather than
	 *  over the sound of a body hitting the grass. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival|Victory", meta = (ClampMin = "0"))
	float VictoryPauseSeconds = 2.0f;

	/** The agents' line. Editable because the wording is the whole beat. */
	UPROPERTY(EditAnywhere, Category = "Battle Arrival")
	FText GrantLine = NSLOCTEXT("Sibelius", "BattleGrant",
		"You gave us somewhere to stand. Here is a body that can reach them.");

protected:
	virtual void BeginPlay() override;

private:
	void ShowTheArmy();
	void SpeakTheGrant();
	void GiveTheBody();
	void LetThemCome();
	void WatchForVictory();
	void DeclareVictory();

	FTimerHandle T1, T2, T3, T4, VictoryPoll, VictoryBeat;
	bool bBattleJoined = false;   // no victory before there is a battle
	bool bWon = false;            // once
};
