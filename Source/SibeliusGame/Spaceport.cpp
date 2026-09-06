// Spaceport.cpp — see the header for why this extends ABuildSite.

#include "Spaceport.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "DancerAgentSubsystem.h"   // tell the guides a spaceport now stands (Phase E)

// Boarding ("C", the pre-flight compile) — see the bottom of this file.
#include "SupplyCounter.h"          // HasSupplies() - the gate, asked where it is answered
#include "SibeliusHUD.h"            // Toast, and every refusal names the next move
#include "Camera/CameraActor.h"     // boarding moves the VIEW, not the body
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"   // ...and the view has to carry its own light
#include "HAL/IConsoleManager.h"              // sib.BoardingLight, so brightness needs no rebuild
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"            // TActorIterator, for FindForPlayer
#include "TravelTransitionSubsystem.h"        // C at the open portal leaves for Grok
#include "NiagaraFunctionLibrary.h"
#include "Components/InputComponent.h"        // skip keys during the launch cutscene
#include "InputCoreTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"       // hard-ref the plume so it cooks

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // GEngine, for FindForPlayer's world lookup
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

	/* THE CABIN LAMP, TUNABLE FROM THE CONSOLE - because it was black, then blown out, and
	   each correction otherwise costs a full editor-closed rebuild.

	   Type "sib.BoardingLight 500" in the Cmd box, press C twice, and the next boarding uses
	   it: Disembark destroys the view actor, so re-boarding builds a fresh lamp that reads
	   this again. Negative means "use the property", which is the shipping default. */
	/* THE PORTAL, SIZED BY EYE. Same lesson as the boarding lamp and sib.WormholeFX: how big
	   a doorway should look across a field is not a question code can answer, and a rebuild
	   per guess is not a way to answer it. Travel out and back to re-open it with new
	   values - the reload path opens it immediately. Negative = the actor's own. */
	/* WHICH SYSTEM, AND WHICH WAY UP.

	   NS_TeleporterHole is almost certainly a HOLE - a flat disc meant to lie on the ground
	   and be stepped into. Seen from 160 m at eye level a horizontal disc is edge-on and
	   effectively invisible, which is both "that tiny purple portal at the base" and "I see
	   no portal anywhere". The vertical doorways in the pack are NS_ArchGate,
	   NS_AnyWhereDoor and NS_GpuPortal.

	   This knob should have existed the first time - sib.WormholeFX got one and this did
	   not, so every candidate cost a rebuild. Bare name resolves under /Game/PortalVFX/NS/. */
	static TAutoConsoleVariable<FString> CVarGrokPortalFX(
		TEXT("sib.GrokPortalFX"), TEXT(""),
		TEXT("Niagara system for the portal to Grok. Bare name (NS_ArchGate) or full path."),
		ECVF_Cheat);

	/** Stand a flat one up: 90 puts a ground disc vertical. Negative = leave it alone. */
	/* HOW FAR DOWN THE FIELD IT STANDS, from the pad toward the player.

	   Walt: "can you please move that big portal closer to the fence?" It opened AT the
	   spaceport, which the catalog plants 160 m ahead of wherever he typed - so the door
	   was on the wrong side of a fence he cannot cross, which is the same geography problem
	   that has bitten every interaction with this thing.

	   Measured from the pad TOWARD him rather than at a fixed local offset, because the
	   spaceport's own rotation is whatever his aim was and "toward the street" is not a
	   direction this actor knows. Clamped to 80% of the gap so it can never overshoot past
	   him and open behind his back. */
	static TAutoConsoleVariable<float> CVarGrokPortalToward(
		TEXT("sib.GrokPortalToward"), -1.0f,
		TEXT("Cm from the pad toward the player where the portal opens. Negative = the actor's own."),
		ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarGrokPortalPitch(
		TEXT("sib.GrokPortalPitch"), -1.0f,
		TEXT("Pitch in degrees for the Grok portal. 90 stands a ground disc up. Negative = none."),
		ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarGrokPortalScale(
		TEXT("sib.GrokPortalScale"), -1.0f,
		TEXT("Scale of the portal to Grok. Negative = the actor's own."), ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarGrokPortalUp(
		TEXT("sib.GrokPortalUp"), -1.0f,
		TEXT("Height of the portal above the spaceport origin, cm. Negative = the actor's own."), ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarBoardingLight(
		TEXT("sib.BoardingLight"), -1.0f,
		TEXT("Crew-compartment lamp brightness in lumens. Negative = use ASpaceport's own value."),
		ECVF_Cheat);

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

	/* HARD-REF THE PLUME so the cooker follows it. Soft path alone is the v0.7.4 miss.
	   The plugin template is the same gas the sauce cauldron uses; launch retints it
	   orange and points it at the pad. A game copy at /Game/Cinematics/NS_RocketPlume
	   is preferred at runtime if build_launch_shot.py has run. */
	/* THE PORTAL TO GROK, hard-referenced for the same reason as the plume: a soft path
	   from C++ is not a package reference, and Content/PortalVFX/ is a gitignored purchased
	   pack. Soft-only, it would open perfectly in PIE and not exist in the shipped build. */
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PortalFinder(
		TEXT("/Game/PortalVFX/NS/NS_TeleporterHole.NS_TeleporterHole"));
	if (PortalFinder.Succeeded())
	{
		GrokPortalSystem = PortalFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PlumeFinder(
		TEXT("/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_ColoredSmoke.Grid3D_Gas_ColoredSmoke"));
	if (PlumeFinder.Succeeded())
	{
		PlumeSystem = PlumeFinder.Object;
	}
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

void ASpaceport::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLaunching)
	{
		BindLaunchSkip(false);
		if (APlayerController* PC = LaunchPawn ? Cast<APlayerController>(LaunchPawn->GetController()) : nullptr)
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
			{
				HUD->ReleaseCinematic();
			}
		}
		DestroyLaunchRig();
		bLaunching = false;
	}
	Super::EndPlay(EndPlayReason);
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
	StartBoardingHintWatch();   // he may have come back from uFoods with supplies
}

void ASpaceport::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);   // the parent still owns its own float-and-spin reveal

	if (bLaunching)
	{
		TickLaunch(DeltaSeconds);
		return;
	}

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

		StartBoardingHintWatch();   // supplies already bought? then the pad can say so

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
	// 1 == the parent's "built". 3 == launched (pad empty). 2 stays reserved for
	// RocketOnPad once a fresh hull can sit on a used pad.
	if (bLaunched)
	{
		return 3;
	}
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
		/* DISCARDING A SPACEPORT HE IS STANDING IN.

		   Test-Drive can branch a spaceport and throw it away, which is the plan's best
		   mechanic and which now has a case it did not have before: he can be 119 metres up
		   inside it when the branch is discarded.  The parts vanish, the boarding floor does
		   not (it is not one of them), and he is left standing on nothing, in the sky, above
		   a bare lawn.  Put him back on the grass first. */
		if (bAboard)
		{
			Disembark(UGameplayStatics::GetPlayerPawn(this, 0));
		}

		bAssembling = false;
		bAssembled = false;
		bLaunching = false;
		bLaunched = false;
		SetActorTickEnabled(false);
		DestroyLaunchRig();
		ClearGrokPortal();
		ClearParts();
		return;
	}

	// SNAP, never animate. See the header: a reload must not replay the show.
	// Flag launched BEFORE SnapAssembled so the boarding-hint watch does not arm
	// on a pad whose hull is about to vanish.
	if (InState >= 3)
	{
		bLaunched = true;
	}
	SnapAssembled();
	if (bLaunched)
	{
		HideFlightParts();
		// He is returning to a pad the ship already left. The door is standing, not opening.
		OpenGrokPortal(/*bImmediate=*/true);
	}
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


