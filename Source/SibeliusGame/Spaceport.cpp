// Spaceport.cpp — see the header for why this extends ABuildSite.

#include "Spaceport.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "DancerAgentSubsystem.h"   // tell the guides a spaceport now stands (Phase E)

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	/* THE VENDOR MESHES, BY PATH — and the cook rule they depend on.

	   Rocket Launch Pad & Interior (PackDev, bought 2026-09-01) is gitignored like every
	   vendor pack. A soft path written here is NOT a package reference and the cooker will
	   not follow it, so these reach a shipped build ONLY because DefaultGame.ini names
	   /Game/Rocket_Launch_Pad/Meshes and .../Materials in DirectoriesToAlwaysCook. Remove
	   those lines and the spaceport materialises flawlessly in PIE and is invisible to
	   every player who downloads the game. */
	const TCHAR* const P_Ground   = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Ground.SM_Ground");
	const TCHAR* const P_Base     = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Base.SM_Base");
	const TCHAR* const P_Pad      = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Launch_Pad.SM_Launch_Pad");
	const TCHAR* const P_Holder   = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Rocket_Holder.SM_Rocket_Holder");
	const TCHAR* const P_Pipes    = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Cooling_Pipes.SM_Cooling_Pipes");
	const TCHAR* const P_Details  = TEXT("/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Details.SM_Details");
	const TCHAR* const P_Rocket   = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Rocket.SM_Rocket");
	const TCHAR* const P_Walls    = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Walls.SM_Walls");
	const TCHAR* const P_BackWall = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Back_Wall.SM_Back_Wall");
	const TCHAR* const P_Iface    = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Interface.SM_Interface");
	const TCHAR* const P_Power    = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Power_Box.SM_Power_Box");
	const TCHAR* const P_Seats    = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Seats.SM_Seats");
	const TCHAR* const P_Bags     = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Storage_Bags.SM_Storage_Bags");
	const TCHAR* const P_Controls = TEXT("/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Controls.SM_Controls");

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
	/* THE ARTIST'S COMPOSITION, NOT MY ARITHMETIC.

	   The first version of this function was twelve greybox parts at positions guessed
	   from a file listing, and Walt's verdict was that it looked like hell. He was right;
	   you cannot compose a launch complex out of mesh names.

	   So these numbers are not invented. dump_launchpad_layout.py read PackDev's own
	   showcase map, L_Rocket_Launch_Pad, and reported the transform of all 41 static
	   meshes the artist placed. What follows is that data, re-centred on the launch pad
	   and with the pad's base at Z=0 so the structure stands on the lawn.

	   WHY 13 PARTS AND NOT 41. The full facility measures 217 m by 119 m — it would
	   swallow the whole of Downtown West. Most of that spread is outlying dressing:
	   floodlights 108 m out, pipe runs, two cooling towers, an observation tower 68 m
	   away, and six concrete barriers ringing a perimeter that does not exist here. The
	   core cluster — base, pad, rocket holder, cooling pipes, details, the rocket, and
	   its seven interior pieces — sits inside about 25 m and is the whole silhouette.

	   SM_Ground is deliberately excluded. It is the showcase map's own concrete apron,
	   and dropping a ground plane onto a city street would clip through the pavement the
	   player is standing on. The city keeps its own ground.

	   THE ROCKET IS ~120 M TALL. The interior sits at Z 11 900, a hundred metres above
	   its own base — Saturn V scale, and correct. That is why PadTopHeight is 1433: the
	   rocket's origin, where Phase C stands its physics body.

	   Everything is EditAnywhere. Tuning is dragging numbers with the level open. */
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

	/* THE ORDER IS THE ORDER IT APPEARS, and it is dramatic rather than structural: the
	   ground works first, the machinery grows on it, and the rocket itself arrives LAST
	   and slowest, so the thing the player came to see is the thing that finishes. */
	const FRotator Flat = FRotator::ZeroRotator;
	const FVector One = FVector::OneVector;

	/* --- the ground works ---------------------------------------------------
	   SM_Ground FIRST, and it is back after being cut. The reasoning for excluding it was
	   that a ground plane dropped on a city street would clip the pavement — true, but it
	   ignored what it is FOR. PackDev's launch pad is ELEVATED, the way a real one is,
	   with a flame trench beneath it; their apron is what fills that space. Without it the
	   pad stands on legs over open daylight and the player walks underneath, which is what
	   Walt found: "platform is above ground in front of me".

	   It is 45 metres out on grass rather than on the road, so the clipping worry it was
	   cut for does not arise there. If it ever does, the fix is moving the spaceport, not
	   removing the floor from under it. */
	Add(P_Ground,  FVector(2212.7f,    0.2f,  -95.4f),
		FRotator(0.0f, 90.0f, 0.0f), One, 0.00f, 0.18f);
	Add(P_Base,    FVector(2410.6f,    0.0f,    0.0f), Flat, One, 0.04f, 0.22f);
	Add(P_Pad,     FVector(   0.0f,    0.0f,  700.3f), Flat, One, 0.06f, 0.24f);
	Add(P_Details, FVector(1665.6f, 1757.5f,  651.7f), Flat, One, 0.18f, 0.20f);

	// --- the machinery ------------------------------------------------------
	Add(P_Pipes,   FVector(   0.0f,  128.5f, 1128.9f), Flat, One, 0.26f, 0.22f);
	Add(P_Holder,  FVector(   0.0f,  -73.7f, 1389.1f), Flat, One, 0.34f, 0.26f);

	/* --- the rocket, last and slowest --------------------------------------
	   Its RiseFor is the longest in the list on purpose: 120 metres of hull fading in
	   over a third of the whole sequence is the shot, and rushing it wastes the asset. */
	Add(P_Rocket,  FVector(   0.0f,  360.2f, 1433.2f),
		FRotator(0.0f, 0.96f, 0.0f), One, 0.48f, 0.40f);

	/* --- the crew compartment, inside the nose ------------------------------
	   A hundred metres up and invisible from the lawn, so this is not for looking at
	   today. It is here because the pack ships it, it costs seven components, and the
	   plan's own seed — "the eighth launch as the third door" — needs somewhere for
	   Leonard to sit if he ever rides one. Scale 1.352 is the artist's, not a guess. */
	const FVector InteriorScale(1.352f, 1.352f, 1.352f);
	const FRotator InteriorRot(0.0f, -52.28f, 0.0f);
	const FVector InteriorAt(-19.6f, 314.1f, 11900.5f);

	Add(P_Walls,    InteriorAt, InteriorRot, InteriorScale, 0.70f, 0.20f);
	Add(P_BackWall, InteriorAt, FRotator(0.0f, -52.94f, 0.0f), InteriorScale, 0.72f, 0.20f);
	Add(P_Iface,    InteriorAt, InteriorRot, InteriorScale, 0.74f, 0.18f);
	Add(P_Power,    InteriorAt, InteriorRot, InteriorScale, 0.76f, 0.18f);
	Add(P_Bags,     InteriorAt, InteriorRot, InteriorScale, 0.78f, 0.18f);
	Add(P_Seats,    FVector(-52.8f, 294.4f, 11900.5f),
		FRotator(0.0f, -60.22f, 0.0f), InteriorScale, 0.80f, 0.18f);
	Add(P_Controls, FVector(-62.0f, 287.2f, 11930.0f), InteriorRot, One, 0.82f, 0.18f);
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
			// Still take a slot in every parallel array. Skipping one here would shift
			// every later part's materials onto the wrong component — a far stranger bug
			// than the missing mesh that caused it.
			PartComponents.Add(nullptr);
			OriginalMaterials.AddDefaulted();
			MaterialiseMIDs.Add(nullptr);
			PartIsMaterialising.Add(false);
			continue;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetStaticMesh(Mesh);
		Comp->SetupAttachment(SceneRoot);
		// Created inert. ApplyPartProgress turns visibility and collision on together;
		// see the note there on why a hidden solid is a bug and not an optimisation.
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetHiddenInGame(true);
		Comp->RegisterComponent();
		PartComponents.Add(Comp);
		OriginalMaterials.AddDefaulted();
		MaterialiseMIDs.Add(nullptr);
		PartIsMaterialising.Add(false);

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
	// All four together, always. They are indexed in lockstep, so a reset that missed one
	// would leave stale materials pointed at components that no longer exist.
	PartComponents.Reset();
	OriginalMaterials.Reset();
	MaterialiseMIDs.Reset();
	PartIsMaterialising.Reset();
}

