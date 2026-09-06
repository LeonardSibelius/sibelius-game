// GrokEndingSubsystem.cpp — see the header for why this is a subsystem and not an actor.

#include "GrokEndingSubsystem.h"

#include "SibeliusGame.h"                  // LogSibeliusGame
#include "SibeliusHUD.h"                   // ShowMemoir / ShowBanner — the two text channels
#include "DancerAgentComponent.h"          // GrokTalkedGrant, GuideStage
#include "DancerAgentSubsystem.h"          // OnGuideTalkFinished
#include "ProgressionSubsystem.h"          // the one-time claim
#include "ProgressionTypes.h"              // AllMemoirMessages
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	/* THE CREDITS, AND WHY THEY ARE IN THE GAME AT ALL.

	   Five artists are named on the itch page and on the Steam draft and were, until now,
	   nowhere in the thing they built. Some Fab licences ask for attribution outright; but
	   the better reason is the one the store page argues — this is a game about a man who
	   could not build alone and finally could, so naming the people whose work he built
	   with is the same sentence as the rest of it.

	   TWO CARDS, NOT ONE. Eleven names on a single card is a wall nobody reads, and
	   ASibeliusHUD::ShowBanner centres a block that keeps growing until it meets the
	   memoir line's territory. Split at the natural seam: who made the game, then whose
	   work is in it.

	   KEPT IN SYNC BY HAND with README.md's Credits, docs/VENDOR_PACKS.md and
	   docs/STEAM_PLAN.md section 3. There is no single machine-readable list of vendor
	   packs in this project and inventing one to serve a credits card would be the tail
	   wagging the dog — but if a pack is ever added, this is the fourth place to add it. */
	const TCHAR* const P_MakersCard =
		TEXT("LEONARD SIBELIUS\n")
		TEXT("\n")
		TEXT("Walt Parkman\n")
		TEXT("with Claude, by Anthropic\n")
		TEXT("\n")
		TEXT("Built in Unreal Engine 5.7");

	const TCHAR* const P_ArtistsCard =
		TEXT("MADE WITH THE WORK OF INDEPENDENT ARTISTS\n")
		TEXT("\n")
		TEXT("Kaia, Nyra, Isla, Aisling and Elise  -  xAndrei\n")
		TEXT("Dance animation  -  Morro Motion\n")
		TEXT("Downtown West  -  Jacob Norris / PurePolygons\n")
		TEXT("The office and its furniture  -  QuadArt\n")
		TEXT("The space port  -  PackDev\n")
		TEXT("Grok  -  Velarion\n")
		TEXT("The passage and the portal  -  Dr.Game\n")
		TEXT("The cathedral  -  Ultima Store\n")
		TEXT("Stained glass  -  twins-creators\n")
		TEXT("Gideon, Greystone and Echo  -  Epic Games");

	/* THE LAST CARD. Both keys are global and neither is bound here — see the header.

	   [O] is named first because it is the one he already knows: it has taken him home
	   from every world since the Many Worlds door. N-N is named second and plainly,
	   because a player who has just finished should be told how to start again rather than
	   left to find it in the controls list. */
	const TCHAR* const P_DoorsCard =
		TEXT("[O]   the office, one more time\n")
		TEXT("[N] [N]   begin again");

	/* SEE IT WITHOUT PLAYING SIX HOURS TO GROK.

	   The whole sequence is about ninety seconds of timed text, and every timing in it is
	   an eye judgement — is the lead-in long enough that the memoir does not read as a
	   reply to her, do the credits hold long enough to find a name. Answering that by
	   playing to Grok each time is not an evening anyone should spend, and the boarding
	   lamp and the wormhole FX both learned the same lesson the same week. */
	static FAutoConsoleCommandWithWorld GCmdGrokEnding(
		TEXT("sib.GrokEnding"),
		TEXT("Play the closing sequence (memoir, credits, the two doors) here and now."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (UGrokEndingSubsystem* Sub = World ? World->GetSubsystem<UGrokEndingSubsystem>() : nullptr)
			{
				Sub->PlayEnding();
			}
		}));
}

void UGrokEndingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	/* SUBSCRIBE IN EVERY WORLD, and let the event never come in most of them. The
	   alternative — testing the map name here — would put a second copy of "which level is
	   Grok" in the codebase, and the event itself already cannot fire anywhere else: it
	   needs a guide at stage 4, and stage 4 IS being on Grok
	   (UDancerAgentComponent::GuideStage). One rule, one place. */
	if (UDancerAgentSubsystem* Dancers = InWorld.GetSubsystem<UDancerAgentSubsystem>())
	{
		// AddUObject, not AddRaw: the engine refuses raw method delegates on a UObject, and
		// it is right to — this binding holds a WEAK reference, so a subsystem torn down
		// mid-roll goes quiet instead of leaving a dangling call behind it.
		TalkFinishedHandle = Dancers->OnGuideTalkFinished.AddUObject(
			this, &UGrokEndingSubsystem::HandleGuideTalkFinished);
	}
}