/* ===================================================================================
   BOARDING — "C", the pre-flight compile.  See the header for why it lives here.
   =================================================================================== */

ASpaceport* ASpaceport::FindForPlayer(const UObject* WorldContext)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);

	ASpaceport* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();

	for (TActorIterator<ASpaceport> It(World); It; ++It)
	{
		ASpaceport* Port = *It;
		if (!IsValid(Port))
		{
			continue;
		}

		/* THE ONE HE IS STANDING IN WINS OUTRIGHT, before any distance is measured.
		   From inside the nose he is 119 metres above this actor's origin, so a nearest
		   test would rank his own spaceport last — and C, the only way back out, would
		   answer some other spaceport across the lawn instead. */
		if (Port->IsAboard())
		{
			return Port;
		}

		if (!Port->IsAssembled() || !Player)
		{
			continue;
		}

		// 2D: the structure is 120 m tall and he is always standing at the bottom of it.
		const double D = FVector::DistSquared2D(Player->GetActorLocation(), Port->GetActorLocation());
		if (D < BestDistSq)
		{
			BestDistSq = D;
			Best = Port;
		}
	}

	return Best;
}

/* HOW FAR HE IS FROM THE SPACEPORT — measured to the STRUCTURE, not to the actor.

   The first version asked FVector::Dist2D against GetActorLocation(), and that is wrong
   in a way that is invisible in the code and total in the game.  The actor's origin is the
   LAUNCH PAD.  SM_Ground, the concrete apron, is centred 22 metres away in +X and is
   itself tens of metres across; the base is 24 metres out.  So a player standing squarely
   ON the apron, looking up at the rocket, with every part of the complex around him, can
   easily be more than 30 metres from the one point the test was measuring to — and both
   the hint and the key go silent at exactly the moment they are most obviously wanted.

   Measuring to the assembled BOUNDS instead makes BoardingRange mean what its comment
   always claimed: how far from the EDGE of the complex.  Standing on it returns 0, and the
   number stops depending on where in the layout the artist happened to put the origin. */
float ASpaceport::BoardingLampLumens() const
{
	const float Override = CVarBoardingLight.GetValueOnGameThread();
	return (Override >= 0.0f) ? Override : BoardingLightLumens;
}

float ASpaceport::DistanceToStructure(const FVector& From) const
{
	FBox Box(ForceInit);
	for (const UStaticMeshComponent* Comp : PartComponents)
	{
		// Hidden parts are ones that have not arrived yet; they are not the building.
		if (IsValid(Comp) && !Comp->bHiddenInGame)
		{
			Box += Comp->Bounds.GetBox();
		}
	}

	// No parts (not assembled, or cleared) — fall back to the origin rather than to zero,
	// which would read as "he is standing on it".
	if (!Box.IsValid)
	{
		return FVector::Dist2D(From, GetActorLocation());
	}

	const double DX = FMath::Max(0.0, FMath::Max(Box.Min.X - From.X, From.X - Box.Max.X));
	const double DY = FMath::Max(0.0, FMath::Max(Box.Min.Y - From.Y, From.Y - Box.Max.Y));
	return static_cast<float>(FMath::Sqrt(DX * DX + DY * DY));
}

/* THE PRE-FLIGHT.

   RETURNS TRUE ONLY WHEN THE PRESS WAS THIS SPACEPORT'S TO ANSWER, because C is the
   Compile key and Compile has other work.  A press from across the lawn has to fall
   through to UBuildComponent so that ordinary build sites still build — a spaceport
   standing 160 metres away must not silently eat the verb everywhere in the city.

   AND WHY EVERY REFUSAL NAMES THE NEXT MOVE: GoToCity's rule, applied again — "a locked
   key that says locked teaches nothing."  Each sentence below says where to go, not that
   he cannot go.  The one silence is a spaceport still assembling: there is nothing to do
   about that but watch, and a toast telling him to wait eight seconds would be noise. */
