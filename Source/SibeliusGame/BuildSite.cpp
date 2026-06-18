#include "BuildSite.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "InventoryComponent.h"
#include "Navigation/NavLinkProxy.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h" // SIB-27: FApp::CanEverRender — headless guard for the reveal (K3)

// SIB-27 reveal feel (PIE-only). Named namespace, never anonymous (CP3 lesson #6).
// Duration and spin rate are per-instance UPROPERTYs; these shape the float curve.
namespace BuildSiteReveal
{
	static constexpr float RiseHeight   = 60.f;  // cm FinalMesh floats up to its hover peak
	static constexpr float RiseFraction = 0.35f; // peak reached in the first 35% of the reveal
	static constexpr float RiseEaseExp  = 2.0f;  // ease-out: quick rise, then settle into the hover
}

ABuildSite::ABuildSite()
{
	// SIB-27: ticks only during the float-and-spin reveal; disabled at all other times
	// (K6). Starts disabled so a default/staircase site never ticks.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GhostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	GhostMesh->SetupAttachment(SceneRoot);
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetCanEverAffectNavigation(false);
	GhostMesh->SetHiddenInGame(true); // shown only when affordable + near (UBuildComponent)

	FinalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinalMesh"));
	FinalMesh->SetupAttachment(SceneRoot);
	FinalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FinalMesh->SetCanEverAffectNavigation(false); // nav comes from the NavLink, not the mesh (C2)
	FinalMesh->SetHiddenInGame(true);

	// SIB-27 art: orb-ghost glow (inert unless bGhostAsOrb). Lit with the ghost.
	GhostGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("GhostGlow"));
	GhostGlow->SetupAttachment(GhostMesh);
	GhostGlow->SetIntensity(4000.f);
	GhostGlow->SetAttenuationRadius(700.f);
	GhostGlow->CastShadows = false;
	GhostGlow->SetVisibility(false);
	GhostOrbMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT")));
}

void ABuildSite::BeginPlay()
{
	Super::BeginPlay();
	GetOrCreateBranchId();     // SIB-29: runtime fallback — only fills an invalid (unbaked) id
	ApplyBuiltState(bIsBuilt); // enforce ghost/final/nav-link coherence from one source of truth

	// SIB-27 art: an unbuilt orb-ghost site restyles its preview to a floating glow orb
	// and ticks to bob it. (A site already built/consumed shows no ghost — skip.)
	if (bGhostAsOrb && !bIsBuilt)
	{
		SetupGhostOrb();
		SetActorTickEnabled(true);
	}
}

void ABuildSite::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	AssignBranchIdAtEditTime(this, BranchId); // SIB-38: bake the id in the editor world
#endif
}

void ABuildSite::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	// SIB-38: an editor copy-paste/duplicate gets a fresh id; a PIE duplicate KEEPS
	// the baked id (so the PIE world resolves the deployed save).
	if (!bDuplicateForPIE)
	{
		BranchId = FGuid::NewGuid();
	}
}

#if WITH_EDITOR
void ABuildSite::PostEditImport()
{
	Super::PostEditImport();
	BranchId = FGuid::NewGuid(); // SIB-38: pasted copy must not share its source's id
}
#endif

bool ABuildSite::CanBuild(const UInventoryComponent* Inventory) const
{
	return !bIsBuilt && Inventory && Inventory->GetCount(CostResource) >= Cost;
}

bool ABuildSite::Build(UInventoryComponent* Inventory)
{
	if (!CanBuild(Inventory))
	{
		return false;
	}
	if (!Inventory->Spend(CostResource, Cost)) // C3: Spend is the gatekeeper
	{
		return false;
	}

	if (Output == EBuildOutput::KeyItem)
	{
		Inventory->Add(EResourceType::Key, 1);
	}

	// SIB-27: a consumable site (KeyBuildSite) doesn't leave a dismantlable prop —
	// building IS acquiring. Latch built (so CanBuild/Build are idempotent, K4), then
	// reveal-then-consume instead of the normal built presentation. The bIsBuilt latch
	// above (in CanBuild) guarantees this runs at most once.
	if (bConsumeOnBuild)
	{
		bIsBuilt = true;
		BeginReveal(); // PIE: float-and-spin; headless: consume synchronously (K3)
	}
	else
	{
		ApplyBuiltState(true);
	}
	OnBuildStateChanged.Broadcast(true);
	return true;
}

