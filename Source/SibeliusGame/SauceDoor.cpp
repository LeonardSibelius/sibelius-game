// SauceDoor.cpp — see header. A hidden door (Code Vision reveal, inherited) whose E
// stages + travels to a generated Elsewhere instead of a fixed level.

#include "SauceDoor.h"
#include "ElsewhereSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSauceDoor, Log, All);

ASauceDoor::ASauceDoor()
{
	// The base class wires the Code-Vision reveal (custom-depth outline + collision
	// flip) on its DoorMesh, but leaves the mesh unset (placed office hidden doors pick
	// theirs in-editor). Give this one a default slab so it's self-contained — the
	// shimmer reveals THIS mesh. DoorMesh is protected on AHiddenDoor.
	if (DoorMesh)
	{
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			DoorMesh->SetStaticMesh(Cube);
			DoorMesh->SetRelativeScale3D(FVector(0.2f, 1.2f, 2.2f));   // a doorway slab
		}
	}
}

void ASauceDoor::Interact_Implementation(AActor* /*Interactor*/)
{
	if (!IsRevealed())
	{
		return;   // unrevealed wall keeps its secret — hold Code Vision to reveal it
	}

	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	if (!Elsewhere)
	{
		UE_LOG(LogSauceDoor, Error, TEXT("[%s] no UElsewhereSubsystem — cannot stage an Elsewhere."), *GetName());
		return;
	}

	const FElsewherePlan Plan = Elsewhere->StageNextElsewhere();
	if (!Plan.IsValid())
	{
		UE_LOG(LogSauceDoor, Error, TEXT("[%s] staged an invalid Elsewhere — not travelling."), *GetName());
		return;
	}

	UE_LOG(LogSauceDoor, Display, TEXT("[%s] stepping through to %s (place=%s, curio=%s, seed=%d)."),
		*GetName(), *ElsewhereLevelName.ToString(), *Plan.PlaceTypeId.ToString(), *Plan.CurioId.ToString(), Plan.Seed);
	UGameplayStatics::OpenLevel(this, ElsewhereLevelName);
}

FText ASauceDoor::GetInteractionPrompt_Implementation() const
{
	return IsRevealed() ? StepThroughPrompt : FText::GetEmpty();
}