UMaterialInterface* ASpaceport::GetMaterialiseMaterial()
{
	// On demand, in a running world. Never a constructor: see the header.
	if (MaterialiseMaterial.IsNull())
	{
		MaterialiseMaterial = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/AIApparition/M_materialise.M_materialise")));
	}
	return MaterialiseMaterial.LoadSynchronous();
}

void ASpaceport::DressPart(int32 Index, bool bMaterialise)
{
	if (!PartComponents.IsValidIndex(Index) || !PartComponents[Index]
		|| !PartIsMaterialising.IsValidIndex(Index))
	{
		return;
	}
	if (PartIsMaterialising[Index] == bMaterialise)
	{
		return;   // already dressed this way — capturing twice would lose the originals
	}

	UStaticMeshComponent* Comp = PartComponents[Index];

	if (bMaterialise)
	{
		UMaterialInterface* Ghost = GetMaterialiseMaterial();
		if (!Ghost)
		{
			/* NO GHOST MATERIAL = NO FADE, BUT STILL A SPACEPORT. It pops in, which is
			   what Walt did not want, and that is much better than a spaceport that never
			   becomes visible because the effect failed silently. Says which asset. */
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Spaceport] no materialise material - parts will appear instantly. "
				     "Run Tools/Scripts/build_materialise_material.py."));
			return;
		}

		// Capture the mesh's own materials ONCE, before anything replaces them.
		FSpaceportPartMaterials& Saved = OriginalMaterials[Index];
		Saved.Slots.Reset();
		const int32 SlotCount = Comp->GetNumMaterials();
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			Saved.Slots.Add(Comp->GetMaterial(Slot));
		}

		// One instance drives every slot, so a mesh with six material slots still fades
		// as a single object rather than six overlapping ghosts at different opacities.
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Ghost, this);
		MaterialiseMIDs[Index] = MID;
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("Glow"), MaterialiseGlow);
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			for (int32 Slot = 0; Slot < SlotCount; ++Slot)
			{
				Comp->SetMaterial(Slot, MID);
			}
		}
		PartIsMaterialising[Index] = true;
		return;
	}

	// SOLIDIFY: hand the real materials back, slot for slot.
	const FSpaceportPartMaterials& Saved = OriginalMaterials[Index];
	for (int32 Slot = 0; Slot < Saved.Slots.Num(); ++Slot)
	{
		Comp->SetMaterial(Slot, Saved.Slots[Slot]);
	}
	MaterialiseMIDs[Index] = nullptr;
	PartIsMaterialising[Index] = false;
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

	/* THE FADE ITSELF — three states, and the order matters.

	   Alpha 0        not there at all: hidden, still solid-materialled, nothing to see.
	   0 < Alpha < 1  wearing the apparition, Opacity ramping — it forms out of the air.
	   Alpha 1        real materials back. The swap lands at full opacity, so the only
	                  visible change is the cyan glow leaving; nothing blinks. */
	if (Alpha <= 0.0f)
	{
		DressPart(Index, false);
	}
	else if (Alpha < 1.0f)
	{
		DressPart(Index, true);
		if (UMaterialInstanceDynamic* MID = MaterialiseMIDs.IsValidIndex(Index)
			? MaterialiseMIDs[Index].Get() : nullptr)
		{
			MID->SetScalarParameterValue(TEXT("Opacity"), Eased);
			// The glow fades out as it solidifies, so the material swap at the top is
			// the quietest moment of the effect rather than the loudest.
			MID->SetScalarParameterValue(TEXT("Glow"), MaterialiseGlow * (1.0f - Eased));
		}
	}
	else
	{
		DressPart(Index, false);
	}

	/* RELATIVE, and it is safe BECAUSE these are not the root. SceneRoot is the root
	   component (ABuildSite sets it explicitly, for the CP3 reason), so "relative" here
	   really is relative to the actor. Calling SetRelativeLocation on a root means WORLD,
	   which is what teleported three coffee cups to the origin in 1.0. */
	Comp->SetRelativeLocation(Where);
	Comp->SetRelativeRotation(P.Rotation);
	Comp->SetRelativeScale3D(P.Scale);

	/* COLLISION FOLLOWS VISIBILITY, ALWAYS — the CoffeeCup lesson, and a real bug until
	   this line existed.

	   Components were created QueryAndPhysics and hidden, so an ASpaceport sitting at
	   branch state 0, or one restored from a save, was thirteen INVISIBLE SOLID OBJECTS
	   on the lawn — including a hundred-and-twenty-metre rocket. Walking into that reads
	   exactly as "W stopped working", with nothing on screen to explain it.

	   A half-formed ghost is not solid either: you cannot lean on something that has not
	   finished arriving. Collision switches on only at full opacity, when the part stops
	   being an apparition and becomes a building. */
	const bool bVisible = Alpha > 0.0f;
	const bool bSolid = Alpha >= 1.0f;
	Comp->SetHiddenInGame(!bVisible);

	/* NEVER TURN SOLID AROUND THE PLAYER (Walt, 2026-09-02, the second time).

	   Making collision follow visibility fixed invisible walls, and introduced this: if he
	   is standing where a part finishes materialising, the part becomes solid AROUND him
	   and physics embeds him in it. Mouse-look still works, WASD does nothing, and there
	   is blackness overhead — the inside of a mesh. That is exactly what he reported, and
	   in a packaged build there is no Stop button to escape it.

	   So a part that would close on him stays intangible and is retried; PokeDeferredParts
	   turns it solid the moment he steps clear. A spaceport briefly walk-through where he
	   happens to be standing is a far smaller sin than a spaceport he is sealed inside.

	   The bounding BOX is deliberately generous rather than accurate. A gantry's box
	   contains a great deal of air, so this defers more often than strictly necessary —
	   and every false positive costs a moment of non-solidity, while every false negative
	   costs the player his game. */
	bool bDefer = false;
	if (bSolid)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FBox Box = Comp->Bounds.GetBox().ExpandBy(120.0f);
			bDefer = Box.IsInside(Player->GetActorLocation());
		}
	}

	Comp->SetCollisionEnabled((bSolid && !bDefer) ? ECollisionEnabled::QueryAndPhysics
	                                              : ECollisionEnabled::NoCollision);

	if (bDefer)
	{
		bHasDeferredCollision = true;
	}
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

		// Anything that stayed intangible because he was standing in it gets retried
		// twice a second until he moves. Cheap, and it stops itself.
		if (bHasDeferredCollision && GetWorld())
		{
			GetWorldTimerManager().SetTimer(DeferredCollisionTimer, this,
				&ASpaceport::PokeDeferredParts, 0.5f, true);
		}
	}
}

