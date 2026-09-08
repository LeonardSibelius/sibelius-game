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
#include "Engine/LevelStreamingDynamic.h"
#include "Misc/PackageName.h"

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
		TEXT("Sibelius Hotel architecture  -  DmitriyDryzhak\n")
		// CC BY 4.0. This line is a LICENCE CONDITION, not the courtesy the rest are -
		// the Golden Trophy may be shipped only if Incg5764 is credited. Do not remove it
		// to shorten the card; remove the trophy instead.
		TEXT("The Transhuman trophy  -  Incg5764\n")
		TEXT("The passage and the portal  -  Dr.Game\n")
		TEXT("The cathedral  -  Ultima Store\n")
		TEXT("Stained glass  -  twins-creators\n")
		TEXT("Gideon, Greystone and Echo  -  Epic Games");

	/* THE CARD OF KEYS LIVES IN UGrokEndingSubsystem::DoorsCard() NOW, not here.

	   It has to be built rather than written down, because [G] belongs on it only while
	   there is still a hotel to generate — and since Walt's reordering that card is shown
	   twice: once as the offer straight after Nyra speaks, and once at the end of the
	   memoir roll, by which time the building is usually already standing.

	   None of the keys are bound here in any case, for the reason ABattleArrival gives
	   about its own pair: [O] is already global and N-N is already the player-facing New
	   Game. This stages the offer and owns none of them. */

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
	if (UWorld::RemovePIEPrefix(InWorld.GetMapName()) == TEXT("L_Grok"))
	{
		const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
		bHotelRequested = Progression && Progression->HasClaimedGrant(TEXT("Grok.HotelGenerated"));
		PrepareHotel(); // Load invisibly during the speech; G is the visible reveal.
	}

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
		// The offer's fallback outlives nothing here, but a pending PlayEnding on a world
		// being torn down is the same call into a dead HUD that ClosingTimer guards against.
		World->GetTimerManager().ClearTimer(OfferTimer);
	}
	TalkFinishedHandle.Reset();
	if (HotelLevel)
	{
		HotelLevel->OnLevelLoaded.RemoveDynamic(this, &UGrokEndingSubsystem::HotelLoaded);
		HotelLevel->OnLevelShown.RemoveDynamic(this, &UGrokEndingSubsystem::HotelShown);
	}

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

	OfferEpilogue();
}

FString UGrokEndingSubsystem::DoorsCard() const
{
	/* DROP THE [G] LINE ONCE THERE IS NOTHING LEFT TO GENERATE. Offering a key that now
	   only answers "already standing" is the kind of small lie a last screen should not
	   tell. */
	FString Card;
	if (!bHotelGenerated)
	{
		Card += TEXT("[G]   Generate the Sibelius Hotel\n");
	}
	Card += TEXT("[O]   the office, one more time\n");
	Card += TEXT("[N] [N]   begin again");
	return Card;
}

void UGrokEndingSubsystem::OfferEpilogue()
{
	if (bOffered || bRunning)
	{
		return;
	}
	bOffered = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowBanner(DoorsCard(), EpilogueFallbackSeconds);
		}
	}

	/* AND THE MEMOIR COMES ANYWAY IF HE NEVER PRESSES IT. The roll is the ending of the
	   game; putting it behind an optional key would mean a player who walked off to look
	   at the sky never saw a word of it. PlayEnding early-outs on bRunning, so whichever
	   of the two paths arrives first wins and the other does nothing. */
	World->GetTimerManager().SetTimer(OfferTimer, this,
		&UGrokEndingSubsystem::PlayEnding, EpilogueFallbackSeconds, /*bLoop=*/false);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[GrokEnding] Offered the hotel. The memoir follows it, or follows %.0fs of silence."),
		EpilogueFallbackSeconds);
}

bool UGrokEndingSubsystem::TryGenerateHotel()
{
	UWorld* World = GetWorld();
	if (!World || UWorld::RemovePIEPrefix(World->GetMapName()) != TEXT("L_Grok"))
	{
		return false;
	}
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;

	/* EVERY REFUSAL SAYS SOMETHING (docs/SIBELIUS_HOTEL.md, and this project's oldest
	   complaint about this exact key).

	   These branches all returned a bare true, which tells ToggleGenerate "handled, stop
	   here" — so on Grok, G opened nothing, built nothing and said nothing. The comment
	   block immediately below the call site in ASibeliusGameCharacter::ToggleGenerate is
	   about Walt's own bug report on this key: "now G does nothing no matter how many times
	   I press it", and its rule is "say so before anything can swallow it".

	   A silent key has two causes that look identical from the hillside — the feature is not
	   ready for you yet, or the feature is broken — and the player cannot tell them apart.
	   Each refusal now names which. */
	/* G IS LIVE THE MOMENT SHE HAS FINISHED SPEAKING — it no longer waits for the roll,
	   because the roll now waits for IT. If the fallback started the memoir first, G still
	   works underneath it; the hotel simply appears while his forty years scroll past,
	   which is not a bad accident. No banner during the roll, though: the credits own the
	   screen, and a second speaker on it is a collision this HUD has hit twice. */
	if (!Progression || !Progression->HasClaimedGrant(UDancerAgentComponent::GrokTalkedGrant))
	{
		if (HUD)
		{
			HUD->ShowBanner(TEXT("Nyra is waiting. Hear her out first."), 4.0f);
		}
		return true;
	}
	if (bHotelGenerated)
	{
		if (HUD)
		{
			HUD->ShowBanner(TEXT("The Sibelius Hotel is already standing. The entrance is ahead of Nyra."), 5.0f);
		}
		return true;
	}
	if (bHotelRequested)
	{
		if (HUD)
		{
			HUD->ShowBanner(TEXT("The Sibelius Hotel is on its way..."), 3.0f);
		}
		return true;
	}
	PrepareHotel();
	if (!HotelLevel)
	{
		if (HUD) { HUD->ShowBanner(TEXT("The Sibelius Hotel is not installed."), 4.0f); }
		return true;
	}
	bHotelRequested = true;
	if (HUD) { HUD->ShowBanner(TEXT("Generating the Sibelius Hotel..."), 4.0f); }
	HotelLoaded();
	return true;
}

