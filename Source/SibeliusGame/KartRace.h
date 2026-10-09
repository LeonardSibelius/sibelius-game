// KartRace.h
//
// THE TRANSHUMAN GRAND PRIX — Leonard against the five agents, three laps round the streets
// of Transhuman City (L_Kart, a copy of L_City), reached from the go-kart parked in the
// living room (AKartLauncher). Walt, 2026-10-09: "Transhuman City streets, go ahead and
// build it."
//
// ===========================================================================
// THE HOUSE PATTERN (AChessTable, ABowlingLane): the race is FKartRace (KartSim.h), a pure
// model tested standalone; this actor only presents it. It takes his input and the view
// target while he races and gives both back in one place (StopPlaying); interaction goes
// through IInteractable; text goes to the HUD's table panel; sounds are synthesised
// (TableSounds), never recorded.
//
// THE TRACK is TrackSpline: its points are the street corners the race turns at, in order,
// set by Tools/Scripts/make_kart_city.py from a survey of the city's streets. The model
// rounds each corner and keeps every kart within HalfWidth of the line; red and white
// kerb blocks mark that edge, so what the player sees is what the karts obey. The city's
// own buildings and lamp posts take no part - nothing here collides with anything.
//
// THE KARTS ARE PLAIN SHAPES for now - the engine's cube, cylinder and sphere, painted per
// driver - so the handling can be judged before any art is bought. Each sits on the street
// by a trace straight down, tilted to the ground under it.
//
// Frame: the model's X and Y are the world's. The model's "left" (+Y of its heading) is the
// world's right, because Unreal's axes are left-handed - which is why steering right on the
// keyboard is +1 to the model.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "KartSim.h"
#include "KartRace.generated.h"

class ASibeliusHUD;
class APlayerController;
class UAudioComponent;
class UBoxComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class EKartAgentPace : uint8
{
	Gentle,
	Racing,
	Fierce,
};

// One kart on the street: its root and the parts that change while it races.
USTRUCT()
struct FKartVisual
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Shell;     // body, nose, wing: hidden while a chest

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Chest;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Flame;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Name;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;

	float GroundZ = 0.0f;
	FVector GroundNormal = FVector::UpVector;
	bool bHaveGround = false;
};

// The plain-shapes kart, as a table both the race and the living room's launcher build from.
namespace KartShapes
{
	enum class EPaint : uint8 { Body, Trim, Driver, Flame, Spark, Chest };
	enum class EShape : uint8 { Cube, Cylinder, Sphere };

	struct FPart
	{
		const TCHAR* Name;
		EShape Shape;
		FVector Location;       // kart-local cm: +X forward, Z up, origin on the ground
		FRotator Rotation;
		FVector Scale;          // of the engine's 100 cm shapes
		EPaint Paint;
		bool bShell;            // hidden while the kart is a treasure chest
	};

	const TArray<FPart>& Parts();
	FLinearColor DriverColor(int32 Kart);   // 0..4 the agents, 5 Leonard

	// THE RACE KART MODEL (KartRace.cpp tells how it was measured: its nose is the mesh's +Y,
	// its origin on the ground midway between the axles, its seat 30 cm up).
	const TCHAR* const ModelPath = TEXT("/Game/Fab/Race_kart/Race_Kart.Race_Kart");
	const FRotator ModelRotation(0.0f, -90.0f, 0.0f);       // the mesh's +Y to the kart's +X

	// The pack's four accent colours, nearest to each driver: 0 red, 1 blue, 2 green, 3 yellow.
	int32 AccentFor(int32 Kart);

	// Paints the model for one driver: the body her colour, wheels and steering wheel the
	// pack's accent nearest it. Accents are [wheel red, blue, green, yellow, steering red, ...].
	void PaintModel(UStaticMeshComponent* Model, int32 Kart, const TArray<TObjectPtr<class UTexture2D>>& Accents,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& Keep);

	// Loads the accent textures by name, as hard references so they reach the pak.
	void FindAccents(TArray<TObjectPtr<class UTexture2D>>& Out);
}

