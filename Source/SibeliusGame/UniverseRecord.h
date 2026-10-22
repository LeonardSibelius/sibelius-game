// UniverseRecord.h
//
// LEGACY (2026-10-22). Walt dropped the universe game - "i don't want to have that game" - and the
// forge and the plinths are gone. This record and UniverseSim stay only because the progression
// save holds FUniverseRecords (FProgressionState::UniverseGallery): 2.2.x-2.3.x saves must still
// load. Nothing writes one now.
//
// THE AI CITY - a universe he kept (GPT-6 Astra, 2026-10-17; tidied the same day). What the
// forge's K writes into the progression save, one per gallery slot (FProgressionState::
// UniverseGallery), and what an AUniverseExhibit reads back.
//
// It keeps the OUTCOME, not just the recipe. The seed and dials alone would regrow it only while
// UniverseSim stays exactly as it is; change an edge or a chronicle line one day and every kept
// universe would quietly become a different one. So the name, the ending, the stages reached and
// the whole chronicle are stored as they were, and Restore() hands them back without running
// the simulation at all.

#pragma once

#include "CoreMinimal.h"
#include "UniverseSim.h"
#include "UniverseRecord.generated.h"

USTRUCT()
struct SIBELIUSGAME_API FUniverseRecord
{
	GENERATED_BODY()

	// --- who it is ---
	UPROPERTY(SaveGame)
	FGuid Id;                        // one per run, so keeping the same run twice is caught

	UPROPERTY(SaveGame)
	int32 ModelVersion = 1;          // the record's shape; IsValid() refuses any it does not know

	// --- how it was made ---
	UPROPERTY(SaveGame)
	uint64 Seed = 0;

	UPROPERTY(SaveGame)
	double Gravity = 0.5;

	UPROPERTY(SaveGame)
	double Light = 0.5;

	UPROPERTY(SaveGame)
	double Time = 0.75;

	// --- what became of it, as it was ---
	UPROPERTY(SaveGame)
	FString Name;

	UPROPERTY(SaveGame)
	int32 Ending = 0;                // EUniverseEnding

	UPROPERTY(SaveGame)
	int32 LastStage = 0;             // EUniverseStage

	UPROPERTY(SaveGame)
	int32 Galaxies = 0;

	UPROPERTY(SaveGame)
	double StarHeat = 0.5;

	UPROPERTY(SaveGame)
	double AgeBillions = 0.0;

	UPROPERTY(SaveGame)
	bool bWorldThawed = false;

	UPROPERTY(SaveGame)
	TArray<FString> Chronicle;

	UPROPERTY(SaveGame)
	FString EndingTitle;

	UPROPERTY(SaveGame)
	FString EndingLine;

	/** A real kept universe, not an empty slot or a damaged save: every field in range. */
	bool IsValid() const;

	/** The record of a run the forge just grew. */
	static FUniverseRecord Capture(const FUniverseHistory& History, const FGuid& Identity);

	/** The history as it was kept - not regrown - for the forge to play back. */
	FUniverseHistory Restore() const;
};
