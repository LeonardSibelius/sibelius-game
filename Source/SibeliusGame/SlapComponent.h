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

	// Walt's clip catch (2026-07-18): the collapse anim played with all collision
	// off, so legs sank into walls and heads into couches. The anim now sells
	// only the first moments of the stagger; after this many seconds the mesh
	// hands off to a ZERO-impulse ragdoll (a crumple, not a launch — the launch
	// was the original complaint) so the body settles against real furniture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap", meta=(ClampMin="0.0"))
	float CollapseRagdollDelay = 0.7f;

	UFUNCTION(BlueprintCallable, Category="Slap")
	void DoSlap();
};
