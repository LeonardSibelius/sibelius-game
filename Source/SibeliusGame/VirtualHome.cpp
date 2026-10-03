// VirtualHome.cpp — see header.

#include "VirtualHome.h"

#include "BattleFormComponent.h"
#include "RefactorComponent.h"
#include "SibeliusGame.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "TimerManager.h"

AVirtualHome::AVirtualHome()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AVirtualHome::LendsPower(const UObject* WorldContext, EPowerVerb Verb)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AVirtualHome> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->LentPowers.Contains(Verb))
		{
			return true;
		}
	}
	return false;
}

void AVirtualHome::BeginPlay()
{
	Super::BeginPlay();

	if (bHairOutOfRayTracing)
	{
		if (IConsoleVariable* Hair = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Raytracing")))
		{
			SavedHairRaytracing = Hair->GetInt();
			Hair->Set(0, ECVF_SetByCode);
		}
		else
		{
			UE_LOG(LogSibeliusGame, Warning, TEXT("[VirtualHome] r.HairStrands.Raytracing not found - hair stays in ray tracing."));
		}
	}

	if (Appearance)
	{
		for (const TPair<FName, float>& Dial : AppearanceScalars)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(this, Appearance, Dial.Key, Dial.Value);
		}
	}

	// Ask again every tenth of a second until the player's pawn exists, for up to five
	// seconds: the GameMode possesses it after the level's actors have begun play.
	GetWorldTimerManager().SetTimer(ArriveTimer, this, &AVirtualHome::Arrive, 0.1f, /*bLoop=*/true, 0.1f);
}

void AVirtualHome::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SavedHairRaytracing != INDEX_NONE)
	{
		if (IConsoleVariable* Hair = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Raytracing")))
		{
			Hair->Set(SavedHairRaytracing, ECVF_SetByCode);
		}
		SavedHairRaytracing = INDEX_NONE;
	}
	Super::EndPlay(Reason);
}

void AVirtualHome::Arrive()
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player)
	{
		if (++ArriveAttempts > 50)
		{
			GetWorldTimerManager().ClearTimer(ArriveTimer);
			UE_LOG(LogSibeliusGame, Warning, TEXT("[VirtualHome] %s's home: no player arrived to welcome."), *AgentName);
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(ArriveTimer);

	if (URefactorComponent* Refactor = Player->FindComponentByClass<URefactorComponent>())
	{
		Refactor->SetMaxWildTargetSize(WildRefactorMaxSize);
		Refactor->SetWildInstancesAllowed(true);
		Refactor->SetMinTreasureSize(MinTreasureSize);
	}

	if (bThirdPerson)
	{
		if (UBattleFormComponent* Battle = Player->FindComponentByClass<UBattleFormComponent>())
		{
			// The powers stay on: there is nothing here to fight, and E and R both aim from
			// level with his eyes now. See the header.
			Battle->bSuspendCameraPowers = false;
			if (!Battle->IsInBattleForm())
			{
				Battle->EnterBattleForm();
			}
		}
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[VirtualHome] arrived at %s's home: third person %s, %d power(s) lent, wild refactor up to %.0f cm, treasure at least %.0f cm."),
		*AgentName, bThirdPerson ? TEXT("on") : TEXT("off"), LentPowers.Num(), WildRefactorMaxSize, MinTreasureSize);
}
