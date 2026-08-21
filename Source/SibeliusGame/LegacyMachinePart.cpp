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

	// THE SOURCE: bigger and hotter, and off until Code Vision is held.
	TrueLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TrueLabel"));
	TrueLabel->SetupAttachment(Mesh);
	TrueLabel->SetRelativeLocation(FVector(-70.0f, 0.0f, 60.0f));
	TrueLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TrueLabel->SetWorldSize(13.0f);
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

	if (Plaque)
	{
		Plaque->SetText(FText::FromString(PlaqueText));
	}
	if (TrueLabel)
	{
		TrueLabel->SetText(FText::FromString(TrueName));
		// The liar reads hot; the honest parts read cool. This is a hint, not the
		// answer -- the player still has to notice WHAT it says.
		TrueLabel->SetTextRenderColor(bIsFaulty ? FColor(255, 90, 60, 255)
		                                        : FColor(120, 230, 255, 255));
		TrueLabel->SetVisibility(false);
	}
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
