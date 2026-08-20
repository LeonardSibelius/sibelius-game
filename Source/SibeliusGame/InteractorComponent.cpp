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
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SibeliusHUD.h"   // the prompt draws on the HUD canvas so it survives packaging

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
	// SweepMulti: a book on a table loses SweepSingle to the tabletop. Take the
	// first IInteractable at or just behind that first contact (not through a wall).
	TArray<FHitResult> Hits;
	if (InteractRadius > 0.f)
	{
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(InteractRadius), Params);
	}
	else
	{
		FHitResult LineHit;
		if (World->LineTraceSingleByChannel(LineHit, Start, End, ECC_Visibility, Params))
		{
			Hits.Add(LineHit);
		}
	}

	auto IsInteractTarget = [](AActor* A) -> bool
	{
		return A && (A->Implements<UInteractable>()
			|| A->FindComponentByClass<UDancerAgentComponent>() != nullptr);
	};

	AActor* Target = nullptr;
	FText Prompt;
	float FurnitureDist = -1.f;
	constexpr float OnSurfaceSlack = 50.f;   // cm past the table/shelf we just hit
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			continue;
		}
		if (IsInteractTarget(HitActor))
		{
			if (FurnitureDist < 0.f || Hit.Distance <= FurnitureDist + OnSurfaceSlack)
			{
				Target = HitActor;
			}
			break;
		}
		if (FurnitureDist < 0.f)
		{
			FurnitureDist = Hit.Distance;
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
	if (Prompt.IsEmpty())
	{
		if (Target->Implements<UInteractable>())
		{
			Prompt = IInteractable::Execute_GetInteractionPrompt(Target);
		}
		else if (const UDancerAgentComponent* Agent = Target->FindComponentByClass<UDancerAgentComponent>())
		{
			Prompt = Agent->GetPrompt();
		}
	}

	/* THE prompt. Every IInteractable in the game arrives here, which is why this one line
	   mattered more than the other seventeen put together.

	   It used to be AddOnScreenDebugMessage, which Shipping COMPILES OUT — so no player
	   who ever downloaded this game has seen an interaction prompt. Perfect in PIE, gone
	   in the build, nothing in any log. The HUD channel draws on the canvas and survives
	   packaging; it self-expires on a short lease, so this still needs no "stop showing"
	   path, exactly as the 0.15s debug message did not. */
	if (!Prompt.IsEmpty())
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
		{
			Hud->ShowInteractPrompt(Prompt.ToString());
		}
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
