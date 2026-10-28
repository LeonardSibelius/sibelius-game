// LanternBurst.h
//
// LANTERN BURST — the lantern of glass orbs in Kaia's jungle (Walt, 2026-10-27: "Please make a new
// game that is addictive and attractive. I don't care what genre or topic it is."). The rules are
// FLanternBurstSim's; this actor is what he sees and plays: a crowned lantern of glowing orbs
// hanging on chains from a hook in the air, a ring of embers under it, and at its foot a launcher
// holding the loaded orb with the next one beside it.
//
// HE AIMS WITH THE MOUSE at the place on the lantern he wants: the shot climbs the glass toward
// it - a corkscrew when he aims off the vertical - and a dotted guide shows the way it will go and
// where it will stick, the orbs it would burst lit white and the ones it would bring down
// shimmering. CLICK shoots; RIGHT CLICK or SPACE swaps the loaded orb and the next; A D (or the
// wheel) turn the lantern to bring its far side round; R twice starts over; ESC or E walks away.
//
// WHAT HE SEES WHEN IT GOES WELL: bursts ripple out from the shot, each orb flashing and breaking
// into sparks of its colour; what no longer hangs drops, bounces on the ground and sparks out; the
// light inside the lantern flares the colour that burst. When it goes badly the crown comes down
// a link at a time toward the embers, and they glow harder the nearer it gets.
//
// The house lessons (OrbitGolfCourse.h, HyperBilliardsTable.h): the model decides, this actor
// shows; his body steps aside while he plays and comes back; every mesh and material is a hard
// reference. Every orb, spark, guide dot and ember is an instance of one sphere in one material
// (M_LanternOrb, made by Tools/Scripts/make_lantern_assets.py), coloured and lit per instance by
// custom data: R, G, B, glow.
//
// Dev (they find the play world themselves, so the editor's console reaches a PIE session):
// sib.Lantern.Play takes the first player to the first lantern; sib.Lantern.Fire turns it to the
// best shot it can find and fires; sib.Lantern.Aim D and sib.Lantern.Turn D set the aim and
// the lantern's turn in degrees (for pictures of a game under way).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "LanternBurstSim.h"
#include "LanternBurst.generated.h"

