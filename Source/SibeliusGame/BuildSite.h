// Ch3 - Compile (SIB-27). An authored build location (scope decision A - no free placement).
// Owns its ghost mesh, final mesh, and a pre-placed NavLinkProxy (enabled only on build, C2).
// The site owns and reverts its own state - same principle as Ch2's URefactorableComponent.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompileTypes.h"
#include "Interactable.h"
#include "Branchable.h"
#include "BuildSite.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UInventoryComponent;
class ANavLinkProxy;
class UPointLightComponent;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildStateChanged, bool, bIsBuilt);

// SIB-27 polish (consume-on-build reveal). Sites with bConsumeOnBuild=true (the
// KeyBuildSite) don't leave a dismantlable prop: building IS acquiring. Lifecycle:
//   None ──Build()──► Revealing (PIE float-and-spin ~1.5s) ──► Consumed (terminal)
//   None ──Build() headless / BeginPlay|restore of a built consumable site──► Consumed
// Consumed is inert: tick off, never interactable, mesh hidden, never re-shown.
UENUM()
enum class EBuildSiteRevealPhase : uint8
{
	None,       // not consuming (default / staircase sites / unbuilt consumable)
	Revealing,  // float-and-spin in progress (PIE only)
	Consumed    // terminal: key taken, mesh gone, site inert
};

UCLASS()
class SIBELIUSGAME_API ABuildSite : public AActor, public IInteractable, public IBranchable
{
	GENERATED_BODY()

public:
	ABuildSite();

	// IBranchable (SIB-28): declared state is bIsBuilt. RestoreBranchState writes
	// it RAW via ApplyBuiltState - no inventory spend/refund (that's the verb).
	virtual uint8 CaptureBranchState() const override { return IsBuilt() ? 1 : 0; }
	virtual void RestoreBranchState(uint8 InState) override;

	// IBranchable identity (SIB-29): stable persisted GUID, assign-once.
	virtual FGuid GetOrCreateBranchId() override { AssignBranchIdIfInvalid(BranchId); return BranchId; }
	virtual FGuid GetBranchId() const override { return BranchId; }

	// SIB-30 P3: force a specific persisted identity when RE-CREATING a generated site
	// from a save, so its GUID is stable across the re-spawn (P3-4). Placed actors keep
	// assign-once semantics; this is the explicit deploy-restore path only.
	void RestoreBranchId(const FGuid& InId) { BranchId = InId; }

	// SIB-30 P3: a runtime-generated site (spawned by UGenerateComponent, not placed in
	// the level) carries the catalog EntryId it came from, so Deploy can persist enough
	// provenance to RE-CREATE it on reload. Placed/staircase sites leave these unset.
	bool IsGenerated() const { return bIsGenerated; }
	FName GetGenerateEntryId() const { return GenerateEntryId; }
	void MarkGenerated(FName InEntryId) { bIsGenerated = true; GenerateEntryId = InEntryId; }

	// Authored default: unbuilt.
	virtual uint8 GetDefaultBranchState() const override { return 0; }

	virtual void BeginPlay() override;

	// SIB-27: drives the float-and-spin reveal; self-disables once Consumed (K6).
	virtual void Tick(float DeltaSeconds) override;

	// SIB-38 GUID baking: assign at edit time (OnConstruction in an editor world), and
	// give a copy-pasted site a fresh id so it never shares its source's.
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

	// IInteractable: E dismantles a built site (refund). Building is the B verb (UBuildComponent::TriggerBuild).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// True when the inventory can afford this site and it isn't built yet.
	bool CanBuild(const UInventoryComponent* Inventory) const;

	// Spend -> swap ghost->final -> enable collision + nav-link (C2) / grant key (EBuildOutput::KeyItem).
	bool Build(UInventoryComponent* Inventory);

	// Full refund (C4), re-disable nav-link, back to ghost state.
	bool Dismantle(UInventoryComponent* Inventory);

	// Ghost shown/hidden by UBuildComponent as the player approaches with enough resources.
	void SetGhostVisible(bool bVisible);

	bool IsBuilt() const { return bIsBuilt; }

	// SIB-27: terminal state for a consumable site (key taken, mesh gone, inert).
	bool IsConsumed() const { return RevealPhase == EBuildSiteRevealPhase::Consumed; }

	// Headless self-test for CompileSmokeTest (bar item 4). Uses a transient inventory.
	bool RunBuildSelfTest(FString& OutError);

	// SIB-27 headless self-test for consumable sites (ledger K2–K5/K6). Requires a fresh
	// un-built site with bConsumeOnBuild=true; builds it, asserts the Consumed terminal
	// state, the no-dismantle soft-lock guard, no double-grant, and a clean reload.
	bool RunConsumeOnBuildSelfTest(FString& OutError);

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<USceneComponent> SceneRoot; // CP3 lesson: explicit root or GetActorLocation() lies

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<UStaticMeshComponent> GhostMesh; // translucent preview, never collides (C8: separate component)

