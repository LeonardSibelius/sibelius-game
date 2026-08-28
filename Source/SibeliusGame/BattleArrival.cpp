// BattleArrival.cpp — see header for why the grant is framed rather than just applied.

#include "BattleArrival.h"

#include "BattleFormComponent.h"
#include "SibeliusGame.h"
#include "SibeliusHUD.h"
#include "SwarmBenchSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
}
