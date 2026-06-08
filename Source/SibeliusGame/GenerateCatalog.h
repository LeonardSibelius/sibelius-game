// GenerateCatalog.h
//
// SIB-30 — Ch6 P0. Thin accessor that loads the curated catalog (DECISION DA: a
// CSV-backed UDataTable) into the matcher's entry list. The matcher itself stays the
// pure function it already is — this just feeds it.
//
// P0 loads the committed CSV at runtime via UDataTable::CreateTableFromCSVString
// (editor/commandlet) so the headless gate needs no .uasset. For a packaged build (P1),
// the same CSV is imported as a persistent UDataTable asset and read the same way.

#pragma once

#include "CoreMinimal.h"
#include "GenerateTypes.h"

// Absolute path to the committed catalog CSV (<ProjectDir>/Data/GenerateCatalog.csv).
SIBELIUSGAME_API FString GetGenerateCatalogCsvPath();

// Load the catalog into OutEntries (EntryId = each DataTable row name). False on any
// failure, with OutError set (missing file, parse errors, zero rows).
SIBELIUSGAME_API bool LoadGenerateCatalog(TArray<FGenerateCatalogEntry>& OutEntries, FString& OutError);
