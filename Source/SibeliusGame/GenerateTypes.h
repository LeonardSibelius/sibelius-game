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
class ABuildSite;

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

	/* ===========================================================================
	   AN ENTRY CAN SPAWN A CLASS INSTEAD OF A MESH (2026-09-01, docs/SPACEPORT_PLAN.md).

	   Until now the catalog's ceiling was one static mesh per row. That is fine for a
	   ladder and useless for anything that has to DO something — the spaceport that
	   assembles itself, and later the rocket that leaves. Set this and the class is
	   spawned; leave it empty and the row behaves exactly as it always has. Every
	   existing row is unaffected, because empty is the old behaviour.

	   WHY ABuildSite AND NOT AActor. The plan first said AActor, which was wrong, and
	   reading the spawn path is what corrected it. EVERYTHING Generate creates is an
	   ABuildSite, and that is not incidental — the site is what carries:

	       IBranchable      Test-Drive can branch it and discard a failure for free
	       MarkGenerated    provenance, so Deploy knows it was generated and from which row
	       the GUID          stable identity across a save and a re-spawn

	   A plain AActor would spawn, look right, and then quietly sit outside branch,
	   Deploy and save — three systems this game is built on. Constraining the class to
	   ABuildSite means ASpaceport INHERITS all of it instead of re-implementing it,
	   which is also exactly what the plan wants Test-Drive to do to a failed launch.

	   Soft, like Mesh, so the catalog does not hard-load every class at startup.
	   =========================================================================== */
	UPROPERTY(EditAnywhere, Category = "Generate")
	TSoftClassPtr<ABuildSite> ActorClass;

	/* HOW FAR IN FRONT OF HIM IT APPEARS. 0 = the component's default (250 cm).

	   Walt, 2026-09-02: "yikes I am under the spaceport - and W doesn't work i can't
	   move." He was inside it. Generate has always placed things two and a half metres
	   ahead, which is correct for a lamp, a chair or a potted plant and absurd for a
	   25-metre launch complex with a 120-metre rocket in the middle of it: the structure
	   assembled AROUND him and then turned solid.

	   The distance therefore belongs to the ENTRY, not to the component. A row that
	   builds something big says so, in data, next to the thing that makes it big. */
	UPROPERTY(EditAnywhere, Category = "Generate", meta = (ClampMin = "0.0"))
	float SpawnAhead = 0.0f;

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