class ASibeliusHUD;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UCapsuleComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API ALanternBurst : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALanternBurst();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The lid on top, gold, and its glowing band. They come down with the crown. */
	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<USceneComponent> Crown;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TArray<TObjectPtr<UStaticMeshComponent>> CrownParts;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TArray<TObjectPtr<UStaticMeshComponent>> Chains;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UStaticMeshComponent> Hook;

	/** A thin flame up the middle, seen through the gaps between the orbs. */
	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TArray<TObjectPtr<UStaticMeshComponent>> LauncherParts;

	/** The launcher and the next orb's stand, solid to the eye up to 1.3 m. */
	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UBoxComponent> Body;

	/** Keeps him from walking into the lantern, and is where E finds it: looking at the lantern
	    anywhere within reach is enough. (At first only the launcher's low box answered, and the
	    interactor's 30 cm sweep met the ground and the plants first - Walt, 2026-10-27: no prompt,
	    and E did nothing.) Blocks pawns and the Visibility channel only. */
	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UCapsuleComponent> Column;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UInstancedStaticMeshComponent> Orbs;     // the lantern

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UInstancedStaticMeshComponent> Live;     // the hand, the ghost, the orb in flight

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UInstancedStaticMeshComponent> Guide;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UInstancedStaticMeshComponent> Effects;  // sparks, bursting and falling orbs

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UInstancedStaticMeshComponent> Embers;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Lantern Burst")
	TObjectPtr<UCameraComponent> LanternCamera;

	/** Every orb's material: per-instance custom data 0-2 colour, 3 glow. */
	UPROPERTY(EditAnywhere, Category = "Lantern Burst")
	TObjectPtr<UMaterialInterface> OrbMaterial;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst", meta = (ClampMin = "300.0"))
	float CameraDistance = 1050.0f;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst")
	float CameraHeight = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst")
	float LookHeight = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst", meta = (ClampMin = "30.0", ClampMax = "110.0"))
	float CameraFieldOfView = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst", meta = (ClampMin = "10.0"))
	float TurnDegreesPerSecond = 80.0f;

	/** Orb diameters a second, up the glass. */
	UPROPERTY(EditAnywhere, Category = "Lantern Burst", meta = (ClampMin = "4.0"))
	float ShotSpeed = 26.0f;

	/** Degrees a second the lantern turns by itself while nobody plays. */
	UPROPERTY(EditAnywhere, Category = "Lantern Burst")
	float IdleSpin = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Lantern Burst", meta = (ClampMin = "0.0"))
	float LightCandelas = 35.0f;

	// The dev commands' entry points (see the header comment).
	void DevStart();
	void DevShootBest();
	void DevAim(double Degrees);    // held until the mouse next moves
	void DevTurn(double Degrees);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	struct FFx
	{
		enum class EKind : uint8 { Spark, Bursting, Falling, Lost };
		EKind Kind = EKind::Spark;
		FVector Pos = FVector::ZeroVector;   // actor space
		FVector Vel = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		float Delay = 0.0f;
		float Age = 0.0f;
		float Life = 1.0f;
		float Size = 10.0f;                  // diameter, cm
		int32 Bounces = 0;
	};

	void StartPlaying(APlayerController* PC);
	void StopPlaying();
	void BindKeys();

	void OnFire();
	void OnSwap();
	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnRightReleased();
	void OnWheelUp();
	void OnWheelDown();
	void OnRestart();
	void OnLeave();

	void NewGame();
	void Launch();               // the loaded orb leaves; the model hears of it when it lands
	void Land();
	void HandleEvents();
	void UpdateAim();
	void RefreshPreview();
	void TickFlight(float DeltaSeconds);
	void TickEffects(float DeltaSeconds);
	void Draw();
	void SyncCamera(float DeltaSeconds, bool bSnap);
	void SyncFrame();            // crown, chains, core and light, from the crown's height
	void RefreshPanel();
	void ApplyPaint();
	void SaveBest();
	void LoadBest();

	// Where things are, in actor space.
	double FrontColumn() const;                         // the lantern's column facing the launcher
	double Row0Z() const;                               // height of row 0's centres, as the crown hangs now
	FVector PointOnLantern(double X, double Y) const;   // the unrolled lantern, rolled up
	FVector CellPosition(int32 Row, int32 Col) const;
	FVector LauncherPosition() const;
	FVector NextPosition() const;
	bool FacesFront(double X) const;

	void SpawnBurst(const FVector& At, const FLinearColor& Color, int32 Count, float Speed, float Delay);
	static FLinearColor ColorOf(int32 Orb);
	ASibeliusHUD* GetHud() const;

	FLanternBurstSim Sim;
	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;
	bool bKeysBound = false;

	double Turn = 0.0;            // radians the lantern is turned
	double TurnTarget = 0.0;
	bool bLeftHeld = false;
	bool bRightHeld = false;
	double AimAngle = 0.0;
	FVector2D LastMouse = FVector2D(-1.0, -1.0);

	// The preview of the shot as aimed.
	bool bPreviewValid = false;
	FLanternTrace PreviewTrace;
	FLanternBurstSim::FPreview PreviewResult;
	TSet<int32> PreviewBurst;     // Row * Columns + Col
	TSet<int32> PreviewFall;
	double PreviewTurn = 1e9;
	double PreviewAngle = 1e9;
	int32 PreviewShots = -1;
	int32 PreviewHand = -1;

	// The orb in flight.
	bool bFlying = false;
	TArray<FVector2D> FlightPath; // unrolled
	TArray<double> FlightLength;  // along the path to each point
	double FlightDistance = 0.0;
	double FlightLaunch = 0.0;
	double FlightAngle = 0.0;
	int32 FlightColor = 0;

	TArray<FFx> Fx;
	bool bLanternFallen = false;  // game over: every orb is on its way down
	float AppearClock = 10.0f;    // since the stage was hung
	float VisualLowered = 0.0f;
	float LightFlash = 0.0f;
	FLinearColor FlashColor = FLinearColor::White;
	float Shake = 0.0f;
	float Clock = 0.0f;
	double RestartArmedUntil = -1.0;
	FString LastNote;
	double LastNoteUntil = 0.0;

	int64 BestScore = 0;
	int32 BestStage = 0;
	bool bNewBestThisGame = false;
	int64 BestAtGameStart = 0;
};