bool ASpaceport::PreflightCompile(APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return false;
	}

	// FIRST, before anything measures a distance. See FindForPlayer.
	if (bLaunching)
	{
		SkipLaunch();
		return true;
	}

	/* THE SHIP HAS GONE, AND C NOW MEANS THE PORTAL.

	   This whole branch used to be two: a bare "that ship is already away" here, and a
	   portal check further down that could never run because this returned first. Walt
	   walked to an OPEN portal, pressed C, and was told the ship had left - which was true,
	   unhelpful, and the entire point of the portal standing there.

	   Merged, so there is exactly one place that answers C after a launch.

	   AND THE RANGE IS MEASURED TO THE PORTAL, not to the structure. HideFlightParts()
	   removes the rocket once the ship is away, and DistanceToStructure ignores hidden
	   parts - so the bounds collapse to the ground works at the very moment the portal
	   becomes the only thing worth walking to. Ask the door where the door is. */
	if (bLaunched)
	{
		if (GrokPortal)
		{
			/* THE NEARER OF THE TWO, and the point measurement alone was a regression.

			   Switching from DistanceToStructure to a straight line to the portal made this
			   STRICTER, not looser: the assembled bounds are enormous (the ground apron
			   alone spans ~45 m) and reach the player at the fence, while the portal is a
			   single point 160 m out - which is exactly the SpawnAhead=16000 the catalog
			   plants the spaceport at. Walt: "I got the 160 meters away message and walked
			   up to the fence and I see no portal anywhere."

			   Boarding worked from that fence because the bounds came to him. So does this,
			   now: whichever of the two is nearer. */
			const float ToPortal = FMath::Min(
				FVector::Dist2D(Pawn->GetActorLocation(), GrokPortal->GetComponentLocation()),
				DistanceToStructure(Pawn->GetActorLocation()));
			if (ToPortal <= GrokPortalRange)
			{
				UE_LOG(LogSibeliusGame, Display,
					TEXT("[Spaceport] Entering the portal to %s (%.0f cm)."),
					*GrokLevelName.ToString(), ToPortal);
				UTravelTransitionSubsystem::Travel(this, GrokLevelName);
				return true;
			}

			/* AND SAY HOW FAR, because "walk to it" was unhelpful advice at a portal behind
			   a fence. With the range now matching boarding this branch should be rare. */
			ASibeliusHUD::Toast(this, FString::Printf(
				TEXT("THE WAY TO GROK IS OPEN WHERE THE SHIP STOOD - %.0f M AWAY"),
				ToPortal / 100.0f), 4.0f, SibeliusToast::Info);
			return true;
		}

		// Launched, but the portal has not opened yet - the toast is still playing.
		ASibeliusHUD::Toast(this,
			TEXT("COMPILE ERROR: THAT SHIP IS ALREADY AWAY"),
			4.0f, SibeliusToast::Warn);
		return true;
	}

	if (bAboard)
	{
		/* C WHILE ABOARD IS IGNITION. Disembark was the way out only while there was
		   nothing to press; Nyra's boarding procedures end in a launch, and Compile is
		   the verb that runs the program. Space / Enter / Escape skip the shot. */
		if (!BeginLaunch(Pawn))
		{
			Disembark(Pawn);
		}
		return true;
	}

	if (!bAssembled)
	{
		return false;
	}

	const float Away = DistanceToStructure(Pawn->GetActorLocation());
	if (Away > BoardingRange)
	{
		/* SELF-DIAGNOSING, but only near the pad — a Display line on every C press in the
		   city would be noise, and this silence was the whole of the first bug report
		   ("I was not prompted to press C at the spaceport.  I pressed it anyway with no
		   result").  A refusal a player cannot see should at least be one the log can. */
		if (Away < BoardingRange * 4.0f)
		{
			UE_LOG(LogSibeliusGame, Display,
				TEXT("[Spaceport] Pre-flight out of range: %.0f cm from the structure ")
				TEXT("(BoardingRange %.0f). Supplies: %s."),
				Away, BoardingRange, ASupplyCounter::HasSupplies(this) ? TEXT("yes") : TEXT("NO"));
		}
		return false;   // not ours; let the build sites have it
	}

	/* THE COMPILE ERROR THAT EXISTS TODAY.

	   The plan's row for this key is "a design with TWR < 1 or an empty tank is a compile
	   error".  Phase C's physics will supply those two.  Until it does, the empty tank IS
	   the empty hold: Nyra sent him down the block for supplies, and the grant that
	   records the purchase is the one gate boarding has.  Same shape of failure, same
	   sentence structure — what is wrong, and where to go and fix it. */
	if (!ASupplyCounter::HasSupplies(this))
	{
		ASibeliusHUD::Toast(this,
			TEXT("COMPILE ERROR: NOTHING IN THE HOLD - BUY SUPPLIES AT THE YOU FOODS DOWN THE BLOCK"),
			5.0f, SibeliusToast::Warn);
		return true;
	}

	if (!Board(Pawn))
	{
		ASibeliusHUD::Toast(this,
			TEXT("COMPILE ERROR: NO CLEAR SPACE IN THE CREW COMPARTMENT"),
			4.0f, SibeliusToast::Bad);
	}
	return true;
}

/* BOARDING SHOWS HIM THE INTERIOR. IT DOES NOT PUT HIS BODY IN IT.

   Walt, after the fourth attempt: "let's not try to fit that stupid Greystone body in a
   little capsule.  Just show the interior and say he has boarded."

   He is right, and the log is the proof.  The crew compartment's parts span 10 m x 10 m x
   3.8 m, but that is the UNION of seven props - the free volume inside them will not take
   a 96 cm capsule anywhere, at any height on the grid.  SM_Seats alone is 6.4 m x 4.1 m in
   a 7.4 m room.  It is a set dressed to be LOOKED at, not a room built to be stood in, and
   every attempt to stand a character in it was arguing with the asset.

   So the pawn never moves.  He stays exactly where he pressed C, and the CAMERA goes
   aboard: a view target inside the compartment, movement and look held while it is there,
   and C again brings the view home.  Nothing to collide with, no floor to lay, no offset
   to guess - and on screen it is the same beat Walt asked for in the first place,
   "suddenly he is inside the ship".

   A camera is also a POINT, which is the part that finally makes the search work.  The
   same grid that could not fit a capsule anywhere finds a spot for a 12 cm probe easily,
   so the view is still guaranteed to be inside the room and not embedded in the seats. */
