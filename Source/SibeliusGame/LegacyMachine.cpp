// LegacyMachine.cpp — see header.

#include "LegacyMachine.h"

#include "LegacyMachinePart.h"
#include "CodeVisionComponent.h"
#include "RefactorableComponent.h"
#include "ProgressionSubsystem.h"
#include "MrsHallSubsystem.h"
#include "BranchSubsystem.h"
#include "SibeliusGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

const FName ALegacyMachine::ClosedTicketGrant(TEXT("Ticket.Legacy.Closed"));
const FName ALegacyMachine::IntermittentTicketGrant(TEXT("Ticket.Legacy.Intermittent.Closed"));

namespace
{
	/** The housing's two moods, shared by the tally and the run log so a producing
	 *  machine goes green everywhere at once. */
	const FColor MachineGood(120, 235, 140, 255);
	const FColor MachineBad(255, 120, 90, 255);

	/** The sign. Brass for the official plate, marker pen for the card taped over it.
	 *  Both deliberately dull: this is the one thing in the room that must NOT glow,
	 *  or it competes with the fault lamps the player is supposed to be reading. */
	const FColor SignBrass(198, 172, 112, 255);
	const FColor SignMarker(26, 26, 30, 255);

	/** The hour Mrs. Hall's ticket names. The run log counts minutes from here. */
	constexpr int32 ThrowHour = 3;
}

ALegacyMachine::ALegacyMachine()
{
	PrimaryActorTick.bCanEverTick = true;

	// See the header: a bare root, so the bed's stretch does not reach its siblings.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Bed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bed"));
	Bed->SetupAttachment(Root);
	Bed->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	if (CubeMesh)
	{
		Bed->SetStaticMesh(CubeMesh);
	}
	Bed->SetRelativeScale3D(FVector(4.4f, 0.5f, 0.12f));

	Workpiece = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Workpiece"));
	Workpiece->SetupAttachment(Root);
	Workpiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		Workpiece->SetStaticMesh(CubeMesh);
	}

	AcceptBin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AcceptBin"));
	AcceptBin->SetupAttachment(Root);
	AcceptBin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		AcceptBin->SetStaticMesh(CubeMesh);
	}

	RejectBin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RejectBin"));
	RejectBin->SetupAttachment(Root);
	RejectBin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		RejectBin->SetStaticMesh(CubeMesh);
	}

	// ---- the verdict, as furniture (see header) ---------------------------------
	// Meshes and transforms are filled in by DressTheVerdict(), which copies them off
	// the Workpiece; all the constructor owes these is existence and a parent.
	AcceptDust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AcceptDust"));
	AcceptDust->SetupAttachment(Root);
	AcceptDust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		AcceptDust->SetStaticMesh(CubeMesh);
	}

	RejectSpill.Reserve(MaxRejectSpill);
	for (int32 i = 0; i < MaxRejectSpill; ++i)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(
			FName(*FString::Printf(TEXT("RejectSpill%02d"), i)));
		Piece->SetupAttachment(Root);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RejectSpill.Add(Piece);
	}

	/* ---- the sign ----------------------------------------------------------------
	   The machine runs along Y and the player walks the -X side, which is why every
	   label in this file is yawed 180. So the sign is a slab that is THIN IN X: 2cm
	   thick, 170cm along the line, 26cm tall, hung above the parts where a gantry sign
	   goes.

	   The card and its text share ONE rotator on purpose. Tilt them separately and the
	   yaw of 180 flips the roll axis on the text only, so the card leans one way and
	   the writing leans the other. Same FRotator for both, and they lean together. */
	/* THE CARD IS TAPED OVER THE TOP, NOT ACROSS THE MIDDLE.

	   First attempt hid the official name behind the card and relied on the name being
	   WIDER than the card so its two ends would show past the edges. That needs the
	   text width to be known, and it is not: width is chars * WorldSize * GLYPH_W, and
	   build_legacy_machine.py names GLYPH_W = 0.60 while saying in its own comment that
	   it is an estimate, deliberately exposed because Unreal's GetTextLocalSize has an
	   axis convention the script would have to guess at. The real font is narrower, the
	   official name came out under the card's 196cm, and the card ate all of it. Walt
	   got a sign with nothing to deface.

	   So the two are separated in Z instead, where the only number involved is one I
	   set. The plate is tall; the official name sits on the bottom strip; the card
	   covers the top. The name is visible because nothing is in front of it, not
	   because of an arithmetic race it might lose against a font.

	     plate   250 x 46, centred on the anchor        (bottom edge at -23)
	     name    on the bottom strip at -15, size 8     (nothing above it but plate)
	     card    196 x 26, at +8                        (spans -5 .. +21)
	     title   on the card at +8, size 12

	   Widths still need to not overflow -- 24 chars at 12 is ~173cm on a 196 card, 44
	   at 8 is ~211cm on a 250 plate, both with room to spare even if GLYPH_W is off by
	   a fifth in either direction. That is the difference between using the estimate
	   for headroom and depending on it for the joke. */
	const FVector SignAnchor(-45.0f, 0.0f, 158.0f);
	const FRotator SignFacing(0.0f, 180.0f, 0.0f);
	const FRotator CardFacing(0.0f, 180.0f, 4.0f);   // taped on crooked

	SignPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignPlate"));
	SignPlate->SetupAttachment(Root);
	SignPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		SignPlate->SetStaticMesh(CubeMesh);
	}
	SignPlate->SetRelativeLocation(SignAnchor);
	SignPlate->SetRelativeScale3D(FVector(0.02f, 2.50f, 0.46f));

	SignOfficialText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignOfficialText"));
	SignOfficialText->SetupAttachment(Root);
	SignOfficialText->SetRelativeLocation(SignAnchor + FVector(-1.6f, 0.0f, -15.0f));
	SignOfficialText->SetRelativeRotation(SignFacing);
	SignOfficialText->SetWorldSize(8.0f);
	SignOfficialText->SetHorizontalAlignment(EHTA_Center);
	SignOfficialText->SetVerticalAlignment(EVRTA_TextCenter);
	SignOfficialText->SetTextRenderColor(SignBrass);

	// SHORTER THAN THE PLATE and pushed off centre, so the real name survives at both
	// ends. A card that covers the plate completely is just a different sign.
	SignCard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignCard"));
	SignCard->SetupAttachment(Root);
	SignCard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		SignCard->SetStaticMesh(CubeMesh);
	}
	SignCard->SetRelativeLocation(SignAnchor + FVector(-3.0f, 4.0f, 8.0f));
	SignCard->SetRelativeRotation(CardFacing);
	SignCard->SetRelativeScale3D(FVector(0.01f, 1.96f, 0.26f));

	SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
	SignText->SetupAttachment(Root);
	SignText->SetRelativeLocation(SignAnchor + FVector(-4.4f, 4.0f, 8.0f));
	SignText->SetRelativeRotation(CardFacing);
	SignText->SetWorldSize(12.0f);
	SignText->SetHorizontalAlignment(EHTA_Center);
	SignText->SetVerticalAlignment(EVRTA_TextCenter);
	SignText->SetTextRenderColor(SignMarker);

	// THE EVIDENCE, at reading height. Offsets here are real centimetres now that these
	// hang off a bare root instead of the stretched bed.
	Tally = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Tally"));
	Tally->SetupAttachment(Root);
	// SMALL, AND ON THE MACHINE. At 18cm and 70cm above the boxes this rendered three
	// metres tall across the middle of the room, shouting over the very lines the player
	// is meant to be reading. It is a gauge on a housing, not a billboard.
	Tally->SetRelativeLocation(FVector(-75.0f, -190.0f, 45.0f));
	Tally->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Tally->SetWorldSize(10.0f);
	Tally->SetHorizontalAlignment(EHTA_Center);
	Tally->SetVerticalAlignment(EVRTA_TextCenter);
	Tally->SetTextRenderColor(MachineBad);

	/* THE RUN LOG, STACKED ON TOP OF THE TALLY. Same anchor, higher up, so the two read
	   as one instrument cluster bolted to the head of the machine — the alternative was
	   a second caption somewhere else in the room, and this file has already learned
	   twice (see LegacyMachinePart.cpp) that text floating away from the thing it
	   describes reads as graffiti.

	   LEFT ALIGNED, unlike everything else here, because it is a table: the timestamps
	   have to form a column or the eye cannot scan down them for a pattern. */
	RunLog = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RunLog"));
	RunLog->SetupAttachment(Root);
	RunLog->SetRelativeLocation(FVector(-75.0f, -150.0f, 95.0f));
	RunLog->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	RunLog->SetWorldSize(7.0f);
	RunLog->SetHorizontalAlignment(EHTA_Left);
	RunLog->SetVerticalAlignment(EVRTA_TextCenter);
	RunLog->SetTextRenderColor(MachineBad);

	// The plates. Thin cube slabs, no collision, geometry set by the build script.
	TallyPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TallyPlate"));
	TallyPlate->SetupAttachment(Root);
	TallyPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		TallyPlate->SetStaticMesh(CubeMesh);
	}

	RunLogPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RunLogPlate"));
	RunLogPlate->SetupAttachment(Root);
	RunLogPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh)
	{
		RunLogPlate->SetStaticMesh(CubeMesh);
	}

	AcceptLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AcceptLabel"));
	AcceptLabel->SetupAttachment(Root);
	AcceptLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	AcceptLabel->SetWorldSize(13.0f);
	AcceptLabel->SetHorizontalAlignment(EHTA_Center);
	AcceptLabel->SetVerticalAlignment(EVRTA_TextCenter);
	AcceptLabel->SetText(FText::FromString(TEXT("ACCEPT")));
	AcceptLabel->SetTextRenderColor(MachineGood);

	RejectLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RejectLabel"));
	RejectLabel->SetupAttachment(Root);
	RejectLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	RejectLabel->SetWorldSize(13.0f);
	RejectLabel->SetHorizontalAlignment(EHTA_Center);
	RejectLabel->SetVerticalAlignment(EVRTA_TextCenter);
	RejectLabel->SetText(FText::FromString(TEXT("REJECT")));
	RejectLabel->SetTextRenderColor(MachineBad);
}