bool ABuildSite::Dismantle(UInventoryComponent* Inventory)
{
	// SIB-27 (K2): a consumable site is never dismantlable. Without this, E on the
	// revealed/consumed key would refund the cost and spend the Key back out — the
	// attic soft-lock. Guard here too so no caller path (E, Blueprint, test) can hit it.
	if (bConsumeOnBuild)
	{
		return false;
	}
	if (!bIsBuilt || !Inventory)
	{
		return false;
	}
	if (Output == EBuildOutput::KeyItem)
	{
		// Key must come back so the refund can't dupe resources (C3/C4).
		if (!Inventory->Spend(EResourceType::Key, 1))
		{
			return false;
		}
	}

	Inventory->Add(CostResource, Cost); // full refund (C4)
	ApplyBuiltState(false);
	OnBuildStateChanged.Broadcast(false);
	return true;
}

void ABuildSite::Interact_Implementation(AActor* Interactor)
{
	if (bConsumeOnBuild)
	{
		return; // SIB-27 (K2): consumable sites are inert to E — no dismantle, no refund.
	}
	if (!bIsBuilt)
	{
		return; // E only dismantles; an unbuilt site has no collision to focus anyway. Build is the B verb.
	}
	// Inventory lives on the pawn (the interactor); no actor-finds-player race (Ch1/R7).
	UInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	Dismantle(Inventory); // full refund (C4)
}

FText ABuildSite::GetInteractionPrompt_Implementation() const
{
	if (bConsumeOnBuild)
	{
		return FText::GetEmpty(); // SIB-27 (K2): consumable sites never prompt for E.
	}
	return bIsBuilt ? NSLOCTEXT("Sibelius", "BuildSiteDismantlePrompt", "Dismantle — full refund [E]") : FText::GetEmpty();
}

void ABuildSite::SetGhostVisible(bool bVisible)
{
	if (!bIsBuilt && GhostMesh)
	{
		GhostMesh->SetHiddenInGame(!bVisible);
		if (bGhostAsOrb && GhostGlow)
		{
			GhostGlow->SetVisibility(bVisible); // the orb's glow follows the preview
		}
	}
}

void ABuildSite::SetupGhostOrb()
{
	if (!GhostMesh)
	{
		return;
	}
	// Floating glowing orb — the same sphere + emissive-MID approach as ACurio, so the
	// buildable key reads like the Sauce Door curio (warm gold vs the curio's cyan).
	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		GhostMesh->SetStaticMesh(Sphere);
	}
	GhostMesh->SetRelativeScale3D(FVector(0.4f));
	OrbBaseRelLoc = FVector(0.f, 0.f, 90.f);          // hovers above the site origin
	GhostMesh->SetRelativeLocation(OrbBaseRelLoc);

	if (UMaterialInterface* Base = GhostOrbMaterial.LoadSynchronous())
	{
		if (UMaterialInstanceDynamic* MID = GhostMesh->CreateDynamicMaterialInstance(0, Base))
		{
			MID->SetVectorParameterValue(TEXT("Emissive"), GhostOrbColor);
			MID->SetVectorParameterValue(TEXT("BaseColor"), GhostOrbColor);
			MID->SetScalarParameterValue(TEXT("Intens"), 9.0f);
			MID->SetScalarParameterValue(TEXT("TurnOn"), 1.0f);
		}
	}
	if (GhostGlow)
	{
		GhostGlow->SetLightColor(GhostOrbColor);       // glow is a child of GhostMesh — bobs with it
	}
}

void ABuildSite::RestoreBranchState(uint8 InState)
{
	ApplyBuiltState(InState != 0); // RAW: swaps mesh/collision/nav, no inventory
}

