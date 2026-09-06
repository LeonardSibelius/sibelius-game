// GrokEndingSubsystem.h — the last two minutes of the game.
//
// ===========================================================================
// docs/FUN_PLAN_2.md A2, and docs/SEVENTH_POWER.md rev 5, which designed this and left it
// unbuilt: "She says 'Good job, Leonard.' Then the messages roll, 1988 to 2022."
//
// WHAT WAS WRONG. Nyra finished her apology on Grok and the game did nothing. Not an
// ending — a stop. UDancerAgentComponent::EndTalkShot handed the controls back and that
// was the last thing that happened, on a hillside forty light years from an office, after
// six hours. A game that stops gets asked whether that was it.
//
// And the eight memoir messages — the strongest writing in this project, Walt's own words
// to eight former employers, 1988 to 2022 — appeared in exactly one place: the finale
// altar's read-back, twelve seconds at the cathedral, which many players never reach and
// nobody can re-read. They are the ending. They always were.
//
// ---------------------------------------------------------------------------
// WHY A SUBSYSTEM AND NOT A PLACED ACTOR.
//
// FUN_PLAN_2 proposed "a small placed actor (AGrokEnding), placed by
// place_grok_arrival.py". That was the right instinct — keep it out of
// UDancerAgentComponent, which is about talking and is already large enough — and the
// wrong container, for a reason that script's own header spells out:
//
//     Content/Maps/L_Grok.umap is GITIGNORED - it is 647 MB of purchased terrain and this
//     repo is public. So the level itself can never be committed, and anything placed BY
//     HAND in it exists on exactly one machine and is one disk failure from gone.
//
// Every actor placed in that level is a thing the repo cannot hold and a script must
// rebuild. The ending of the game should not be one of them. A world subsystem is created
// by the engine in every game world, needs no placement, cannot be dragged, cannot be lost
// with the .umap, and cannot be forgotten when the level is rebuilt from the recipe.
//
// It costs nothing in return: this listens for one event that only ever fires on Grok, so
// living in every level is free.
//
// ---------------------------------------------------------------------------
// IT ANSWERS AN EVENT, IT DOES NOT POLL.
//
// UDancerAgentSubsystem::OnGuideTalkFinished fires from ResumeAfterGreeting, which is the
// NATURAL end of a talk and only that — CancelGreeting clears the timer that reaches it,
// so an F press mid-sentence produces nothing here. Cutting her off does not roll the
// credits, and pressing E again gives him the whole speech and then the ending.
//
// ---------------------------------------------------------------------------
// AND IT RUNS ONCE, EVER, ON A SAVED GRANT.
//
// A second E on Nyra restarts her line — that is deliberate and shipped, so a player can
// hear it again. It must not roll a second set of credits over the first. The one-time
// claim is UDancerAgentComponent::GrokTalkedGrant, which is already saved, so the ending
// also does not replay for a player who quits on the hillside and comes back.
//
// ---------------------------------------------------------------------------
// THE TWO DOORS ARE NOT BOUND HERE, for exactly the reason ABattleArrival gives about its
// own pair: [O] is already global (ASibeliusGameCharacter binds it, and it no-ops unless
// IsAwayFromOffice), and N-N is already the player-facing New Game. This stages the OFFER
// and owns neither key. Both work whether the card is on screen or not.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GrokEndingSubsystem.generated.h"

class UDancerAgentComponent;

UCLASS()
class SIBELIUSGAME_API UGrokEndingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Real gameplay worlds only — the same rule UDancerAgentSubsystem uses, so the
	 *  commandlet gates never start a credits roll nobody is watching. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** Start the closing sequence now, whatever the state. Console: sib.GrokEnding.
	 *  The only way to see this without playing six hours to reach Grok. */
	void PlayEnding();

	/** Mid-roll. The objective banner stays out of the way while this is true. */
	bool IsRunning() const { return bRunning && !bFinished; }

	/* THE LAST CARD HAS BEEN DRAWN.

	   After which ASibeliusHUD::CityObjective takes the two doors over permanently, as the
	   gold line at the top. That is the one place in this game where a standing objective
	   is right: there is nothing else on Grok to do, the credits card has faded, and a
	   player who looked away during it would otherwise be left on a hillside with no way
	   out that he has been told about. Session state, deliberately — a player who reloads
	   into Grok gets the banner from CityObjective's own rules instead. */
	bool HasFinished() const { return bFinished; }

private:
	void HandleGuideTalkFinished(UDancerAgentComponent* Dancer);
	void AdvanceClosing();

	/* THE INDEX WALKS ONE LIST OF BEATS, the way AFinaleAltar::AdvanceClosingSequence
	   already does, because that code is proven and this is the same shape: a timer, an
	   index, and a dwell that changes at the seams.

	     -1        the beat after her voice stops, before anything is drawn
	      0..7     the eight memoir messages
	      8        the makers
	      9        the artists
	     10        the two doors
	     11+       done; no further timer */
	int32 ClosingIndex = -1;
	FTimerHandle ClosingTimer;
	bool bRunning = false;
	bool bFinished = false;

	/** Her line has ended and the screen is empty. Long enough that the memoir does not
	 *  feel like a reply to her, short enough that it does not read as the game hanging. */
	float LeadInSeconds = 4.0f;

	/** Gap between memoir messages. AFinaleAltar's own value — they were paced once. */
	float MemoirDwellSeconds = 5.0f;

	/** The credits cards hold longer than a memoir line: they are lists, and someone is
	 *  looking for their own name on them. */
	float CreditsDwellSeconds = 11.0f;

	/** The doors card, which is the last thing drawn and stays up long enough to act on. */
	float DoorsDwellSeconds = 30.0f;

	FDelegateHandle TalkFinishedHandle;
};