void ASpaceport::PokeDeferredParts()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	bool bStillWaiting = false;

	for (int32 i = 0; i < PartComponents.Num(); ++i)
	{
		UStaticMeshComponent* Comp = PartComponents[i];
		if (!Comp || Comp->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			continue;   // already solid, or never had a mesh
		}

		const bool bClear = !Player
			|| !Comp->Bounds.GetBox().ExpandBy(120.0f).IsInside(Player->GetActorLocation());
		if (bClear)
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			bStillWaiting = true;
		}
	}

	if (!bStillWaiting)
	{
		bHasDeferredCollision = false;
		GetWorldTimerManager().ClearTimer(DeferredCollisionTimer);
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Spaceport] player clear - every part is solid."));
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

	/* AND THAT IS EXACTLY WHY THE GUIDE IS TOLD NOW (docs/SPACEPORT_PLAN.md Phase E).

	   Nyra's stage 2 is "a spaceport stands", and she has to get to the lawn's edge
	   without walking — L_City has no navmesh. The assembly above runs for
	   AssemblySeconds with the player watching a launch complex build itself 160 metres
	   away, which is the one window in this game where his attention is somewhere else by
	   construction rather than by luck.

	   She still checks his view cone and waits if he is looking, so this is a good moment
	   offered rather than a teleport demanded. */
	if (const UWorld* World = GetWorld())
	{
		if (UDancerAgentSubsystem* Dancers = World->GetSubsystem<UDancerAgentSubsystem>())
		{
			Dancers->RestageGuides();
		}
	}
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
