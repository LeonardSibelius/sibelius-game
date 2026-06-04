// Ch3 - Compile (SIB-27). Closed resource enum + build output types.
// Hard-listed per ledger C3/C4 (and Ch2's R5 lesson): no free-form resource types.

#pragma once

#include "CoreMinimal.h"
#include "CompileTypes.generated.h"

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	Book	UMETA(DisplayName = "Book"),
	Key		UMETA(DisplayName = "Key")
};

UENUM(BlueprintType)
enum class EBuildOutput : uint8
{
	Structure	UMETA(DisplayName = "Structure (mesh + nav-link)"),
	KeyItem		UMETA(DisplayName = "Key Item (grants Key resource)")
};

// Groundwork for Ch5 Deploy (persistence) - intentionally unused this chapter (ledger C5).
USTRUCT()
struct FBuildRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SiteId;

	UPROPERTY()
	bool bIsBuilt = false;
};
