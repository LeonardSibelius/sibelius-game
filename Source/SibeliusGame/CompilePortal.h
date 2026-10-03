// CompilePortal.h
//
// A PORTAL THAT C TAKES YOU THROUGH. Walt, 2026-10-02: "when the player starts the game he
// is looking down the hall at BP_Door_Outside_A3. I want one of the portals from my portal
// pack right there with a "C" prompt to go thru it, just like at the spaceport. Might as
// well make him go upstairs and earn Compile from Elise before he can visit Kaia's home."
//
// Portals are for the long jumps (Walt, the same day: "The portal is for traveling for
// many light years"), and Kaia's home is one.
//
// THE GATE IS THE KEY ITSELF. C is the Compile verb, and ASibeliusGameCharacter::
// OnCompilePressed refuses an unearned Compile with the game's own banner ("COMPILE IS NOT
// YET YOURS - SEEK THE PLACE IT IS GRANTED") before it asks any claimant. So the portal
// joins the claimants after that check, like the sauce bowl and the spaceport, and needs no
// gate of its own. The first thing a new player sees, straight down the hall, is a way out
// he cannot take yet, and the banner says it is earned somewhere.
//
// NOTHING ON THE OTHER SIDE YET. Kaia's planet is not built. A travel to a map that does
// not exist drops the player onto the default map, so while TravelTargetLevel is unset (or,
// in the editor, names a map that is not there) C says the far side is not ready instead.
//
// WHICH EFFECT. Any Niagara system from Content/PortalVFX (gitignored, Walt's purchased
// pack), picked in the Details panel; the pick is a hard reference on the placed actor, so
// whatever is chosen is cooked with the level.
//
// The spaceport's NS_TeleporterHole, STOOD UP. The first try was NS_AnyWhereDoor, one of
// the pack's upright doorways, and Walt saw a purple glow with a hairline in it - the
// effect edge-on - and asked "not a spinning portal like in the spaceport?" The hole is a
// ground disc, so it is rolled 90 degrees to face down the hall; at scale 1 it fills the
// doorway from the floor to just above the door (screenshot, 2026-10-02).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CompilePortal.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;

UCLASS()
class SIBELIUSGAME_API ACompilePortal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACompilePortal();

	/** The character's Compile verb asks this after its own unlock check. True when the
	 *  press was this portal's to answer: the player is within ClaimRadius, or is looking
	 *  at it (its prompt is the one on screen), which wins over the radius. */
	bool TryClaim(APawn* Claimer, bool bLookedAt = false);

	// IInteractable. The prompt is what the player reads; E only points him at C, because E
	// going through would skip the Compile gate.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Where C goes. None until the destination exists. */
	UPROPERTY(EditAnywhere, Category = "Portal")
	FName TravelTargetLevel = NAME_None;

	/** The PlayerStart tag to arrive at on the far side. None = the level's default. */
	UPROPERTY(EditAnywhere, Category = "Portal")
	FName ArrivalTag = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Portal")
	FText PromptText;

	UPROPERTY(EditAnywhere, Category = "Portal")
	FString NotReadyText = TEXT("THE PORTAL HUMS. NOTHING IS ON THE OTHER SIDE YET.");

	/** How close the player must stand for C to be this portal's. A hall's length is
	 *  14 m, and the player starts at the far end of it, so this is well short of that. */
	UPROPERTY(EditAnywhere, Category = "Portal", meta = (ClampMin = "50.0"))
	float ClaimRadius = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Portal|Look")
	TObjectPtr<UNiagaraSystem> PortalSystem;

	/** The effect's placement relative to the actor, for systems authored off-centre,
	 *  lying down, or at an odd size. Live in the editor. */
	UPROPERTY(EditAnywhere, Category = "Portal|Look")
	FVector EffectOffset = FVector::ZeroVector;

	/** Roll 90 stands the ground disc up, facing the actor's -Y and +Y. */
	UPROPERTY(EditAnywhere, Category = "Portal|Look")
	FRotator EffectRotation = FRotator(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, Category = "Portal|Look", meta = (ClampMin = "0.01"))
	float EffectScale = 1.0f;

	/** A little light of its own, so it reads as a glow in the hall and not a sticker. */
	UPROPERTY(EditAnywhere, Category = "Portal|Look", meta = (ClampMin = "0.0"))
	float GlowLumens = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Portal|Look")
	FLinearColor GlowColor = FLinearColor(0.55f, 0.35f, 1.0f, 1.0f);

	/** The doorway the prompt answers to: half-extents, cm. */
	UPROPERTY(EditAnywhere, Category = "Portal|Look")
	FVector PromptExtent = FVector(60.0f, 20.0f, 110.0f);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	/** Effect, light and prompt box from the properties. From OnConstruction so the editor
	 *  shows it live, and again at BeginPlay, because this project has been caught three
	 *  times by things that were only true in the editor. */
	void ApplyLook();

	/** In the editor, false for a map name that is not on disk. A packaged build cannot
	 *  ask, and trusts the name. */
	bool DestinationExists() const;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UNiagaraComponent> Effect;

	/** What the interaction sweep finds. Blocks the Visibility channel and nothing else, so
	 *  the prompt appears but the player is never stopped by an invisible box. */
	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UBoxComponent> PromptVolume;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UPointLightComponent> Glow;
};
