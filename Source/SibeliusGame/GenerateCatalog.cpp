// GenerateCatalog.cpp — SIB-30 Ch6. Catalog -> matcher entries.
//
// P1: prefer a PERSISTENT UDataTable asset (cooks into a package), falling back to the
// committed CSV in the editor (so the headless gate stays green before the asset exists).

#include "GenerateCatalog.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

// Where the imported DataTable asset lives once Walt creates it (import the CSV in-editor).
static const TCHAR* GGenerateCatalogAssetPath = TEXT("/Game/Data/GenerateCatalog.GenerateCatalog");

FString GetGenerateCatalogCsvPath()
{
	// SIB-43/PK20: staged-first (Content/Data ships as loose NonUFS files —
	// the Journal/WebGame pattern), dev fallback to <ProjectDir>/Data.
	const FString Staged = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Data/GenerateCatalog.csv"));
	if (FPaths::FileExists(Staged))
	{
		return Staged;
	}
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/GenerateCatalog.csv"));
}

// Copy each DataTable row into a matcher entry (EntryId = the row name).
static void FillEntriesFromTable(const UDataTable* Table, TArray<FGenerateCatalogEntry>& Out)
{
	if (!Table)
	{
		return;
	}
	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FGenerateCatalogEntry* RowData = reinterpret_cast<const FGenerateCatalogEntry*>(Row.Value);
		if (!RowData)
		{
			continue;
		}
		FGenerateCatalogEntry Entry = *RowData;
		Entry.EntryId = Row.Key; // the row name is the stable EntryId
		Out.Add(Entry);
	}
}

bool LoadGenerateCatalog(TArray<FGenerateCatalogEntry>& OutEntries, FString& OutError)
{
	OutEntries.Reset();
	OutError.Reset();

	// 1) Prefer the persistent DataTable asset (works in PIE AND packaged builds).
	if (UDataTable* Asset = LoadObject<UDataTable>(nullptr, GGenerateCatalogAssetPath))
	{
		FillEntriesFromTable(Asset, OutEntries);
		if (OutEntries.Num() > 0)
		{
			return true;
		}
		OutError = TEXT("DataTable asset loaded but produced zero rows");
	}

	// 2) Editor fallback: build a transient table from the committed CSV. Lets the
	//    headless gate run before the asset is imported. (Editor-only API.)
#if WITH_EDITOR
	const FString Path = GetGenerateCatalogCsvPath();
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		OutError = FString::Printf(TEXT("no DataTable asset at %s and could not read CSV at %s"),
			GGenerateCatalogAssetPath, *Path);
		return false;
	}

	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage()); // auto-unique name
	Table->RowStruct = FGenerateCatalogEntry::StaticStruct();
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	if (Problems.Num() > 0)
	{
		OutError = FString::Printf(TEXT("CSV import reported %d problem(s): %s"),
			Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
	}

	FillEntriesFromTable(Table, OutEntries);
	if (OutEntries.Num() == 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("CSV produced zero rows");
		}
		return false;
	}
	return true;
#else
	if (OutError.IsEmpty())
	{
		OutError = FString::Printf(TEXT("no DataTable asset at %s (CSV fallback is editor-only)"), GGenerateCatalogAssetPath);
	}
	return false;
#endif
}
