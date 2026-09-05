// WormholeArrival.h — the passage to Grok, which is also the arrival on it.
//
// ===========================================================================
// docs/SEVENTH_POWER.md rev 2-5.
//
// Walt: "a vague idea about approaching a blue ghost and being invited to travel through
// an abstract dreamlike realm of particle art to get to planet Grok, instead of buying a
// spaceship asset and going through all the 40 light year journey routing."
//
// ---------------------------------------------------------------------------
// WHY THE PASSAGE IS NOT ITS OWN LEVEL.
//
// The obvious build is a cinematic level between the city and Grok: travel there, fly a
// Sequencer shot through particles, travel again. That is two loading screens and a hard
// cut into Grok at the end — and the reveal of the planet is the shot the whole ending
// rests on, so a seam is the last thing it can afford.
//
// So there is no passage level. He travels straight to L_Grok and arrives BLIND, inside a
// dense swarm that thins away over a few seconds until the landscape is simply there. Rev
// 2 argued for this before anyone knew it was cheaper: "the drift does not cut to a place,
// it thickens into one."
//
// ---------------------------------------------------------------------------
// IT IS MADE OF THE GAME'S OWN APPARITION, RUN BACKWARDS.
//
// M_materialise is the cyan shader every spaceport part wears while it is forming —
// Walt's own, built by Tools/Scripts/build_materialise_material.py, and already one of
// the 26 things package_v130.ps1 refuses to ship without. The spaceport fades it OUT as
// geometry becomes real. This fades it out as a WORLD becomes real. Same idea, same
// asset, no import, and nothing new that can go missing from a pak.
//
// A SWARM OF SHAPES, NOT ONE ENCLOSING SPHERE. A sphere around the camera would be
// backface-culled and invisible from inside — the classic version of this effect that
// does not work. Many small shapes seen from within a cloud have no such problem, and
// they read as "abstract particle art" rather than as a fogged lens.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WormholeArrival.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class SIBELIUSGAME_API AWormholeArrival : public AActor
{
	GENERATED_BODY()

public:
	AWormholeArrival();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** How long the swarm takes to thin away and hand him the controls. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.5"))
	float PassageSeconds = 7.0f;

	/** How many shapes are in the cloud. Cheap: unlit-ish translucent, no shadows. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "1", ClampMax = "600"))
	int32 ShapeCount = 140;

	/** Radius of the cloud he arrives inside, in cm. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "50.0"))
	float CloudRadius = 900.0f;

	/** Nearest a shape may sit to his eyes, so nothing spawns clipped through his face. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.0"))
	float CloudInnerRadius = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "1.0"))
	float ShapeSize = 55.0f;

	/** How fast the swarm drifts outward as it fades. Slow: this is a dream, not a blast. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.0"))
	float DriftSpeed = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.0"))
	float PassageGlow = 6.0f;

	/* THE SHADER, SOFT AND LOADED ON DEMAND — deliberately not in the constructor.

	   ASpaceport learned this one the hard way and its comment is worth repeating: a
	   constructor runs before the editor exists, so a bad asset reference there is a
	   project that will not open. GS_Idle_MH cost a session that way.

	   Cooking is not at risk. M_materialise is already checked by name in
	   package_v130.ps1 ("the materialise fx"), so it reaches the pak by a route that does
	   not depend on this pointer. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	TSoftObjectPtr<UMaterialInterface> PassageMaterial;

	/** Any key ends it early. On by default: nobody wants a cutscene twice. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	bool bSkippable = true;

private:
	void BuildCloud();
	void ClearCloud();
	void Finish();
	void HoldPlayer(bool bHold);

	UMaterialInterface* GetPassageMaterial();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Shapes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShapeMIDs;

	/** Unit direction each shape drifts along, so the cloud opens outward around him. */
	TArray<FVector> Drifts;

	float Elapsed = 0.0f;
	bool bRunning = false;
	bool bFinished = false;
};
