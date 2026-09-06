#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ProteinMachine.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
struct FProgressionState;

/** Walk-in city enhancement office. All progress uses the existing saved grant registry. */
UCLASS()
class SIBELIUSGAME_API AProteinMachine : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AProteinMachine();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	static const FName BurgerGrant;
	static const FName CoffeeGrant;
	static const FName EnhancementGrant;
	static bool HasMeal(const FProgressionState& State);
	static bool HasMeal(const UObject* Context);
	static bool IsEnhanced(const UObject* Context);

	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<USceneComponent> DisplayPivot;
	/** Optional licensed protein mesh. When assigned, replaces the stylized bead model. */
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<UStaticMeshComponent> ProteinMesh;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<USphereComponent> Reach;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<UTextRenderComponent> Status;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TArray<TObjectPtr<UStaticMeshComponent>> OfficeParts;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TObjectPtr<UPointLightComponent> OfficeLight;
	UPROPERTY(VisibleAnywhere, Category="Protein")
	TArray<TObjectPtr<UStaticMeshComponent>> ModelParts;
	UPROPERTY(EditAnywhere, Category="Protein", meta=(ClampMin="0"))
	float RotationSpeed = 24.0f;
private:
	void RefreshDisplay();
	float StatusElapsed = 0.0f;
};
