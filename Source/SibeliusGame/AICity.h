// AICity.h
//
// THE AI CITY - what is through the seed (AUniverseSeed), in L_Genesis. Walt, 2026-10-15:
// "Something cosmic. Something that shows a vision of the future where AI has solved anything
// and can build anything and this is what the AI have built to amuse themselves. Not a place
// for transhumans ... but an AI city." Seed 1 of three he was offered: a universe in a seed.
//
// A STAR, AND A CITY THAT IS ALSO A MIND WRAPPED ROUND IT: three shells, each a glowing geodesic
// lattice - struts with thought running down them, twinkling nodes where they meet (since
// 2026-10-18; at first they were blocks along avenues) - each turning slowly on its own axis; three transit rings; a far sky of stars. Leonard stands on a glass gallery inside
// the innermost shell, the star filling a third of his view. Round the gallery float the
// universes the AIs have grown - glass bubbles, a galaxy turning in each. (Growing his own is
// the next step: the model, the three dials, the six powers.)
//
// Frame: the actor's origin is the star's centre. The gallery is GalleryDistance out along -X,
// its floor level with the star's equator; place_genesis.py puts the arrival there, facing +X.
// Centimetres. Everything is built here from the engine's shapes and /Game/Genesis's materials
// (make_genesis_assets.py) - nothing bought, so it can be rendered and judged by eye.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AICity.generated.h"

class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPostProcessComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AAICity : public AActor
{
	GENERATED_BODY()

public:
	AAICity();

	virtual void Tick(float DeltaSeconds) override;

	/** A universe at the forge has grown machines: the city answers it (see the .cpp). */
	void AnswerUniverse();
	bool IsAnswering() const { return ResponseAge >= 0.0f; }

	static constexpr int32 ShellCount = 3;
	static constexpr int32 UniverseCount = 7;

	/** Where he stands: the middle of the gallery floor, in the actor's frame. */
	FVector GalleryCentre() const { return FVector(-GalleryDistance, 0.0, 0.0); }

	// --- components ---------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UStaticMeshComponent> Star;

	/** A glass skin a little larger than the star: its rim burns. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UStaticMeshComponent> Corona;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UPointLightComponent> StarLight;

	/** A faint blue light from the far side, so what faces away from the star is not a hole. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	/** Fixed exposure and the bloom: the whole level's look, carried by the city. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UPostProcessComponent> Look;

	// THE INSTANCE LISTS ARE TRANSIENT, as the court's parts are: the constructor makes the
	// components and Build fills them, at construction and again at play.

	// Each shell's lattice (LayLattice): its struts, thought running down them, and the nodes
	// where they meet.
	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShellStruts;

	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShellNodes;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> Rings;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> RingLights;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> SkyStars;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UStaticMeshComponent> GalleryFloor;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> GalleryRail;

	/** The light round the floor's edge, and a circle inlaid in it. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> GalleryLines;

	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UStaticMeshComponent>> UniverseGlass;

	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> UniverseStars;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "1000"))
	float StarRadius = 8000.0f;

	/** From the star's centre to the middle of the gallery. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "10000"))
	float GalleryDistance = 26000.0f;

	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "500"))
	float GalleryRadius = 1500.0f;

	/** The star's light where he stands, lux. With the fixed exposure below, ~3 is a dim room. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "0"))
	float GalleryLux = 3.0f;

	/** The fixed exposure's bias, EV: up is brighter. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	float ExposureBias = 0.0f;

	/** The star's brightness: its core's emissive (M_GenesisStar's Intensity). 60 was a white
	    disc with no edge; a few, and the limb darkening shows. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	float StarGlow = 4.0f;

	UPROPERTY(EditAnywhere, Category = "AI City")
	float CityGlow = 12.0f;

	/** Degrees a second each shell turns, inner to outer. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	TArray<float> ShellSpin = { 0.6f, -0.35f, 0.2f };

	UPROPERTY(EditAnywhere, Category = "AI City")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category = "AI City")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	/** M_GoSurface (Color x Pattern), for everything that is lit rather than lighting. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void Build();     // every instance, from fixed seeds: the same city every time
	void Paint();     // the materials
	void Light();     // the star's light and the look
	UMaterialInstanceDynamic* Glow(const FLinearColor& Color, float Intensity);
	UMaterialInstanceDynamic* Glass(const FLinearColor& Color, float Intensity, float RimOpacity, float BaseOpacity);
	UMaterialInstanceDynamic* Surface(const FLinearColor& Color, float Roughness);

	float Age = 0.0f;
	// The city's answer: seconds into it, or -1 when quiet; whether it is the long first one;
	// and each shell's nodes, to breathe.
	float ResponseAge = -1.0f;
	bool bFirstResponse = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ResponseLights;
	// Each shell's struts, so the answer can surge through them.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ThoughtLights;
	TArray<FVector> UniverseHome;     // each bubble's resting place, for its bob
	TArray<FVector> UniverseAxis;     // each galaxy's spin axis
	TArray<float> UniverseRate;       // and how fast
};