void ALegacyMachine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DressTheVerdict();
}

void ALegacyMachine::DressTheVerdict()
{
	/* THE SPILL IS THE WORKPIECE. Read the mesh and the scale off the component that
	   already carries them instead of naming an asset here: the heap is then literally
	   the thing this machine rejects, it needs no entry in build_legacy_machine.py, and
	   it keeps matching if WORKPIECE_MESH is ever swapped. */
	UStaticMesh* PieceMesh = Workpiece ? Workpiece->GetStaticMesh() : nullptr;
	const FVector PieceScale = Workpiece ? Workpiece->GetRelativeScale3D() : FVector::OneVector;

	/* Offsets are relative to the REJECT bin, so the heap follows if the build script
	   moves it. HAND PLACED, not seeded: a random scatter reads as noise, and twelve
	   boxes is few enough that where each one lies is a decision. The crate is 55 wide,
	   its rim is +14 from centre, and the carpet is -9. */
	static const FVector SpillOffset[MaxRejectSpill] = {
		FVector( -6.0f,   4.0f,  23.0f),   // heaped on the crate
		FVector(  8.0f,  -7.0f,  23.0f),
		FVector(  0.0f,   9.0f,  40.0f),
		FVector(-24.0f,  18.0f,  14.0f),   // going over the rim
		FVector( 26.0f, -14.0f,  12.0f),
		FVector(-41.0f,  34.0f,  -9.0f),   // on the carpet
		FVector(-14.0f,  47.0f,  -9.0f),
		FVector( 23.0f,  42.0f,  -9.0f),
		FVector(-47.0f, -21.0f,  -9.0f),
		FVector( 11.0f, -45.0f,  -9.0f),
		FVector( 45.0f,  23.0f,  -9.0f),
		FVector(-31.0f, -47.0f,  -9.0f),
	};
	static const FRotator SpillRot[MaxRejectSpill] = {
		FRotator( 0.0f,  15.0f,   6.0f),
		FRotator( 0.0f, -40.0f,  -4.0f),
		FRotator( 8.0f,  70.0f,  12.0f),
		FRotator( 0.0f,  30.0f,  34.0f),   // tipped, caught going over the rim
		FRotator( 0.0f, -20.0f, -28.0f),
		FRotator( 0.0f,  12.0f,   0.0f),
		FRotator( 0.0f, -55.0f,   0.0f),
		FRotator( 0.0f,  78.0f,   0.0f),
		FRotator( 0.0f,  34.0f,   0.0f),
		FRotator( 0.0f, -12.0f,   0.0f),
		FRotator( 0.0f,  51.0f,   0.0f),
		FRotator( 0.0f, -70.0f,   0.0f),
	};

	const FVector BinAt = RejectBin ? RejectBin->GetRelativeLocation() : FVector::ZeroVector;
	const int32 Shown = FMath::Clamp(RejectSpillCount, 0, MaxRejectSpill);

	for (int32 i = 0; i < RejectSpill.Num() && i < MaxRejectSpill; ++i)
	{
		UStaticMeshComponent* Piece = RejectSpill[i];
		if (!Piece)
		{
			continue;
		}
		// No workpiece mesh means the build script has not run yet: hide, rather than
		// litter the carpet with engine cubes.
		const bool bShow = (i < Shown) && (PieceMesh != nullptr);
		Piece->SetVisibility(bShow);
		Piece->SetHiddenInGame(!bShow);
		if (!bShow)
		{
			continue;
		}
		Piece->SetStaticMesh(PieceMesh);
		Piece->SetRelativeScale3D(PieceScale);
		Piece->SetRelativeLocation(BinAt + SpillOffset[i]);
		Piece->SetRelativeRotation(SpillRot[i]);
	}

	// The grime in ACCEPT, just inside the open crate. The emptiness around it is what
	// actually reads; this only denies it the look of a bin that is ready for work.
	if (AcceptDust && AcceptBin)
	{
		AcceptDust->SetRelativeLocation(AcceptBin->GetRelativeLocation() + FVector(0.0f, 0.0f, -11.0f));
		AcceptDust->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.02f));
	}

	// The two housing readouts, off by default -- see the header for what that costs.
	// Hidden rather than removed, so one checkbox brings them back.
	for (USceneComponent* C : { static_cast<USceneComponent*>(Tally), static_cast<USceneComponent*>(RunLog),
	                            static_cast<USceneComponent*>(TallyPlate), static_cast<USceneComponent*>(RunLogPlate) })
	{
		if (C)
		{
			C->SetVisibility(bShowHousingReadouts);
			C->SetHiddenInGame(!bShowHousingReadouts);
		}
	}

	if (SignOfficialText)
	{
		SignOfficialText->SetText(FText::FromString(OfficialName));
	}
	if (SignText)
	{
		SignText->SetText(FText::FromString(HandLetteredName));
	}
}

