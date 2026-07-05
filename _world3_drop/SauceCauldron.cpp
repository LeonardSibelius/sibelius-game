// SauceCauldron.cpp — P0 STUB (June 13, 2026). Source/SibeliusGame/ (RUNTIME module).

#include "SauceCauldron.h"
#include "Components/StaticMeshComponent.h"

ASauceCauldron::ASauceCauldron()
{
	PrimaryActorTick.bCanEverTick = false;   // pure state holder; no tick needed

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CauldronMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CauldronMesh"));
	CauldronMesh->SetupAttachment(SceneRoot);

	ContentsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContentsMesh"));
	ContentsMesh->SetupAttachment(SceneRoot);
	ContentsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ASauceCauldron::FeedSauce(float Delta)
{
	if (bComplete)
	{
		return false;   // idempotent once cooked (SS9)
	}

	BlendProgress = FMath::Clamp(BlendProgress + Delta, 0.f, 1.f);

	if (BlendProgress >= CompleteThreshold)
	{
		HandleComplete();
		return true;
	}
	return false;
}

void ASauceCauldron::HandleComplete()
{
	if (bComplete)
	{
		return;   // one-shot latch
	}
	bComplete = true;

	UE_LOG(LogTemp, Display, TEXT("[Sauce] The Sauce of All Knowledge is complete (BlendProgress=%.2f)."), BlendProgress);

	// P5 TODO: set USibeliusProgressSubsystem::bSauceComplete and TriggerApparition(clue voice) here.
	OnSauceComplete.Broadcast();
}

void ASauceCauldron::Interact_Implementation(AActor* /*Interactor*/)
{
	// P0: status read-out only. P3 will commit a held "true" book here.
	UE_LOG(LogTemp, Display, TEXT("[Sauce] Interact: BlendProgress=%.2f, Complete=%s"),
		BlendProgress, bComplete ? TEXT("true") : TEXT("false"));
}

FText ASauceCauldron::GetInteractionPrompt_Implementation() const
{
	return PromptText;
}
