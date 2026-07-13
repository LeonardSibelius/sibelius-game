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
};
