// SauceDoor.cpp — see header. A hidden door (Code Vision reveal, inherited) that travels
// to its TravelTargetLevel on reveal + E — a plain travel door, exactly like the office
// obelisk / its AHiddenDoor parent. The curio / cabinet / AElsewhereBuilder /
// UElsewhereSubsystem "roll a fresh Elsewhere" flow is set aside; this door no longer
// touches it. ASauceDoor only customises the cosmetics (slab mesh, sign, prompt).

#include "SauceDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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
	// now drive the travel via TravelTargetLevel (set on the placed door to L_Poplar_Forest);
	// this just swaps the parent's "Enter the Stacks [E]" default for the kitchen door's text.
	TravelPromptText = NSLOCTEXT("Sibelius", "SauceDoorPrompt", "Step through [E]");
}