void ABuildSite::ApplyBuiltState(bool bBuilt)
{
	bIsBuilt = bBuilt;

	if (GhostMesh)
	{
		GhostMesh->SetHiddenInGame(true); // ghost never lingers (C8)
		GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GhostGlow)
	{
		GhostGlow->SetVisibility(false); // orb glow off once built/restored
	}

	// SIB-27 (K5): this is the RAW restore path (BeginPlay + RestoreBranchState). A
	// consumable site that was already built must present Consumed immediately — mesh
	// hidden, inert, NO reveal animation and NO re-shown key. The Build() verb takes the
	// reveal path instead (it never routes through here for consumable sites).
	if (bConsumeOnBuild && bBuilt)
	{
		EnterConsumed();
		return;
	}

	RevealPhase = EBuildSiteRevealPhase::None;
	if (FinalMesh)
	{
		FinalMesh->SetVisibility(true); // undo any prior reveal/consume hide
		FinalMesh->SetHiddenInGame(!bBuilt);
		FinalMesh->SetCollisionEnabled(bBuilt ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	SetNavLinkEnabled(bBuilt);
}

void ABuildSite::SetNavLinkEnabled(bool bEnabled)
{
	if (NavLink)
	{
		NavLink->SetSmartLinkEnabled(bEnabled); // C2: pre-baked link, flipped at runtime
	}
}

void ABuildSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// SIB-27 art: gently bob the floating orb ghost while it's the unbuilt preview (the
	// reveal/consume below takes over once built). Cheap; only orb sites tick here.
	if (bGhostAsOrb && !bIsBuilt && GhostMesh)
	{
		OrbBobTime += DeltaSeconds;
		const float Bob = FMath::Sin(OrbBobTime * 2.0f) * 8.0f; // ±8cm hover
		GhostMesh->SetRelativeLocation(OrbBaseRelLoc + FVector(0.f, 0.f, Bob));
	}

	// K6: only the reveal drives Tick. Once Consumed (or never revealing), bail — and
	// EnterConsumed also disables the tick outright, so this is belt-and-braces.
	if (RevealPhase != EBuildSiteRevealPhase::Revealing || !FinalMesh)
	{
		return;
	}

	RevealElapsed += DeltaSeconds;
	const float Duration = FMath::Max(RevealDurationSeconds, 0.01f); // guard /0 if tuned to 0
	const float Alpha = FMath::Clamp(RevealElapsed / Duration, 0.f, 1.f);

	// Float: rise to the hover peak over the first portion (ease-out so it eases in to a
	// hover), then hold there — not a slow drift across the whole reveal.
	const float RiseAlpha = FMath::Clamp(RevealElapsed / (Duration * BuildSiteReveal::RiseFraction), 0.f, 1.f);
	const float Height = BuildSiteReveal::RiseHeight *
		FMath::InterpEaseOut(0.f, 1.f, RiseAlpha, BuildSiteReveal::RiseEaseExp);

	// Spin: CONSTANT rate (deg/sec), accumulated from elapsed time — a longer duration
	// yields more revolutions, it does not slow the spin to fill the window.
	const float Yaw = RevealSpinRateDegPerSec * RevealElapsed;

	FinalMesh->SetRelativeLocation(RevealBaseLocation + FVector(0.f, 0.f, Height));
	FinalMesh->SetRelativeRotation(RevealBaseRotation + FRotator(0.f, Yaw, 0.f));

	if (Alpha >= 1.f)
	{
		EnterConsumed(); // terminal: hide the key, stop ticking
	}
}

void ABuildSite::BeginReveal()
{
	// Ghost never lingers during the reveal (C8).
	if (GhostMesh)
	{
		GhostMesh->SetHiddenInGame(true);
		GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// K3: headless (commandlet / -nullrhi / non-game world) has no renderer — skip the
	// animation and consume synchronously so the gate stays green and deterministic.
	const UWorld* World = GetWorld();
	const bool bHeadless = IsRunningCommandlet() || !FApp::CanEverRender() || !World || !World->IsGameWorld();
	if (bHeadless)
	{
		EnterConsumed();
		return;
	}

	// PIE feel: show the key briefly, then float-and-spin it away. No collision while it
	// floats (ephemeral — the player can't bump or interact with it).
	if (FinalMesh)
	{
		FinalMesh->SetVisibility(true);
		FinalMesh->SetHiddenInGame(false);
		FinalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RevealBaseLocation = FinalMesh->GetRelativeLocation();
		RevealBaseRotation = FinalMesh->GetRelativeRotation();
	}
	RevealElapsed = 0.f;
	RevealPhase = EBuildSiteRevealPhase::Revealing;
	SetActorTickEnabled(true);
}

void ABuildSite::EnterConsumed()
{
	RevealPhase = EBuildSiteRevealPhase::Consumed;
	SetActorTickEnabled(false); // K6: terminal state never drives Tick again

	// The key is gone: hidden, invisible, no collision. Nothing left to focus or refund.
	if (FinalMesh)
	{
		FinalMesh->SetVisibility(false);
		FinalMesh->SetHiddenInGame(true);
		FinalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GhostMesh)
	{
		GhostMesh->SetHiddenInGame(true);
		GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GhostGlow)
	{
		GhostGlow->SetVisibility(false); // orb glow gone with the consumed key
	}
	SetNavLinkEnabled(false); // key sites carry no NavLink, but stay coherent regardless
}

bool ABuildSite::RunBuildSelfTest(FString& OutError)
{
	// Bar item 4, headless: insufficient rejected; sufficient builds + decrements; dismantle refunds.
	UInventoryComponent* TempInv = NewObject<UInventoryComponent>(this, TEXT("SelfTestInventory"));
	const bool bWasBuilt = bIsBuilt;

	// Insufficient inventory must be rejected.
	if (bIsBuilt)
	{
		OutError = TEXT("Self-test requires an un-built site (run before gameplay)");
		return false;
	}
	if (Build(TempInv))
	{
		OutError = TEXT("Build accepted with empty inventory");
		return false;
	}

	// Sufficient inventory must build, decrement, flip state (+ grant key for KeyItem sites).
	TempInv->Add(CostResource, Cost + 2);
	if (!Build(TempInv))
	{
		OutError = TEXT("Build rejected with sufficient inventory");
		return false;
	}
	if (!bIsBuilt || TempInv->GetCount(CostResource) != 2)
	{
		OutError = TEXT("Build did not set state or decrement cost correctly");
		return false;
	}
	if (Output == EBuildOutput::KeyItem && TempInv->GetCount(EResourceType::Key) != 1)
	{
		OutError = TEXT("KeyItem build did not grant a Key");
		return false;
	}
	if (FinalMesh && FinalMesh->bHiddenInGame)
	{
		OutError = TEXT("FinalMesh still hidden after build (C8)");
		return false;
	}
	if (GhostMesh && !GhostMesh->bHiddenInGame)
	{
		OutError = TEXT("GhostMesh visible after build (C8)");
		return false;
	}

	// Dismantle must refund in full and restore pre-build state (C4).
	if (!Dismantle(TempInv))
	{
		OutError = TEXT("Dismantle rejected on a built site");
		return false;
	}
	if (bIsBuilt || TempInv->GetCount(CostResource) != Cost + 2)
	{
		OutError = TEXT("Dismantle did not refund in full (C4)");
		return false;
	}
	if (Output == EBuildOutput::KeyItem && TempInv->GetCount(EResourceType::Key) != 0)
	{
		OutError = TEXT("Dismantle left a duplicate Key (C3)");
		return false;
	}

	ApplyBuiltState(bWasBuilt); // leave the site as found
	return true;
}

bool ABuildSite::RunConsumeOnBuildSelfTest(FString& OutError)
{
	// SIB-27 ledger K2–K6, headless. Requires a fresh, un-built consumable site; the
	// reveal consumes synchronously here (no renderer), so the terminal Consumed state
	// is observable inline. Reveal *feel* is PIE-only and not asserted.
	if (!bConsumeOnBuild)
	{
		OutError = TEXT("Self-test requires bConsumeOnBuild=true");
		return false;
	}
	if (bIsBuilt)
	{
		OutError = TEXT("Self-test requires an un-built site (run before gameplay)");
		return false;
	}

	UInventoryComponent* Inv = NewObject<UInventoryComponent>(this, TEXT("ConsumeSelfTestInventory"));
	Inv->Add(CostResource, Cost);

	// Build → grant Key once, consume synchronously (headless), land in Consumed.
	if (!Build(Inv))
	{
		OutError = TEXT("Build rejected with sufficient inventory");
		return false;
	}

	// K3: ended Consumed, key mesh hidden + no collision, Key granted exactly once.
	if (!IsConsumed())
	{
		OutError = TEXT("Not in Consumed state after headless build (K3)");
		return false;
	}
	if (Output == EBuildOutput::KeyItem && Inv->GetCount(EResourceType::Key) != 1)
	{
		OutError = TEXT("Consumable build did not grant exactly one Key (K3)");
		return false;
	}
	if (FinalMesh && (FinalMesh->IsVisible() || !FinalMesh->bHiddenInGame))
	{
		OutError = TEXT("Key mesh still shown after consume (K3)");
		return false;
	}
	if (FinalMesh && FinalMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		OutError = TEXT("Key mesh still collides after consume (K3)");
		return false;
	}

	// K6: the terminal state must not be driving Tick.
	if (IsActorTickEnabled())
	{
		OutError = TEXT("Tick still enabled after consume (K6)");
		return false;
	}

	// K4: building an already-built/consumed site must not re-grant — give it enough to
	// *try* again and confirm the bIsBuilt latch refuses.
	const int32 KeysAfterBuild = Inv->GetCount(EResourceType::Key);
	Inv->Add(CostResource, Cost);
	if (Build(Inv))
	{
		OutError = TEXT("Re-build of a built site succeeded (K4 double-grant)");
		return false;
	}
	if (Inv->GetCount(EResourceType::Key) != KeysAfterBuild)
	{
		OutError = TEXT("Key re-granted on a built site (K4)");
		return false;
	}

	// K2: E is inert — Dismantle refuses, inventory unchanged, prompt empty. This is the
	// soft-lock guard: without it, E would refund the cost and spend the Key back out.
	const int32 BooksBefore = Inv->GetCount(CostResource);
	const int32 KeysBefore  = Inv->GetCount(EResourceType::Key);
	if (Dismantle(Inv))
	{
		OutError = TEXT("Dismantle succeeded on a consumable site (K2 soft-lock)");
		return false;
	}
	if (Inv->GetCount(CostResource) != BooksBefore || Inv->GetCount(EResourceType::Key) != KeysBefore)
	{
		OutError = TEXT("Dismantle changed inventory on a consumable site (K2)");
		return false;
	}
	if (!GetInteractionPrompt_Implementation().IsEmpty())
	{
		OutError = TEXT("Consumed site still shows an interaction prompt (K2)");
		return false;
	}

	// K5: a reload/restore presents Consumed immediately — no reveal, no re-shown key,
	// no Key minted (RestoreBranchState is RAW; it must not touch inventory).
	const int32 KeysAtReload = Inv->GetCount(EResourceType::Key);
	RestoreBranchState(1); // simulate loading a built site
	if (!IsConsumed())
	{
		OutError = TEXT("Restored built consumable site not Consumed (K5)");
		return false;
	}
	if (FinalMesh && (FinalMesh->IsVisible() || !FinalMesh->bHiddenInGame))
	{
		OutError = TEXT("Key mesh re-shown after reload (K5)");
		return false;
	}
	if (IsActorTickEnabled())
	{
		OutError = TEXT("Tick re-enabled after reload (K6)");
		return false;
	}
	if (Inv->GetCount(EResourceType::Key) != KeysAtReload)
	{
		OutError = TEXT("Reload minted a Key (K5 — restore must be RAW)");
		return false;
	}

	// Leave the site un-built so a caller reusing this instance starts clean.
	ApplyBuiltState(false);
	return true;
}
