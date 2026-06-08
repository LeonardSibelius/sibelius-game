// GenerateTypes.h
//
// SIB-30 — Ch6 "Generate" SPIKE. The data seam for Option B: a curated, CLOSED
// catalog of pre-authored objects matched to natural-language input by keyword
// overlap. Keywords live in DATA so the synonym set grows without recompiling, and
// the closed set is content-safe by construction. No runtime/live generation.
//
// FGenerateCatalogEntry derives FTableRowBase so a CSV-backed UDataTable can hold the
// catalog (DECISION DA recommendation) — easiest place to grow keywords over time.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GenerateTypes.generated.h"

class UStaticMesh;

// Why a request resolved or was refused. Refusals become an in-fiction Mrs. Hall line.
UENUM()
enum class EGenerateOutcome : uint8
{
	Resolved,
	RefusedNoMatch,
	RefusedAmbiguous,
	RefusedOverBudget,
	RefusedUnsafe
};

// One catalog entry — a DataTable row (DECISION DA: CSV-backed UDataTable). The only
// thing that can ever enter the world is a resolved EntryId.
USTRUCT()
struct FGenerateCatalogEntry : public FTableRowBase
{
	GENERATED_BODY()

	// Stable key, e.g. "ladder". Set from the DataTable ROW NAME by the accessor — not
	// a CSV column.
	UPROPERTY()
	FName EntryId;

	// Player-facing name, e.g. "Wooden Ladder".
	UPROPERTY(EditAnywhere, Category = "Generate")
	FText DisplayName;

	// Synonym/intent keywords — DATA, grows over time. PIPE-DELIMITED in the CSV so it's
	// spreadsheet-friendly, e.g. "ladder|steps|rungs|climb". Parse via GetKeywordTokens.
	UPROPERTY(EditAnywhere, Category = "Generate")
	FString Keywords;

	// What resolving this spawns (route through the Ch3 build pipeline). Soft so the
	// catalog doesn't hard-load every asset.
	UPROPERTY(EditAnywhere, Category = "Generate")
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Charged against the per-area generation budget (the in-fiction economy).
	UPROPERTY(EditAnywhere, Category = "Generate")
	int32 Cost = 1;

	// Per-entry spawn transform so an authored mesh looks right when built. SIB-40
	// lesson baked into DATA: the gold key needed Scale 0.25 + Yaw 90 or it read as an
	// invisible/flat "stick" — authoring transform-in-data means no code change per object.
	UPROPERTY(EditAnywhere, Category = "Generate")
	FVector SpawnScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, Category = "Generate")
	FRotator SpawnRotation = FRotator::ZeroRotator;

	// Split the pipe-delimited Keywords into individual tokens for the matcher.
	void GetKeywordTokens(TArray<FString>& Out) const
	{
		Out.Reset();
		Keywords.ParseIntoArray(Out, TEXT("|"), /*bCullEmpty*/ true);
	}
};

// Transient result of classifying one request. Not serialized — plain struct.
struct FGenerateResolution
{
	EGenerateOutcome Outcome = EGenerateOutcome::RefusedNoMatch;
	FName            EntryId;        // valid only when Outcome == Resolved
	int32            Cost = 0;       // the entry's cost (for budgeting / over-budget reporting)
	FString          RefusalReason;  // for the Mrs. Hall line / debug
};
