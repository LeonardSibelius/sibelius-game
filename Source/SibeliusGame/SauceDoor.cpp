// SauceDoor.cpp — see header. Plan B: the door shuffles a deck of baked forest
// levels. ASauceDoor customises the cosmetics (slab mesh, sign, prompt) and the
// deck pick; AHiddenDoor's inherited Interact does the actual travel.

#include "SauceDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	// Which deck entry the LAST walk-through used. File-scope static: survives the
	// office level reloading between visits (actors are recreated, this is not),
	// resets only when the game restarts — exactly the "not the same world twice
	// in a row" feel we want, with zero save-game machinery.
	int32 GLastDeckPick = INDEX_NONE;
}

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

	// "Many Worlds" sign placement Walt dialed in by hand — baked as ASauceDoor defaults
	// so a placement-script re-run can't reset it (the office/attic signs keep AHiddenDoor's
	// own defaults). FRotator(Pitch, Yaw, Roll): Details X(roll)=90, Z(yaw)=90 (a wide
	// plaque rotated flat onto this kitchen-facing door).
	SignRelativeLocation = FVector(-90.0f, 0.0f, 10.0f);
	SignRelativeRotation = FRotator(0.0f, 90.0f, 90.0f);
	SignWidth = 450.0f;
	SignHeight = 100.0f;

	// The Many-Worlds travel prompt. AHiddenDoor's inherited Interact/GetInteractionPrompt
	// drive the travel; this just swaps the parent's default prompt text.
	TravelPromptText = NSLOCTEXT("Sibelius", "SauceDoorPrompt", "Step through [E]");
}

void ASauceDoor::Interact_Implementation(AActor* Interactor)
{
	// Shuffle the deck (if there is one): random pick, never the same twice in a row.
	if (TravelTargetLevels.Num() > 0)
	{
		int32 Pick = FMath::RandRange(0, TravelTargetLevels.Num() - 1);
		if (TravelTargetLevels.Num() > 1 && Pick == GLastDeckPick)
		{
			Pick = (Pick + 1) % TravelTargetLevels.Num();
		}
		GLastDeckPick = Pick;
		TravelTargetLevel = TravelTargetLevels[Pick];
		UE_LOG(LogTemp, Display, TEXT("[SauceDoor] deck pick %d of %d -> %s"),
			Pick + 1, TravelTargetLevels.Num(), *TravelTargetLevel.ToString());
	}

	// Parent does the real work: reveal check, branch gate, travel cover, OpenLevel.
	Super::Interact_Implementation(Interactor);
}
