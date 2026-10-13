// FreakbookSubsystem.cpp — see the header.

#include "FreakbookSubsystem.h"

#include "FreakbookSaveGame.h"
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusSaveIO.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

UFreakbookSubsystem* UFreakbookSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UFreakbookSubsystem>() : nullptr;
}

void UFreakbookSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromDisk();
}

void UFreakbookSubsystem::LoadFromDisk()
{
	Model = FFreakbookState();
	if (!FSibeliusSaveIO::Has(SlotName()))
	{
		return;   // never logged in. Not an error.
	}
	const UFreakbookSaveGame* Save = Cast<UFreakbookSaveGame>(FSibeliusSaveIO::Load(SlotName()));
	if (!Save || Save->SaveVersion <= 0 || Save->SaveVersion > UFreakbookSaveGame::CurrentSaveVersion)
	{
		// Left on disk untouched, so a newer build can still read it; this one starts fresh.
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Freakbook] the save could not be read; starting fresh"));
		return;
	}
	FFreakbookState Loaded;
	if (FFreakbook::LoadText(ToUtf8(Save->State), Loaded))
	{
		Model = Loaded;
	}
	else
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Freakbook] the saved state was not readable; starting fresh"));
	}
}

void UFreakbookSubsystem::Changed()
{
	UFreakbookSaveGame* Save = NewObject<UFreakbookSaveGame>(GetTransientPackage());
	Save->SaveVersion = UFreakbookSaveGame::CurrentSaveVersion;
	Save->State = ToFString(FFreakbook::SaveText(Model));
	if (!FSibeliusSaveIO::Commit(Save, SlotName()))
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Freakbook] could not write the save slot"));
	}
	OnChanged.Broadcast();
}

void UFreakbookSubsystem::SetVerified(int32 MachinePercent)
{
	Model.bVerified = true;
	Model.MachinePercent = MachinePercent;
	Changed();
}

void UFreakbookSubsystem::Login()
{
	FFreakbook::Login(Model);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Freakbook] Leonard logs in (visit %d)"), Model.Logins);
	Changed();
}

FFreakMessage UFreakbookSubsystem::Send(EFreakPerson Who, const FString& Message)
{
	const FFreakMessage Reply = FFreakbook::Send(Model, Who, ToUtf8(Message));
	if (!Reply.Text.empty())
	{
		Changed();
	}
	return Reply;
}

bool UFreakbookSubsystem::Grant(EFreakPerson Who)
{
	if (!FFreakbook::Grant(Model, Who))
	{
		return false;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[Freakbook] %s got what they wanted (%d friends)"),
		*ToFString(FFreakbook::Profile(Who).Short), FFreakbook::Friends(Model));
	Changed();
	return true;
}

void UFreakbookSubsystem::TakeItem(EFreakPerson Who)
{
	FFreakbook::TakeItem(Model, Who);
	Changed();
}

void UFreakbookSubsystem::ToggleLike(const FString& PostId)
{
	FFreakbook::ToggleLike(Model, ToUtf8(PostId));
	Changed();
}

void UFreakbookSubsystem::ForgetAll()
{
	Model = FFreakbookState();
	FSibeliusSaveIO::Delete(SlotName());
	OnChanged.Broadcast();
}
