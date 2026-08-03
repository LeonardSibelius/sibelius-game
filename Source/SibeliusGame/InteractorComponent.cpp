// InteractorComponent.cpp

#include "InteractorComponent.h"

#include "Interactable.h"
#include "DancerAgentComponent.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UInteractorComponent::UInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		CameraComp = Owner->FindComponentByClass<UCameraComponent>();
	}
}

void UInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

void UInteractorComponent::UpdateFocus()
{
	FocusedActor = nullptr;

	UWorld* World = GetWorld();
	if (!World || !CameraComp.IsValid())
	{
		return;
	}

	const FVector Start = CameraComp->GetComponentLocation();
	const FVector End = Start + CameraComp->GetForwardVector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, GetOwner());
	Params.AddIgnoredActor(GetOwner());

	// Sphere sweep instead of a razor-thin line: a 71-year-old thumb should not
	// need sniper aim to play a slot machine. Radius 0 falls back to a line.
	FHitResult Hit;
	const bool bHit = (InteractRadius > 0.f)
		? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(InteractRadius), Params)
		: World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return;
	}

	// Two kinds of focus target. Actors implementing IInteractable are the general
	// case; the dancing girls are the exception — they are MetaHuman actors we do not
	// own and cannot re-parent, so they are recognised by carrying a dancer component
	// instead. See DancerAgentSubsystem.h for why that component is attached by a scan.
	FText Prompt;
	if (HitActor->Implements<UInteractable>())
	{
		Prompt = IInteractable::Execute_GetInteractionPrompt(HitActor);
	}
	else if (const UDancerAgentComponent* Agent = HitActor->FindComponentByClass<UDancerAgentComponent>())
	{
		Prompt = Agent->GetPrompt();
	}
	else
	{
		return;
	}

	FocusedActor = HitActor;

	// Stable key so the prompt refreshes in place each tick instead of stacking.
	constexpr uint64 InteractPromptKey = 0xACE0F1;

	if (GEngine && !Prompt.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(InteractPromptKey, 0.15f, FColor::White, Prompt.ToString());
	}
}

void UInteractorComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return;
	}

	if (Target->Implements<UInteractable>())
	{
		IInteractable::Execute_Interact(Target, GetOwner());
	}
	else if (UDancerAgentComponent* Agent = Target->FindComponentByClass<UDancerAgentComponent>())
	{
		Agent->Greet();
	}
}