void ALegacyMachine::BeginPlay()
{
	Super::BeginPlay();

	DressTheVerdict();

	/* THE OVERNIGHT THROW, AS A NUMBER. Mrs. Hall says it threw again overnight; the
	   machine says how badly. Starting the tally at 47 rejects means the player walks up
	   to a system that has ALREADY been failing for hours rather than one that starts
	   failing when they arrive — the fault is inherited, which is the entire premise of
	   the job. */
	Rejected = 47;
	Accepted = 0;

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[LegacyMachine] '%s' online with %d part(s); healthy=%s"),
		*GetName(), Parts.Num(), IsHealthy() ? TEXT("true") : TEXT("false"));

	/* EVERY PART LEARNS WHO IT BELONGS TO. The machine holds the authoritative order, so
	   it does the wiring — a part that went looking for its own machine would pick the
	   wrong one the day there are two lines on the floor, and it would do it silently. */
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->OwningMachine = this;
			Part->SetTrueNameVisible(false);
			Part->SetFaultLampVisible(false);
		}
	}

	BatchStream.Initialize(20260822);
	if (Workpiece)
	{
		// Captured, never assumed: the build script fits this mesh to 18cm, so the
		// squash has to scale RELATIVE to whatever it was authored at.
		WorkpieceBaseScale = Workpiece->GetRelativeScale3D();
	}
	SeedOvernightLog();
	UpdateTally();

	BeginCycle();
	TryBindCodeVision();

	/* THE JOB STAYS DONE without Deploy. URefactorableComponent::BeginPlay resets
	   bIsRefactored, and Deploy apply is a next-tick on the pawn — both would put a
	   closed ticket back to broken if we applied here. 0.15s is after those resets
	   and still well before the first workpiece reaches a bin (~5s). */
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RestoreTicketHandle, this,
			&ALegacyMachine::MaybeRestoreClosedTicket, 0.15f, false);
	}
}

void ALegacyMachine::TryBindCodeVision()
{
	if (bBoundToCodeVision)
	{
		return;
	}

	// The pawn is not guaranteed to exist at our BeginPlay, so retry — the AHiddenDoor
	// pattern, and the same reason: a missed bind means Code Vision silently reveals
	// nothing here and the puzzle has no solution with no error to say why.
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCodeVisionComponent* CV = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;
	if (CV)
	{
		CV->OnCodeVisionChanged.AddDynamic(this, &ALegacyMachine::HandleCodeVisionChanged);
		bBoundToCodeVision = true;
		HandleCodeVisionChanged(CV->IsCodeVisionActive());
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[LegacyMachine] bound to Code Vision after %d attempt(s)"), BindAttempts + 1);
		return;
	}

	if (++BindAttempts < 20)
	{
		GetWorldTimerManager().SetTimer(BindRetryHandle, this,
			&ALegacyMachine::TryBindCodeVision, 0.5f, false);
	}
	else
	{
		UE_LOG(LogSibeliusGame, Error,
			TEXT("[LegacyMachine] '%s' never found a UCodeVisionComponent — the true names "
			     "can never be read and the fault cannot be diagnosed."), *GetName());
	}
}

void ALegacyMachine::HandleCodeVisionChanged(bool bIsActive)
{
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->SetTrueNameVisible(bIsActive);
		}
	}
}

bool ALegacyMachine::IsHealthy() const
{
	return FindFaultStage() == INDEX_NONE;
}

int32 ALegacyMachine::FindFaultStage() const
{
	// FIRST, not worst. A piece cannot reach stage four if stage two already threw it
	// out, so the first misbehaving stage is the only one the machine can demonstrate —
	// which is also the reason RunMachineSelfTest insists on exactly one fault.
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (Parts[Index] && !Parts[Index]->IsBehaving())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void ALegacyMachine::ClearFaultLamps()
{
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->SetFaultLampVisible(false);
		}
	}
}

void ALegacyMachine::BeginCycle()
{
	// A lit lamp always refers to the piece in front of you, so a new blank douses the
	// row before it sets off.
	ClearFaultLamps();
	JammedStage = INDEX_NONE;

	// EVERY PART ROLLS ITS LUCK ONCE, HERE. An intermittent fault latches one verdict for
	// the whole cycle so the stage the piece jams at, the row the log writes and the bin
	// it lands in are guaranteed to be the same answer to the same question.
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->RollForCycle();
		}
	}

	EnterStage(0);
}

void ALegacyMachine::EnterStage(int32 Index)
{
	StageIndex = Index;
	StageElapsed = 0.0f;
	ConsumeStep();

	ALegacyMachinePart* Part = Parts.IsValidIndex(Index) ? Parts[Index].Get() : nullptr;

	/* THE VERDICT HAPPENS WHERE THE FAULT IS. The old build ran the piece down the whole
	   row and only chose a bin on the last leg, which meant a broken INTAKE and a broken
	   GRADER produced identical footage: the machine's behaviour carried no information
	   at all and the only evidence in the level was five label pairs to compare by eye.
	   Asking the stage the piece is standing on is all it takes to fix that, and it is
	   what makes a longer row possible later — the player follows the piece to where it
	   stops instead of auditing every box. */
	if (Part && !Part->IsBehaving())
	{
		JammedStage = Index;
		Part->SetFaultLampVisible(true);
		Phase = ELegacyCyclePhase::Jammed;
		return;
	}

	Phase = (Index >= Parts.Num() - 1) ? ELegacyCyclePhase::Exiting
	                                   : ELegacyCyclePhase::Travelling;
}

