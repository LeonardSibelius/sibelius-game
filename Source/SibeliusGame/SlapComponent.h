#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlapComponent.generated.h"

class USoundBase;

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

	UFUNCTION(BlueprintCallable, Category="Slap")
	void DoSlap();
};
