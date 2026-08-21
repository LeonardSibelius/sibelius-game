// LegacyMachine.cpp — see header.

#include "LegacyMachine.h"

#include "LegacyMachinePart.h"
#include "CodeVisionComponent.h"
#include "RefactorableComponent.h"
#include "ProgressionSubsystem.h"
#include "MrsHallSubsystem.h"
#include "SibeliusGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

const FName ALegacyMachine::ClosedTicketGrant(TEXT("Ticket.Legacy.Closed"));

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
	Tally->SetTextRenderColor(FColor(255, 120, 90, 255));

	AcceptLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AcceptLabel"));
	AcceptLabel->SetupAttachment(Root);
	AcceptLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	AcceptLabel->SetWorldSize(13.0f);
	AcceptLabel->SetHorizontalAlignment(EHTA_Center);
	AcceptLabel->SetVerticalAlignment(EVRTA_TextCenter);
	AcceptLabel->SetText(FText::FromString(TEXT("ACCEPT")));
	AcceptLabel->SetTextRenderColor(FColor(120, 235, 140, 255));

	RejectLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RejectLabel"));
	RejectLabel->SetupAttachment(Root);
	RejectLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	RejectLabel->SetWorldSize(13.0f);
	RejectLabel->SetHorizontalAlignment(EHTA_Center);
	RejectLabel->SetVerticalAlignment(EVRTA_TextCenter);
	RejectLabel->SetText(FText::FromString(TEXT("REJECT")));
	RejectLabel->SetTextRenderColor(FColor(255, 120, 90, 255));
}

void ALegacyMachine::BeginPlay()
{
	Super::BeginPlay();

	/* THE OVERNIGHT THROW, AS A NUMBER. Mrs. Hall says it threw again overnight; the
	   machine says how badly. Starting the tally at 47 rejects means the player walks up
	   to a system that has ALREADY been failing for hours rather than one that starts
	   failing when they arrive — the fault is inherited, which is the entire premise of
	   the job. */
	Rejected = 47;
	Accepted = 0;
	UpdateTally();

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[LegacyMachine] '%s' online with %d part(s); healthy=%s"),
		*GetName(), Parts.Num(), IsHealthy() ? TEXT("true") : TEXT("false"));

	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part)
		{
			Part->SetTrueNameVisible(false);
		}
	}

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
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && !Part->IsBehaving())
		{
			return false;
		}
	}
	return true;
}

void ALegacyMachine::BeginCycle()
{
	StageIndex = 0;
	StageElapsed = 0.0f;
	bResting = false;
}

void ALegacyMachine::FinishCycle()
{
	// THE VERDICT IS READ FRESH, every cycle, from the parts themselves. Nothing is
	// cached, so a Test-Drive branch that reverts a refactor needs no notification —
	// the very next piece simply comes out the other way.
	bLastRunAccepted = IsHealthy();
	if (bLastRunAccepted)
	{
		++Accepted;
		TryCloseTicket();
	}
	else
	{
		++Rejected;
	}
	UpdateTally();
	bResting = true;
	StageElapsed = 0.0f;
}

void ALegacyMachine::TryCloseTicket()
{
	// Editor commandlets load this map without a player. Do not write the
	// progression slot just because a self-test made one piece accept.
	if (!UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression || !Progression->ClaimOneTimeGrant(ClosedTicketGrant))
	{
		return;
	}

	if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
	{
		Hall->Say(TEXT("Ticket.Closed"));
	}
}

void ALegacyMachine::MaybeRestoreClosedTicket()
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression || !Progression->HasClaimedGrant(ClosedTicketGrant))
	{
		return;
	}

	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && Part->bIsFaulty && Part->Refactorable && !Part->Refactorable->IsRefactored())
		{
			Part->Refactorable->ApplyRefactor();
		}
	}
}

void ALegacyMachine::UpdateTally()
{
	if (!Tally)
	{
		return;
	}
	Tally->SetText(FText::FromString(FString::Printf(
		TEXT("SINCE 03:00\nACCEPTED %d   REJECTED %d"), Accepted, Rejected)));
	// Green once it is producing again — the reward for the fix is that the housing
	// stops being angry at you.
	Tally->SetTextRenderColor(Accepted > 0 ? FColor(120, 235, 140, 255)
	                                       : FColor(255, 120, 90, 255));
}

void ALegacyMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Parts.Num() < 2 || !Workpiece)
	{
		return;   // not wired up yet; nothing to animate
	}

	StageElapsed += DeltaSeconds;

	if (bResting)
	{
		if (StageElapsed >= ResetSeconds)
		{
			BeginCycle();
		}
		return;
	}

	// Travel between part N and part N+1, then from the last part into a bin.
	const int32 LastLeg = Parts.Num() - 1;
	const float Alpha = FMath::Clamp(StageElapsed / StageSeconds, 0.0f, 1.0f);

	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;

	if (StageIndex < LastLeg)
	{
		const ALegacyMachinePart* A = Parts[StageIndex];
		const ALegacyMachinePart* B = Parts[StageIndex + 1];
		if (!A || !B)
		{
			return;
		}
		From = A->GetActorLocation();
		To = B->GetActorLocation();
	}
	else
	{
		// The last leg: out of the final part and into whichever bin the machine's
		// health decides. This is the moment the fault becomes VISIBLE.
		const ALegacyMachinePart* A = Parts[LastLeg];
		if (!A)
		{
			return;
		}
		From = A->GetActorLocation();
		To = (IsHealthy() ? AcceptBin : RejectBin)->GetComponentLocation();
	}

	Workpiece->SetWorldLocation(FMath::Lerp(From, To, Alpha) + FVector(0, 0, 40.0f));

	if (Alpha >= 1.0f)
	{
		if (StageIndex < LastLeg)
		{
			++StageIndex;
			StageElapsed = 0.0f;
		}
		else
		{
			FinishCycle();
		}
	}
}

bool ALegacyMachine::RunMachineSelfTest(FString& OutError) const
{
	/* Pure state, no world: the commandlet gate can run this without a player, a tick or
	   a spawned level. It asserts the three facts the whole test rests on — the machine
	   is born broken, refactoring the faulty part fixes it, and reverting breaks it
	   again (which is what makes a discarded Test-Drive branch behave). */
	if (Parts.Num() < 2)
	{
		OutError = TEXT("machine has fewer than 2 parts");
		return false;
	}

	ALegacyMachinePart* Faulty = nullptr;
	int32 FaultyCount = 0;
	for (const TObjectPtr<ALegacyMachinePart>& Part : Parts)
	{
		if (Part && Part->bIsFaulty)
		{
			Faulty = Part;
			++FaultyCount;
		}
	}

	if (FaultyCount != 1)
	{
		OutError = FString::Printf(
			TEXT("expected exactly 1 faulty part, found %d — a machine with no fault has "
			     "no puzzle, and one with two has no answer"), FaultyCount);
		return false;
	}
	if (!Faulty->Refactorable)
	{
		OutError = TEXT("the faulty part has no URefactorableComponent — it cannot be fixed");
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
		const FString Claim = Part->GetPlaqueClaim();
		const bool bAgrees = Claim.Equals(Part->TrueName.TrimStartAndEnd());
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

	if (IsHealthy())
	{
		OutError = TEXT("machine reports healthy while its faulty part is un-refactored");
		return false;
	}

	Faulty->Refactorable->ApplyRefactor();
	Faulty->SyncLabelsToState();
	if (!IsHealthy())
	{
		OutError = TEXT("refactoring the faulty part did not make the machine healthy");
		return false;
	}
	{
		const FString Shown = Faulty->TrueLabel
			? Faulty->TrueLabel->Text.ToString().TrimStartAndEnd()
			: FString();
		if (!Shown.Equals(Faulty->GetPlaqueClaim()))
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
	{
		const FString Shown = Faulty->TrueLabel
			? Faulty->TrueLabel->Text.ToString().TrimStartAndEnd()
			: FString();
		if (Shown.Equals(Faulty->GetPlaqueClaim()))
		{
			OutError = TEXT("reverting the refactor left the true name agreeing with the "
			                "plaque — a discarded Test-Drive would hide the lie");
			return false;
		}
	}

	return true;
}
