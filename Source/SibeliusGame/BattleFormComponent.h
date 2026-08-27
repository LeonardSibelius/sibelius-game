// BattleFormComponent.h — the avatar the agents give him, and the camera that shows it.
//
// WHAT THIS IS FOR. The whole game so far is a pair of eyes: first person, forty years
// at a desk, and the player has never once seen the man he is playing. Battle form
// switches to a camera behind him — and the first time you ever see Leonard Sibelius is
// the moment he has become something that can fight back. That beat is free. It comes
// out of the switch itself, paid for by every hour of first person before it.
//
// You also finally see the five agents. All those dancers he has been taking powers
// from, fighting beside him, visible at last.
//
// IT IS AN AVATAR, NOT A TRANSFORMATION, and the difference is the whole game. A
// 71-year-old programmer who turns into a Paragon sword hero has quietly stopped being
// a memoir. An AI that GIVES him a body which can fight is the same move Kaia made in
// the opening when she gave him his name — his employer calls him "Programmer" and
// refuses the name; the agents hand him both. Frame the grant on screen and it lands.
// Skip the framing and it is a costume change.
//
// THE BODY WAS ALREADY THERE. ASibeliusGameCharacter carries the stock UE5 first-person
// rig: a full skeletal body on GetMesh() with SetOwnerNoSee(true), plus arms only the
// owner sees. Leonard has had a body walking around the whole game — nobody was allowed
// to look at it. So this is a visibility flip, a mesh swap and a boom, not a rewrite.
//
// ---------------------------------------------------------------------------
// THIS IS A MODE, NOT A MIGRATION — and that is deliberate.
//
// Every power in this game traces FROM THE CAMERA. Interact, Code Vision, Refactor: all
// of them start a ray at PlayerCameraManager->GetCameraLocation(), tuned for a camera
// sitting inside the character's head. Put the camera three and a half metres behind him
// and every one of those rays starts in mid-air behind his back — E stops finding things
// at arm's length, and R transmutes whatever happens to be over his shoulder.
//
// Converting the game to third person would mean re-tuning all of it, and all of it
// currently works. So the office stays first person and the battlefield is a mode.
//
// WHAT THIS COMPONENT DOES ABOUT IT, HONESTLY: it suspends the tick on the camera-trace
// components, which kills the targeting half — no E prompt, no Code Vision highlight, no
// refactor target. It CANNOT stop the input-driven half from here, because the R binding
// lives on the character and calls straight through. **Gating the power inputs on
// IsInBattleForm() is a separate change on ASibeliusGameCharacter and has not been made
// yet.** Until it is, pressing R in battle form will still roll the menagerie on whatever
// the camera is pointed at. Written down rather than discovered.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "BattleFormComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USkeletalMesh;

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UBattleFormComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleFormComponent();

	/** Take the avatar: body visible, Greystone's mesh on it, camera behind the shoulder. */
	UFUNCTION(BlueprintCallable, Category = "Battle Form")
	void EnterBattleForm();

	/** Give it back. Restores every value it saved, so leaving is not a second edit. */
	UFUNCTION(BlueprintCallable, Category = "Battle Form")
	void ExitBattleForm();

	UFUNCTION(BlueprintPure, Category = "Battle Form")
	bool IsInBattleForm() const { return bInBattleForm; }

	// ---- the camera ----

	/** Boom length. 350 puts him low in frame with room to see what he is about to hit;
	 *  shorter and the swing animations leave the screen, longer and he stops mattering. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Camera", meta = (ClampMin = "50"))
	float BoomLength = 350.0f;

	/** Off-centre, and above the shoulder. Dead-centre framing hides the thing in front
	 *  of him behind his own back, which in a fight is the only thing worth seeing. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Camera")
	FVector BoomSocketOffset = FVector(0.0f, 60.0f, 70.0f);

	/** Lag gives the camera weight. Too high and it feels like ice; 12 trails a step
	 *  behind a sprint and settles instantly on a stop. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Camera", meta = (ClampMin = "0"))
	float CameraLagSpeed = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Battle Form|Camera", meta = (ClampMin = "10"))
	float BattleFieldOfView = 80.0f;

	// ---- the body ----

	/** Greystone: a melee hero, where Gideon is a caster. Five swordsmen against an army
	 *  of casters is a better-looking fight than five casters against five hundred. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Body")
	TSoftObjectPtr<USkeletalMesh> AvatarMesh;

	/** Greystone's own AnimBlueprint drives him. His 174 animations already include
	 *  Attack_A/B in three speeds and four abilities; none of that needs authoring. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Body")
	TSoftClassPtr<UAnimInstance> AvatarAnimClass;

	/** Paragon heroes are not built to a 96cm half-height capsule. Expect to tune this
	 *  once rather than to have guessed it right. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Body", meta = (ClampMin = "0.1"))
	float AvatarScale = 1.0f;

	/** He turns to face where he is running instead of where the camera looks. This is
	 *  what makes a third-person fighter read as a body rather than a turret. */
	UPROPERTY(EditAnywhere, Category = "Battle Form|Body", meta = (ClampMin = "1"))
	float TurnRateDegPerSec = 540.0f;

	/** Stop the camera-trace powers targeting from behind his back. See the header for
	 *  what this does NOT cover. */
	UPROPERTY(EditAnywhere, Category = "Battle Form")
	bool bSuspendCameraPowers = true;

protected:
	virtual void BeginPlay() override;

private:
	/** Boom and camera are made on first entry, not in the constructor: a player who
	 *  never reaches the battlefield should not carry a spring arm around the office. */
	void EnsureRig();

	/** The arms mesh, found by elimination — it is the skeletal mesh that is not the
	 *  character's body. Naming it here would couple this to a private member. */
	USkeletalMeshComponent* FindArmsMesh() const;

	void SetCameraPowersSuspended(bool bSuspended);

	ACharacter* GetCharacterOwner() const;

	UPROPERTY() TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY() TObjectPtr<UCameraComponent> BattleCamera;

	/* EVERY SAVED VALUE IS RESTORED ON EXIT. A mode that leaves residue behind is not a
	   mode, it is a one-way edit that happens to have a name. */
	UPROPERTY() TObjectPtr<USkeletalMesh> SavedBodyMesh;
	UPROPERTY() TSubclassOf<UAnimInstance> SavedAnimClass;
	FVector SavedBodyScale = FVector::OneVector;
	FRotator SavedRotationRate = FRotator::ZeroRotator;
	bool bSavedOrientToMovement = false;
	bool bSavedUseControllerYaw = false;
	bool bInBattleForm = false;
};
