// InteractorComponent.cpp

#include "InteractorComponent.h"

#include "Interactable.h"
#include "DancerAgentComponent.h"
#include "DancerAgentSubsystem.h"
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

	const FVector Forward = CameraComp->GetForwardVector();
	const FVector Start = CameraComp->GetComponentLocation();
	const FVector End = Start + Forward * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, GetOwner());
	Params.AddIgnoredActor(GetOwner());

	// Sphere sweep instead of a razor-thin line: a 71-year-old thumb should not
	// need sniper aim to play a slot machine. Radius 0 falls back to a line.
	FHitResult Hit;
	const bool bHit = (InteractRadius > 0.f)
		? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(InteractRadius), Params)
		: World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Two kinds of focus target. Actors implementing IInteractable are the general
	// case; the dancing girls are the exception — they are MetaHuman actors we do not
	// own and cannot re-parent, so they are recognised by carrying a dancer component
	// instead. See DancerAgentSubsystem.h for why that component is attached by a scan.
	AActor* Target = nullptr;
	FText Prompt;

	if (AActor* HitActor = bHit ? Hit.GetActor() : nullptr)
	{
		if (HitActor->Implements<UInteractable>())
		{
			Target = HitActor;
			Prompt = IInteractable::Execute_GetInteractionPrompt(HitActor);
		}
		else if (const UDancerAgentComponent* Agent = HitActor->FindComponentByClass<UDancerAgentComponent>())
		{
			Target = HitActor;
			Prompt = Agent->GetPrompt();
		}
	}

	// Nothing under the crosshair — try the dancer aim assist. A travelling dance leaves
	// her body outside her stationary capsule, so the trace above misses her even at
	// arm's length (Walt needed seven presses on Nyra). This asks a different question:
	// is a dancer near the middle of the screen and actually visible?
	if (!Target)
	{
		if (AActor* Dancer = FindDancerByAim(Start, Forward))
		{
			Target = Dancer;
			if (const UDancerAgentComponent* Agent = Dancer->FindComponentByClass<UDancerAgentComponent>())
			{
				Prompt = Agent->GetPrompt();
			}
		}
	}

	if (!Target)
	{
		return;
	}

	FocusedActor = Target;

	// Stable key so the prompt refreshes in place each tick instead of stacking.
	constexpr uint64 InteractPromptKey = 0xACE0F1;

	if (GEngine && !Prompt.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(InteractPromptKey, 0.15f, FColor::White, Prompt.ToString());
	}
}

AActor* UInteractorComponent::FindDancerByAim(const FVector& Start, const FVector& Forward) const
{
	UWorld* World = GetWorld();
	UDancerAgentSubsystem* Sub = World ? World->GetSubsystem<UDancerAgentSubsystem>() : nullptr;
	if (!Sub)
	{
		return nullptr;
	}

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(DancerAssistAngle));
	const float RangeSq = DancerAssistRange * DancerAssistRange;

	AActor* Best = nullptr;
	float BestDot = MinDot;   // anything accepted must beat the cone, then each other

	for (const TWeakObjectPtr<UDancerAgentComponent>& Weak : Sub->GetDancers())
	{
		const UDancerAgentComponent* Agent = Weak.Get();
		AActor* Owner = Agent ? Agent->GetOwner() : nullptr;
		if (!Owner)
		{
			continue;
		}

		// Her mesh bounds, so this tracks the pose rather than the actor origin.
		const FVector AimPoint = Agent->GetAimPoint();
		const FVector ToDancer = AimPoint - Start;

		if (ToDancer.SizeSquared() > RangeSq)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(Forward, ToDancer.GetSafeNormal());
		if (Dot <= BestDot)
		{
			continue;   // outside the cone, or a worse candidate than one we have
		}

		// Line of sight, so a dancer through a wall is not selectable. Ignore her own
		// actor: we are asking whether anything is IN THE WAY, not whether we hit her.
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(DancerSight), false, GetOwner());
		SightParams.AddIgnoredActor(GetOwner());
		SightParams.AddIgnoredActor(Owner);

		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, Start, AimPoint, ECC_Visibility, SightParams))
		{
			continue;
		}

		Best = Owner;
		BestDot = Dot;
	}

	return Best;
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
