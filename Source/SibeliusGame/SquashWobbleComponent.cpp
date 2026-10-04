// SquashWobbleComponent.cpp — see header.

#include "SquashWobbleComponent.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

USquashWobbleComponent::USquashWobbleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USquashWobbleComponent* USquashWobbleComponent::Find(AActor* Owner, const UInstancedStaticMeshComponent* InField, int32 InIndex)
{
	TArray<USquashWobbleComponent*> Wobblers;
	Owner->GetComponents<USquashWobbleComponent>(Wobblers);
	for (USquashWobbleComponent* W : Wobblers)
	{
		if (W && W->Field == InField && (!InField || W->Index == InIndex))
		{
			return W;
		}
	}
	return nullptr;
}

void USquashWobbleComponent::Wobble(AActor* Target, float Strength)
{
	USceneComponent* Root = Target ? Target->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	if (USquashWobbleComponent* Already = Find(Target, nullptr, INDEX_NONE))
	{
		Already->Start(Strength);
		return;
	}
	// Placed scenery is static; a static root cannot be rescaled at runtime.
	if (Root->Mobility != EComponentMobility::Movable)
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
	USquashWobbleComponent* Wobbler = NewObject<USquashWobbleComponent>(Target);
	Wobbler->Rest = Root->GetRelativeTransform();
	Wobbler->RegisterComponent();
	Wobbler->Start(Strength);
}

void USquashWobbleComponent::WobbleInstance(UInstancedStaticMeshComponent* InField, int32 InIndex, float Strength)
{
	AActor* Owner = InField ? InField->GetOwner() : nullptr;
	FTransform InstanceRest;
	if (!Owner || !InField->GetInstanceTransform(InIndex, InstanceRest, /*bWorldSpace=*/true))
	{
		return;
	}
	if (USquashWobbleComponent* Already = Find(Owner, InField, InIndex))
	{
		Already->Start(Strength);
		return;
	}
	USquashWobbleComponent* Wobbler = NewObject<USquashWobbleComponent>(Owner);
	Wobbler->Field = InField;
	Wobbler->Index = InIndex;
	Wobbler->Rest = InstanceRest;
	Wobbler->RegisterComponent();
	Wobbler->Start(Strength);
}

void USquashWobbleComponent::Start(float InStrength)
{
	Strength = InStrength;
	Elapsed = 0.0f;
	SetComponentTickEnabled(true);
}

void USquashWobbleComponent::Apply(const FVector& Squash)
{
	FTransform Now = Rest;
	Now.SetScale3D(Rest.GetScale3D() * Squash);
	if (Field)
	{
		Field->UpdateInstanceTransform(Index, Now, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	}
	else if (USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr)
	{
		Root->SetRelativeScale3D(Now.GetScale3D());
	}
}

void USquashWobbleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Elapsed += DeltaTime;
	if (Elapsed >= Duration)
	{
		Finish();
		return;
	}
	// Starts by squashing (the blow lands from above-ish), then rings: stretch, squash, ...
	const float Ring = -Strength * FMath::Exp(-Damping * Elapsed) * FMath::Sin(UE_TWO_PI * Frequency * Elapsed);
	const float Up = 1.0f + Ring;
	const float Across = 1.0f / FMath::Sqrt(FMath::Max(Up, 0.1f));   // keep the volume
	Apply(FVector(Across, Across, Up));
}

void USquashWobbleComponent::Finish()
{
	Apply(FVector::OneVector);
	DestroyComponent();
}