void ALegacyMachine::FinishCycle()
{
	/* THE VERDICT IS THE JOURNEY'S, not a fresh poll of the parts. By the time a piece is
	   falling into a bin it has already been accepted or thrown out by a specific stage,
	   and JammedStage is that stage — latched when it jammed. Re-asking IsHealthy() here
	   would let a refactor performed while the piece was in mid-air rewrite a rejection
	   that the player just watched happen. A fix applies to the NEXT piece; that is what
	   makes the first ACCEPT feel earned rather than retroactive. */
	const bool bAccepted = (JammedStage == INDEX_NONE);

	// The counters move first: AppendLogEntry numbers the cycle from their sum, so the
	// log's clock and the tally's totals can never disagree about how many pieces this
	// machine has seen.
	if (bAccepted)
	{
		++Accepted;
	}
	else
	{
		++Rejected;
	}

	AppendLogEntry(bAccepted, JammedStage);

	/* A VERDICT PARKS THE LINE, it does not release it. If the player halted anywhere in
	   this cycle the machine stops here, between pieces, and says LINE HALTED until they
	   hand it back — see RequestHaltOrStep for why auto-resuming read as a broken
	   button. The one-shot step is spent either way. */
	bStepRequested = false;

	UpdateTally();

	if (bAccepted)
	{
		TryCloseTicket();
	}

	// The winning bin announces itself.
	VerdictFlashElapsed = 0.0f;
	bFlashAccepted = bAccepted;

	Phase = ELegacyCyclePhase::Resting;
	StageElapsed = 0.0f;
}

/* ------------------------------------------------------------------ the transport */

void ALegacyMachine::RequestHaltOrStep()
{
	if (!bHalted)
	{
		/* HALT LANDS ON A BEAT BOUNDARY, NOT IN MID-AIR. Requesting the step along with
		   the halt lets the beat in flight finish, so the piece comes to rest ON a stage
		   where the plaque under it means something. Freezing instantly would strand a
		   crate hovering between two boxes, which looks like a bug rather than a
		   decision. */
		bHalted = true;
		bStepRequested = true;
	}
	else if (Phase == ELegacyCyclePhase::Resting)
	{
		/* PARKED BETWEEN PIECES IS WHERE THE LINE IS HANDED BACK. Stepping a piece all
		   the way to its bin leaves the machine stopped at the one boundary where
		   "carry on" is unambiguous, and here E means exactly that.

		   The alternative — auto-resuming the moment a piece lands — was worse in a way
		   that only shows up in play: about a fifth of a cycle is the drop into the bin,
		   so a player who pressed halt during it watched the machine carry on running
		   and reasonably concluded the button was broken. Every halt now visibly stops
		   the line; it just may stop it at the end of the piece rather than mid-row. */
		bHalted = false;
		bStepRequested = false;
	}
	else
	{
		bStepRequested = true;
	}
	UpdateTally();
}

void ALegacyMachine::ConsumeStep()
{
	bStepRequested = false;
}

void ALegacyMachine::Interact_Implementation(AActor* Interactor)
{
	(void)Interactor;

	/* INSIDE A BRANCH, E MEASURES. Outside it, E drives the line.
	   One key, disambiguated by whether reality is branched — the same trick the transport
	   already plays with the cycle phase, and here it teaches the discipline instead of
	   announcing it: the only place you can run twenty test pieces is the place where they
	   do not go on Mrs. Hall's record. */
	if (CanRunTestBatch())
	{
		RunTestBatch();
		return;
	}
	RequestHaltOrStep();
}

FText ALegacyMachine::GetInteractionPrompt_Implementation() const
{
	if (CanRunTestBatch())
	{
		return FText::FromString(FString::Printf(
			TEXT("[E] run a test batch (%d) — off the record"), FMath::Max(TestBatchSize, 1)));
	}

	if (!bHalted)
	{
		return FText::FromString(TEXT("[E] halt the line"));
	}

	/* THE PROMPT SAYS WHAT THE NEXT PRESS WILL DO, by name. "[E] step" alone would make
	   the player press it to find out, which is the opposite of a debugger: stepping is
	   only useful when you can decide whether you want the next beat before you take it.
	   Naming the stage also keeps the housing, the log and the prompt in one vocabulary,
	   so a GRADER in the log is visibly the GRADER on the box. */
	switch (Phase)
	{
	case ELegacyCyclePhase::Resting:
		return FText::FromString(TEXT("[E] release the line"));

	case ELegacyCyclePhase::Jammed:
	{
		const ALegacyMachinePart* Part = Parts.IsValidIndex(JammedStage) ? Parts[JammedStage].Get() : nullptr;
		return FText::FromString(Part
			? FString::Printf(TEXT("[E] step — %s throws it out"), *Part->GetPlaqueHeading())
			: TEXT("[E] step — it is thrown out here"));
	}

	case ELegacyCyclePhase::Exiting:
		return FText::FromString(JammedStage == INDEX_NONE
			? TEXT("[E] step — into ACCEPT")
			: TEXT("[E] step — into REJECT"));

	case ELegacyCyclePhase::Travelling:
	default:
	{
		const ALegacyMachinePart* Next = Parts.IsValidIndex(StageIndex + 1) ? Parts[StageIndex + 1].Get() : nullptr;
		return FText::FromString(Next
			? FString::Printf(TEXT("[E] step to %s"), *Next->GetPlaqueHeading())
			: TEXT("[E] step"));
	}
	}
}


/* ---------------------------------------------------- ticket 2: measuring the line */

bool ALegacyMachine::AreAllFaultsCleared() const
{
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && !Part->IsFaultCleared())
		{
			return false;
		}
	}
	return true;
}

bool ALegacyMachine::CanRunTestBatch() const
{
	const UBranchSubsystem* Branch = GetWorld()
		? GetWorld()->GetSubsystem<UBranchSubsystem>() : nullptr;
	return Branch && Branch->IsBranched();
}

FString ALegacyMachine::RunTestBatch()
{
	/* PURE ARITHMETIC. No pieces animate, no timers run, the tally does not move: a test
	   batch is a question asked of the machine's state, and the answer is a number. That
	   is also what lets the headless gate assert against it — the same call the player
	   presses E for is the one the commandlet runs. */
	const int32 Trials = FMath::Max(TestBatchSize, 1);
	int32 Failures = 0;
	TMap<int32, int32> FailuresByStage;

	for (int32 Trial = 0; Trial < Trials; ++Trial)
	{
		// One piece walks the row and dies at the FIRST stage that fails it — the same
		// rule the live cycle follows, so the batch measures the machine the player is
		// actually watching rather than a second model of it.
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			ALegacyMachinePart* Part = Parts[Index].Get();
			if (Part && Part->WouldMisbehaveOnTrial(BatchStream))
			{
				++Failures;
				FailuresByStage.FindOrAdd(Index) += 1;
				break;
			}
		}
	}

	FString Row;
	if (Failures == 0)
	{
		Row = FString::Printf(TEXT("BATCH %d   %d PASS"), Trials, Trials);
	}
	else
	{
		int32 WorstStage = INDEX_NONE;
		int32 WorstCount = 0;
		for (const TPair<int32, int32>& Pair : FailuresByStage)
		{
			if (Pair.Value > WorstCount)
			{
				WorstCount = Pair.Value;
				WorstStage = Pair.Key;
			}
		}
		const ALegacyMachinePart* Part = Parts.IsValidIndex(WorstStage) ? Parts[WorstStage].Get() : nullptr;
		Row = Part
			? FString::Printf(TEXT("BATCH %d   %d FAIL AT %s"), Trials, Failures, *Part->GetPlaqueHeading())
			: FString::Printf(TEXT("BATCH %d   %d FAIL"), Trials, Failures);
	}

	// A clean batch is the evidence ticket 2 closes on. It is only meaningful once the
	// fault is actually fixed, so both are required at the close — see TryCloseTicket.
	if (Failures == 0)
	{
		bBatchProvenClean = true;
	}

	AppendRawLogRow(Row, Failures == 0);

	UE_LOG(LogSibeliusGame, Display, TEXT("[LegacyMachine] test batch: %s"), *Row);
	return Row;
}
/* -------------------------------------------------------------------- the run log */

