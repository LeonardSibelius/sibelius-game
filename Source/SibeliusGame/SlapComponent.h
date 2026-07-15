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

	// APPEAL-6 (slap juice): on a rigid knockback the victim PLAYS this death
	// animation while flying, instead of freezing mid-stride — single-node,
	// non-looping, so he holds the collapsed pose where he lands. Defaults to
	// Gideon's own Paragon Death_Back; skipped (freeze fallback) if the asset
	// is absent or the victim's skeleton doesn't match.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap")
	TSoftObjectPtr<UAnimSequence> SlapDeathAnim;

	// FUN-2: a connected slap pays a little Sauce, so standing up to a Refuser
	// is rewarded, not just survived.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slap", meta=(ClampMin="0"))
	int32 SauceOnSlap = 2;

	UFUNCTION(BlueprintCallable, Category="Slap")
	void DoSlap();
};