bool ASpaceport::Board(APawn* Pawn)
{
	UWorld* World = GetWorld();
	APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!World || !PC)
	{
		return false;
	}

	// The room, in world space: the union of the seven interior pieces. Past this is the
	// hull, or the sky - and open air beside the hull passes an overlap test perfectly,
	// which is exactly where the first three attempts kept putting him.
	FBox Room(ForceInit);
	for (int32 i = FirstInteriorPart; i < PartComponents.Num(); ++i)
	{
		if (IsValid(PartComponents[i]))
		{
			Room += PartComponents[i]->Bounds.GetBox();
		}
	}
	if (!Room.IsValid)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Spaceport] Boarding refused: no interior parts."));
		return false;
	}

	/* WHERE THE CAMERA SITS.  A 12 cm probe, not a capsule - it only has to avoid being
	   buried in a mesh, and looking out from inside SM_Seats is the one result worth
	   ruling out.  Searched outward from BoardingOffset and preferring near and low, so
	   the view lands in the middle of the room rather than in a corner of it. */
	const FCollisionShape Probe = FCollisionShape::MakeSphere(12.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpaceportBoarding), false, Pawn);

	FVector Eye = Room.GetCenter();
	double BestScore = TNumericLimits<double>::Max();
	bool bFound = false;

	for (int32 iz = 0; iz <= 3; ++iz)
	{
		for (int32 ix = -5; ix <= 5; ++ix)
		{
			for (int32 iy = -5; iy <= 5; ++iy)
			{
				const FVector Try = GetActorTransform().TransformPosition(
					BoardingOffset + FVector(ix * 70.0f, iy * 70.0f, 120.0f + iz * 60.0f));

				if (!Room.IsInsideOrOn(Try))
				{
					continue;
				}
				if (World->OverlapBlockingTestByChannel(Try, FQuat::Identity, ECC_Visibility, Probe, Params))
				{
					continue;
				}

				const double Score = double(ix * ix + iy * iy) + double(iz * iz) * 4.0;
				if (Score < BestScore)
				{
					BestScore = Score;
					Eye = Try;
					bFound = true;
				}
			}
		}
	}

	// Even with nothing free, the room's centre is a better view than a refusal: this is a
	// camera, and the worst case is a frame of upholstery, not a player stuck in a wall.
	if (!bFound)
	{
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Spaceport] No clear camera spot; using the room centre. Room %s."),
			*Room.GetSize().ToCompactString());
	}

	if (!BoardingView)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = this;
		Spawn.ObjectFlags |= RF_Transient;   // a view is not something a save should carry
		BoardingView = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Spawn);

		/* AND A LIGHT, BECAUSE THE COMPARTMENT IS SEALED (Walt: "yikes, it is extremely
		   dark in there").

		   Of course it is.  It is the inside of a hull 119 metres up: the level's sun is
		   outside it, the set was authored to be lit by whatever scene it is dropped into,
		   and this one drops it into the dark.  The first boarding shot had a working
		   composition - the instrument panel and the altimeter dead ahead - and no way to
		   see it.

		   Carried by the camera, so it lights what he is actually looking at and dies with
		   the view when he steps back out.  Offset ABOVE the eye rather than sitting on it,
		   because a light exactly at the camera is a flash photograph: flat, shadowless,
		   and it would iron out the one panel worth seeing.

		   NO SHADOW CASTING.  A single unshadowed light is cheap, and its only cost here is
		   bleeding a little glow onto the hull within its radius - 10 m, inside a rocket
		   23 m across, for as long as he is aboard and nobody is outside looking at it. */
		if (BoardingView)
		{
			UPointLightComponent* Lamp = NewObject<UPointLightComponent>(BoardingView, TEXT("BoardingLamp"));
			Lamp->SetupAttachment(BoardingView->GetRootComponent());
			Lamp->RegisterComponent();
			Lamp->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
			Lamp->SetIntensityUnits(ELightUnits::Lumens);
			Lamp->SetIntensity(BoardingLampLumens());
			Lamp->SetAttenuationRadius(BoardingLightRadius);
			Lamp->SetLightColor(FLinearColor(1.0f, 0.94f, 0.86f));   // warm, like cabin lighting
			Lamp->SetCastShadows(false);
		}
	}
	if (!BoardingView)
	{
		return false;
	}

	BoardingView->SetActorLocationAndRotation(
		Eye, FRotator(0.0f, GetActorRotation().Yaw + BoardingYaw, 0.0f));

	/* HOLD THE CONTROLS WHILE HE IS ABOARD.  The pawn is still standing on the lawn, and
	   without this he walks around out there while looking at the inside of a rocket. */
	PC->SetViewTargetWithBlend(BoardingView, 0.0f);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);

	bAboard = true;
	FadeIn(Pawn);

	/* AND SHE IS ALREADY THERE, because she said she would be:
	   "I will upload myself into the spaceship computer and I will be going with you!"

	   A toast rather than her voice, for now — every other line she speaks is a recorded
	   clip and this one is not cut yet.  docs/SPACEPORT_PLAN.md carries it as the next
	   ElevenLabs pass.

	   THE SECOND HALF OF THE SENTENCE IS THE VERB.  C boarded him; C again is ignition.
	   A player told only "aboard" is a player hunting for a button that does not exist. */
	ASibeliusHUD::Toast(this,
		TEXT("ABOARD - NYRA IS IN THE SHIP'S COMPUTER   (C TO LAUNCH)"),
		6.0f, SibeliusToast::Good);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Spaceport] Aboard; view at local %s."),
		*GetActorTransform().InverseTransformPosition(Eye).ToCompactString());
	return true;
}

void ASpaceport::Disembark(APawn* Pawn)
{
	bAboard = false;

	APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	// He never left the lawn, so coming back is giving him his eyes and his legs again.
	PC->SetViewTargetWithBlend(Pawn, 0.0f);
	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);

	if (BoardingView)
	{
		BoardingView->Destroy();
		BoardingView = nullptr;
	}

	FadeIn(Pawn);
	if (!bLaunched)
	{
		ASibeliusHUD::Toast(this, TEXT("BACK ON THE LAWN."), 3.0f, SibeliusToast::Info);
	}
}

