// BasketballCourt.h
//
// THE THREE-POINT CONTEST'S COURT - a half-court in the Transhuman Chess Park (L_Chess), reached
// by the BASKETBALL door in the living room (Walt, 2026-10-15: "let's do the basketball shooting
// contest"; "in the chess park"). Leonard walks up and presses E: the five AI agents shoot their
// rounds, one after another, and then it is his turn.
//
// ===========================================================================
// THE HOUSE PATTERN (AGoTable, ABowlingLane): the contest is FBasketballContest
// (BasketballSim.h), a pure model tested standalone; this actor only presents it. It takes his
// input and the view while the contest runs and gives both back in one place (StopPlaying);
// its parts are built in the constructor so the court can be seen and placed; interaction goes
// through IInteractable; text goes to the HUD's table panel.
//
// THE COURT IS BUILT HERE, from the engine's own basic shapes: the slab, every painted line (the
// arc, the key, the free-throw circle) as thin flat pieces, the pole, the backboard and its
// square, a rim of short segments, a net of strands, five racks of five balls. No purchased
// asset, so it can be rendered and fixed by eye. M_GoSurface (the Go table's material) gives
// the balls their leather, T_Basketball (make_basketball_art.py); everything else is a colour.
//
// THE AGENTS' BODIES (2026-10-15): the shooter's own MetaHuman stands at her rack and throws
// each ball with Mixamo's "Throw" (import_mixamo_basketball.py), dribbles to the next rack, and
// cheers or shrugs at the end. Her throw is started early enough that her hand lets go at the
// moment the model shoots, and the ball flies from her hand to where the model says it lands.
// The animation decides nothing: the game decides every shot, never the chat model.
//
// Frame: the actor's origin is the floor directly under the middle of the rim, as in the model.
// Local +X points out from the basket toward the top of the key, +Y to the left as seen from the
// top of the key facing the basket. Centimetres.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BasketballSim.h"
#include "BasketballCourt.generated.h"

class ASibeliusHUD;
class APlayerController;
class UAnimInstance;
class UAnimSequence;
struct FStreamableHandle;
class UBoxComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;

UENUM()
enum class EHoopState : uint8
{
	Idle,           // nobody is shooting; E starts a contest
	AgentRound,     // an agent's sixty seconds
	PlayerRound,    // Leonard's
	Between,        // a short pause between shooters
	Results,        // the standings, until [R] or he walks away
};

