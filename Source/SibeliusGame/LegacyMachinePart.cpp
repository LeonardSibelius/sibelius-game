// LegacyMachinePart.cpp — see header.

#include "LegacyMachinePart.h"

#include "LegacyMachine.h"
#include "RefactorableComponent.h"
#include "ProgressionSubsystem.h"
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
	// did not, R would sail through and hit whatever is behind the machine. The same
	// collision is what the interactor's sphere sweep now finds for E.
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

	/* THE FAULT LIGHT, ABOVE THE PLAQUE. Lit for one beat while a piece dies here.

	   It sits ABOVE the plaque on purpose: the plaque/true-name pair below is the puzzle
	   and has to stay a clean doubling, so the lamp is never allowed to land between them.

	   And it is NOT Code Vision gated. A machine that says where it broke is not a
	   spoiler, it is a log line -- every real system has one, and Mrs. Hall's ticket
	   ("it threw again overnight") is a woman reading exactly this. The power is still
	   needed for the part that matters: the lamp says GRADER, and nothing in the level
	   says why GRADER, because its plaque sounds perfectly reasonable until you hold V
	   and read what it really does. */
	FaultLamp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FaultLamp"));
	FaultLamp->SetupAttachment(Mesh);
	FaultLamp->SetRelativeLocation(FVector(-55.0f, 0.0f, 44.0f));
	FaultLamp->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	FaultLamp->SetWorldSize(9.0f);
	FaultLamp->SetHorizontalAlignment(EHTA_Center);
	FaultLamp->SetVerticalAlignment(EVRTA_TextCenter);
	FaultLamp->SetText(FText::FromString(TEXT("REJECTED HERE")));
	FaultLamp->SetTextRenderColor(FColor(255, 90, 60, 255));
	FaultLamp->SetVisibility(false);

	/* THE BACKING PLATES. Thin cube slabs rather than planes: a plane has one facing and
	   this file has already lost a day to a rotation convention (the plaques hung upside
	   down because Python's Rotator args are (roll, pitch, yaw)). A slab reads correctly
	   from any side and cannot be silently inside-out.

	   Sized and placed by the build script — see the header. */
	PlaquePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaquePlate"));
	PlaquePlate->SetupAttachment(Mesh);
	PlaquePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded())
	{
		PlaquePlate->SetStaticMesh(Cube.Object);
	}

	TruePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TruePlate"));
	TruePlate->SetupAttachment(Mesh);
	TruePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TruePlate->SetVisibility(false);
	if (Cube.Succeeded())
	{
		TruePlate->SetStaticMesh(Cube.Object);
	}

	LampPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampPlate"));

	LampPlate->SetupAttachment(Mesh);
	LampPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LampPlate->SetVisibility(false);
	if (Cube.Succeeded())
	{
		LampPlate->SetStaticMesh(Cube.Object);
	}

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
		// Colour is set by SyncLabelsToState -- a fault can arm mid-session.
		TrueLabel->SetVisibility(false);
	}
	if (FaultLamp)
	{
		FaultLamp->SetRelativeRotation(FacePlayer);
	}
	SetFaultLampVisible(false);

	FaultStream.Initialize(FaultSeed);

	if (Refactorable)
	{
		Refactorable->OnRefactorChanged.AddDynamic(this, &ALegacyMachinePart::HandleRefactorChanged);
	}
	SyncLabelsToState();
}

FString ALegacyMachinePart::GetPlaqueClaim() const
{
	/* EVERYTHING AFTER THE HEADING, newlines and all. This used to take only the LAST
	   line, which was fine while every claim was a single line -- and then the plates
	   arrived and the claims had to WRAP. "grade B or better passes" at a readable size
	   is 78cm wide bolted to a 44cm crate standing 75cm from its neighbour, so the five
	   plates merged into one black strip across the row and hid the travelling workpiece.
	   Two short lines instead of one long one halves the width and the plates separate.

	   Which means the claim is now multi-line, and anything COMPARING a claim to a true
	   name has to flatten both first -- see FlattenLabel. The pairing is still word for
	   word; only where the line breaks has changed. */
	FString Claim = PlaqueText;
	int32 Newline = INDEX_NONE;
	if (Claim.FindChar(TEXT('\n'), Newline))
	{
		Claim.RightChopInline(Newline + 1);
	}
	else
	{
		// A single-line plaque is all heading and claims nothing.
		return FString();
	}
	return Claim.TrimStartAndEnd();
}

FString ALegacyMachinePart::FlattenLabel(const FString& In)
{
	// Line breaks are layout, not meaning. Two labels that say the same words in the same
	// order agree, however they happen to be wrapped on their plates.
	FString Out = In;
	Out.ReplaceInline(TEXT("\r"), TEXT(" "));
	Out.ReplaceInline(TEXT("\n"), TEXT(" "));
	while (Out.ReplaceInline(TEXT("  "), TEXT(" ")) > 0)
	{
	}
	return Out.TrimStartAndEnd();
}

FString ALegacyMachinePart::GetPlaqueHeading() const
{
	// "GRADER\ngrade B or better passes" -> "GRADER". A single-line plaque is its own
	// heading, so an un-authored stage still names itself in the log and in the step
	// prompt rather than printing an empty string where a stage name should be.
	FString Heading = PlaqueText;
	int32 Newline = INDEX_NONE;
	if (Heading.FindChar(TEXT('\n'), Newline))
	{
		Heading.LeftInline(Newline);
	}
	Heading.TrimStartAndEndInline();
	return Heading.IsEmpty() ? GetName() : Heading;
}