/* A FADE UP, NOT A FADE OUT AND BACK.

   Walt's words were "suddenly he is inside the ship", and suddenly is the point.  Fading
   OUT first would need a timer and would turn a cut into a transition.  Fading up from
   black on the far side reads as a cut that landed, and costs one call. */
void ASpaceport::FadeIn(APawn* Pawn) const
{
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.45f, FLinearColor::Black,
				/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
		}
	}
}

/* ===================================================================================
   TELLING HIM THE KEY EXISTS.

   A mechanic nobody can find is not built.  Nyra's stage 3 sends him back to the
   spaceport — "we will do the boarding procedures" — and then the game goes quiet: he is
   standing on the pad with supplies in hand and nothing on screen saying which of his six
   powers to try.

   So the pad says it, once, the first time he walks up to it ready.  A one-second timer
   rather than Tick, because this is a proximity poll on a finished building and it would
   be wrong to hold an eight-second assembly's tick open for the rest of the level to run
   it — the same reasoning as PokeDeferredParts, which is the neighbour it sits beside.

   It arms itself only when he can actually act on it (assembled AND supplies bought), and
   it stops itself the moment it has spoken.  A player who never buys supplies never sees
   it, which is right: the sentence he needs THEN is the compile error, and that one comes
   from pressing the key.
   =================================================================================== */
void ASpaceport::StartBoardingHintWatch()
{
	if (bBoardingHintShown || !bAssembled || bLaunched)
	{
		return;
	}

	if (!ASupplyCounter::HasSupplies(this))
	{
		/* SAY SO, ONCE.  "No hint appeared" has two causes that look identical from the
		   lawn - he never bought supplies, or he did and the watch never armed - and one
		   line at load tells them apart without another round trip. */
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Spaceport] Boarding hint idle: no supplies bought yet."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(BoardingHintTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(BoardingHintTimer, this,
		&ASpaceport::PollBoardingHint, 1.0f, /*bLoop=*/true);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Spaceport] Boarding hint armed - watching for the player within %.0f cm."),
		BoardingRange);
}

void ASpaceport::PollBoardingHint()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bAboard || bLaunched || bLaunching || !bAssembled)
	{
		World->GetTimerManager().ClearTimer(BoardingHintTimer);
		return;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const float Away = DistanceToStructure(Player->GetActorLocation());

	/* RE-ARM WHEN HE WALKS AWAY. The hint is not once-per-save, it is once-per-approach -
	   otherwise passing within range on some other errand spends it forever. */
	if (bBoardingHintShown)
	{
		if (Away > BoardingHintRearmRange)
		{
			bBoardingHintShown = false;
		}
		return;
	}

	// A TIGHTER radius than the key: the hint should meet him at the pad, not shout across
	// the district. See BoardingHintRange.
	if (Away > BoardingHintRange)
	{
		/* AND IT SAYS HOW FAR, every five seconds, until it can stop saying it.

		   The second failure ("I did NN and went through the entire game... I was not
		   prompted") proved the watch ARMS - the log said so - and then went quiet, which
		   left exactly one unknown: the distance.  A silent out-of-range branch is a
		   branch that cannot be debugged from a log, and this one had already cost two
		   round trips of guessing.  Throttled to 5 s so walking the block is six lines,
		   not sixty, and it stops the moment the hint fires. */
		if ((PollsWhileFar++ % 5) == 0)
		{
			const FVector P = Player->GetActorLocation();
			UE_LOG(LogSibeliusGame, Display,
				TEXT("[Spaceport] Hint waiting: player %.0f cm from the structure ")
				TEXT("(range %.0f). Player %s, spaceport origin %s, parts %d."),
				Away, BoardingRange, *P.ToCompactString(),
				*GetActorLocation().ToCompactString(), PartComponents.Num());
		}
		return;
	}

	bBoardingHintShown = true;
	// Timer keeps running: it is what re-arms the hint when he walks away again.

	ASibeliusHUD::Toast(this,
		TEXT("PRE-FLIGHT READY - PRESS C TO BOARD"),
		8.0f, SibeliusToast::Prize);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Spaceport] Hint shown at %.0f cm."), Away);
}

/* ===================================================================================
   LAUNCH CUTSCENE — live cameras on THIS pad, Niagara under THIS hull.
   =================================================================================== */

namespace
{
	FRotator LookAt(const FVector& From, const FVector& To)
	{
		const FVector Delta = To - From;
		if (Delta.IsNearlyZero())
		{
			return FRotator::ZeroRotator;
		}
		return Delta.Rotation();
	}

	void TintNiagara(UNiagaraComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp)
		{
			return;
		}
		static const FName ColorNames[] = {
			TEXT("User.Color"), TEXT("Color"), TEXT("User.SmokeColor"), TEXT("Smoke Color"),
			TEXT("User.Albedo"), TEXT("Albedo"), TEXT("User.ParticleColor"), TEXT("ParticleColor"),
			TEXT("Grid3D_Gas.Color"), TEXT("User.DensityColor"), TEXT("Density Color")
		};
		for (const FName& Name : ColorNames)
		{
			Comp->SetVariableLinearColor(Name, Color);
		}
	}
}

bool ASpaceport::IsFlightPart(int32 Index) const
{
	if (!Parts.IsValidIndex(Index))
	{
		return false;
	}
	return Parts[Index].Mesh.ToSoftObjectPath().ToString().Contains(TEXT("/Meshes/Rocket/"));
}

UStaticMeshComponent* ASpaceport::FindRocketMesh() const
{
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (!IsFlightPart(i) || !PartComponents.IsValidIndex(i) || !PartComponents[i])
		{
			continue;
		}
		if (Parts[i].Mesh.ToSoftObjectPath().GetAssetName() == TEXT("SM_Rocket"))
		{
			return PartComponents[i];
		}
	}
	return nullptr;
}

