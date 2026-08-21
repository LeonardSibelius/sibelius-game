// LegacyMachinePart.cpp — see header.

#include "LegacyMachinePart.h"

#include "RefactorableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ALegacyMachinePart::ALegacyMachinePart()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// BlockAll so the Refactor line trace (ECC_Visibility) lands on this part -- if it
	// did not, R would sail through and hit whatever is behind the machine.
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}

	// THE DOCS: small, engraved, always readable. Faces -X, which is the side the office
	// approach comes from.
	Plaque = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Plaque"));
	Plaque->SetupAttachment(Mesh);
	Plaque->SetRelativeLocation(FVector(-55.0f, 0.0f, 20.0f));
	Plaque->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Plaque->SetWorldSize(9.0f);
	Plaque->SetHorizontalAlignment(EHTA_Center);
	Plaque->SetVerticalAlignment(EVRTA_TextCenter);
	Plaque->SetTextRenderColor(FColor(180, 176, 165, 255));   // engraved grey

	/* THE SOURCE: ON THE BOX FACE, DIRECTLY UNDER THE PLAQUE. Off until Code Vision.

	   This label has now been wrong twice, and both times for the same reason. First it
	   floated 60cm above the part; then 78cm and smaller. Walt held V, looked straight at
	   a working machine and asked "what am I looking at here?" -- because a line hanging
	   in mid-air reads as graffiti over the room, not as THAT BOX's truth, and pairing it
	   back to its own part is mental work.

	   The pairing IS the puzzle. It has to be effortless, so the two lines share one
	   surface: the plaque above, what it really does immediately below, same size, same
	   alignment, same face. Hold V and each box shows "what I claim" over "what I do".
	   Four boxes say the same sentence twice, which reads instantly as a doubling. GRADER
	   shows two different sentences in the same place, and there is nothing to
	   cross-reference.

	   Colour is NOT the mechanism, only a garnish: Code Vision floods the screen green
	   and flattens the red/cyan distinction to almost nothing. The disagreement carries
	   itself. */
	TrueLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TrueLabel"));
	TrueLabel->SetupAttachment(Mesh);
	TrueLabel->SetRelativeLocation(FVector(-57.0f, 0.0f, -8.0f));
	TrueLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TrueLabel->SetWorldSize(9.0f);
	TrueLabel->SetHorizontalAlignment(EHTA_Center);
	TrueLabel->SetVerticalAlignment(EVRTA_TextCenter);
	TrueLabel->SetVisibility(false);

	// The fix. What matters is bIsRefactored -- that is what IsBehaving() and the whole
	// branch/deploy chain read.
	//
	// EditType and RefactoredScale are PROTECTED on URefactorableComponent and are set
	// per-part by Tools/Scripts/build_legacy_machine.py instead. Widening that class's
	// access so this constructor could reach them would be changing a shipped, gated
	// system to suit a one-day experiment; they are UPROPERTY(EditAnywhere), so the
	// placement script sets them the same way a human would in the Details panel.
	Refactorable = CreateDefaultSubobject<URefactorableComponent>(TEXT("Refactorable"));
}

void ALegacyMachinePart::BeginPlay()
{
	Super::BeginPlay();

	/* Face the office approach (-X). The placement script once copied C++'s
	   FRotator(0, 180, 0) into Python as Rotator(0, 180, 0). Python's positional
	   args are (roll, pitch, yaw), so that wrote pitch 180 and the plaques hung
	   upside down. Force yaw 180 here so a bad instance value cannot restamp it. */
	static const FRotator FacePlayer(0.0f, 180.0f, 0.0f);
	if (Plaque)
	{
		Plaque->SetRelativeRotation(FacePlayer);
	}
	if (TrueLabel)
	{
		TrueLabel->SetRelativeRotation(FacePlayer);
		// The liar reads hot; the honest parts read cool. This is a hint, not the
		// answer -- the player still has to notice WHAT it says. Colour is garnish:
		// Code Vision floods the screen green, so the disagreement has to carry itself.
		TrueLabel->SetTextRenderColor(bIsFaulty ? FColor(255, 90, 60, 255)
		                                        : FColor(120, 230, 255, 255));
		TrueLabel->SetVisibility(false);
	}

	if (Refactorable)
	{
		Refactorable->OnRefactorChanged.AddDynamic(this, &ALegacyMachinePart::HandleRefactorChanged);
	}
	SyncLabelsToState();
}

FString ALegacyMachinePart::GetPlaqueClaim() const
{
	FString Claim = PlaqueText;
	int32 Newline = INDEX_NONE;
	if (Claim.FindLastChar(TEXT('\n'), Newline))
	{
		Claim.RightChopInline(Newline + 1);
	}
	return Claim.TrimStartAndEnd();
}

void ALegacyMachinePart::SyncLabelsToState()
{
	if (Plaque)
	{
		Plaque->SetText(FText::FromString(PlaqueText));
	}
	if (TrueLabel)
	{
		// THE FIX MADE VISIBLE. A refactored liar's source agrees with its docs, so
		// holding V after R shows the same sentence twice — the same scan that found
		// the fault now confirms it is gone. Revert (a discarded Test-Drive) puts the
		// authored lie back. Authored TrueName is never overwritten; only the label.
		const bool bSourceAgrees = Refactorable && Refactorable->IsRefactored();
		TrueLabel->SetText(FText::FromString(bSourceAgrees ? GetPlaqueClaim() : TrueName));
	}
}

void ALegacyMachinePart::HandleRefactorChanged(bool bIsRefactored)
{
	(void)bIsRefactored;
	SyncLabelsToState();
}

bool ALegacyMachinePart::IsBehaving() const
{
	if (!bIsFaulty)
	{
		return true;
	}
	// A faulty part behaves once it has been refactored -- and asking the component
	// rather than caching a bool here is what makes Test-Drive correct: a discarded
	// branch reverts bIsRefactored underneath us and the machine simply reads false again
	// on its next cycle, with nothing to keep in sync.
	return Refactorable && Refactorable->IsRefactored();
}

void ALegacyMachinePart::SetTrueNameVisible(bool bVisible)
{
	if (TrueLabel)
	{
		TrueLabel->SetVisibility(bVisible);
	}
}
