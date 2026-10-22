// AICity.h
//
// THE AI CITY - what is through the portal at the end of the upstairs hall (AICityPortal, a
// CompilePortal: C takes him, once Compile is his - since 2026-10-22, when it replaced the seed
// that wanted all six powers), in L_Genesis. Walt, 2026-10-15:
// "Something cosmic. Something that shows a vision of the future where AI has solved anything
// and can build anything and this is what the AI have built to amuse themselves. Not a place
// for transhumans ... but an AI city."
//
// THE DOME (2026-10-22). Walt, of the glowing lattice that was here: "nice try but actually not
// very good ... I want to feel awestruck by the scene, not underwhelmed." He bought cglib team's
// "Megastructure: Dome 1" (Fab, /Game/Megastructure_Dome_1, gitignored): a geodesic shell of 162
// hexagonal cells, thick with city, in seven concentric layers. Where the pack is present the
// city is that dome, 3.45 km in radius round the star; the lattice is the fallback for a clone of
// the public repo without it. Nothing of the pack is saved in L_Genesis: the layers are loaded by
// path, here.
//
// THE VISIT (the same day): "drop all the plot about universe building. I want the scene to begin
// with the player flying into the sphere from outside, far enough outside that stars are visible
// and then the player lands among the details in the sphere ... The only game option I want
// available in L_Genesis for now is simply to fly across the sphere and land in a detailed spot
// on the other side." He arrives 25 km out among the stars, flies in through one of the cells,
// past the star, and down onto a landing pad on the far inner face; [F] flies him across to the
// pad on the opposite side, and back. The flights are the city's own camera (FlightCam) on a
// path, not something he steers; [Space] cuts one short.
//
// STANDING ON THE INSIDE OF A SPHERE, where "down" is outward: rather than bend gravity, the city
// turns. Everything he sees is a part of this actor, so its turning is invisible; at each landing
// it has turned so that the pad is below him and the star straight overhead, and he walks on it
// as on any floor.
//
// THE PADS, because the dome cannot be stood on: its material displaces the surface by up to
// 1.5 cm at the pack's scale - 15 m at ours - and no collision follows displacement. So each
// landing is a dark platform on three legs with a glass rail, set just above the relief on a
// ridge between two of the dome's junction spires (AICityDetail::Landings, measured from the
// pack's meshes, 2026-10-22).
//
// Frame: the actor's origin is the star's centre. Centimetres.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/InterpCurve.h"
#include "AICity.generated.h"

class APawn;
class UCameraComponent;
class UChildActorComponent;
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

	static constexpr int32 ShellCount = 3;
	static constexpr int32 DomeLayerCount = 7;
	static constexpr int32 PadCount = 2;

	/** On the way in, or across: the city's camera has him. */
	bool IsFlying() const { return Flight != EFlight::None; }

	// --- components ---------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UStaticMeshComponent> Star;

	/** Celestial Fortune's sun, a disc that always faces whoever is looking. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UStaticMeshComponent> Corona;

	/** The star's light, on the dome (lighting channel 1) and on him and the pads (channel 0). */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UPointLightComponent> StarLight;

	/** A faint blue light from the far side, so what faces away from the star is not a hole. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	/** Fixed exposure and the bloom: the whole level's look, carried by the city. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UPostProcessComponent> Look;

	/** The dome's layers, inner to outer: the pack's own blueprints, which paint themselves. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TArray<TObjectPtr<UChildActorComponent>> DomeLayers;

	// THE INSTANCE LISTS ARE TRANSIENT, as the court's parts are: the constructor makes the
	// components and Build fills them, at construction and again at play.

	/** The fallback lattice's struts and nodes, a shell each - empty while the dome is up. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShellStruts;

	UPROPERTY(VisibleAnywhere, Transient, Category = "AI City")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShellNodes;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> SkyStars;

	/** The landing pads: a floor each, and between them their rails, edge lights and legs. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TArray<TObjectPtr<UStaticMeshComponent>> PadFloors;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> PadRails;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> PadLines;

	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UInstancedStaticMeshComponent> PadLegs;

	/** The camera that flies him: in the city's frame, so it turns with it. */
	UPROPERTY(VisibleAnywhere, Category = "AI City")
	TObjectPtr<UCameraComponent> FlightCam;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "1000"))
	float StarRadius = 8000.0f;

	/** The fixed exposure's bias, EV: up is brighter. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	float ExposureBias = 0.0f;

	/** The star's brightness: its core's emissive (M_GenesisStar's Intensity). 60 was a white
	    disc with no edge; a few, and the limb darkening shows. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	float StarGlow = 4.0f;

	UPROPERTY(EditAnywhere, Category = "AI City")
	float CityGlow = 12.0f;

	/** The dome, where the pack is present; off, the lattice shells. */
	UPROPERTY(EditAnywhere, Category = "AI City")
	bool bUseDome = true;

	/** The dome's radius. The pack's shell is 6.9 m across: x1000 makes its buildings buildings. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "50000"))
	float DomeRadius = 345000.0f;

	/** The star's light on the dome's inner face and on the pads, lux. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "0"))
	float DomeLux = 4.0f;

	/** A landing pad's radius. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "300"))
	float PadRadius = 1200.0f;

	/** How far a pad's floor stands above the ridge it is on: clear of the 15 m of displacement. */
	UPROPERTY(EditAnywhere, Category = "AI City", meta = (ClampMin = "0"))
	float PadLift = 1600.0f;

	/** Degrees a second each fallback lattice shell turns, inner to outer. */
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
	virtual void PostInitializeComponents() override;

private:
	enum class EFlight : uint8 { None, Intro, Across };

	void Build();     // every instance, from fixed seeds: the same city every time
	bool RaiseDome(); // the pack's layers round the star; false when the pack is not here
	void QuietDome(); // no collision, no shadow, lit only by the star: whatever layers exist now
	void Paint();     // the materials
	void Light();     // the star's light and the look
	UMaterialInstanceDynamic* Glow(const FLinearColor& Color, float Intensity);
	UMaterialInstanceDynamic* Glass(const FLinearColor& Color, float Intensity, float RimOpacity, float BaseOpacity);
	UMaterialInstanceDynamic* Surface(const FLinearColor& Color, float Roughness);

	/** A pad in the city's frame: origin on its floor's centre, X along its ridge, Z toward the star. */
	FTransform PadFrame(int32 Pad) const;
	/** The city's rotation that puts a pad below him, its ridge running along world X. */
	FQuat PadBelow(int32 Pad) const;

	void StartFlight(EFlight Kind, int32 ToPad);
	void EndFlight();
	/** The visit's pawn put away while the city's camera has him (true), or set down on the pad he flew to. */
	void HoldPawn(APawn* Pawn, bool bHold);
	/** The sun's disc turned to face the eye. */
	void FaceSun();

	float Age = 0.0f;
	bool bDomeUp = false;
	bool bSunDisc = false;
	bool bVisitStarted = false;

	EFlight Flight = EFlight::None;
	float FlightAge = 0.0f;
	float FlightLength = 0.0f;
	int32 FlightTo = 0;
	int32 OnPad = -1;
	FQuat TurnFrom = FQuat::Identity;
	FQuat TurnTo = FQuat::Identity;
	float TurnStart = 0.0f;
	float TurnEnd = 0.0f;
	FInterpCurveVector PathPlace;
	FInterpCurveQuat PathTurn;
};
