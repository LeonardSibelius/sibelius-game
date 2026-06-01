// InteractorComponent.cpp

#include "InteractorComponent.h"

#include "Interactable.h"
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

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor || !HitActor->Implements<UInteractable>())
	{
		return;
	}

	FocusedActor = HitActor;

	// Stable key so the prompt refreshes in place each tick instead of stacking.
	constexpr uint64 InteractPromptKey = 0xACE0F1;

	const FText Prompt = IInteractable::Execute_GetInteractionPrompt(HitActor);
	if (GEngine && !Prompt.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(InteractPromptKey, 0.15f, FColor::White, Prompt.ToString());
	}
}

void UInteractorComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	if (Target && Target->Implements<UInteractable>())
	{
		IInteractable::Execute_Interact(Target, GetOwner());
	}
}
