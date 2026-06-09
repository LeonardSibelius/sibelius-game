// MrsHallLines.h
//
// SIB-30 — Ch6 P2. The STORY touches the mechanic here: refusals are spoken by Mrs.
// Hall (the protagonist's stern, AI-skeptic manager), in her own voice, from DATA Walt
// can edit/grow without recompiling — a CSV-backed UDataTable (mirrors GenerateCatalog).
//
// Two tables:
//   - Data/MrsHallLines.csv  (Reason, Line, AudioKey) — multiple lines per refusal reason,
//     selected deterministically by a rotating counter (NO RNG/clock — preserves the
//     smoke-test discipline). P2.5: AudioKey names the pre-generated voice clip for the
//     line — the WAV filename and imported USoundWave both equal the key, loaded from
//     /Game/Audio/MrsHall/<AudioKey> at refusal time (silent if not yet imported).
//   - Data/MrsHallBlocklist.csv (Word)       — DECISION DC: obviously-bad words/intents
//     that classify as RefusedUnsafe.
//
// CRITICAL voice note: Mrs. Hall never uses the protagonist's name (he earns "Leonard
// Sibelius" over the game; she refuses it). She addresses him only as "Programmer" —
// her clipped "You. Programmer." Keep NO "Leonard" in any line.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GenerateTypes.h"   // EGenerateOutcome
#include "MrsHallLines.generated.h"

// One refusal line. DataTable row (CSV-backed). Several rows may share a Reason; the
// matcher's refusal outcome selects the group. Row NAME is just a unique key.
USTRUCT()
struct FMrsHallLineRow : public FTableRowBase
{
	GENERATED_BODY()

	// Which refusal this answers: NoMatch | Ambiguous | OverBudget | Unsafe.
	UPROPERTY(EditAnywhere, Category = "MrsHall")
	FString Reason;

	// The line she says. Plain string (FText::FromString at display) to dodge FText-in-CSV
	// import quirks; these are dev-authored display strings, not localized yet.
	UPROPERTY(EditAnywhere, Category = "MrsHall")
	FString Line;

	// P2.5: stable key for this line's pre-generated voice clip. The WAV filename and the
	// imported USoundWave asset both = this key (e.g. mrshall_nomatch_1.wav -> asset
	// mrshall_nomatch_1), loaded from /Game/Audio/MrsHall/<AudioKey> on refusal.
	UPROPERTY(EditAnywhere, Category = "MrsHall")
	FString AudioKey;
};

// One picked refusal line carried to the handler: the display text + its voice-clip key.
struct FMrsHallLine
{
	FString Line;
	FString AudioKey;

	bool IsEmpty() const { return Line.IsEmpty(); }
};

// One disallowed word/intent (DECISION DC). An input token hit -> RefusedUnsafe. Row
// name is just a key; the curated word lives in the Word column.
USTRUCT()
struct FGenerateBlocklistRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "MrsHall")
	FString Word;
};

// Map a CSV Reason string to a refusal outcome. Unrecognized -> RefusedNoMatch.
SIBELIUSGAME_API EGenerateOutcome MrsHallReasonToOutcome(const FString& Reason);

// Load all refusal lines grouped by outcome (asset-first, CSV fallback in-editor). Each
// line carries its display text + AudioKey.
SIBELIUSGAME_API bool LoadMrsHallLines(TMap<EGenerateOutcome, TArray<FMrsHallLine>>& OutLines, FString& OutError);

// Load the unsafe-word blocklist, lowercased + trimmed (asset-first, CSV fallback).
SIBELIUSGAME_API bool LoadGenerateBlocklist(TArray<FString>& OutWords, FString& OutError);

// Deterministically pick one line for a refusal outcome. Selector rotates within the
// group (e.g. a per-session refusal counter) — pure given (Lines, Reason, Selector), no
// RNG/clock. Returns an empty FMrsHallLine if there are no lines for that outcome.
SIBELIUSGAME_API FMrsHallLine PickMrsHallLine(
	const TMap<EGenerateOutcome, TArray<FMrsHallLine>>& Lines, EGenerateOutcome Reason, int32 Selector);

// P2.5: print the full Line -> AudioKey manifest to the log, so Walt knows exactly which
// clip to record per line and how to name it (filename + asset = AudioKey, .wav).
SIBELIUSGAME_API void LogMrsHallAudioManifest(const TMap<EGenerateOutcome, TArray<FMrsHallLine>>& Lines);
