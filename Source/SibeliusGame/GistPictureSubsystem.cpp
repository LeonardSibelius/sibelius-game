// GistPictureSubsystem.cpp — see the header.

#include "GistPictureSubsystem.h"
#include "GistSite.h"
#include "MrsHallSubsystem.h"
#include "NpcChatSubsystem.h"
#include "SibeliusGame.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace GistPictureDetail
{
	TAutoConsoleVariable<int32> CVarPicturesEnable(
		TEXT("sib.Pictures.Enable"),
		1,
		TEXT("THE GIST, illustrated: 1 asks the chat server to paint each gist's words, 0 never asks. ")
		TEXT("A Steam copy never asks whatever this says (UGistPictureSubsystem::IsSteamCopy)."),
		ECVF_Default);

	// The server draws in ~3-5 s and screens twice; a dead connection is what this catches.
	constexpr float PictureHttpTimeoutSeconds = 40.0f;
	const TCHAR* PicturePath = TEXT("/v1/gist-picture");
	constexpr int32 NotesKept = 10;

	FString PicturesDir() { return FPaths::ProjectSavedDir() / TEXT("GistPictures"); }
	FString NotesPath() { return PicturesDir() / TEXT("paintings.json"); }

	/** The words as the server reads them (pictures.js cleanWords), lowercased: one painting per wish. */
	FString Normalize(const FString& Words)
	{
		FString Out;
		bool bSpace = false;
		for (const TCHAR C : Words.ToLower())
		{
			const bool bBlank = FChar::IsWhitespace(C) || C < 32;
			if (bBlank) { bSpace = !Out.IsEmpty(); continue; }
			if (bSpace) { Out.AppendChar(TEXT(' ')); bSpace = false; }
			Out.AppendChar(C);
		}
		return Out.Left(200);
	}

	FString LocationOf(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString();
	}
}

UGistPictureSubsystem* UGistPictureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGistPictureSubsystem>() : nullptr;
}

bool UGistPictureSubsystem::IsSteamCopy()
{
	/* THE SWITCH IS LOCAL, NEVER REMOTE (plan §6). Valve has not reviewed pictures, so a Steam copy
	   must not be able to ask for one - and the server must not be able to turn that on, or the
	   switch would be a way round the review. The Steam upload script leaves SteamBuild.txt
	   beside the game; Steam itself sets SteamAppId for what it launches. Either one is enough. */
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("SteamAppId")).IsEmpty())
	{
		return true;
	}
	return FPaths::FileExists(FPaths::LaunchDir() / TEXT("SteamBuild.txt"))
		|| FPaths::FileExists(FPaths::ProjectDir() / TEXT("SteamBuild.txt"));
}

bool UGistPictureSubsystem::IsEnabled()
{
	return GistPictureDetail::CVarPicturesEnable.GetValueOnGameThread() != 0 && !IsSteamCopy();
}

bool UGistPictureSubsystem::CanPaint()
{
	return IsEnabled() && !PictureUrl().IsEmpty()
		&& UNpcChatSubsystem::IsUsableProxyUrl(UNpcChatSubsystem::ConfiguredProxyUrl());
}

FString UGistPictureSubsystem::PictureUrl()
{
	// The chat endpoint's host, the picture path: one setting (sib.Chat.ProxyUrl) for both.
	const FString Chat = UNpcChatSubsystem::ProxyUrl();
	const int32 Path = Chat.Find(TEXT("/v1/"));
	return Path == INDEX_NONE ? FString() : Chat.Left(Path) + GistPictureDetail::PicturePath;
}

FString UGistPictureSubsystem::CacheBase(const FString& Words)
{
	return GistPictureDetail::PicturesDir() / FMD5::HashAnsiString(*GistPictureDetail::Normalize(Words));
}

void UGistPictureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Error;
	if (!LoadMrsHallLines(Lines, Error))
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Picture] no Mrs. Hall lines (%s) - she will use her fallbacks."), *Error);
	}
	LoadNotes();
}