void UGrokEndingSubsystem::PrepareHotel()
{
	if (HotelLevel) { return; }
	const FString Map(TEXT("/Game/Maps/L_SibeliusHotel_Generated"));
	if (!FPackageName::DoesPackageExist(Map)) { return; }
	// Entrance is 40 m ahead of the Grok arrival, facing into the landscape.
	// The approach descends 5.25 m back toward Nyra without moving the player.
	const FRotator Rotation(0.0f, 294.6f, 0.0f);
	FVector Entrance = FVector(193891.4, 149016.0, -1000.0) + Rotation.RotateVector(FVector(4000, 0, 0));
	const FVector Offset = Entrance - Rotation.RotateVector(FVector(1900, 500, 275.36388));
	ULevelStreamingDynamic::FLoadLevelInstanceParams Params(GetWorld(), Map, FTransform(Rotation, Offset));
	Params.bInitiallyVisible = false;
	bool bSuccess = false;
	HotelLevel = ULevelStreamingDynamic::LoadLevelInstance(Params, bSuccess);
	if (!bSuccess) { HotelLevel = nullptr; return; }
	HotelLevel->OnLevelLoaded.AddDynamic(this, &UGrokEndingSubsystem::HotelLoaded);
	HotelLevel->OnLevelShown.AddDynamic(this, &UGrokEndingSubsystem::HotelShown);
	HotelLoaded();
}

void UGrokEndingSubsystem::HotelLoaded()
{
	if (HotelLevel && bHotelRequested)
	{
		HotelLevel->SetShouldBeVisible(true);
	}
}

void UGrokEndingSubsystem::HotelShown()
{
	bHotelGenerated = true;
	if (UProgressionSubsystem* Progression = bHotelPreview ? nullptr : UProgressionSubsystem::Get(this))
	{
		Progression->ClaimOneTimeGrant(TEXT("Grok.HotelGenerated"));
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			/* SIXTEEN SECONDS, NOT EIGHT (Walt: "could you please make this menu last a
			   little longer? it vanishes quickly").

			   Three lines, and the middle one is the point of the whole epilogue — a man
			   who spent forty years building systems nobody kept finally builds something
			   beautiful. It is also the only time the game says where the entrance is.
			   Eight seconds was less time than it takes to look up at the building. */
			HUD->ShowBanner(TEXT("SIBELIUS HOTEL\nSomething beautiful, at last.\nThe entrance is ahead of Nyra."),
				HotelBannerSeconds);
		}
	}

	/* AND NOW THE MEMOIR, WITH THE HOTEL STANDING BEHIND IT.

	   This is the whole of Walt's reordering. He built the thing; the eight messages about
	   the systems that were never kept now read as the answer to it rather than the
	   preamble. Held until the hotel's own banner has cleared so the two do not share the
	   screen — ShowBanner and ShowMemoir are different channels drawn 90 px apart, and the
	   memoir roll starts with a banner of its own. */
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OfferTimer);   // the fallback is no longer needed
		if (!bRunning)
		{
			World->GetTimerManager().SetTimer(OfferTimer, this,
				&UGrokEndingSubsystem::PlayEnding, HotelBannerSeconds + 1.0f, /*bLoop=*/false);
		}
	}
}

void UGrokEndingSubsystem::PreviewHotel()
{
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::PIE ||
		UWorld::RemovePIEPrefix(GetWorld()->GetMapName()) != TEXT("L_Grok")) { return; }
	bHotelPreview = true;
	bHotelRequested = true;
	PrepareHotel();
	HotelLoaded();
}

FString UGrokEndingSubsystem::HotelStatus() const
{
	return FString::Printf(TEXT("world=%s type=%d package=%d requested=%d generated=%d stream=%d loaded=%d visible=%d"),
		*GetWorld()->GetMapName(), int32(GetWorld()->WorldType),
		FPackageName::DoesPackageExist(TEXT("/Game/Maps/L_SibeliusHotel_Generated")),
		bHotelRequested, bHotelGenerated, HotelLevel != nullptr,
		HotelLevel && HotelLevel->IsLevelLoaded(), HotelLevel && HotelLevel->IsLevelVisible());
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
			// DoorsCard(), not the fixed string: by the time the roll ends the hotel is
			// usually already standing, and offering [G] then would be a key that only
			// answers "already standing".
			HUD->ShowBanner(DoorsCard(), DoorsDwellSeconds);
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