UCLASS()
class SIBELIUSGAME_API AKartRace : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AKartRace();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The street corners the race turns at, in order round the loop (world space). */
	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<USplineComponent> TrackSpline;

	/** What the interaction trace lands on: his kart on the grid. */
	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UBoxComponent> Seat;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UInstancedStaticMeshComponent> KerbRed;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UInstancedStaticMeshComponent> KerbWhite;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UInstancedStaticMeshComponent> LineDark;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UInstancedStaticMeshComponent> LineLight;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TArray<TObjectPtr<UStaticMeshComponent>> BoxMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleMeshes;

	/** Code Vision's racing line: dots on the road ahead, coloured by the speed to be doing. */
	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TArray<TObjectPtr<UStaticMeshComponent>> LineDots;

	// --- tuning -------------------------------------------------------------------------

	/** Half the road's width, in cm: the karts' centres stay a kart's radius inside it. */
	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "200.0"))
	float HalfWidth = 600.0f;

	/** The road's half-width at each TrackSpline point, in order; 0 or missing = HalfWidth.
	    A narrowing is two extra points on a straight with a smaller width. */
	UPROPERTY(EditAnywhere, Category = "Kart")
	TArray<float> CornerHalfWidths;

	/** Every corner's radius at the centre line. Must exceed the road's width at each corner. */
	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "250.0"))
	float CornerRadius = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "1", ClampMax = "9"))
	int32 Laps = 3;

	UPROPERTY(EditAnywhere, Category = "Kart")
	EKartAgentPace AgentPace = EKartAgentPace::Racing;

	UPROPERTY(EditAnywhere, Category = "Kart")
	float CameraBack = 560.0f;

	UPROPERTY(EditAnywhere, Category = "Kart")
	float CameraHeight = 210.0f;

	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "40.0", ClampMax = "120.0"))
	float CameraFov = 82.0f;

	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.6f;

	/** The kart everyone drives: Polygonal Assets' Race kart (Fab, 2026-10-09, "Generated with
	    AI: No"; Content/Fab/Race_kart, gitignored). Without it, the plain shapes. */
	UPROPERTY(EditAnywhere, Category = "Kart")
	TObjectPtr<UStaticMesh> KartModel;

	/** The model is twice a real kart's size; this brings it to the race's own karts. */
	UPROPERTY(EditAnywhere, Category = "Kart", meta = (ClampMin = "0.1"))
	float KartModelScale = 0.85f;

	/** Rebuilt from TrackSpline on every construction, so the kerbs show in the editor. */
	UFUNCTION(CallInEditor, Category = "Kart")
	void RebuildTrack();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	static constexpr int32 KartCount = 6;
	static constexpr int32 PlayerKart = 5;      // the back of the grid: there is a field to pass

	// --- the track and the race -----------------------------------------------------------

	bool BuildModelTrack();
	void ResetRace(bool bRunning);              // everyone back on the grid
	double PaceValue() const;

	FKartTrack Track;
	FKartRace Race;
	bool bTrackOk = false;
	bool bRaceRunning = false;
	FString TrackError;

	// --- the look ---------------------------------------------------------------------------

	void BuildKarts();
	void PlaceKerbs();
	void UpdateKarts(float DeltaSeconds);
	void UpdateTrackProps(float DeltaSeconds);
	float GroundAt(const FVector2D& XY, float Guess, FVector* Normal = nullptr) const;
	FVector WorldAt(double S, double D, float Lift = 0.0f) const;
	UStaticMeshComponent* MakePart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location,
		const FRotator& Rotation, const FVector& Scale, UMaterialInterface* Material);
	UMaterialInstanceDynamic* Paint(const FLinearColor& Color);

	UPROPERTY()
	TArray<FKartVisual> Karts;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY()
	TArray<TObjectPtr<class UTexture2D>> Accents;          // the model's colour variants

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Paints;   // every MID made, kept alive

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DotGreen;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DotAmber;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DotRed;

	float PropClock = 0.0f;

	// --- him ------------------------------------------------------------------------------

	void StartPlaying(APlayerController* PC);
	void StopPlaying();                          // the one place his controls come back
	void BindRaceKeys();
	FKartInput ReadInput(float DeltaSeconds);
	void HandleEvents();
	void Say(const FString& Text, float Seconds = 2.5f);
	void RefreshPanel();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	bool IsCodeVisionOn() const;
	ASibeliusHUD* GetHud() const;
	FString DriverName(int32 Kart) const;

	void OnThrottleDown();
	void OnThrottleUp();
	void OnBrakeDown();
	void OnBrakeUp();
	void OnLeftDown();
	void OnLeftUp();
	void OnRightDown();
	void OnRightUp();
	void OnDriftDown();
	void OnDriftUp();
	void OnItem();
	void OnRestart();
	void OnLeave();
	void OnPace1();
	void OnPace2();
	void OnPace3();

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;
	bool bThrottleKey = false;
	bool bBrakeKey = false;
	bool bLeftKey = false;
	bool bRightKey = false;
	bool bDriftKey = false;
	bool bItemPressed = false;
	float SteerSmoothed = 0.0f;

	FString Message;
	float MessageLeft = 0.0f;
	float GoShown = 0.0f;
	int32 FinishedPlace = 0;

	FVector CameraEye = FVector::ZeroVector;
	float CameraYaw = 0.0f;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Engine;
};

// The go-kart parked in the living room: [E] to race the agents in Transhuman City. The same
// plain-shapes kart as the race, so the player knows what he is getting into. No sign over
// it (Walt, 2026-10-09: "i don't need the big yellow letters. The [E] instruction is enough").
UCLASS()
class SIBELIUSGAME_API AKartLauncher : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AKartLauncher();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UBoxComponent> Seat;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	/** The race kart model, red, when the pack is there; then the plain shapes hide. */
	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UStaticMeshComponent> Model;

	UPROPERTY(EditAnywhere, Category = "Kart")
	FName TravelTargetLevel = TEXT("L_Kart");

	UPROPERTY(EditAnywhere, Category = "Kart")
	FName ArrivalTag = TEXT("KartRace");

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY()
	TArray<TObjectPtr<class UTexture2D>> Accents;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Paints;
};
