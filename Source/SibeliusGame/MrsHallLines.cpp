// MrsHallLines.cpp — SIB-30 Ch6 P2. Load Mrs. Hall's refusal lines + the DC blocklist.
//
// Same load strategy as GenerateCatalog: prefer a PERSISTENT UDataTable asset (cooks into
// a package), fall back to the committed CSV in-editor (so PIE + the headless gate work
// before the asset is imported). Packaging caveat is identical: import the CSVs as
// DataTable assets for a packaged build.

#include "MrsHallLines.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

// Load a UDataTable from an asset path, falling back to a CSV string in-editor. Returns
// null (with OutError set) only when neither source yields a usable table.
static UDataTable* LoadTableAssetOrCsv(const TCHAR* AssetPath, const FString& CsvPath,
	UScriptStruct* RowStruct, FString& OutError)
{
	// 1) Persistent asset (works in PIE AND packaged builds).
	if (UDataTable* Asset = LoadObject<UDataTable>(nullptr, AssetPath))
	{
		if (Asset->GetRowMap().Num() > 0)
		{
			return Asset;
		}
		OutError = TEXT("DataTable asset loaded but produced zero rows");
	}

	// 2) Editor fallback: a transient table from the committed CSV.
#if WITH_EDITOR
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
	{
		OutError = FString::Printf(TEXT("no DataTable asset at %s and could not read CSV at %s"), AssetPath, *CsvPath);
		return nullptr;
	}

	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage()); // auto-unique name
	Table->RowStruct = RowStruct;
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	if (Problems.Num() > 0)
	{
		OutError = FString::Printf(TEXT("CSV import reported %d problem(s): %s"),
			Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
	}
	return Table;
#else
	if (OutError.IsEmpty())
	{
		OutError = FString::Printf(TEXT("no DataTable asset at %s (CSV fallback is editor-only)"), AssetPath);
	}
	return nullptr;
#endif
}

EGenerateOutcome MrsHallReasonToOutcome(const FString& Reason)
{
	if (Reason.Equals(TEXT("Ambiguous"), ESearchCase::IgnoreCase))  { return EGenerateOutcome::RefusedAmbiguous; }
	if (Reason.Equals(TEXT("OverBudget"), ESearchCase::IgnoreCase)) { return EGenerateOutcome::RefusedOverBudget; }
	if (Reason.Equals(TEXT("Unsafe"), ESearchCase::IgnoreCase))     { return EGenerateOutcome::RefusedUnsafe; }
	return EGenerateOutcome::RefusedNoMatch; // "NoMatch" + anything unrecognized
}

bool LoadMrsHallLines(TMap<EGenerateOutcome, TArray<FString>>& OutLines, FString& OutError)
{
	OutLines.Reset();
	OutError.Reset();

	const FString CsvPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/MrsHallLines.csv"));
	UDataTable* Table = LoadTableAssetOrCsv(TEXT("/Game/Data/MrsHallLines.MrsHallLines"),
		CsvPath, FMrsHallLineRow::StaticStruct(), OutError);
	if (!Table)
	{
		return false;
	}

	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FMrsHallLineRow* Data = reinterpret_cast<const FMrsHallLineRow*>(Row.Value);
		if (!Data || Data->Line.IsEmpty())
		{
			continue;
		}
		OutLines.FindOrAdd(MrsHallReasonToOutcome(Data->Reason)).Add(Data->Line);
	}

	if (OutLines.Num() == 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("no Mrs. Hall lines parsed");
		}
		return false;
	}
	return true;
}

bool LoadGenerateBlocklist(TArray<FString>& OutWords, FString& OutError)
{
	OutWords.Reset();
	OutError.Reset();

	const FString CsvPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/MrsHallBlocklist.csv"));
	UDataTable* Table = LoadTableAssetOrCsv(TEXT("/Game/Data/MrsHallBlocklist.MrsHallBlocklist"),
		CsvPath, FGenerateBlocklistRow::StaticStruct(), OutError);
	if (!Table)
	{
		return false;
	}

	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FGenerateBlocklistRow* Data = reinterpret_cast<const FGenerateBlocklistRow*>(Row.Value);
		if (!Data || Data->Word.IsEmpty())
		{
			continue;
		}
		OutWords.Add(Data->Word.ToLower().TrimStartAndEnd());
	}

	if (OutWords.Num() == 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("blocklist produced zero words");
		}
		return false;
	}
	return true;
}

FString PickMrsHallLine(const TMap<EGenerateOutcome, TArray<FString>>& Lines,
	EGenerateOutcome Reason, int32 Selector)
{
	const TArray<FString>* Group = Lines.Find(Reason);
	if (!Group || Group->Num() == 0)
	{
		return FString();
	}
	const int32 N = Group->Num();
	const int32 Idx = ((Selector % N) + N) % N; // safe rotation, even for a negative selector
	return (*Group)[Idx];
}