UCLASS()
class SIBELIUSGAME_API ABasketballCourt : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABasketballCourt();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void Tick(float DeltaSeconds) override;

	static constexpr int32 RimSegments = 24;
	static constexpr int32 NetStrands = 12;
	static constexpr int32 FlightBallCount = 3;   // an agent's next ball can leave before the last lands
	static constexpr int32 RackBalls = FBasketballContest::Racks * FBasketballContest::BallsPerRack;

	// --- components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The concrete the court is painted on, its top at the actor's origin. */
	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> Slab;

	/** The painted court inside the lines, and the key. */
	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> PlayingSurface;

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> KeyPaint;

	// THE PART LISTS ARE TRANSIENT, as the Go table's are: the constructor builds them, and a
	// placed court's saved copy must never replace them (AGoTable, 2026-10-10).

	/** Every white line: the baseline, the sidelines, the half-court line, the key, the arc. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> Lines;

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> Arm;

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> Backboard;

	/** The backboard's border and the square over the rim. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> BoardMarks;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> RimParts;

	/** The net: strands from the rim, crossing, down to a smaller ring. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> NetParts;

	/** Each rack is a top and two legs; Racks * 3 parts. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> RackParts;

	/** The balls waiting on the racks: rack * 5 + ball. The fifth of each is the money ball. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> RackBallMeshes;

	/** The balls in the air. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Basketball")
	TArray<TObjectPtr<UStaticMeshComponent>> FlightBalls;

	/** What the interaction trace lands on: the hoop, from the pole to the rim. Blocks only
	    Visibility, so he can walk under it. */
	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UBoxComponent> HoopBox;

	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UCameraComponent> CourtCamera;

	/** The ball in the shooter's hand: from the rack to her palm as she winds up, then gone
	    at the release, where a flight ball takes over; bouncing while she dribbles. */
	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<UStaticMeshComponent> HandBall;

	/** Leonard at the rack in his own round: Manny, seen from behind as the agents are. */
	UPROPERTY(VisibleAnywhere, Category = "Basketball")
	TObjectPtr<USkeletalMeshComponent> CourtManny;

	// --- tuning -------------------------------------------------------------------------

	/** How fast an agent's round plays: 2 shows her sixty seconds in thirty. */
	UPROPERTY(EditAnywhere, Category = "Basketball", meta = (ClampMin = "0.5", ClampMax = "6.0"))
	float AgentSpeed = 2.0f;

	/** The pause between shooters, for the score to be read. */
	UPROPERTY(EditAnywhere, Category = "Basketball", meta = (ClampMin = "0.0"))
	float BetweenSeconds = 2.5f;

	/** Leonard's meter: how long it takes to fill, and where in it the sweet spot is (0..1). */
	UPROPERTY(EditAnywhere, Category = "Basketball", meta = (ClampMin = "0.3"))
	float MeterSeconds = 1.1f;

	UPROPERTY(EditAnywhere, Category = "Basketball", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float MeterSweetSpot = 0.78f;

	/** Sauce for Leonard's place: first, second, third, and anything lower. */
	UPROPERTY(EditAnywhere, Category = "Basketball")
	TArray<int32> PlacePrizes = { 60, 30, 15, 5 };

	UPROPERTY(EditAnywhere, Category = "Basketball", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 0.8f;

	/** M_GoSurface (Color x Pattern), hard-referenced by the class so it cooks; cleared, the
	    engine's BasicShapeMaterial (Color only - the balls go plain orange). */
	UPROPERTY(EditAnywhere, Category = "Basketball")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(EditAnywhere, Category = "Basketball")
	TObjectPtr<UTexture> BallTexture;

	/* WHO HE IS ON THE COURT (Walt, 2026-10-15: "i would rather the player be Manny than
	   Greystone, Greystone looks silly standing on the court with his sword").

	   His body is the player Blueprint's, hidden from his own eyes; the court's camera is not
	   his eyes, so it shows. It was Greystone, sword and all, and since make_player_manny.py
	   (the same day) it is Manny everywhere but battle form. For the contest the court still
	   makes sure: a body that is anything but Manny is dressed as him and given back when he
	   walks away, the one place his controls come back (StopPlaying). Hard references, so
	   they cook with the court; Manny's mesh is the first-person arms' own and is loaded
	   anyway. Cleared, he stays as he is. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Leonard")
	TObjectPtr<USkeletalMesh> PlayerBodyMesh;

	/** The third-person template's Manny: stands, idles, walks. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Leonard")
	TSubclassOf<UAnimInstance> PlayerBodyAnim;

	// --- the shooters' bodies (import_mixamo_basketball.py; the _MH clips for the agents, the
	// _Manny ones for Leonard). Hard references, so they cook with the court. ---------------

	/** Off: the rounds are the ball and the scoreboard only, as before the bodies came. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	bool bShooterBodies = true;

	/** Mixamo "Throw": standing, one hand over the top; it starts and ends at rest. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> AgentThrow;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> AgentDribble;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> AgentCheer;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> AgentSad;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> MannyThrow;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> MannyDribble;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> MannyCheer;

	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies")
	TObjectPtr<UAnimSequence> MannySad;

	/** Seconds into the throw that the hand lets go (measured: frame 24 of 66, both bodies). */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies", meta = (ClampMin = "0.1"))
	float ReleaseSeconds = 0.8f;

	/** Seconds into the throw that the ball reaches her palm from the rack: before it her hand
	    hangs by her thigh, where a ball would sit inside it (measured). */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies", meta = (ClampMin = "0.05"))
	float PickupSeconds = 0.4f;

	/** A round of this many points or more ends in a cheer; fewer, a shrug. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies", meta = (ClampMin = "0"))
	int32 CheerScore = 12;

	/** How fast the cheer and the shrug play; the pause between shooters waits for them. */
	UPROPERTY(EditAnywhere, Category = "Basketball|Bodies", meta = (ClampMin = "0.5"))
	float ReactRate = 1.25f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// --- the court ----------------------------------------------------------------------

	void MakeMaterials();
	UMaterialInstanceDynamic* Paint(const FLinearColor& Color, float Roughness, UTexture* Pattern = nullptr);
	void ResetRackBalls();
	FVector Local(const FHoopVec3& V) const;            // model to actor-local
	FVector World(const FHoopVec3& V) const;            // model to world

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	// --- the contest --------------------------------------------------------------------

	void StartPlaying(APlayerController* PC);
	void StopPlaying();   // the one place his controls come back
	void BindCourtKeys();
	void NewContest();
	void BeginShooter(int32 Index);
	void FinishShooter();
	void ShowResults();
	void LaunchBall(const FShot& Shot);
	void UpdateFlights(float DeltaSeconds);
	void UpdateMeter(float DeltaSeconds);
	void ReleaseMeter();
	void AimCamera(bool bInstant);
	void RefreshPanel();
	FString RacksLine() const;
	FString StandingsLine() const;
	ASibeliusHUD* GetHud() const;

	/** On: Manny for the contest. Off: his own body back, exactly as it was. */
	void DressPlayer(bool bOn);
	/** In his own round Manny stands at the rack for him, and his body would be a second one. */
	void ShowPlayerBody(bool bShow);

	// --- the shooters' bodies -----------------------------------------------------------

	enum class EBodyMove : uint8 { None, Rest, Throw, Glide, React };

	void LoadAgentBodies();
	void SpawnShooterBody();     // this shooter's, at rack 0; the last one's goes
	void DespawnShooterBody();
	void PlaceShooter(const FVector2D& Spot);
	void PlayBody(UAnimSequence* Anim, bool bLoop, float Rate, float StartAt = 0.0f);
	void HoldRest();
	void StartThrow(float StartAt, float Rate);
	void UpdateShooterBody(float DeltaSeconds);
	void UpdateHandBall();
	bool PalmBallAt(FVector& OutWorld) const;   // where a ball held in her right palm is now
	float BodyPosition() const;
	UAnimSequence* BodyAnim(UAnimSequence* ForAgent, UAnimSequence* ForManny) const;
	FVector2D SpotXY(int32 Rack) const;

	UPROPERTY()
	TObjectPtr<AActor> ShooterActor;              // her MetaHuman; null in Leonard's round

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ShooterMesh;   // her Body, or CourtManny

	TSharedPtr<FStreamableHandle> BodyLoad;
	EBodyMove BodyMove = EBodyMove::None;
	double ThrowFor = -1.0;          // the ball (its NextBallAt) the throw under way is for
	int32 BodyRack = 0;
	double GlideStart = 0.0;
	double GlideEnd = 0.0;
	FVector2D GlideFrom = FVector2D::ZeroVector;
	FVector2D GlideTo = FVector2D::ZeroVector;
	FVector PickupFrom = FVector::ZeroVector;
	bool bHandBallLive = false;      // the ball is hers until the release
	bool bDribbling = false;         // the glide has begun: the dribble is playing

	void OnShootPressed();
	void OnShootReleased();
	void OnSkipShooter();
	void OnSkipToPlayer();
	void OnAgain();
	void OnLeave();

	TArray<FShooterStyle> Order;      // the five agents, then Leonard
	TArray<int32> Scores;             // -1: not yet shot
	int32 Current = -1;
	FBasketballContest Contest;
	EHoopState State = EHoopState::Idle;
	float BetweenLeft = 0.0f;
	uint64 ContestSeed = 1;
	int32 PlayerPlace = -1;
	int32 PrizeGiven = 0;

	// Leonard's meter.
	bool bMeterHeld = false;
	float Meter = 0.0f;
	FString LastShotNote;

	// The balls in the air: out along the solved arc to the rim, then whatever the rim made of it.
	struct FFlight
	{
		bool bActive = false;
		FShot Shot;
		float T = 0.0f;              // seconds since release
		bool bAfter = false;         // past the rim: falling through, off the iron or the board
		FVector AfterPos = FVector::ZeroVector;
		FVector AfterVel = FVector::ZeroVector;
		float AfterT = 0.0f;
	};
	FFlight Flights[FlightBallCount];

	// The scoreboard shows a shot only once its ball reaches the rim: the model knows at the
	// release, and the panel must not tell him before his eyes do.
	int32 Revealed = 0;
	int32 RevealedScore() const;

	TWeakObjectPtr<APlayerController> PlayingPC;
	bool bPlaying = false;

	// What DressPlayer took off him, to put back.
	TWeakObjectPtr<USkeletalMeshComponent> DressedBody;
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SavedBodyMesh;
	UPROPERTY()
	TSubclassOf<UAnimInstance> SavedBodyAnim;
	bool bSavedBodyVisible = true;
	bool bKeysBound = false;
	FVector CameraGoal = FVector::ZeroVector;
	FRotator CameraGoalRot = FRotator::ZeroRotator;
	int32 CameraRack = -1;        // the rack the camera was last aimed at

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BallMaterial;

	/** The money balls: the same leather, gilded, so the fifth ball of a rack stands out. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MoneyBallMaterial;
};