FString ALegacyMachine::CycleTimestamp(int32 CycleNumber) const
{
	// One minute per cycle from the hour the ticket names. Cycle 47 is 03:47, which is
	// why the machine the player walks up to has a night's history on it instead of a
	// log that starts the moment they arrive.
	const int32 TotalMinutes = ThrowHour * 60 + FMath::Max(CycleNumber, 0);
	return FString::Printf(TEXT("%02d:%02d"), (TotalMinutes / 60) % 24, TotalMinutes % 60);
}

void ALegacyMachine::SeedOvernightLog()
{
	/* THE LOG THE TICKET IS ABOUT. docs/MACHINE_PLAN.md §4 opens the whole mechanic with
	   "the log says it threw at 03:00" — and until now there was no log, so the player
	   was told about evidence that did not exist. These rows are the overnight run: the
	   same stage, over and over, for hours before the player was hired to look at it.

	   It seeds from bIsFaulty rather than from IsBehaving() because this is HISTORY. A
	   save that reloads with the ticket already closed still shows the night it threw;
	   the green ACCEPTED lines then push it off the top, which is a nicer record of the
	   fix than any banner would be. */
	LogRows.Reset();

	FString FaultName;
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && Part->bIsFaulty)
		{
			FaultName = Part->GetPlaqueHeading();
			break;
		}
	}

	const FString Verdict = FaultName.IsEmpty()
		? FString(TEXT("REJECTED"))
		: FString::Printf(TEXT("REJECTED AT %s"), *FaultName);

	const int32 Seen = Accepted + Rejected;
	const int32 Rows = FMath::Clamp(RunLogRows, 0, Seen);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		LogRows.Add(FString::Printf(TEXT("%s  %s"), *CycleTimestamp(Seen - 1 - Row), *Verdict));
	}

	bLastLoggedAccept = false;
	UpdateRunLog();
}

void ALegacyMachine::AppendRawLogRow(const FString& Row, bool bGood)
{
	// The one way anything reaches the housing's history, so a cycle verdict and a test
	// batch verdict cannot drift apart in how they are capped, ordered or coloured.
	LogRows.Insert(Row, 0);
	while (LogRows.Num() > FMath::Max(RunLogRows, 1))
	{
		LogRows.Pop();
	}
	bLastLoggedAccept = bGood;
	UpdateRunLog();
}

void ALegacyMachine::AppendLogEntry(bool bAccepted, int32 FaultStage)
{
	// Called after the counters move, so the cycle just finished is the last one counted.
	const int32 CycleNumber = FMath::Max(Accepted + Rejected - 1, 0);

	FString Verdict = TEXT("ACCEPTED");
	if (!bAccepted)
	{
		const ALegacyMachinePart* Part = Parts.IsValidIndex(FaultStage) ? Parts[FaultStage].Get() : nullptr;
		Verdict = Part ? FString::Printf(TEXT("REJECTED AT %s"), *Part->GetPlaqueHeading())
		               : FString(TEXT("REJECTED"));
	}

	AppendRawLogRow(FString::Printf(TEXT("%s  %s"), *CycleTimestamp(CycleNumber), *Verdict), bAccepted);
}

void ALegacyMachine::UpdateRunLog()
{
	if (!RunLog)
	{
		return;
	}

	FString Text = TEXT("RUN LOG");
	for (const FString& Row : LogRows)
	{
		Text += TEXT("\n");
		Text += Row;
	}
	RunLog->SetText(FText::FromString(Text));

	// One UTextRenderComponent is one colour, so the newest verdict sets the mood of the
	// whole block and the per-row detail lives in the words. Trying to colour rows
	// individually would mean one component per row and a layout to keep in sync.
	RunLog->SetTextRenderColor(bLastLoggedAccept ? MachineGood : MachineBad);
}

/* ------------------------------------------------------------ ticket + the housing */

void ALegacyMachine::ArmTheMenagerie()
{
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && !Part->ActorHasTag(TEXT("WildRefactorOK")))
		{
			Part->Tags.Add(TEXT("WildRefactorOK"));
		}
	}
}

void ALegacyMachine::TryCloseTicket()
{
	// Editor commandlets load this map without a player. Do not write the
	// progression slot just because a self-test made one piece accept.
	if (!UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}

	/* ASK "IS IT FIXED", NOT "DID THIS ONE PASS". AreAllFaultsCleared() means every armed
	   fault has actually been refactored; IsHealthy() only means nothing misbehaved on
	   THIS cycle. Against ticket 1's deterministic fault those are the same sentence.
	   Against ticket 2's one-in-three they are not, and closing on the second would hand
	   the player a completed job for a lucky roll — the exact thing the batch exists to
	   stop them doing to themselves. */
	if (!AreAllFaultsCleared())
	{
		return;
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;
	}

	/* THE FIRST TICKET ARMS THE SECOND. Claiming Ticket.Legacy.Closed brings the
	   intermittent fault into existence — and its plaque/true-name disagreement with it —
	   so every part has to re-read itself the moment the grant lands, or a liar would
	   stand in the row still showing the honest label it wore a second ago. */
	if (Progression->ClaimOneTimeGrant(ClosedTicketGrant))
	{
		for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
		{
			if (Part)
			{
				Part->SyncLabelsToState();
			}
		}
		// THE JOB IS DONE, SO THE LINE BECOMES A TOY. From here R on a stage rolls
		// the menagerie instead of repairing it.
		ArmTheMenagerie();

		if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
		{
			Hall->Say(TEXT("Ticket.Closed"));
		}
		return;
	}

	// The second job: proven fixed, not merely behaving.
	// PROVEN, not merely fixed. Without the batch this closes on the first good piece,
	// which at a one-in-three fault happens two cycles in three whether the player fixed
	// anything or not — and Test-Drive goes straight back to being a key nobody presses.
	if (bBatchProvenClean
		&& Progression->HasClaimedGrant(ClosedTicketGrant)
		&& Progression->ClaimOneTimeGrant(IntermittentTicketGrant))
	{
		if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
		{
			Hall->Say(TEXT("Ticket.Closed"));
		}
	}
}