void ASpaceport::AttachFlightPartsToRocket()
{
	UStaticMeshComponent* Rocket = FindRocketMesh();
	if (!Rocket)
	{
		return;
	}

	/* ONE RIG. The crew compartment is seven meshes a hundred metres up the nose. If
	   each pitched around its own origin the interior would shear out of the hull on
	   the gravity turn. Parent them to SM_Rocket with KEEP_WORLD so the cutscene moves
	   one transform. */
	for (int32 i = 0; i < PartComponents.Num(); ++i)
	{
		UStaticMeshComponent* Comp = PartComponents[i];
		if (!Comp || Comp == Rocket || !IsFlightPart(i))
		{
			continue;
		}
		Comp->AttachToComponent(Rocket, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void ASpaceport::AttachPlume()
{
	UStaticMeshComponent* Rocket = FindRocketMesh();
	if (!Rocket)
	{
		return;
	}

	UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Cinematics/NS_RocketPlume.NS_RocketPlume"),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sys)
	{
		Sys = PlumeSystem.LoadSynchronous();
	}
	if (!Sys)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Spaceport] no Niagara plume - launch will be a silent climb. ")
			TEXT("Is NiagaraFluids enabled?"));
		return;
	}

	if (!Plume)
	{
		Plume = NewObject<UNiagaraComponent>(this, TEXT("RocketPlume"));
		Plume->SetupAttachment(Rocket);
		Plume->SetAutoActivate(false);
		Plume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plume->SetCastShadow(false);
		Plume->SetMobility(EComponentMobility::Movable);
		Plume->RegisterComponent();
	}

	Plume->SetAsset(Sys);
	Plume->SetRelativeLocation(PlumeRelativeOffset);
	Plume->SetRelativeRotation(PlumeRelativeRotation);
	Plume->SetRelativeScale3D(FVector(PlumeScale));
	TintNiagara(Plume, PlumeColor);
	Plume->Activate(true);

	if (!PlumeLight)
	{
		PlumeLight = NewObject<UPointLightComponent>(this, TEXT("PlumeLight"));
		PlumeLight->SetupAttachment(Rocket);
		PlumeLight->RegisterComponent();
		PlumeLight->SetIntensityUnits(ELightUnits::Lumens);
		PlumeLight->SetCastShadows(false);
		PlumeLight->SetMobility(EComponentMobility::Movable);
	}
	PlumeLight->SetRelativeLocation(PlumeRelativeOffset + FVector(0.0f, 0.0f, 80.0f));
	PlumeLight->SetIntensity(140000.0f);
	PlumeLight->SetAttenuationRadius(7000.0f);
	PlumeLight->SetLightColor(PlumeColor);
	PlumeLight->SetVisibility(true);
}

void ASpaceport::SpawnLaunchCameras()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (ACameraActor* Cam : LaunchCams)
	{
		if (IsValid(Cam))
		{
			Cam->Destroy();
		}
	}
	LaunchCams.Reset();

	const FTransform Xf = GetActorTransform();
	auto SpawnCam = [this, World, &Xf](const FVector& LocalPos, const FVector& LocalLook, float Fov) -> ACameraActor*
	{
		const FVector WPos = Xf.TransformPosition(LocalPos);
		const FVector WLook = Xf.TransformPosition(LocalLook);
		FActorSpawnParameters Spawn;
		Spawn.Owner = this;
		Spawn.ObjectFlags |= RF_Transient;
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(WPos, LookAt(WPos, WLook), Spawn);
		if (Cam)
		{
			if (UCameraComponent* C = Cam->GetCameraComponent())
			{
				C->bConstrainAspectRatio = false;
				C->SetFieldOfView(Fov);
			}
		}
		return Cam;
	};

	/* Four exteriors. Shot 0 is the cabin camera already sitting in BoardingView.
	   Distances are Saturn-V scale: the hull is 120 m tall. */
	LaunchCams.Reset();
	LaunchCams.Add(SpawnCam(FVector(-14000.0f, -9000.0f,  2200.0f), FVector(0.0f, 360.0f,  4000.0f), 38.0f)); // wide pad
	LaunchCams.Add(SpawnCam(FVector(  3400.0f,  2800.0f,   380.0f), FVector(0.0f, 360.0f,   900.0f), 42.0f)); // engines
	LaunchCams.Add(SpawnCam(FVector(  5200.0f, -3200.0f,  5200.0f), FVector(0.0f, 360.0f,  6500.0f), 48.0f)); // track
	LaunchCams.Add(SpawnCam(FVector(-18000.0f, 11000.0f, 28000.0f), FVector(0.0f, 360.0f, 22000.0f), 35.0f)); // sky
}

void ASpaceport::DestroyLaunchRig()
{
	if (Plume)
	{
		Plume->Deactivate();
		Plume->DestroyComponent();
		Plume = nullptr;
	}
	if (PlumeLight)
	{
		PlumeLight->DestroyComponent();
		PlumeLight = nullptr;
	}
	for (ACameraActor* Cam : LaunchCams)
	{
		if (IsValid(Cam))
		{
			Cam->Destroy();
		}
	}
	LaunchCams.Reset();
}

void ASpaceport::CutToLaunchShot(int32 Shot)
{
	APlayerController* PC = LaunchPawn
		? Cast<APlayerController>(LaunchPawn->GetController())
		: nullptr;
	if (!PC)
	{
		return;
	}

	AActor* Target = nullptr;
	if (Shot <= 0)
	{
		Target = BoardingView;
	}
	else if (LaunchCams.IsValidIndex(Shot - 1))
	{
		Target = LaunchCams[Shot - 1];
	}

	if (Target)
	{
		PC->SetViewTargetWithBlend(Target, 0.25f);
	}
	LaunchShot = Shot;
}

void ASpaceport::BindLaunchSkip(bool bBind)
{
	APlayerController* PC = LaunchPawn
		? Cast<APlayerController>(LaunchPawn->GetController())
		: nullptr;
	if (!PC)
	{
		return;
	}

	if (bBind && !bLaunchSkipBound)
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpaceport::SkipLaunch);
			InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ASpaceport::SkipLaunch);
			InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASpaceport::SkipLaunch);
			InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ASpaceport::SkipLaunch);
			InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpaceport::SkipLaunch);
		}
		bLaunchSkipBound = true;
	}
	else if (!bBind && bLaunchSkipBound)
	{
		DisableInput(PC);
		bLaunchSkipBound = false;
	}
}

