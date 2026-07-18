// CarouselMachine.h
//
// SIB-46 Presentation (grey-box) — the playable slot cabinet. Reads UCarouselRunSubsystem and
// drives it via input (the lever); reacts to OnSpinResolved with a "big win" pulse. It NEVER
// computes payouts — the sim is the single source of truth. Placeholder cube meshes only; real
// art comes later in the lean fork.
//
// Input is bound directly (no Input Mapping assets) so the slice is portable: E = pull lever (Spin),
// 1/2/3 = buy shop offering, R = reroll, Enter = continue to next round, N = new run.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CarouselTypes.h"
#include "CarouselMachine.generated.h"

class UStaticMeshComponent;
class UCarouselRunSubsystem;

UCLASS()
class SIBELIUSGAME_API ACarouselMachine : public AActor
{
	GENERATED_BODY()

public:
	ACarouselMachine();
	virtual void Tick(float DeltaSeconds) override;

	// Presentation-only reaction state the HUD/mesh read (0..1, decays). Set from OnSpinResolved.
	float GetBigWinFlash() const { return BigWinFlash; }

	// Large-payout threshold for the "big win" reaction (chips).
	UPROPERTY(EditAnywhere, Category = "Carousel") int32 BigWinThreshold = 200;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Carousel") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Carousel") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Carousel") TObjectPtr<UStaticMeshComponent> Lever;

private:
	UCarouselRunSubsystem* GetRun() const;
	void TryEnableInput();

	// Walt's poker-cube catch: this machine's keys are LEVEL-GLOBAL raw binds,
	// so E at the poker cabinet pulled the carousel lever. Every machine verb
	// now demands the player actually stand at the machine (O stays global —
	// it's the way home, not a machine verb).
	bool IsPlayerNear(float Radius) const;
	static constexpr float MachineKeyRadius = 1000.0f;   // matches the HUD panel radius

	// Input handlers (commands to the subsystem — never payout math).
	void OnPullLever();
	void OnBuy0();
	void OnBuy1();
	void OnBuy2();
	void OnReroll();
	void OnContinue();
	void OnNewRun();
	void OnLeave();   // FUN-4: O -> back to the office (the room's pawn has no character binding)

	UFUNCTION()
	void HandleSpinResolved(const FSpinResult& Result);

	float BigWinFlash = 0.f;     // decays each tick; HUD draws a scaled flash, lever/body pulse
	float LeverPull = 0.f;       // 1 -> 0 lever-pull animation
	int32 InputAttempts = 0;
	FTimerHandle InputRetryHandle;
};
