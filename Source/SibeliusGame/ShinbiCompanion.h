#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShinbiCompanion.generated.h"

class USlapComponent;

// A friendly follower for the Many Worlds forests. Follows the player along
// the road and slaps Refusers with the SAME USlapComponent the player uses —
// one slap system, two slappers (the "one stat, one code point" rule).
// Blueprint side: BP_Shinbi_Companion reparents this and supplies the Paragon
// Shinbi mesh + AnimBP, exactly like BP_Gideon_Refuser does for the Refuser.
UCLASS()
class SIBELIUSGAME_API AShinbiCompanion : public ACharacter
{
	GENERATED_BODY()

public:
	AShinbiCompanion();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Companion")
	TObjectPtr<USlapComponent> SlapComponent;

	// The Paragon Shinbi's converted APEX cloth (arm ribbons + hair) ships with
	// near-zero damping and vibrates like hummingbird wings. Tamed at runtime
	// via the Chaos cloth interactor — tune here, no rebuild needed.
	UPROPERTY(EditAnywhere, Category="Companion|Cloth")
	bool bTameClothFlutter = true;

	UPROPERTY(EditAnywhere, Category="Companion|Cloth", meta=(ClampMin="0", ClampMax="1"))
	float ClothDamping = 0.8f;

	UPROPERTY(EditAnywhere, Category="Companion|Cloth", meta=(ClampMin="0", ClampMax="1"))
	float ClothLocalDamping = 0.5f;

	// Nuclear option: freeze the cloth entirely (rigid ribbons, zero flutter).
	UPROPERTY(EditAnywhere, Category="Companion|Cloth")
	bool bSuspendClothEntirely = false;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyClothTuning();

	FTimerHandle ClothTuneRetryHandle;
	int32 ClothTuneAttempts = 0;
};