void ALegacyMachine::MaybeRestoreClosedTicket()
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;
	}

	// A closed ticket stays closed across a reload even without Deploy, which the player
	// may not own yet. Each grant restores only its own fault.
	const bool bTicketOne = Progression->HasClaimedGrant(ClosedTicketGrant);
	const bool bTicketTwo = Progression->HasClaimedGrant(IntermittentTicketGrant);

	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (!Part || !Part->bIsFaulty || !Part->Refactorable || Part->Refactorable->IsRefactored())
		{
			continue;
		}
		const bool bIsTicketTwoFault = !Part->ArmedByGrant.IsNone();
		if ((bIsTicketTwoFault && bTicketTwo) || (!bIsTicketTwoFault && bTicketOne))
		{
			Part->Refactorable->ApplyRefactor();
		}
	}

	// The menagerie survives a reload for the same reason the fix does: the ticket is
	// closed, and it stays closed whether or not the player owns Deploy yet.
	if (bTicketOne)
	{
		ArmTheMenagerie();
	}

	// Arming is a display change too: re-read every label once the grants are known.
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->SyncLabelsToState();
		}
	}
}


void ALegacyMachine::UpdateTally()
{
	if (!Tally)
	{
		return;
	}

	/* ALWAYS THREE LINES. A stopped machine has to say so — without it a player who halts
	   the line and wanders off comes back to a frozen room and reads it as a bug, which is
	   the one real cost of giving them a pause button. But the mode line is present even
	   while RUNNING, because the plate behind this text is sized once, for the tallest the
	   readout can ever get: a two-line tally under a three-line plate leaves a dark strip
	   hanging off the bottom, and a plate that grows every time the player halts reads as
	   a glitch rather than as equipment. */
	const FString Text = FString::Printf(
		TEXT("SINCE 03:00\nACCEPTED %d   REJECTED %d\n%s"),
		Accepted, Rejected, bHalted ? TEXT("LINE HALTED") : TEXT("RUNNING"));

	Tally->SetText(FText::FromString(Text));
	// Green once it is producing again — the reward for the fix is that the housing
	// stops being angry at you.
	Tally->SetTextRenderColor(Accepted > 0 ? MachineGood : MachineBad);
}

/* --------------------------------------------------------------- making it look alive */

void ALegacyMachine::TickPresentation(float DeltaSeconds)
{
	/* RUNS EVEN WHEN THE LINE IS HALTED, which is why it sits before the CanAdvance gate
	   in Tick. A player who halts mid-flash should watch the flash finish rather than see
	   a bin label frozen mid-swell. Nothing here reads or writes a verdict. */

	// The piece recovering its shape after a stage pressed on it.
	if (SquashElapsed >= 0.0f && Workpiece)
	{
		SquashElapsed += DeltaSeconds;
		const float T = SquashSeconds > 0.0f ? SquashElapsed / SquashSeconds : 1.0f;
		if (T >= 1.0f)
		{
			Workpiece->SetRelativeScale3D(WorkpieceBaseScale);
			SquashElapsed = -1.0f;
		}
		else
		{
			// One decaying bounce: flat and wide, then back.
			const float K = FMath::Sin(T * PI) * (1.0f - T);
			Workpiece->SetRelativeScale3D(WorkpieceBaseScale
				* FVector(1.0f + K * 0.30f, 1.0f + K * 0.30f, 1.0f - K * 0.45f));
		}
	}

	// The winning bin's label swelling as the piece lands in it.
	if (VerdictFlashElapsed >= 0.0f)
	{
		VerdictFlashElapsed += DeltaSeconds;
		UTextRenderComponent* Label = bFlashAccepted ? AcceptLabel.Get() : RejectLabel.Get();
		const float T = VerdictFlashSeconds > 0.0f ? VerdictFlashElapsed / VerdictFlashSeconds : 1.0f;
		if (Label)
		{
			if (T >= 1.0f)
			{
				Label->SetWorldSize(13.0f);
			}
			else
			{
				Label->SetWorldSize(13.0f + 5.0f * FMath::Sin(T * PI));
			}
		}
		if (T >= 1.0f)
		{
			VerdictFlashElapsed = -1.0f;
		}
	}
}

/* ------------------------------------------------------------------------ the run */

