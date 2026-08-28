#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlapComponent.generated.h"

class USoundBase;
class UAnimSequence;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SIBELIUSGAME_API USlapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlapComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float SlapRange = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float SlapRadius = 60.f;

	/** How high up his body the swing starts, in battle form only. Chest height: a
	 *  greatsword arc, not a shin kick. In first person the trace still starts at the
	 *  camera, which IS his head, so this is unused there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float BattleSwingHeight = 90.f;

	/** The sweep radius in battle form only. A GREATSWORD ARC, not a jab.
	 *
	 *  Walt, after his first real fight: "I pressed A and F a million times until he
	 *  finally cut down his chasers." The first-person slap is a 60 cm probe - right for
	 *  swatting one Refuser at arm's length in a corridor, and it caught two or three out
	 *  of thirty in a press. Clearing a crowd became a key-mashing exercise.
	 *
	 *  150 cm cuts through a front rank, so the fight is about where you stand and how
	 *  fast they close rather than about your F key. It is deliberately a starting point:
	 *  too wide and thirty fall to five swings, which is its own kind of boring. This is
	 *  the number to argue with once it has been felt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float BattleSwingRadius = 150.f;

	/** Which of Greystone's three primary attacks comes next. Cycles so repeated
	 *  presses read as a combo instead of the same swing three times. Not saved and not
	 *  reset — where the chain happens to be when a fight starts does not matter. */
	int32 SwingIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float LaunchSpeed = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float UpwardSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	USoundBase* SlapSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	bool bDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	float RagdollLifetime = 4.0f;

	// True (default): the victim freezes mid-pose and is launched as ONE rigid
	// piece — nothing deforms, so nothing can stretch. False: classic floppy
	// ragdoll; the Paragon-era Gideon mesh stretches on this path (bad physics
	// asset coverage + fragile converted APEX cloth), kept as a toggle for
	// experimenting with other victim meshes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	bool bRigidKnockback = true;

	// Only Characters currently possessed by ARefuserController are slappable.
	// Keeps the player from launching friendly Shinbi companions, and keeps
	// the companions from slapping the player or each other.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	bool bOnlySlapRefusers = true;

	// APPEAL-6 (slap juice): the collapse-in-place death animation — Gideon's
	// own Paragon Death_Back, held on its final pose. HARD reference on
	// purpose (v0.7.4 lesson): a soft path referenced only from C++ is
	// invisible to the cooker, so the anim shipped in no pak and packaged
	// builds silently fell back to the freeze pose while PIE looked perfect.
	// Skipped (freeze fallback) if null or the victim's skeleton mismatches.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	TObjectPtr<UAnimSequence> SlapDeathAnim = nullptr;

	// FUN-2: a connected slap pays a little Sauce, so standing up to a Refuser
	// is rewarded, not just survived.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap", meta=(ClampMin="0"))
	int32 SauceOnSlap = 2;

	// Walt's clip catch (2026-07-19): the collapse anim ignored the room —
	// legs in walls, head in the couch. A ragdoll handoff was tried and
	// REJECTED (it resurrects the Paragon mesh stretch this rigid path exists
	// to avoid). Instead the slap picks the clearest FALL LANE: 8 knee-height
	// traces, spin the victim so Death_Back lays him into open floor (ties
	// prefer falling away from the slapper). See DoSlap.

	UFUNCTION(BlueprintCallable, Category="Slap")
	void DoSlap();
};