void UGistPictureSubsystem::Deinitialize()
{
	if (PendingHttp.IsValid())
	{
		PendingHttp->OnProcessRequestComplete().Unbind();
		PendingHttp->CancelRequest();
		PendingHttp.Reset();
	}
	PendingDone = nullptr;   // the game is closing: nobody is left to pay or to be told
	Super::Deinitialize();
}

bool UGistPictureSubsystem::Request(const FString& Words, AGistSite* Site, TFunction<void(bool)> OnDone)
{
	if (!Site || !CanPaint() || IsBusy())
	{
		return false;
	}
	PendingSite = Site;
	PendingWords = Words;
	PendingDone = MoveTemp(OnDone);

	// Painted before, by these very words: no need to ask again (and pay Google again).
	if (UTexture2D* Cached = LoadCached(Words))
	{
		Site->ShowPicture(Cached, /*bFresh=*/true);
		Note(Words, Site);
		Say(EGenerateOutcome::PicturePainted, TEXT("The artist finished it. Don't touch the paint, Programmer."));
		Finish(true);
		return true;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("requestId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	Body->SetStringField(TEXT("words"), Words);
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));

	PendingHttp = FHttpModule::Get().CreateRequest();
	PendingHttp->SetURL(PictureUrl());
	PendingHttp->SetVerb(TEXT("POST"));
	PendingHttp->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	PendingHttp->SetTimeout(GistPictureDetail::PictureHttpTimeoutSeconds);
	PendingHttp->SetContentAsString(Json);
	Site->ShowEasel();

	TWeakObjectPtr<UGistPictureSubsystem> WeakThis(this);
	PendingHttp->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			UGistPictureSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->PendingHttp.Reset();
			if (!bConnectedOk || !Response.IsValid())
			{
				Self->OnRefused(TEXT("network"));
				return;
			}
			TSharedPtr<FJsonObject> Reply;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Reply) || !Reply.IsValid())
			{
				Self->OnRefused(TEXT("service"));
				return;
			}
			FString Status, Base64, Mime;
			Reply->TryGetStringField(TEXT("status"), Status);
			if (Status == TEXT("ok") && Reply->TryGetStringField(TEXT("imageBase64"), Base64))
			{
				Reply->TryGetStringField(TEXT("mime"), Mime);
				TArray<uint8> Bytes;
				if (FBase64::Decode(Base64, Bytes) && Bytes.Num() > 0)
				{
					Self->OnPainted(Self->PendingWords, Bytes, Mime);
					return;
				}
			}
			FString Reason;
			Reply->TryGetStringField(TEXT("reason"), Reason);
			Self->OnRefused(Reason.IsEmpty() ? FString(TEXT("service")) : Reason);
		});

	if (!PendingHttp->ProcessRequest())
	{
		PendingHttp.Reset();
		PendingDone = nullptr;   // false is the answer; OnDone is only for a true
		Site->HideEasel();
		return false;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Picture] asked for \"%s\"."), *Words);
	return true;
}

void UGistPictureSubsystem::OnPainted(const FString& Words, const TArray<uint8>& Bytes, const FString& Mime)
{
	using namespace GistPictureDetail;
	// Kept on disk first: the texture is for now, the file is for every visit after.
	const FString File = CacheBase(Words) + (Mime.Contains(TEXT("png")) ? TEXT(".png") : TEXT(".jpg"));
	IFileManager::Get().MakeDirectory(*PicturesDir(), /*Tree=*/true);
	FFileHelper::SaveArrayToFile(Bytes, *File);

	UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(Bytes);
	AGistSite* Site = PendingSite.Get();
	if (!Texture)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Picture] \"%s\" arrived but could not be decoded (%d bytes, %s)."), *Words, Bytes.Num(), *Mime);
		OnRefused(TEXT("undecodable"));
		return;
	}
	if (Site)
	{
		Site->ShowPicture(Texture, /*bFresh=*/true);
	}
	Note(Words, Site);
	Say(EGenerateOutcome::PicturePainted, TEXT("The artist finished it. Don't touch the paint, Programmer."));
	UE_LOG(LogSibeliusGame, Display, TEXT("[Picture] painted \"%s\" (%d KB)."), *Words, Bytes.Num() / 1024);
	Finish(true);
}