void ALegacyMachine::Tick(float DeltaSeconds)
{
	/* MRS. HALL DOES NOT NOTICE. A stage has been replaced by an animal, the line is
	   still producing, and she comments approvingly on throughput. That is the whole
	   reframe in one line -- she oversees comically bad projects and cannot tell a goat
	   from a machine -- and the player performed it rather than being told it.

	   POLLED, NOT PUSHED. URefactorComponent is a generic power and has no business
	   knowing this machine exists; the alternative was a delegate threaded from the
	   transmutation back into a specific actor, which is more plumbing than a gag is
	   worth. This costs five null checks a frame, and only until she has said it. */
	if (!bSaidTheLivestockLine)
	{
		for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
		{
			if (Part && Part->IsBodyHidden())
			{
				bSaidTheLivestockLine = true;
				if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
				{
					Hall->Say(TEXT("Ticket.Livestock"));
				}
				break;
			}
		}
	}

	Super::Tick(DeltaSeconds);

	TickPresentation(DeltaSeconds);

	if (Parts.Num() < 2 || !Workpiece || !AcceptBin || !RejectBin)
	{
		return;   // not wired up yet; nothing to animate
	}

	/* HALTED AND NOTHING ASKED FOR: the piece stays exactly where it is, mid-leg or
	   sitting on a stage. Returning before the clock moves is what makes a step a whole
	   beat rather than a frame — the step flag survives until the beat that consumes it
	   completes, so one press always buys one thing the player can name. */
	if (!CanAdvance())
	{
		return;
	}

	StageElapsed += DeltaSeconds;

	// The piece rides above whatever it is travelling between — high enough to clear the
	// crates and their plates, or its motion is hidden behind them. See CarryHeight.
	const FVector Carry(0.0f, 0.0f, CarryHeight);

	switch (Phase)
	{
	case ELegacyCyclePhase::Resting:
	{
		if (StageElapsed >= ResetSeconds)
		{
			BeginCycle();
		}
		return;
	}

	case ELegacyCyclePhase::Jammed:
	{
		ALegacyMachinePart* Stuck = Parts.IsValidIndex(JammedStage) ? Parts[JammedStage].Get() : nullptr;
		if (Stuck)
		{
			// Rattling against the stage that will not take it, and the lamp blinking
			// over its steady plate. A dead-still piece under a static label reads as the
			// machine switched off rather than the machine failing.
			const float Shake = FMath::Sin(StageElapsed * 38.0f) * JamRattle;
			Workpiece->SetWorldLocation(Stuck->GetActorLocation() + Carry + FVector(0.0f, Shake, 0.0f));
			const bool bLampOn = LampBlinkHz <= 0.0f
				|| FMath::Fmod(StageElapsed * LampBlinkHz, 1.0f) < 0.55f;
			Stuck->SetLampTextVisible(bLampOn);
		}

		/* FIXED WHILE IT IS STANDING THERE: the piece moves on. Holding R on the stage a
		   crate is stuck against and watching that crate continue is the most direct
		   statement this machine can make about cause and effect, and it costs one
		   re-check. The reverse is not symmetrical — see FinishCycle: once a piece has
		   left the row its verdict is history. */
		if (!Stuck || Stuck->IsBehaving())
		{
			JammedStage = INDEX_NONE;
			ClearFaultLamps();
			EnterStage(StageIndex);
			return;
		}

		if (StageElapsed >= JamSeconds)
		{
			// The lamp stays lit through the drop, so "this is where it died" is still on
			// screen while the player watches it land in REJECT.
			Phase = ELegacyCyclePhase::Exiting;
			StageElapsed = 0.0f;
			ConsumeStep();
		}
		return;
	}

	case ELegacyCyclePhase::Exiting:
	{
		const ALegacyMachinePart* From = Parts.IsValidIndex(StageIndex) ? Parts[StageIndex].Get() : nullptr;
		if (!From)
		{
			return;
		}
		const USceneComponent* Bin = (JammedStage == INDEX_NONE) ? AcceptBin.Get() : RejectBin.Get();
		const float Alpha = FMath::Clamp(StageElapsed / StageSeconds, 0.0f, 1.0f);

		/* IT FALLS INTO THE BIN rather than gliding down a ramp. Across the floor it
		   travels evenly; the height drops on Alpha squared, so it tips off the row and
		   accelerates in, which is the one bit of this machine that gets to look like
		   gravity. */
		const FVector Flat = FMath::Lerp(From->GetActorLocation(), Bin->GetComponentLocation(), Alpha);
		const float StartZ = From->GetActorLocation().Z + Carry.Z;
		const float EndZ = Bin->GetComponentLocation().Z;
		Workpiece->SetWorldLocation(FVector(Flat.X, Flat.Y, FMath::Lerp(StartZ, EndZ, Alpha * Alpha)));
		Workpiece->SetWorldRotation(FRotator(0.0f, (StageIndex + Alpha) * 90.0f, Alpha * 18.0f));

		if (Alpha >= 1.0f)
		{
			SquashElapsed = 0.0f;      // it lands
			FinishCycle();
		}
		return;
	}

	case ELegacyCyclePhase::Travelling:
	default:
	{
		const ALegacyMachinePart* A = Parts.IsValidIndex(StageIndex) ? Parts[StageIndex].Get() : nullptr;
		const ALegacyMachinePart* B = Parts.IsValidIndex(StageIndex + 1) ? Parts[StageIndex + 1].Get() : nullptr;
		if (!A || !B)
		{
			return;
		}
		const float Alpha = FMath::Clamp(StageElapsed / StageSeconds, 0.0f, 1.0f);

		/* AN ARC, NOT A SLIDE, and a quarter turn per stage. A crate tracking down a
		   straight line at constant speed is the single most placeholder-looking thing
		   this machine did. The hop peaks at mid-gap, which is exactly where there is no
		   plate to clip through, and arriving square each time reads as indexed rather
		   than drifting. */
		FVector Pos = FMath::Lerp(A->GetActorLocation(), B->GetActorLocation(), Alpha) + Carry;
		Pos.Z += HopHeight * FMath::Sin(Alpha * PI);
		Workpiece->SetWorldLocation(Pos);
		Workpiece->SetWorldRotation(FRotator(0.0f, (StageIndex + Alpha) * 90.0f, 0.0f));

		if (Alpha >= 1.0f)
		{
			// Landed: the stage presses on it.
			SquashElapsed = 0.0f;
			EnterStage(StageIndex + 1);
		}
		return;
	}
	}
}

/* ----------------------------------------------------------------- demand diagnostics */

