// Spaceport.cpp — see the header for why this extends ABuildSite.

#include "Spaceport.h"

#include "SibeliusGame.h"   // LogSibeliusGame

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	/* THE VENDOR MESHES, BY PATH — and the cook rule they depend on.

	   ModularSciFiEnv_F and _J are gitignored. A soft path written here is NOT a package
	   reference and the cooker will not follow it, so these reach a shipped build ONLY
	   because DefaultGame.ini names their directories in DirectoriesToAlwaysCook. Remove
	   those lines and the spaceport assembles flawlessly in PIE and is invisible to every
	   player. Checked before writing this file: all three assets exist on disk. */
	const TCHAR* const PadMeshPath    = TEXT("/Game/ModularSciFiEnv_J/Meshes/SM_Floor_A_4x2m_A.SM_Floor_A_4x2m_A");
	const TCHAR* const ColumnMeshPath = TEXT("/Game/ModularSciFiEnv_J/Meshes/SM_Column_A.SM_Column_A");
	const TCHAR* const PanelMeshPath  = TEXT("/Game/ModularSciFiEnv_F/Meshes/SM_Panel_A_2x2m.SM_Panel_A_2x2m");

	/** Ease-out cubic. A part decelerates into place instead of stopping dead. */
	float Settle(float A)
	{
		const float T = 1.0f - FMath::Clamp(A, 0.0f, 1.0f);
		return 1.0f - (T * T * T);
	}
}

ASpaceport::ASpaceport()
{
	/* TICK IS THE RIGHT TOOL HERE, and that is not a contradiction of the standing lesson.

	   The rule this project learned the hard way is that a component NewObject'd onto an
	   already-live actor never gets its TickComponent called — UDancerAgentComponent lost
	   three rounds of debugging to it and runs on a timer to this day. That is a COMPONENT
	   tick problem. AActor::Tick is a different mechanism, and ABuildSite already drives
	   its float-and-spin reveal from it in this very class hierarchy.

	   So: inherit the parent's policy exactly — can tick, starts disabled, switched on
	   only while something is actually moving (its K6 rule) — and stay off the rest of
	   the time. A spaceport standing finished on a lawn costs nothing per frame. */
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MakeDefaultLayout();
}

void ASpaceport::MakeDefaultLayout()
{
	/* A GREYBOX SILHOUETTE, NOT A FINISHED BUILD (Phase B).

	   The numbers below are round metres and deliberately generous, because the exact
	   pivots and dimensions of a vendor pack are not knowable from a file listing. A
	   layout that tries to tile precisely against guessed dimensions looks worse than one
	   that spaces parts out and reads clearly at a distance.

	   Everything here is EditAnywhere. The art pass is dragging these numbers in the
	   Details panel with the level open, which needs no rebuild and no code. */
	Parts.Reset();

	auto Add = [this](const TCHAR* Path, FVector Loc, FRotator Rot, FVector Scale,
		float At, float For)
	{
		FSpaceportPart P;
		P.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path));
		P.Location = Loc;
		P.Rotation = Rot;
		P.Scale = Scale;
		P.RiseAt = At;
		P.RiseFor = For;
		Parts.Add(P);
	};

	// The pad first — the ground becomes concrete before anything stands on it.
	Add(PadMeshPath, FVector(0, 0, 0), FRotator::ZeroRotator, FVector(3.0f, 3.0f, 1.0f), 0.00f, 0.30f);

	// Four gantry legs at the corners, rising together.
	const float Leg = 550.0f;
	Add(ColumnMeshPath, FVector( Leg,  Leg, 0), FRotator::ZeroRotator, FVector(1, 1, 4), 0.20f, 0.35f);
	Add(ColumnMeshPath, FVector( Leg, -Leg, 0), FRotator::ZeroRotator, FVector(1, 1, 4), 0.22f, 0.35f);
	Add(ColumnMeshPath, FVector(-Leg,  Leg, 0), FRotator::ZeroRotator, FVector(1, 1, 4), 0.24f, 0.35f);
	Add(ColumnMeshPath, FVector(-Leg, -Leg, 0), FRotator::ZeroRotator, FVector(1, 1, 4), 0.26f, 0.35f);

	// The service tower on one side, stacking upward — the part that reads as "spaceport".
	for (int32 i = 0; i < 4; ++i)
	{
		Add(PanelMeshPath,
			FVector(-780.0f, 0.0f, 200.0f * i),
			FRotator(0.0f, 90.0f, 0.0f),
			FVector(1.0f, 1.0f, 1.0f),
			0.40f + 0.10f * i, 0.25f);
	}

	// Two fuel tanks, last, off to the side.
	Add(ColumnMeshPath, FVector(300.0f, 950.0f, 0), FRotator::ZeroRotator, FVector(2.5f, 2.5f, 2.0f), 0.72f, 0.28f);
	Add(ColumnMeshPath, FVector(-300.0f, 950.0f, 0), FRotator::ZeroRotator, FVector(2.5f, 2.5f, 2.0f), 0.76f, 0.24f);
}

void ASpaceport::BeginPlay()
{
	Super::BeginPlay();
	BuildPartComponents();
}

