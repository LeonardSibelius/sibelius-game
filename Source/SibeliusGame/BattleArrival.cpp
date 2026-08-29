// BattleArrival.cpp — see header for why the grant is framed rather than just applied.

#include "BattleArrival.h"

#include "BattleFormComponent.h"
#include "SibeliusGame.h"
#include "SibeliusHUD.h"
#include "SwarmBenchSubsystem.h"
#include "RefuserController.h"
#include "ProgressionSubsystem.h"

#include "EngineUtils.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

const FName ABattleArrival::BattleWonGrant(TEXT("Battle.Won"));

ABattleArrival::ABattleArrival()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABattleArrival::BeginPlay()
{
	Super::BeginPlay();

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(T1, this, &ABattleArrival::ShowTheArmy,   SeeThemAtSeconds,   false);
	Timers.SetTimer(T2, this, &ABattleArrival::SpeakTheGrant, GrantLineAtSeconds, false);
	Timers.SetTimer(T3, this, &ABattleArrival::GiveTheBody,   GrantBodyAtSeconds, false);
	Timers.SetTimer(T4, this, &ABattleArrival::LetThemCome,   TheyComeAtSeconds,  false);
}

/* HELD, so they are SEEN before they are fought. A charge that begins the instant he
   loads gives him no moment to understand what he has walked into - and the whole reason
   the meadow exists rather than a forest is that an army you cannot see is not an army. */
void ABattleArrival::ShowTheArmy()
{
	if (USwarmBenchSubsystem* Swarm = GetWorld() ? GetWorld()->GetSubsystem<USwarmBenchSubsystem>() : nullptr)
	{
		const int32 Landed = Swarm->SpawnRidge(ArmyCount, ArmyDistanceMetres,
			ArmyArcDegrees, ArmyRanks, /*bLetThemCharge=*/false);
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[BattleArrival] %d Architects on the field."), Landed);
	}
}

void ABattleArrival::SpeakTheGrant()
{
	/* THE AGENTS SPEAK, NOT MRS. HALL. She has her own line and it fires at the cathedral
	   machine when the toll is paid - contemptuous, about margins, about what he has not
	   won. This one is the opposite voice and it belongs to the five who have spent the
	   whole game handing him things. */
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowBanner(GrantLine.ToString(), 5.0f);
			return;
		}
	}
	ASibeliusHUD::Toast(this, GrantLine.ToString(), 5.0f, SibeliusToast::Good);
}

void ABattleArrival::GiveTheBody()
{
	ACharacter* C = UGameplayStatics::GetPlayerCharacter(this, 0);
	UBattleFormComponent* Battle = C ? C->FindComponentByClass<UBattleFormComponent>() : nullptr;
	if (Battle && !Battle->IsInBattleForm())
	{
		// No blend on purpose - a transformation is a cut. See BattleFormComponent.
		Battle->EnterBattleForm();
	}
}

/* AND ONLY NOW DO THEY MOVE. Held Refusers gave their navigation invokers back on spawn,
   so this hands them over to their own controllers by clearing them and spawning the
   charge - the simplest thing that works, and the army is already the right shape. */
void ABattleArrival::LetThemCome()
{
	USwarmBenchSubsystem* Swarm = GetWorld() ? GetWorld()->GetSubsystem<USwarmBenchSubsystem>() : nullptr;
	if (!Swarm)
	{
		return;
	}
	Swarm->ClearAll();
	const int32 Landed = Swarm->SpawnRidge(ArmyCount, ArmyDistanceMetres,
		ArmyArcDegrees, ArmyRanks, /*bLetThemCharge=*/true);
	UE_LOG(LogSibeliusGame, Display, TEXT("[BattleArrival] %d Architects moving."), Landed);

	// Only now is there a battle to win. Poll once a second from here.
	bBattleJoined = true;
	GetWorldTimerManager().SetTimer(VictoryPoll, this,
		&ABattleArrival::WatchForVictory, 1.0f, /*bLoop=*/true, /*FirstDelay=*/2.0f);
}