void ALegacyMachinePart::SyncLabelsToState()
{
	if (Plaque)
	{
		Plaque->SetText(FText::FromString(PlaqueText));
	}
	if (TrueLabel)
	{
		/* THE FIX MADE VISIBLE — and the fault that has not been issued yet made INVISIBLE.

		   IsFaultCleared() covers both, which is the point of routing through it. A
		   refactored liar's source agrees with its docs, so holding V after R shows the
		   same sentence twice and the scan that found the fault confirms it is gone.
		   Revert (a discarded Test-Drive) puts the authored lie back.

		   And a part whose fault is not ARMED yet reads as honest — word for word, like
		   its four neighbours. That is what lets ticket 2's liar stand in the row from
		   the first minute without spoiling ticket 1's "find the ONE part that is lying".

		   Authored TrueName is never overwritten; only the label. */
		const bool bSourceAgrees = IsFaultCleared();
		TrueLabel->SetText(FText::FromString(bSourceAgrees ? GetPlaqueClaim() : TrueName));

		// The liar reads hot; the honest parts read cool. A hint, not the answer — the
		// player still has to notice WHAT it says. Set here rather than in BeginPlay
		// because a fault can arm mid-session, the moment the previous ticket closes.
		TrueLabel->SetTextRenderColor(bSourceAgrees ? FColor(120, 230, 255, 255)
		                                            : FColor(255, 90, 60, 255));
	}
}

void ALegacyMachinePart::HandleRefactorChanged(bool bIsRefactored)
{
	(void)bIsRefactored;
	SyncLabelsToState();
	// A part that has just been fixed must not be left wearing the lamp it lit on its
	// last bad cycle: R is the moment the player is looking straight at it.
	if (IsBehaving())
	{
		SetFaultLampVisible(false);
	}
}

bool ALegacyMachinePart::IsFaultArmed() const
{
	if (!bIsFaulty)
	{
		return false;
	}
	if (ArmedByGrant.IsNone())
	{
		return true;   // ticket 1's fault: live from the first minute
	}
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	return Progression && Progression->HasClaimedGrant(ArmedByGrant);
}

bool ALegacyMachinePart::IsFaultCleared() const
{
	if (!IsFaultArmed())
	{
		return true;
	}
	return Refactorable && Refactorable->IsRefactored();
}

void ALegacyMachinePart::RollForCycle()
{
	// Deterministic fault: no luck involved, it is wrong every time.
	// Intermittent: latch one verdict for the whole cycle. See the header for why this
	// cannot live inside IsBehaving().
	bMisbehavingThisCycle = IsFaultArmed() && FaultChance > 0.0f
		&& FaultStream.FRand() < FaultChance;
}

bool ALegacyMachinePart::WouldMisbehaveIfArmed(FRandomStream& Stream) const
{
	if (!bIsFaulty)
	{
		return false;
	}
	if (Refactorable && Refactorable->IsRefactored())
	{
		return false;
	}
	// A deterministic fault fails every trial; an intermittent one fails at its rate.
	return FaultChance <= 0.0f || Stream.FRand() < FaultChance;
}

bool ALegacyMachinePart::WouldMisbehaveOnTrial(FRandomStream& Stream) const
{
	if (!IsFaultArmed())
	{
		return false;
	}
	return WouldMisbehaveIfArmed(Stream);
}

bool ALegacyMachinePart::IsBehaving() const
{
	if (IsFaultCleared())
	{
		return true;
	}
	// An armed, un-refactored fault. Deterministic ones are always misbehaving;
	// intermittent ones only on the cycles they rolled badly.
	//
	// Asking the component rather than caching a bool here is what makes Test-Drive
	// correct: a discarded branch reverts bIsRefactored underneath us and the machine
	// simply reads false again on its next cycle, with nothing to keep in sync.
	return FaultChance > 0.0f && !bMisbehavingThisCycle;
}

void ALegacyMachinePart::SetTrueNameVisible(bool bVisible)
{
	// The label and its plate are one object as far as the rest of the game is concerned.
	if (TrueLabel)
	{
		TrueLabel->SetVisibility(bVisible);
	}
	if (TruePlate)
	{
		TruePlate->SetVisibility(bVisible);
	}
}

void ALegacyMachinePart::SetFaultLampVisible(bool bVisible)
{
	// The lamp and its plate are one object as far as the rest of the game is concerned.
	if (FaultLamp)
	{
		FaultLamp->SetVisibility(bVisible);
	}
	if (LampPlate)
	{
		LampPlate->SetVisibility(bVisible);
	}
}

void ALegacyMachinePart::Interact_Implementation(AActor* Interactor)
{
	// The transport belongs to the line, not the stage. Execute_ rather than the
	// virtual, per the contract in Interactable.h.
	if (OwningMachine)
	{
		IInteractable::Execute_Interact(OwningMachine, Interactor);
	}
}

FText ALegacyMachinePart::GetInteractionPrompt_Implementation() const
{
	if (OwningMachine)
	{
		return IInteractable::Execute_GetInteractionPrompt(OwningMachine);
	}
	// An unwired part says nothing rather than offering a control that does nothing.
	return FText::GetEmpty();
}

void ALegacyMachinePart::SetLampTextVisible(bool bVisible)
{
	// Deliberately does NOT touch LampPlate — see the header.
	if (FaultLamp)
	{
		FaultLamp->SetVisibility(bVisible);
	}
}