void ASpaceport::BuildPartComponents()
{
	if (PartComponents.Num() == Parts.Num() && Parts.Num() > 0)
	{
		return;   // idempotent: BeginPlay and a re-assembly must not double the structure
	}

	ClearParts();

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMesh* Mesh = Parts[i].Mesh.LoadSynchronous();
		if (!Mesh)
		{
			/* Named, not silent. A missing part is the DirectoriesToAlwaysCook failure
			   this file's header warns about, and in a packaged build it is the ONLY
			   symptom: a spaceport with a hole in it. Say which asset. */
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Spaceport] part %d failed to load (%s) - it will be missing. "
				     "Is its folder in DirectoriesToAlwaysCook?"),
				i, *Parts[i].Mesh.ToString());
			PartComponents.Add(nullptr);
			continue;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetStaticMesh(Mesh);
		Comp->SetupAttachment(SceneRoot);
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetHiddenInGame(true);   // shown when its slice of the assembly begins
		Comp->RegisterComponent();
		PartComponents.Add(Comp);

		ApplyPartProgress(i, 0.0f);
	}
}

void ASpaceport::ClearParts()
{
	for (UStaticMeshComponent* Comp : PartComponents)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	PartComponents.Reset();
}

void ASpaceport::ApplyPartProgress(int32 Index, float Alpha)
{
	if (!PartComponents.IsValidIndex(Index) || !PartComponents[Index])
	{
		return;
	}

	const FSpaceportPart& P = Parts[Index];
	UStaticMeshComponent* Comp = PartComponents[Index];

	const float Eased = Settle(Alpha);
	FVector Where = P.Location;
	Where.Z -= RiseFromBelow * (1.0f - Eased);

	/* RELATIVE, and it is safe BECAUSE these are not the root. SceneRoot is the root
	   component (ABuildSite sets it explicitly, for the CP3 reason), so "relative" here
	   really is relative to the actor. Calling SetRelativeLocation on a root means WORLD,
	   which is what teleported three coffee cups to the origin in 1.0. */
	Comp->SetRelativeLocation(Where);
	Comp->SetRelativeRotation(P.Rotation);
	Comp->SetRelativeScale3D(P.Scale);

	// Hidden until it has actually started moving, so nothing pops at the sunk position.
	Comp->SetHiddenInGame(Alpha <= 0.0f);
}

void ASpaceport::PlayAssembly()
{
	BuildPartComponents();

	if (Parts.Num() == 0)
	{
		bAssembled = true;
		return;
	}

	bAssembling = true;
	bAssembled = false;
	AssemblyElapsed = 0.0f;

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		ApplyPartProgress(i, 0.0f);
	}

	SetActorTickEnabled(true);   // the parent's K6 idiom: tick only while something moves

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Spaceport] assembling %d parts over %.1fs."), Parts.Num(), AssemblySeconds);
}

void ASpaceport::SnapAssembled()
{
	BuildPartComponents();

	bAssembling = false;
	bAssembled = true;
	AssemblyElapsed = AssemblySeconds;

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		ApplyPartProgress(i, 1.0f);
	}

	SetActorTickEnabled(false);
}

void ASpaceport::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);   // the parent still owns its own float-and-spin reveal

	if (!bAssembling)
	{
		return;
	}

	AssemblyElapsed += DeltaSeconds;
	const float Whole = FMath::Max(0.01f, AssemblySeconds);
	const float Global = FMath::Clamp(AssemblyElapsed / Whole, 0.0f, 1.0f);

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FSpaceportPart& P = Parts[i];
		const float Span = FMath::Max(0.02f, P.RiseFor);
		// Each part reads the global clock through its own window, so retuning
		// AssemblySeconds re-times the whole structure without desynchronising it.
		const float Local = FMath::Clamp((Global - P.RiseAt) / Span, 0.0f, 1.0f);
		ApplyPartProgress(i, Local);
	}

	if (Global >= 1.0f)
	{
		bAssembling = false;
		bAssembled = true;
		SetActorTickEnabled(false);
		UE_LOG(LogSibeliusGame, Display, TEXT("[Spaceport] assembled."));
	}
}

uint8 ASpaceport::CaptureBranchState() const
{
	// 1 == the parent's "built". Phase C adds 2 (rocket on pad) and 3 (launched) here.
	return bAssembled ? 1 : 0;
}

void ASpaceport::RestoreBranchState(uint8 InState)
{
	/* Super FIRST, so the parent's built flag, collision and nav all still change exactly
	   as they would for any other site — Deploy, Test-Drive and the save all read the
	   parent's notion of built, and a subclass that quietly stopped updating it would
	   branch and persist wrongly while looking fine on screen. */
	Super::RestoreBranchState(InState);

	if (InState == 0)
	{
		bAssembling = false;
		bAssembled = false;
		SetActorTickEnabled(false);
		ClearParts();
		return;
	}

	// SNAP, never animate. See the header: a reload must not replay the show.
	SnapAssembled();
}

void ASpaceport::OnGeneratedFresh()
{
	// The one path that DOES animate: he asked for it and he is standing there watching.
	PlayAssembly();
}

FText ASpaceport::GetInteractionPrompt_Implementation() const
{
	// Phase C gives this a rocket to offer. Until then it is scenery, and scenery that
	// advertises an E that does nothing is worse than scenery that says nothing.
	return FText::GetEmpty();
}

FVector ASpaceport::GetPadTopLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, PadTopHeight);
}