void ASpaceport::HideFlightParts()
{
	for (int32 i = 0; i < PartComponents.Num(); ++i)
	{
		if (!IsFlightPart(i) || !PartComponents[i])
		{
			continue;
		}
		PartComponents[i]->SetHiddenInGame(true);
		PartComponents[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Plume)
	{
		Plume->Deactivate();
		Plume->SetVisibility(false);
	}
	if (PlumeLight)
	{
		PlumeLight->SetVisibility(false);
	}
}

bool ASpaceport::BeginLaunch(APawn* Pawn)
{
	if (bLaunching || bLaunched || !IsValid(Pawn))
	{
		return false;
	}

	UStaticMeshComponent* Rocket = FindRocketMesh();
	if (!Rocket)
	{
		ASibeliusHUD::Toast(this,
			TEXT("COMPILE ERROR: NO HULL ON THE PAD"),
			4.0f, SibeliusToast::Bad);
		return false;
	}

	LaunchPawn = Pawn;
	AttachFlightPartsToRocket();
	RocketRestRelative = Rocket->GetRelativeTransform();
	AttachPlume();
	SpawnLaunchCameras();

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->HoldCinematic(LaunchSeconds + 8.0f);
		}
	}

	BindLaunchSkip(true);
	bLaunching = true;
	LaunchElapsed = 0.0f;
	LaunchShot = INDEX_NONE;
	SetActorTickEnabled(true);
	CutToLaunchShot(0);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Spaceport] ignition - Niagara %s, cameras %d."),
		Plume && Plume->GetAsset() ? *Plume->GetAsset()->GetName() : TEXT("NONE"),
		LaunchCams.Num());
	return true;
}

void ASpaceport::TickLaunch(float DeltaSeconds)
{
	LaunchElapsed += DeltaSeconds;

	const float Hold = FMath::Max(0.0f, IgnitionHoldSeconds);
	const float Total = FMath::Max(Hold + 0.5f, LaunchSeconds);
	const float ClimbTime = FMath::Max(0.01f, Total - Hold);
	const float Raw = FMath::Clamp((LaunchElapsed - Hold) / ClimbTime, 0.0f, 1.0f);
	const float Eased = Raw * Raw * Raw;

	if (UStaticMeshComponent* Rocket = FindRocketMesh())
	{
		FVector Loc = RocketRestRelative.GetLocation();
		Loc.Z += ClimbHeight * Eased;
		FRotator Rot = RocketRestRelative.Rotator();
		const float TurnAlpha = FMath::Clamp((Raw - 0.12f) / 0.55f, 0.0f, 1.0f);
		Rot.Pitch += GravityTurnDegrees * TurnAlpha;
		Rocket->SetRelativeLocationAndRotation(Loc, Rot);
	}

	// Camera cuts: cabin, wide, engines, track, sky.
	int32 Wanted = 0;
	if (LaunchElapsed >= 16.0f)      { Wanted = 4; }
	else if (LaunchElapsed >= 10.0f) { Wanted = 3; }
	else if (LaunchElapsed >= 5.5f)  { Wanted = 2; }
	else if (LaunchElapsed >= 2.5f)  { Wanted = 1; }

	if (Wanted != LaunchShot)
	{
		CutToLaunchShot(Wanted);
	}

	if (Wanted >= 3 && LaunchCams.IsValidIndex(Wanted - 1))
	{
		if (UStaticMeshComponent* Rocket = FindRocketMesh())
		{
			const FVector Hull = Rocket->GetComponentLocation();
			ACameraActor* Cam = LaunchCams[Wanted - 1];
			if (Wanted == 3)
			{
				const FVector Offset = GetActorRotation().RotateVector(FVector(5200.0f, -3200.0f, 800.0f));
				const FVector CamLoc = Hull + Offset;
				Cam->SetActorLocation(CamLoc);
				Cam->SetActorRotation(LookAt(CamLoc, Hull + FVector(0.0f, 0.0f, 2500.0f)));
			}
			else if (Wanted == 4)
			{
				Cam->SetActorRotation(LookAt(Cam->GetActorLocation(), Hull));
			}
		}
	}

	if (LaunchElapsed >= Total)
	{
		EndLaunch();
	}
}

void ASpaceport::SkipLaunch()
{
	if (bLaunching)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Spaceport] launch skipped at %.1fs."), LaunchElapsed);
		EndLaunch();
	}
}

void ASpaceport::EndLaunch()
{
	if (!bLaunching && bLaunched)
	{
		return;
	}

	bLaunching = false;
	bLaunched = true;

	BindLaunchSkip(false);
	HideFlightParts();
	DestroyLaunchRig();

	if (APlayerController* PC = LaunchPawn ? Cast<APlayerController>(LaunchPawn->GetController()) : nullptr)
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ReleaseCinematic();
		}
	}

	APawn* Pawn = LaunchPawn;
	LaunchPawn = nullptr;
	SetActorTickEnabled(false);

	if (IsValid(Pawn))
	{
		Disembark(Pawn);
		ASibeliusHUD::Toast(this,
			TEXT("THE SHIP IS AWAY. NYRA WILL CALL FROM GROK."),
			6.0f, SibeliusToast::Good);
	}

	/* AND THE GUIDE STANDS DOWN (docs/FUN_PLAN_2.md A4).

	   Her stage-3 errand is "go back to the spaceport and we will do the boarding
	   procedures", and the spaceport has just left without either of them. Until now
	   nothing told her, so walking back to the uFoods sidewalk found her repeating it at
	   an empty pad.

	   Told the same way the assembly tells her, through the subsystem, so this class still
	   does not need to know what a dancer is. RestageGuides waits until she is unseen
	   before it changes anything — and she almost certainly is, since he is standing at
	   the pad and she is a street away. */
	if (const UWorld* World = GetWorld())
	{
		if (UDancerAgentSubsystem* Dancers = World->GetSubsystem<UDancerAgentSubsystem>())
		{
			Dancers->RestageGuides();
		}
	}

	/* AND THE WAY OUT OPENS WHERE THE SHIP STOOD, once the toast above has had its say. */
	OpenGrokPortal(/*bImmediate=*/false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Spaceport] launched."));
}

