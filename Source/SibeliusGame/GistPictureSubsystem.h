// GistPictureSubsystem.h — THE GIST, illustrated: the painting of his words (docs/GIST_PICTURES_PLAN.md).
//
// G puts up an easel with a blank canvas; this asks the chat server (POST /v1/gist-picture) for
// a painting of the words, and the canvas shows it a few seconds later. No clay toy (Walt,
// 2026-11-01: "pictures only please"); a copy that cannot paint refuses. Every safeguard is
// on the server (backend/pictures.js); the game only asks, shows, and remembers:
//
//   - a painting arrives once and is kept in Saved/GistPictures/, so a saved painting comes
//     back with it and is never paid for twice;
//   - the last few paintings are what the agents hear about (backend/paintings.js);
//   - a Steam copy never asks - by a local switch, never a remote one (plan §6).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "GenerateTypes.h"
#include "MrsHallLines.h"
#include "GistPictureSubsystem.generated.h"

class AGistSite;
class UTexture2D;

/** A painting he had made: what the agents hear about. */
struct FGistPaintingNote
{
	FString Words;
	FString LocationId;
	FDateTime MadeUtc;
};

UCLASS()
class SIBELIUSGAME_API UGistPictureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGistPictureSubsystem* Get(const UObject* WorldContext);

	/** May this copy ask for paintings at all? sib.Pictures.Enable, and not a Steam copy. */
	static bool IsEnabled();

	/** A copy Steam installed or launched: SteamBuild.txt beside the game (written by the Steam
	    upload script), or Steam's SteamAppId in the environment. */
	static bool IsSteamCopy();

	/** Can G paint at all right now? Enabled, and a chat server to ask. When it cannot, a wish
	    the catalog does not cover gets Mrs. Hall's refusal (Walt chose that over the clay toy). */
	static bool CanPaint();

	/** Ask for a painting of these words for this easel. False, and no easel, when pictures are
	    off or one is already being painted (the server allows one at a time per player).
	    When it returns true, OnDone is called exactly once - true when the painting is on the
	    canvas (at once if painted before), false when it was refused or the artist is away. */
	bool Request(const FString& Words, AGistSite* Site, TFunction<void(bool)> OnDone = nullptr);

	/** The painting of these words from an earlier visit, or null. */
	UTexture2D* LoadCached(const FString& Words) const;

	/** His latest paintings, newest first. */
	TArray<FGistPaintingNote> RecentPaintings(int32 Max) const;

	bool IsBusy() const { return PendingHttp.IsValid(); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The cache file for these words (no extension: .jpg or .png is added). */
	static FString CacheBase(const FString& Words);

private:
	static FString PictureUrl();
	void OnPainted(const FString& Words, const TArray<uint8>& Bytes, const FString& Mime);
	void OnRefused(const FString& Reason);
	void Note(const FString& Words, const AGistSite* Site);
	void Finish(bool bPainted);
	void Say(EGenerateOutcome Reason, const TCHAR* Fallback);
	void LoadNotes();
	void SaveNotes() const;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttp;
	TWeakObjectPtr<AGistSite> PendingSite;
	FString PendingWords;
	TFunction<void(bool)> PendingDone;
	TArray<FGistPaintingNote> Notes;
	TMap<EGenerateOutcome, TArray<FMrsHallLine>> Lines;
	int32 LineCount = 0;
};
