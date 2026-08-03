// DancerAgentSubsystem.cpp — see header.

#include "DancerAgentSubsystem.h"

#include "DancerAgentComponent.h"
#include "SibeliusGame.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

void UDancerAgentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const int32 Found = ScanForDancers();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] first scan found %d dancer(s)"), Found);

	// The cathedral dancer is spawned by AFinaleAltar when the player completes the
	// Synthesis, long after BeginPlay. Rather than couple the altar to this subsystem,
	// re-scan on a slow repeating timer — it is a handful of actor iterations every few
	// seconds and it catches any dancer that arrives later, however she arrives.
	FTimerHandle Handle;
	InWorld.GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const int32 Added = ScanForDancers();
			if (Added > 0)
			{
				UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] late scan adopted %d dancer(s)"), Added);
			}
		}),
		5.0f,
		/*bLoop=*/true);
}

int32 UDancerAgentSubsystem::ScanForDancers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Added = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->FindComponentByClass<UDancerAgentComponent>())
		{
			continue;   // already an agent (auto-adopted earlier, or added by hand)
		}

		// Is this actor dancing? Any skeletal mesh playing one of the ten dances.
		bool bIsDancing = false;
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents<USkeletalMeshComponent>(Meshes);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && UDancerAgentComponent::IsKnownDance(Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay)))
			{
				bIsDancing = true;
				break;
			}
		}

		if (!bIsDancing)
		{
			continue;
		}

		UDancerAgentComponent* Agent = NewObject<UDancerAgentComponent>(Actor);
		if (!Agent)
		{
			continue;
		}

		// RegisterComponent already calls BeginPlay when the owning actor has begun
		// play, which is every case here. Only drive it manually if it did not — calling
		// BeginPlay twice on a component is a genuine bug, not a harmless repeat.
		Agent->RegisterComponent();
		if (!Agent->HasBegunPlay())
		{
			Agent->BeginPlay();
		}

		++Added;
		UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] adopted '%s' as AI Agent %s"),
			*Actor->GetActorNameOrLabel(), *Agent->AgentName);
	}

	return Added;
}