/* ===================================================================================
   THE WAY TO GROK. See the header for why it is here, on this key, on this ground.
   =================================================================================== */

void ASpaceport::OpenGrokPortal(bool bImmediate)
{
	UWorld* World = GetWorld();
	if (!World || GrokPortal)
	{
		return;
	}

	/* A RELOAD MUST NOT REPLAY THE WAIT. The seven-second delay exists so the portal opens
	   as the launch toast clears - it is a beat in a sequence he just watched. Coming back
	   to a level where the ship already left, he should find the portal standing, not wait
	   seven seconds for something he has no reason to expect. Same rule as SnapAssembled:
	   the show happens once. */
	if (!bImmediate && GrokPortalDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(GrokPortalTimer,
			[this]() { OpenGrokPortal(true); }, GrokPortalDelay, false);
		return;
	}

	UNiagaraSystem* System = nullptr;
	const FString Override = CVarGrokPortalFX.GetValueOnGameThread();
	if (!Override.IsEmpty())
	{
		FString Path = Override;
		if (!Path.StartsWith(TEXT("/")))
		{
			Path = FString::Printf(TEXT("/Game/PortalVFX/NS/%s.%s"), *Override, *Override);
		}
		System = LoadObject<UNiagaraSystem>(nullptr, *Path);
		if (!System)
		{
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Spaceport] sib.GrokPortalFX '%s' did not load; using the default."),
				*Override);
		}
	}
	if (!System)
	{
		System = GrokPortalSystem.Get();
	}
	if (!System)
	{
		System = GrokPortalSystem.LoadSynchronous();
	}
	if (!System)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Spaceport] No portal system; the way to Grok will have no door."));
		return;
	}

	const float Scale = (CVarGrokPortalScale.GetValueOnGameThread() >= 0.0f)
		? CVarGrokPortalScale.GetValueOnGameThread() : GrokPortalScale;
	const float Up = (CVarGrokPortalUp.GetValueOnGameThread() >= 0.0f)
		? CVarGrokPortalUp.GetValueOnGameThread() : GrokPortalOffset.Z;

	const float Pitch = CVarGrokPortalPitch.GetValueOnGameThread();
	const FRotator PortalRot = (Pitch >= 0.0f) ? FRotator(Pitch, 0.0f, 0.0f) : FRotator::ZeroRotator;

	/* WHERE IT STANDS. Down the field from the pad, toward wherever he is standing when it
	   opens - and at HIS ground height, not the elevated pad's, so it is a door on the lawn
	   rather than a light on a rooftop. */
	const float TowardWanted = (CVarGrokPortalToward.GetValueOnGameThread() >= 0.0f)
		? CVarGrokPortalToward.GetValueOnGameThread() : GrokPortalTowardPlayer;

	FVector Where = GetActorLocation() + FVector(GrokPortalOffset.X, GrokPortalOffset.Y, Up);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector ToHim = Player->GetActorLocation() - GetActorLocation();
		const float Gap = ToHim.Size2D();
		if (Gap > 100.0f)
		{
			/* MEASURED BACK FROM HIM, NOT FORWARD FROM THE PAD.

			   Walt: "if possible move it to the player's side of the fence." Measuring
			   forward from the pad could not get there - the 80%-of-the-gap clamp meant a
			   160 m gap capped the door at 128 m, which is a few metres the WRONG side of
			   the fence no matter how large TowardWanted was set.

			   A standoff from the player has no such ceiling and is what he actually asked
			   for: the door opens a fixed few metres in front of him, on the line toward the
			   pad, so it still reads as being on the way to the spaceport. */
			const float Along = FMath::Max(0.0f,
				Gap - FMath::Min(TowardWanted, Gap - 200.0f));
			Where = GetActorLocation() + ToHim.GetSafeNormal2D() * Along;

			/* ON THE GROUND, not at his eyeline. His actor location is his capsule CENTRE,
			   so a "height above ground" measured from it floats by a capsule - and
			   TeleporterHole is a disc that has to LIE on the lawn to read as a hole. */
			Where.Z = Player->GetActorLocation().Z - Player->GetSimpleCollisionHalfHeight() + Up;
		}
	}

	GrokPortal = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, System, Where, PortalRot, FVector(Scale),
		/*bAutoDestroy=*/false, /*bAutoActivate=*/true);

	if (GrokPortal)
	{
		GrokPortal->SetWorldScale3D(FVector(Scale));
		// Not every system in the pack exposes the same overrides; setting one it does not
		// have is a no-op rather than an error, so these are safe to attempt blind.
		GrokPortal->SetVariableFloat(TEXT("ScalableSize"), Scale);

		/* AND STOP IT BEING CULLED AT DISTANCE. A Niagara system carries its own bounds and
		   they do NOT grow with the component's scale, so a portal meant to be seen from
		   across a field vanishes long before the player is near enough to enter it - which
		   is the other half of "I see no portal anywhere". Fixed bounds, generous, in local
		   space: this is one effect standing alone on a lawn, not something whose culling
		   anybody needs to be thrifty about. */
		GrokPortal->SetSystemFixedBounds(FBox(FVector(-6000.0f), FVector(6000.0f)));
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Spaceport] Portal '%s' at %s (+%.0f, %.0f cm toward him), scale %.1f, from %.0f cm."),
			*System->GetName(), *Where.ToCompactString(), Up, TowardWanted, Scale, GrokPortalRange);
	}

	ASibeliusHUD::Toast(this,
		TEXT("A WAY HAS OPENED WHERE THE SHIP STOOD - PRESS C TO GO THROUGH"),
		7.0f, SibeliusToast::Prize);

	UE_LOG(LogSibeliusGame, Display, TEXT("[Spaceport] Grok portal open."));
}

void ASpaceport::ClearGrokPortal()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrokPortalTimer);
	}
	if (GrokPortal)
	{
		GrokPortal->DestroyComponent();
		GrokPortal = nullptr;
	}
}