void UGistPictureSubsystem::OnRefused(const FString& Reason)
{
	if (AGistSite* Site = PendingSite.Get())
	{
		Site->HideEasel();
	}
	// "blocked": the screens or Google said no. Anything else - the cap, the player's five a day,
	// pictures switched off, the network - is the artist having gone home.
	const bool bRefused = Reason == TEXT("blocked");
	Say(bRefused ? EGenerateOutcome::PictureRefused : EGenerateOutcome::PictureClosed,
		bRefused ? TEXT("The artist won't paint that. Neither would I.") : TEXT("The artist has gone home for the day, Programmer."));
	UE_LOG(LogSibeliusGame, Display, TEXT("[Picture] no painting for \"%s\": %s."), *PendingWords, *Reason);
	Finish(false);
}

void UGistPictureSubsystem::Note(const FString& Words, const AGistSite* Site)
{
	using namespace GistPictureDetail;
	Notes.Insert({Words, LocationOf(Site ? static_cast<const UObject*>(Site) : GetGameInstance()), FDateTime::UtcNow()}, 0);
	if (Notes.Num() > NotesKept)
	{
		Notes.SetNum(NotesKept);
	}
	SaveNotes();
}

void UGistPictureSubsystem::Finish(bool bPainted)
{
	// Taken out first: the caller may ask for the next painting from inside its answer.
	TFunction<void(bool)> Done = MoveTemp(PendingDone);
	PendingDone = nullptr;
	if (Done)
	{
		Done(bPainted);
	}
}

void UGistPictureSubsystem::Say(EGenerateOutcome Reason, const TCHAR* Fallback)
{
	const FMrsHallLine Picked = PickMrsHallLine(Lines, Reason, LineCount++);
	const FString Line = Picked.IsEmpty() ? FString(Fallback) : Picked.Line;
	const UObject* Context = PendingSite.IsValid() ? static_cast<const UObject*>(PendingSite.Get()) : GetGameInstance();
	if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(Context))
	{
		Hall->SayLine(Line, Picked.AudioKey);
	}
}

UTexture2D* UGistPictureSubsystem::LoadCached(const FString& Words) const
{
	for (const TCHAR* Ext : {TEXT(".jpg"), TEXT(".png")})
	{
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *(CacheBase(Words) + Ext), FILEREAD_Silent) && Bytes.Num() > 0)
		{
			return FImageUtils::ImportBufferAsTexture2D(Bytes);
		}
	}
	return nullptr;
}

TArray<FGistPaintingNote> UGistPictureSubsystem::RecentPaintings(int32 Max) const
{
	TArray<FGistPaintingNote> Out = Notes;
	if (Out.Num() > Max)
	{
		Out.SetNum(Max);
	}
	return Out;
}

void UGistPictureSubsystem::LoadNotes()
{
	Notes.Reset();
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *GistPictureDetail::NotesPath())
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return;   // none yet, or unreadable: no paintings to talk about, which is not an error
	}
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Root->TryGetArrayField(TEXT("paintings"), Items))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Item.IsValid() || !Item->TryGetObject(Obj))
		{
			continue;
		}
		FGistPaintingNote Note;
		FString Made;
		(*Obj)->TryGetStringField(TEXT("words"), Note.Words);
		(*Obj)->TryGetStringField(TEXT("locationId"), Note.LocationId);
		(*Obj)->TryGetStringField(TEXT("madeUtc"), Made);
		FDateTime::ParseIso8601(*Made, Note.MadeUtc);
		if (!Note.Words.IsEmpty() && Notes.Num() < GistPictureDetail::NotesKept)
		{
			Notes.Add(Note);
		}
	}
}

void UGistPictureSubsystem::SaveNotes() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FGistPaintingNote& Note : Notes)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("words"), Note.Words);
		Obj->SetStringField(TEXT("locationId"), Note.LocationId);
		Obj->SetStringField(TEXT("madeUtc"), Note.MadeUtc.ToIso8601());
		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("paintings"), Items);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	IFileManager::Get().MakeDirectory(*GistPictureDetail::PicturesDir(), /*Tree=*/true);
	FFileHelper::SaveStringToFile(Json, *GistPictureDetail::NotesPath());
}