bool ALegacyMachine::RunMachineSelfTest(FString& OutError) const
{
	/* Pure state, no world: the commandlet gate can run this without a player, a tick or
	   a spawned level. It asserts the facts the whole test rests on — the machine is born
	   broken, the break is localised to one nameable stage, refactoring that stage fixes
	   it, and reverting breaks it again (which is what makes a discarded Test-Drive
	   branch behave). */
	if (Parts.Num() < 2)
	{
		OutError = TEXT("machine has fewer than 2 parts");
		return false;
	}

	ALegacyMachinePart* Faulty = nullptr;
	int32 FaultyIndex = INDEX_NONE;
	int32 UnconditionalFaults = 0;
	ALegacyMachinePart* Intermittent = nullptr;
	int32 ArmedFaults = 0;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (!Parts[Index] || !Parts[Index]->bIsFaulty)
		{
			continue;
		}
		if (Parts[Index]->ArmedByGrant.IsNone())
		{
			Faulty = Parts[Index];
			FaultyIndex = Index;
			++UnconditionalFaults;
		}
		else
		{
			Intermittent = Parts[Index];
			++ArmedFaults;
		}
	}

	/* ONE LIVE FAULT PER TICKET. Ticket 1's is unconditional; ticket 2's is dormant until
	   its grant lands. Two live at once and "find the part that is lying" has no answer —
	   which is the whole reason the second fault is gated rather than simply authored in. */
	if (UnconditionalFaults != 1)
	{
		OutError = FString::Printf(
			TEXT("expected exactly 1 un-gated faulty part, found %d — a machine with no "
			     "fault has no puzzle, and one with two has no answer"), UnconditionalFaults);
		return false;
	}
	if (ArmedFaults > 1)
	{
		OutError = FString::Printf(
			TEXT("found %d grant-armed faults; one ticket at a time or the second job has "
			     "two answers too"), ArmedFaults);
		return false;
	}
	if (!Faulty->Refactorable)
	{
		OutError = TEXT("the faulty part has no URefactorableComponent — it cannot be fixed");
		return false;
	}
	if (Faulty->GetPlaqueHeading().IsEmpty())
	{
		OutError = TEXT("the faulty part has no plaque heading — the fault lamp and the run "
		                "log would name the stage as an empty string");
		return false;
	}

	/* THE INVARIANT THE PUZZLE RESTS ON: exactly the faulty part disagrees with itself.
	   On an honest part the plaque and the true name are word for word identical, so the
	   eye skims them; the liar is the only pair that differs. Get this wrong in either
	   direction and the puzzle breaks silently — an honest part whose lines disagree is a
	   red herring with no fix behind it, and a faulty part whose lines AGREE cannot be
	   diagnosed at all. Neither shows up as a crash, only as a player wandering. */
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (!Part)
		{
			continue;
		}
		// Flattened: the claims wrap across two lines on their plates now, and a true name
		// may wrap at a different word. Line breaks are layout, not meaning.
		const FString Claim = ALegacyMachinePart::FlattenLabel(Part->GetPlaqueClaim());
		const bool bAgrees = Claim.Equals(ALegacyMachinePart::FlattenLabel(Part->TrueName));
		if (bAgrees == Part->bIsFaulty)
		{
			OutError = FString::Printf(
				TEXT("'%s' is %s but its plaque and true name %s — the only part whose two "
				     "lines disagree must be the faulty one"),
				*Part->GetName(),
				Part->bIsFaulty ? TEXT("FAULTY") : TEXT("honest"),
				bAgrees ? TEXT("agree") : TEXT("disagree"));
			return false;
		}
	}

	/* ---- TICKET 2: THE INTERMITTENT FAULT ACTUALLY HAS TO BE INTERMITTENT ----------
	   This is the one assertion the whole second ticket rests on, and it is the one that
	   would rot silently. A FaultChance typo'd to 1.0 gives a deterministic fault wearing
	   ticket 2's clothes — the log would say REJECTED every cycle, the player would fix it
	   and confirm by eye, and Test-Drive would go back to being a key nobody presses. A
	   typo'd 0.0 gives a machine with no second job at all. Neither crashes. */
	if (Intermittent)
	{
		if (!(Intermittent->FaultChance > 0.0f && Intermittent->FaultChance < 1.0f))
		{
			OutError = FString::Printf(
				TEXT("'%s' is the gated fault but its FaultChance is %.2f — it has to be "
				     "between 0 and 1, or it is not intermittent and Test-Drive has no job"),
				*Intermittent->GetPlaqueHeading(), Intermittent->FaultChance);
			return false;
		}

		// Measured, not asserted from the constant: 4000 trials off a fixed seed.
		FRandomStream Stream(4242);
		const int32 Trials = 4000;
		int32 Misbehaved = 0;
		for (int32 Trial = 0; Trial < Trials; ++Trial)
		{
			if (Intermittent->WouldMisbehaveIfArmed(Stream))
			{
				++Misbehaved;
			}
		}
		const float Measured = static_cast<float>(Misbehaved) / static_cast<float>(Trials);
		if (FMath::Abs(Measured - Intermittent->FaultChance) > 0.05f)
		{
			OutError = FString::Printf(
				TEXT("'%s' is authored at %.2f but measured %.3f over %d trials — the roll "
				     "does not match the dial"),
				*Intermittent->GetPlaqueHeading(), Intermittent->FaultChance, Measured, Trials);
			return false;
		}

		// A refactored intermittent part must be sound on EVERY trial, or a fix the
		// player proved with a batch would still drop pieces in production.
		Intermittent->Refactorable->ApplyRefactor();
		for (int32 Trial = 0; Trial < 500; ++Trial)
		{
			if (Intermittent->WouldMisbehaveIfArmed(Stream))
			{
				OutError = FString::Printf(
					TEXT("'%s' still failed a trial after being refactored — a proven fix "
					     "that is not actually a fix is worse than no fix"),
					*Intermittent->GetPlaqueHeading());
				Intermittent->Refactorable->RevertRefactor();
				return false;
			}
		}
		Intermittent->Refactorable->RevertRefactor();

		/* THE PER-CYCLE LATCH. IsBehaving() is asked several times inside one cycle — by
		   the tick that jams the piece, by the log that names the stage, by the tally. If
		   the roll happened inside it rather than once in RollForCycle, those answers
		   would disagree with each other and the piece would jam at a stage the log then
		   denied. Roll once, then confirm the answer holds still. */
		for (int32 Cycle = 0; Cycle < 200; ++Cycle)
		{
			Intermittent->RollForCycle();
			const bool bFirst = Intermittent->IsBehaving();
			for (int32 Repeat = 0; Repeat < 5; ++Repeat)
			{
				if (Intermittent->IsBehaving() != bFirst)
				{
					OutError = TEXT("an intermittent part changed its mind inside one cycle "
					                "— IsBehaving must be latched by RollForCycle, never rolled");
					return false;
				}
			}
		}
	}

	/* THE CLOCK THE LOG IS WRITTEN IN. One minute per cycle from the hour the ticket
	   names, so the 47 overnight rejects run out to 03:47. It is three lines of
	   arithmetic and it would drift silently: a wrong clock does not crash, it just
	   quietly stops agreeing with the sentence Mrs. Hall says in the first minute. */
	if (!CycleTimestamp(0).Equals(TEXT("03:00")) || !CycleTimestamp(47).Equals(TEXT("03:47")))
	{
		OutError = FString::Printf(
			TEXT("run-log clock drifted: cycle 0 reads '%s' and cycle 47 reads '%s' — "
			     "they must be 03:00 and 03:47, the hour the ticket names"),
			*CycleTimestamp(0), *CycleTimestamp(47));
		return false;
	}

	if (IsHealthy())
	{
		OutError = TEXT("machine reports healthy while its faulty part is un-refactored");
		return false;
	}

	/* WHERE IT DIES IS WHERE IT IS BROKEN. This is the assertion that protects the first
	   mechanic: the piece is thrown out AT FindFaultStage(), so if that ever stops
	   resolving to the authored faulty part, the machine goes back to rejecting somewhere
	   uninformative and the fault lamp lights on an innocent box. Nothing about that
	   would crash — it would just quietly become a five-way guess again. */
	if (FindFaultStage() != FaultyIndex)
	{
		OutError = FString::Printf(
			TEXT("the fault localises to stage %d but '%s' is the faulty part at stage %d — "
			     "the workpiece would die at the wrong box and the log would name it"),
			FindFaultStage(), *Faulty->GetPlaqueHeading(), FaultyIndex);
		return false;
	}

	Faulty->Refactorable->ApplyRefactor();
	Faulty->SyncLabelsToState();
	if (!IsHealthy())
	{
		OutError = TEXT("refactoring the faulty part did not make the machine healthy");
		return false;
	}
	if (FindFaultStage() != INDEX_NONE)
	{
		OutError = FString::Printf(
			TEXT("after refactor the fault still localises to stage %d — the piece would "
			     "keep jamming on a part the player has already fixed"), FindFaultStage());
		return false;
	}
	{
		const FString Shown = Faulty->TrueLabel
			? ALegacyMachinePart::FlattenLabel(Faulty->TrueLabel->Text.ToString())
			: FString();
		if (!Shown.Equals(ALegacyMachinePart::FlattenLabel(Faulty->GetPlaqueClaim())))
		{
			OutError = FString::Printf(
				TEXT("after refactor the true name still reads '%s' — the fix must make "
				     "the two lines agree, or holding V after R still shows a liar"),
				*Shown);
			return false;
		}
	}

	Faulty->Refactorable->RevertRefactor();
	Faulty->SyncLabelsToState();
	if (IsHealthy())
	{
		OutError = TEXT("reverting the refactor did not restore the fault — a discarded "
		                "Test-Drive branch would leave the machine wrongly fixed");
		return false;
	}
	if (FindFaultStage() != FaultyIndex)
	{
		OutError = FString::Printf(
			TEXT("after revert the fault localises to stage %d, not %d — a discarded "
			     "Test-Drive would move the jam to the wrong box"),
			FindFaultStage(), FaultyIndex);
		return false;
	}
	{
		const FString Shown = Faulty->TrueLabel
			? ALegacyMachinePart::FlattenLabel(Faulty->TrueLabel->Text.ToString())
			: FString();
		if (Shown.Equals(ALegacyMachinePart::FlattenLabel(Faulty->GetPlaqueClaim())))
		{
			OutError = TEXT("reverting the refactor left the true name agreeing with the "
			                "plaque — a discarded Test-Drive would hide the lie");
			return false;
		}
	}

	return true;
}
