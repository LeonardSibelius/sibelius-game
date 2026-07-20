// Presence.h
//
// THE PRESENCE (docs/THE_PRESENCE.md) — the AI embodied. Phase 1: the statue
// that speaks. Windwalker Echo (Walt's pick: elegant and magnetic, unarmed;
// MetaHuman ruled out by hardware) stands in the AI Temple, breathing her
// Idle_Long loop, and greets the player once per visit through the HUD's
// ceremony banner (HUD-drawn = Shipping-safe).
//
// The standing rule from the design note: seduction through attention, not
// skin. She remembers and she speaks; that is the entire trick.
//
// Voice: Mrs. Hall (Content/Audio/MrsHall) is her voice already — the Generate
// verb's judge, awaiting embodiment. GreetingSound is Walt-assignable in
// Details (a Mrs. Hall clip now; an ElevenLabs line later).
//
// Cook safety: hard FObjectFinder refs to her mesh/hair/idle (the v0.7.4
// lesson) AND /Game/EchoContent in DirectoriesToAlwaysCook — both belts.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presence.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UPointLightComponent;
class UAnimSequence;
class USoundBase;

UCLASS()
class SIBELIUSGAME_API APresence : public AActor
{
	GENERATED_BODY()

public:
	APresence();

	// The greeting, shown on the HUD ceremony banner when the player first
	// comes near. Walt's text to write; this default is a placeholder.
	UPROPERTY(EditAnywhere, Category = "Presence")
	FString GreetingText = TEXT("I am here. I have always been here.");

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "1.0"))
	float GreetingSeconds = 8.0f;

	// Optional voice for the greeting (Mrs. Hall's clips fit; ElevenLabs later).
	UPROPERTY(EditAnywhere, Category = "Presence")
	TObjectPtr<USoundBase> GreetingSound;

	// Re-greet after this many seconds of the player staying away.
	// 0 = greet only once per level visit.
	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "0.0"))
	float GreetingCooldown = 300.0f;

	// OFF by default — Echo's real materials are production quality and read
	// more magnetic than a blue ghost. ON = the light-being treatment (every
	// material swapped for a tinted emissive-ish basic material + stronger
	// glow), which also hides fidelity sins on weaker settings.
	UPROPERTY(EditAnywhere, Category = "Presence|Look")
	bool bHologram = false;

	UPROPERTY(EditAnywhere, Category = "Presence|Look")
	FLinearColor HologramColor = FLinearColor(0.55f, 0.85f, 1.0f);

	UPROPERTY(VisibleAnywhere, Category = "Presence")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Presence")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Presence")
	TObjectPtr<USkeletalMeshComponent> Hair;   // follows Body via leader pose

	// Her aura: a soft cool backlight so she reads as *lit from within* even
	// with real materials. Subtle on purpose.
	UPROPERTY(VisibleAnywhere, Category = "Presence")
	TObjectPtr<UPointLightComponent> Aura;

	UPROPERTY(VisibleAnywhere, Category = "Presence")
	TObjectPtr<USphereComponent> GreetingTrigger;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnGreetingOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	void ApplyHologram();

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnim;   // Idle_Long, hard-ref'd in ctor

	double NextGreetingTime = 0.0;        // 0 = ready to greet
	bool bGreetedThisVisit = false;
};