void UGrokEndingSubsystem::Deinitialize()
{
	/* UNSUBSCRIBING IS TIDINESS; CLEARING THE TIMER IS NOT.

	   The AddUObject binding is weak and would go quiet on its own, so the Remove below is
	   housekeeping rather than a crash fix. The timer is the real one: he can press O
	   mid-roll, and a pending AdvanceClosing on a world being torn down is a call into a
	   HUD that is already gone. */
	if (const UWorld* World = GetWorld())
	{
		if (UDancerAgentSubsystem* Dancers = World->GetSubsystem<UDancerAgentSubsystem>())
		{
			Dancers->OnGuideTalkFinished.Remove(TalkFinishedHandle);
		}
		World->GetTimerManager().ClearTimer(ClosingTimer);
	}
	TalkFinishedHandle.Reset();

	Super::Deinitialize();
}

void UGrokEndingSubsystem::HandleGuideTalkFinished(UDancerAgentComponent* Dancer)
{
	// The channel deliberately does not pre-filter (see NotifyGuideTalkFinished), so every
	// guide in the game arrives here and all but one of them leaves again.
	if (!Dancer || Dancer->GuideStage() < 4)
	{
		return;
	}

	PlayEnding();
}

void UGrokEndingSubsystem::PlayEnding()
{
	if (bRunning)
	{
		return;   // a second E on her restarts her line; it must not stack a second roll
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bRunning = true;
	ClosingIndex = -1;

	UE_LOG(LogSibeliusGame, Display, TEXT("[GrokEnding] Good job, Leonard. Rolling."));

	World->GetTimerManager().SetTimer(ClosingTimer, this,
		&UGrokEndingSubsystem::AdvanceClosing, FMath::Max(0.5f, LeadInSeconds), /*bLoop=*/false);
}

void UGrokEndingSubsystem::AdvanceClosing()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<FString>& Messages = AllMemoirMessages();
	const int32 MemoirCount = Messages.Num();

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;

	float NextDelay = MemoirDwellSeconds;

	if (ClosingIndex < 0)
	{
		/* THE FIRST BEAT IS SILENCE, deliberately. Her last words are "Good job, Leonard",
		   and putting a message on screen underneath them would make forty years of grudges
		   read as his reply to a compliment. The gap is what turns them into credits. */
		NextDelay = 0.0f;   // fall straight through to the first message
	}
	else if (ClosingIndex < MemoirCount)
	{
		if (HUD)
		{
			/* THE SAME CHANNEL AND THE SAME OVERLAP the finale altar uses: dwell a little
			   longer than the gap so each line is still up as the next arrives. It reads as
			   a continuous roll instead of a flicker between blank screens.

			   EXCEPT THE LAST ONE, which is followed by a credits card rather than another
			   message. The two live on DIFFERENT channels drawn 90 px apart, so the overlap
			   that reads as continuity between memoir lines would here be a six-line banner
			   landing on top of iKrome 2022. The final message clears half a second early
			   instead, which also gives the credits a beat of black to arrive into. */
			const bool bLastMessage = (ClosingIndex == MemoirCount - 1);
			HUD->ShowMemoir(Messages[ClosingIndex],
				bLastMessage ? FMath::Max(0.5f, MemoirDwellSeconds - 0.5f)
				             : MemoirDwellSeconds + 0.75f);
		}
		NextDelay = MemoirDwellSeconds;
	}
	else if (ClosingIndex == MemoirCount)
	{
		if (HUD)
		{
			HUD->ShowBanner(P_MakersCard, CreditsDwellSeconds + 0.75f);
		}
		NextDelay = CreditsDwellSeconds;
	}
	else if (ClosingIndex == MemoirCount + 1)
	{
		if (HUD)
		{
			HUD->ShowBanner(P_ArtistsCard, CreditsDwellSeconds + 0.75f);
		}
		NextDelay = CreditsDwellSeconds;
	}
	else if (ClosingIndex == MemoirCount + 2)
	{
		if (HUD)
		{
			HUD->ShowBanner(P_DoorsCard, DoorsDwellSeconds);
		}
		/* AND THE OBJECTIVE BANNER PICKS THEM UP FROM HERE. This card fades; the two doors
		   do not, so ASibeliusHUD::CityObjective reads HasFinished and keeps them on screen
		   in gold from now on. Without that, a player who looked away for thirty seconds is
		   standing on an alien hillside with no stated way off it. */
		bFinished = true;
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[GrokEnding] done: %d memoir message(s), then the doors."), MemoirCount);
		return;   // the last card. No further timer; both keys were always live.
	}
	else
	{
		return;
	}

	++ClosingIndex;

	// A zero delay would never fire, so the lead-in's fall-through gets the shortest tick
	// the timer manager will honour rather than a special case at the call site.
	World->GetTimerManager().SetTimer(ClosingTimer, this,
		&UGrokEndingSubsystem::AdvanceClosing, FMath::Max(0.01f, NextDelay), /*bLoop=*/false);
}
