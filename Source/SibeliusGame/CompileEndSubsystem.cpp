// CompileEndSubsystem.cpp

#include "CompileEndSubsystem.h"

#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // FUN-2: on-screen reward line
#include "ProgressionSubsystem.h"   // FUN-2: chapter completion pays Sauce
#include "SibeliusGame.h"           // LogSibeliusGame

const FName UCompileEndSubsystem::EndTriggerTag = TEXT("CompileEndTrigger");

bool UCompileEndSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Gameplay worlds only - never the editor preview or the smoke-test commandlet world.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCompileEndSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Level actors exist by world BeginPlay; bind to every tagged trigger so the
	// designer can place the volume without wiring anything up (Ch1/R7: no race).
	int32 BoundCount = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(EndTriggerTag))
		{
			Actor->OnActorBeginOverlap.AddDynamic(this, &UCompileEndSubsystem::HandleEndTriggerOverlap);
			++BoundCount;
		}
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[CompileEnd] Bound to %d '%s' trigger(s)."),
		BoundCount, *EndTriggerTag.ToString());
}

void UCompileEndSubsystem::HandleEndTriggerOverlap(AActor* /*OverlappedActor*/, AActor* OtherActor)
{
	if (bFired)
	{
		return; // fire exactly once (idempotent, mirrors UHallAlarmSubsystem)
	}

	// Only the local player pawn completes the chapter - ignore Refusers, props, etc.
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!OtherActor || OtherActor != PlayerPawn)
	{
		return;
	}

	bFired = true;
	UE_LOG(LogSibeliusGame, Display, TEXT("[CompileEnd] Ch3 complete"));

	// FUN-2: a chapter completion is a big one-time Sauce payday. Claimed through
	// the progression save, so replaying the level can't farm it.
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		constexpr int32 ChapterReward = 50;
		if (Progression->ClaimOneTimeGrant(TEXT("Chapter.Compile.End")))
		{
			Progression->GrantSauce(ChapterReward);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Emerald,
					FString::Printf(TEXT("CHAPTER COMPLETE  +%d SAUCE  (total %d)"),
						ChapterReward, Progression->GetSauce()));
			}
		}
	}

	OnCompileChapterComplete.Broadcast();
}
