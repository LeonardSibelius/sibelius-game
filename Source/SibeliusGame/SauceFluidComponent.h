// SauceFluidComponent.h
//
// v0.9.7.1 experiment 1 of 7 — "sauce is a fluid". Niagara Fluids on the kitchen
// cauldron and the temple bowl. The stove simmers harder the more sauce you hold;
// a shop open or a blend makes it boil. The temple [E] pour is a 3D FLIP hose, not
// a green cylinder. Escape hatch: console `sib.SauceFluids 0`.
//
// Systems are HARD-REFFED from the NiagaraFluids plugin templates (v0.7.4 cook
// lesson). Tools/Scripts/build_sauce_fluids.py optionally duplicates them into
// /Game/Sauce so Walt can retint without touching Engine; BeginPlay prefers the
// game copies when they exist.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SauceFluidComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProgressionSubsystem;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESauceFluidRole : uint8
{
	Cauldron UMETA(DisplayName = "Cauldron (always simmers)"),
	Bowl     UMETA(DisplayName = "Bowl (pours, then simmers when full)")
};

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API USauceFluidComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USauceFluidComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// --- Gameplay drivers (actors call these; they are the whole public API) ---

	/** 0..1 blend meter (temple BookRain → cauldron). */
	void SetBlendHeat(float Blend01);

	/** Shop opened or a blend completed — the pot rolls for a few seconds. */
	void NotifyBoilOver();

	/** Extra heat while the shop widget is on screen. */
	void SetShopOpen(bool bOpen);

	void SetPouring(bool bPouring);
	void SetFilled(bool bFilled);

	/** Translucent green liquid look (MID of M_SauceSurface, else BasicShape). */
	static void DressLiquidMesh(UStaticMeshComponent* Mesh);

	/** See-through bubble: bright rim, dark centre. */
	static void DressBubbleMesh(UStaticMeshComponent* Mesh);

	/** Bob + squash so a filled pot reads as liquid, not a puck. */
	void SloshLiquid(UStaticMeshComponent* Mesh, const FVector& BaseScale, const FVector& BaseLoc) const;

	// --- Headless gate hooks ---
	bool HasSimmerSystem() const;
	bool HasPourSystem() const;
	bool HasPoolSystem() const;
	float GetSimmerIntensityForTest() const { return SimmerIntensity; }
	bool IsPouringForTest() const { return bPouring; }
	bool IsFilledForTest() const { return bFilled; }

	UPROPERTY(EditAnywhere, Category = "SauceFluid")
	ESauceFluidRole Role = ESauceFluidRole::Cauldron;

	/** Sauce-green. Drives Niagara color parameters AND the glow light. */
	UPROPERTY(EditAnywhere, Category = "SauceFluid")
	FLinearColor SauceColor = FLinearColor(0.15f, 1.0f, 0.35f);

	/** Wallet sauce mapped onto simmer. 400 sauce = full simmer. */
	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "1"))
	int32 SauceForFullSimmer = 400;

	/** Beyond this (cm) the simmer deactivates so a 3D gas grid is not always on. */
	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "0.0"))
	float MaxSimDistance = 2200.f;

	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "0.05"))
	float SimmerScale = 0.28f;

	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "0.05"))
	float PourScale = 0.22f;

	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "0.05"))
	float PoolScale = 0.18f;

	/** Gas rises; leave identity. (Old 2D hose needed -90 pitch.) */
	UPROPERTY(EditAnywhere, Category = "SauceFluid")
	FRotator PourRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "SauceFluid")
	FVector PourOffset = FVector(0.f, 0.f, 90.f);

	UPROPERTY(EditAnywhere, Category = "SauceFluid", meta = (ClampMin = "0.1"))
	float BoilDecaySeconds = 2.8f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "SauceFluid")
	TObjectPtr<UNiagaraComponent> SimmerComp;

	UPROPERTY(VisibleAnywhere, Category = "SauceFluid")
	TObjectPtr<UNiagaraComponent> PourComp;

	UPROPERTY(VisibleAnywhere, Category = "SauceFluid")
	TObjectPtr<UNiagaraComponent> PoolComp;

	UPROPERTY(VisibleAnywhere, Category = "SauceFluid")
	TObjectPtr<UPointLightComponent> Glow;

private:
	void ResolveGameCopies();
	void ApplyLook(UNiagaraComponent* Comp) const;
	void ApplyIntensity(UNiagaraComponent* Comp, float Intensity) const;
	void RefreshSimmer();
	void RecomputeIntensity();
	bool FluidsEnabled() const;
	bool ShouldSimmer() const;
	bool IsPlayerNear() const;
	void HandleSauceChanged(int32 NewTotal, int32 Delta);

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> SimmerSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> PourSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> PoolSystem;

	FDelegateHandle SauceChangedHandle;

	float BlendHeat = 0.f;
	float BoilSpike = 0.f;
	float SimmerIntensity = 0.f;
	bool bShopOpen = false;
	bool bPouring = false;
	bool bFilled = false;
	bool bSimmerActive = false;
};