/* WATCHING FOR AN EMPTY FIELD.

   Once a second, not per frame: this is looking for a state that takes minutes to
   arrive, and a hundred and fifty pawns do not need counting sixty times a second. The
   same file already learned that lesson the expensive way with the chase log.

   A slapped Refuser is unpossessed and then destroyed on its ragdoll lifespan, so
   "still has an ARefuserController" is exactly "still fighting" - and it is the same
   test the slap and the crowd both use to decide what a Refuser is. Three places, one
   answer. */
void ABattleArrival::WatchForVictory()
{
	if (bWon || !bBattleJoined)
	{
		return;
	}
	int32 Standing = 0;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (Cast<ARefuserController>(It->GetController()))
		{
			++Standing;
			break;   // one is enough to know the fight is not over
		}
	}
	if (Standing == 0)
	{
		bWon = true;
		GetWorldTimerManager().ClearTimer(VictoryPoll);
		GetWorldTimerManager().SetTimer(VictoryBeat, this,
			&ABattleArrival::DeclareVictory, VictoryPauseSeconds, false);
	}
}

void ABattleArrival::DeclareVictory()
{
	/* THE CITY OPENS HERE, not when he picks a door. Claimed before a single word is
	   drawn, so a player who walks away from the choice, presses O out of habit, or
	   alt-F4s in triumph still finds [>] live in the office tomorrow. Winning is what
	   unlocks the city; reading the banner is not. */
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->ClaimOneTimeGrant(BattleWonGrant);
	}

	const FString Line = VictoryLine.ToString() + LINE_TERMINATOR + ComingSoonLine.ToString();

	// The doors come AFTER the ending has been read, never on top of it.
	GetWorldTimerManager().SetTimer(ChoiceBeat, this,
		&ABattleArrival::OfferTheChoice, ChoicePauseSeconds, false);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			// Long, and deliberately so. This is the last thing the game says.
			HUD->ShowBanner(Line, 12.0f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[BattleArrival] the field is empty."));
			return;
		}
	}
	ASibeliusHUD::Toast(this, Line, 12.0f, SibeliusToast::Good);
}

/* ===========================================================================
   THE TWO DOORS.

   NEITHER KEY IS BOUND HERE, and that is the whole shape of it.

   [O] was already global - ASibeliusGameCharacter binds it to ReturnToOffice, which
   no-ops unless IsAwayFromOffice(). [>] is now global too, bound beside it and gated on
   the saved BattleWonGrant instead of on this actor being alive.

   That was Walt's call and it is the better design: "I want the player to know that a
   city is waiting." A key that only exists for six seconds at the end of one fight
   cannot advertise anything. A key that is in the CONTROLS list from the first minute,
   greyed out and saying what it wants, is a promise the game keeps.

   So this class stages the OFFER and owns nothing else. Both doors work whether the
   banner is on screen or not.
   =========================================================================== */

void ABattleArrival::OfferTheChoice()
{
	if (bChoiceOffered)
	{
		return;
	}
	bChoiceOffered = true;

	KeepTheChoiceUp();

	// Held, not fired once: see the header. The banner is a deadline, so re-arming it
	// well inside its own duration holds it on screen without a flicker. It stops when
	// the level is torn down by whichever door he picks.
	GetWorldTimerManager().SetTimer(ChoiceHold, this,
		&ABattleArrival::KeepTheChoiceUp, 3.0f, /*bLoop=*/true);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[BattleArrival] the doors are open: O -> office, > -> the city."));
}

void ABattleArrival::KeepTheChoiceUp()
{
	const FString Line = ChoiceLine.ToString();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowBanner(Line, 5.0f);
			return;
		}
	}
	ASibeliusHUD::Toast(this, Line, 5.0f, SibeliusToast::Info);
}