	UPROPERTY(VisibleAnywhere, Category = "Build")
	TObjectPtr<UStaticMeshComponent> FinalMesh; // hidden + no collision until built

	// SIB-27 art polish (orb ghost): a glow light for the floating-orb preview, lit only
	// while the orb ghost is shown. Inert unless bGhostAsOrb (below).
	UPROPERTY(VisibleAnywhere, Category = "Build|Art")
	TObjectPtr<UPointLightComponent> GhostGlow;

	UPROPERTY(EditAnywhere, Category = "Build")
	EBuildOutput Output = EBuildOutput::Structure;

	UPROPERTY(EditAnywhere, Category = "Build")
	EResourceType CostResource = EResourceType::Book;

	// SIB-27: false (default) = normal build/dismantle (the staircase). true = building
	// consumes the site (the KeyBuildSite): brief reveal, then mesh gone and inert — no
	// dismantlable prop, so E can't refund the Key and soft-lock the attic (K1/K2).
	// Set by hand on the KeyBuildSite instance in the level.
	UPROPERTY(EditAnywhere, Category = "Build")
	bool bConsumeOnBuild = false;

	// SIB-27 reveal tuning (consumable sites only; PIE feel). Per-instance, no recompile.
	// Total seconds of the float-and-spin before the key vanishes.
	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "0.1"))
	float RevealDurationSeconds = 3.0f;

	// Yaw spin speed, held CONSTANT — a longer RevealDurationSeconds yields more
	// revolutions rather than a slower spin. Default 360 = one revolution per second.
	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "0.0"))
	float RevealSpinRateDegPerSec = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "1"))
	int32 Cost = 8;

	// Pre-placed in the level, smart-link disabled until built (C2). Optional for key sites.
	UPROPERTY(EditInstanceOnly, Category = "Build")
	TObjectPtr<ANavLinkProxy> NavLink;

	// How close the player must be for the ghost/prompt (cm).
	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "50.0"))
	float InteractRadius = 350.f;

	// --- SIB-27 art polish: render the ghost/preview as a floating glowing ORB (matching
	// the Sauce Door curio) instead of the placeholder cube. Per-instance opt-in — set on
	// the KeyBuildSite; plain build sites keep their cube ghost. Visual ONLY: the build/
	// consume mechanic is untouched. ---
	UPROPERTY(EditAnywhere, Category = "Build|Art")
	bool bGhostAsOrb = false;

	// Warm gold/amber so a buildable key reads distinct from the curio's cyan.
	UPROPERTY(EditAnywhere, Category = "Build|Art")
	FLinearColor GhostOrbColor = FLinearColor(1.0f, 0.6f, 0.12f);

	// Emissive base for the orb (tinted to GhostOrbColor). Defaults to the kit's lamp
	// emissive material — graceful fallback (plain sphere) if it can't load.
	UPROPERTY(EditAnywhere, Category = "Build|Art")
	TSoftObjectPtr<UMaterialInterface> GhostOrbMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Build")
	FOnBuildStateChanged OnBuildStateChanged;

private:
	void ApplyBuiltState(bool bBuilt);
	void SetNavLinkEnabled(bool bEnabled);

	// SIB-27 art: restyle GhostMesh into a floating glowing orb (sphere + emissive MID +
	// glow light) when bGhostAsOrb. Called once on BeginPlay for an unbuilt orb site.
	void SetupGhostOrb();
	FVector OrbBaseRelLoc = FVector::ZeroVector; // GhostMesh rel loc the bob oscillates around
	float OrbBobTime = 0.f;

	// SIB-27 consume-on-build reveal. BeginReveal starts the float-and-spin (PIE) or
	// consumes synchronously (headless, K3); EnterConsumed is the terminal hide + tick-off.
	void BeginReveal();
	void EnterConsumed();

	EBuildSiteRevealPhase RevealPhase = EBuildSiteRevealPhase::None;
	float RevealElapsed = 0.f;                 // seconds into the float-and-spin
	FVector RevealBaseLocation = FVector::ZeroVector; // FinalMesh relative loc captured at reveal start
	FRotator RevealBaseRotation = FRotator::ZeroRotator;

	// SIB-29/38: stable cross-reload identity, BAKED into the level package so it
	// survives across sessions (assigned at edit time in OnConstruction). Plain
	// serialized UPROPERTY (not SaveGame-only); VisibleAnywhere to inspect in-editor.
	UPROPERTY(VisibleAnywhere, Category = "Branch")
	FGuid BranchId;

	// SIB-30 P3: runtime-generation provenance. Set on sites spawned by UGenerateComponent
	// (and re-set when re-created from a save). Transient by nature — a generated site is
	// never serialized into the .umap; it's re-created from the deploy save instead.
	UPROPERTY()
	bool bIsGenerated = false;

	UPROPERTY()
	FName GenerateEntryId;

	bool bIsBuilt = false;
};
