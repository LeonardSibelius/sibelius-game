// WrestlingMat.h
//
// THE WRESTLING MAT (Walt, 2026-11-13): "what i really want, agent wrestling matches, perhaps in
// AI City" - and then: "i actually don't want any hitting, just agents writhing around with each
// other." So: two of the five agents on a mat, on their knees to begin, grappling on the ground
// until one holds the other's shoulders down for a count of three. Best of three falls.
//
// NO ANIMATIONS. Nothing in the project, and nothing on Fab, shows two people struggling on a mat
// without strikes (the paired packs are throws and finishers). So each wrestler's body is physics
// - her MetaHuman's own ragdoll - with muscles (a UPhysicalAnimationComponent pulling every body
// toward a pose UGrapplePoseAnimInstance makes up from a few numbers) and a will: small pushes on
// her chest and hips toward what she is trying to do, and hands that grip the other's arms.
// Nobody animates the struggle; it is what happens when two bodies want opposite things.
//
// ONE SCRIPT FOR THE TWO OF THEM (Walt, 2026-11-13: "they look like they are having convulsions
// ... they should look like two women grappling in a wrestling match"). The first match gave each
// wrestler a mind of her own that changed several times a second and threw her at the other with
// pushes stronger than gravity: they met, bounced a metre apart through the air, and flew back in,
// every second and a half. Wrestling is the opposite - long slow strain, and now and then one
// decisive move. So the PAIR has a script (Direct), in beats that each last seconds:
//   the tie-up   - on their knees, collar and elbow, leaning in, one driving and the other giving
//                  ground, circling slowly, changing grips;
//   the shot     - the fresher one snaps the other down: forward onto her hands, then flat on her front;
//   the ground   - she sprawls on the other's shoulders; the one beneath works her knees up under her;
//   the ride     - turned over onto her back: shoulders down, and the referee counts;
//   the kick-out - a bridge, over onto her front and her knees under her, if she has it left - or
//                  the count reaches three;
//   the rise     - both back up to their knees, and the tie-up again.
// Each beat says where each wrestler's hips and chest should be and which way she faces, and she is
// CARRIED there (Carry): a damped spring to a target that itself moves smoothly, most of her weight
// taken for her - never a shove. The bodies are still physics, so the two still press, block and
// lean on each other; but nothing can throw anyone, every body is damped, and no limb can whip.
// Stamina decides who shoots, and whether the one beneath kicks out or is pinned.
//
// A SPORT, ON PURPOSE: a mat with a circle, a referee's count, a clock and falls. Two of the agents
// rolling on a floor reads as wrestling when there is a sport round it (the Steam point made to
// Walt the same day).
//
// The match lives on this actor and nothing else: AAICity spawns one on the landing pad that is
// not the premiere's, and strikes it before every flight, because the city turns under a flight
// and physics bodies would not turn with it. bAutoStart starts a match by itself (a test level).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "GrapplePoseAnimInstance.h"
#include "Math/RandomStream.h"
#include "WrestlingMat.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPhysicalAnimationComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AWrestlingMat : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AWrestlingMat();

	virtual void Tick(float DeltaSeconds) override;

	// IInteractable: [E] at the mat starts a match, or stops the one on.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	void StartBout();
	void StopBout();

	/** The next match begins by itself in this many seconds: the AI city's welcome. */
	void StartSoon(float Seconds) { AutoStartIn = FMath::Max(0.01f, Seconds); }
	bool IsBoutOn() const { return Phase != EPhase::Idle; }

	/** "Kaia vs Nyra": who wrestles next. */
	FString NextPairText() const;

	// --- components ---------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Wrestling")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The mat: a slab, its top MatThick above the actor's origin. */
	UPROPERTY(VisibleAnywhere, Category = "Wrestling")
	TObjectPtr<UStaticMeshComponent> Mat;

	/** The circle and the centre mark, in short straight pieces. */
	UPROPERTY(VisibleAnywhere, Category = "Wrestling")
	TObjectPtr<UInstancedStaticMeshComponent> Markings;

	/** Warm lamps on the wrestlers, so skin is skin under any sky (the premiere's lesson). */
	UPROPERTY(VisibleAnywhere, Category = "Wrestling")
	TArray<TObjectPtr<USpotLightComponent>> Lamps;

	// --- tuning -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Wrestling|Mat", meta = (ClampMin = "200"))
	float MatHalf = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Mat", meta = (ClampMin = "100"))
	float CircleRadius = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Mat", meta = (ClampMin = "1"))
	float MatThick = 8.0f;

	/** The lamps' light on the wrestlers, lux. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Mat", meta = (ClampMin = "0"))
	float LampLux = 12.0f;

	/** Start a match as soon as play begins, and the next when one ends (a test level). */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Match")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Match", meta = (ClampMin = "10"))
	float BoutSeconds = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Match", meta = (ClampMin = "1"))
	int32 FallsToWin = 2;

	/** Seconds between the referee's numbers. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Match", meta = (ClampMin = "0.2"))
	float CountSeconds = 0.85f;

	/** How close to the mat both shoulders must be for a pin, cm. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Match", meta = (ClampMin = "5"))
	float PinHeight = 32.0f;

	/** The muscles: how hard each body is pulled toward her pose, and how much that is damped. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float PoseStrength = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float PoseDamping = 200.0f;

	/** The will: how strongly a wrestler is carried into each move, scaled. 0 is two dolls in a heap. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float Drive = 1.0f;

	/** How firmly her hips and chest are carried to where the move puts them: the spring's rate,
	    1/s. It is critically damped, so higher is quicker and firmer, never bouncier. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "1", ClampMax = "20"))
	float Carry = 8.0f;

	/** Every body's drag, linear and angular (1/s), and the fastest any part of her may turn
	    (radians/s): what makes it strain rather than flail. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float LinearDrag = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float AngularDrag = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "1"))
	float MaxSpin = 9.0f;

	/** A HOLD (Walt, 2026-11-13: "they must grab each other"): a hand that touches the other
	    wrestler - her back, her arms, her waist, her legs - locks on with a physics joint, so the two
	    of them move as one tangle. How close counts as a touch, cm. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float GrabReach = 7.0f;

	/** The pull that tears a hold loose, in Unreal force units (kg cm/s^2). Her whole weight is
	    about 60,000: a hold survives the struggle and gives way to a real heave. */
	UPROPERTY(EditAnywhere, Category = "Wrestling|Bodies", meta = (ClampMin = "0"))
	float HoldBreak = 70000.0f;

	/** A line a second in the log: heights, moves, stamina, the count. For tuning; off in play. */
	UPROPERTY(EditAnywhere, Category = "Wrestling")
	bool bLog = false;

	/** M_GoSurface (Color x Pattern), as the AI city's pads use. */
	UPROPERTY(EditAnywhere, Category = "Wrestling")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	enum class EPhase : uint8 { Idle, Settle, Wrestle, Fall, Won };
	/** What the pair is doing: see the top of the file. Over is after a fall, or the bell. */
	enum class EBeat : uint8 { Tie, Shot, Ground, Ride, KickOut, Rise, Over };
	/** What one wrestler's body is doing in that beat: her posture, and her line in the log. */
	enum class EMove : uint8 { Kneel, Tie, Shoot, GoDown, Guard, Top, Flat, Bridge, Base, Back, Sit, Tall };
	/** Where a hand is sent. Collar to Arms are places on the other wrestler, and can become holds. */
	enum class EGrip : uint8 { Collar, Elbow, Shoulder, Waist, Wrist, Arms, Ready, Post, Rest, High };

	/** Where a wrestler's body should be: her hips, the way her spine runs from them, the way her
	    chest faces, and how much of her upper body's weight is taken for her. */
	struct FGoal
	{
		FVector Pelvis = FVector::ZeroVector;
		FVector Torso = FVector::UpVector;
		FVector Face = FVector::ForwardVector;
		float Support = 0.0f;
	};

	struct FHold
	{
		class UPhysicsConstraintComponent* Joint = nullptr;   // alive through Joints, below
		FName Target;
		float Age = 0.0f;
		float Keep = 6.0f;     // she lets go after this long, to take hold somewhere else
		float Cool = 0.0f;     // and waits this long before the hand grabs again
	};

	struct FWrestler
	{
		FString Name;
		AActor* Actor = nullptr;                       // alive through Actors, below
		USkeletalMeshComponent* Body = nullptr;
		UPhysicalAnimationComponent* Muscles = nullptr;
		UGrapplePoseAnimInstance* Pose = nullptr;
		USceneComponent* BodyParent = nullptr;         // where her body hangs when it is not a ragdoll
		FTransform BodyRelative;
		float KneelDrop = 45.0f;                       // how far her root goes below the mat to kneel on it
		float KneelHeight = 49.0f;                     // her hips above the mat on her knees, measured at the whistle
		float SpineLength = 21.0f;                     // hips to chest, measured then too

		// The bones whose bodies matter, each the nearest bone that HAS a body.
		FName Pelvis, Chest, Neck;
		FName Hand[2], UpperArm[2], LowerArm[2];
		TArray<FName> Holdable;      // what the other one can take hold of: back, waist, arms, legs, neck
		TArray<FName> Upper;         // every body from her hips up: what Support carries

		EMove Move = EMove::Kneel;
		float MoveAge = 0.0f;
		float Stamina = 1.0f;
		float Heart = 1.0f;          // this match's appetite: some days she has it
		float Rhythm[6] = {};        // her breathing's own phases
		EGrip Grip[2] = { EGrip::Collar, EGrip::Elbow };
		float GripIn = 0.0f;         // seconds until she changes her grips in the tie-up
		FHold Hold[2];
		FGrapplePosture Now;
		FGoal Goal1, Goal2;          // the goal, eased twice: where she is being carried right now
		bool bGoal = false;
		float FacingSign = 1.0f;
		int32 Falls = 0;
		bool bPhysics = false;
		float Tone = 1.0f;           // the muscles' strength now: soft while she rests
	};

	void BuildMat();
	void PaintMat();

	bool SpawnPair();
	bool SpawnWrestler(int32 Slot, int32 Who);
	void ClearPair();
	void PlaceForStart();
	void SetPhysics(FWrestler& Wr, bool bOn);

	void Direct(float Dt);
	void SetBeat(EBeat Next);
	void BeginTie(bool bFromWhereTheyAre);
	void BeginShot();
	void SetMove(FWrestler& Wr, EMove Move);
	void SetGrips(FWrestler& Wr, EGrip Left, EGrip Right);
	double TiePress() const;
	FGoal GoalFor(int32 Me) const;
	float GoalEase() const;
	void Act(int32 Me, float Dt);
	FGrapplePosture PostureFor(const FWrestler& Wr) const;
	void AimHands(int32 Me, FGrapplePosture& P) const;
	FVector GripPoint(int32 Me, int32 Hand) const;
	void Steer(FWrestler& Wr, FName Bone, const FVector& Target, double Rate);
	void TickBout(float Dt);
	void TickLabShots(float Dt);
	void TickPin(float Dt);
	bool IsPinned(int32 Slot) const;
	void EndFall(int32 Winner, const FString& Why);
	void Restart(const FString& Why);
	bool LooksBroken(int32 Slot) const;
	void TickHolds(int32 Me, float Dt);
	void Grab(int32 Me, int32 Hand, FName Target);
	void LetGo(FWrestler& Wr, int32 Hand);
	void LetGoAll();
	int32 HoldCount(const FWrestler& Wr) const;

	// Where things are, in the world, and how high above the mat.
	FVector BonePos(const FWrestler& Wr, FName Bone) const;
	FVector MatUp() const;
	FVector MatTop() const;
	float Height(const FVector& P) const;
	FVector OnMat(const FVector& P, double AtHeight) const;
	FVector Flat(const FVector& V) const;
	FVector Facing(const FWrestler& Wr) const;
	bool ChestFrame(const FWrestler& Wr, FVector& Fwd, FVector& Wide, FVector& Tall) const;
	void Push(FWrestler& Wr, FName Bone, const FVector& Accel);

	void Announce(const FString& Text, float Seconds);
	void ShowStatus();
	void LogState();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Actors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UPhysicsConstraintComponent>> Joints;

	FWrestler W[2];
	EPhase Phase = EPhase::Idle;
	float PhaseAge = 0.0f;

	// The script.
	EBeat Beat = EBeat::Tie;
	float BeatAge = 0.0f;
	float BeatFor = 0.0f;                              // how long this beat is meant to last
	int32 Top = 0;                                     // who shot, and is on top
	FVector Centre = FVector::ZeroVector;              // the tie-up: the point between them,
	FVector Axis = FVector::ForwardVector;             // and the line from wrestler 0 to wrestler 1
	float Sway[3] = {};                                // the tie-up's own phases
	FVector GroundAt = FVector::ZeroVector;            // the ground: where the one beneath knelt,
	FVector GroundAlong = FVector::ForwardVector;      // the way she lies from there, hips to head,
	FVector GroundSide = FVector::RightVector;         // and the side she is turned over by
	bool bGround = false;                              // those three mean something
	bool bEscape = false;                              // she has a kick-out left in her
	float EffortAt = 0.0f;                             // how far into the count she makes it
	float Clock = 0.0f;
	int32 PairIndex = 0;
	int32 PinnedSlot = -1;
	float PinHeld = 0.0f;
	float PinBroken = 0.0f;
	float ApartFor = 0.0f;
	int32 Count = 0;
	int32 WinnerSlot = -1;
	FString Note;                // the line under the score: the count, a restart
	float NoteFor = 0.0f;
	float LogAge = 0.0f;
	float AutoStartIn = -1.0f;
	float LabShotEvery = -1.0f;   // -1: not read from the command line yet
	float LabShotIn = 0.0f;
	int32 LabShotIndex = 0;
	bool bLabCamera = false;
	FRandomStream Rng;
};
