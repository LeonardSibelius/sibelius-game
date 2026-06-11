// AIApparition.h
//
// The opening bang. Seconds after the game starts in the frumpy office, AI
// manifests: nine fate glyphs (the same sprites that orbit the Carousel of
// Fates — the ending foreshadowed in the first minute) spinning around a
// blazing white-gold core. The god-voice speaks, names the quest, and
// dissolves. Controls are locked for the duration and restored through ONE
// path (AP2 in docs/ai-apparition-notes.md).
//
// Placed by Tools/Scripts/build_ai_apparition.py (BUILD FIRST, then script —
// the script spawns this class). Visuals reuse M_fate_base + T_sym_*; the
// core uses M_ai_core (created by the script).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIApparition.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USoundBase;
class UMaterialInstanceDynamic;

UENUM()
enum class EApparitionPhase : uint8
{
	Waiting,
	Materializing,
	Speaking,
	Lingering,
	Dissolving,
	Done
};

UCLASS()
class SIBELIUSGAME_API AAIApparition : public AActor
{
	GENERATED_BODY()

public:
	AAIApparition();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// --- timing dials ---
	UPROPERTY(EditAnywhere, Category = "Apparition|Timing")
	float DelaySeconds = 3.0f;          // ordinary frumpiness before the bang

	UPROPERTY(EditAnywhere, Category = "Apparition|Timing")
	float MaterializeSeconds = 1.8f;

	UPROPERTY(EditAnywhere, Category = "Apparition|Timing")
	float FallbackSpeakSeconds = 9.0f;  // used only if the voice asset is missing (AP3)

	UPROPERTY(EditAnywhere, Category = "Apparition|Timing")
	float LingerSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Apparition|Timing")
	float DissolveSeconds = 1.5f;

	// --- look dials ---
	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float RingRadius = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float GlyphSize = 34.0f;            // cm, square

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float RingDegPerSec = 40.0f;        // quicker than the carousel — urgent, alive

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float RingTiltDeg = 18.0f;          // ring leans like a halo, not a table

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float CoreRadius = 55.0f;           // cm

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float CoreGlowPeak = 60.0f;         // emissive at the flare moment

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float CoreGlowSteady = 25.0f;       // emissive while speaking

	// While the cinematic plays, the camera turns itself toward the core —
	// a god does not wait to be noticed. Higher = snappier turn.
	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float GazeInterpSpeed = 2.5f;

	// Orientation knobs (AP1 — Details-fixable, never a recompile)
	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float GlyphPitch = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Apparition|Look")
	float GlyphYawOffset = 90.0f;

	// --- the voice ---
	// Defaults to /Game/AIApparition/S_ai_intro (imported by the script);
	// EditAnywhere so a different take is a Details swap.
	UPROPERTY(EditAnywhere, Category = "Apparition|Voice")
	TObjectPtr<USoundBase> VoiceLine;

private:
	void BuildVisuals();                       // OnConstruction — ring + core
	void SetPhase(EApparitionPhase NewPhase);
	void SetApparitionScale(float S);          // uniform, on the Hub's children
	void SetCoreGlow(float Glow);
	void LockPlayerInput(bool bLock);          // AP2: only EndCinematic/Begin call this
	void EndCinematic();                       // THE single restore path
	void SteerGaze(float DeltaSeconds);        // turn the locked camera toward the core

	UPROPERTY(VisibleAnywhere, Category = "Apparition")
	TObjectPtr<USceneComponent> Hub;

	UPROPERTY(VisibleAnywhere, Category = "Apparition")
	TObjectPtr<USceneComponent> RingPivot;     // tilted; spins in Tick

	UPROPERTY(VisibleAnywhere, Category = "Apparition")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Glyphs;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CoreMID;

	EApparitionPhase Phase = EApparitionPhase::Waiting;
	float PhaseTime = 0.0f;
	float SpeakSeconds = 9.0f;                 // resolved in BeginPlay from VoiceLine
	bool bInputLocked = false;                 // AP2 guard
};
