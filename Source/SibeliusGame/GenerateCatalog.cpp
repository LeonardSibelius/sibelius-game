// GenerateCatalog.cpp — SIB-30 Ch6 P0. CSV -> UDataTable -> matcher entries.

#include "GenerateCatalog.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

FString GetGenerateCatalogCsvPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/GenerateCatalog.csv"));
}

bool LoadGenerateCatalog(TArray<FGenerateCatalogEntry>& OutEntries, FString& OutError)
{
	OutEntries.Reset();
	OutError.Reset();

	const FString Path = GetGenerateCatalogCsvPath();

	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		OutError = FString::Printf(TEXT("could not read catalog CSV at %s"), *Path);
		return false;
	}

#if WITH_EDITOR
	// Build a transient DataTable straight from the committed CSV — no .uasset needed.
	// (CreateTableFromCSVString is editor-only; a packaged build reads a DataTable asset.)
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage()); // auto-unique name
	Table->RowStruct = FGenerateCatalogEntry::StaticStruct();

	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	if (Problems.Num() > 0)
	{
		// Report but keep going — well-formed rows are still usable.
		OutError = FString::Printf(TEXT("CSV import reported %d problem(s): %s"),
			Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
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
		OutEntries.Add(Entry);
	}

	if (OutEntries.Num() == 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("catalog loaded zero rows");
		}
		return false;
	}
	return true;
#else
	OutError = TEXT("CSV catalog load is editor-only in P0; a packaged build loads a DataTable asset (P1)");
	return false;
#endif
}
