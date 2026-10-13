// FreakbookSubsystem.h
//
// Freakbook's one state (FreakbookModel.h), held for the whole session and saved to its own slot
// (FreakbookSaveGame.h). A UGameInstanceSubsystem because it has to outlive level travel: the
// laptop is in the living room, Dee's binder is in the attic, and later things may lie in other
// worlds. Everything that changes the state comes through here and is written at once - the file
// is a few kilobytes, and a crash should never cost him a conversation.
//
// THE CLIENT SAYS EVERYTHING ITSELF, FOR NOW. Send answers from the lines written in
// FreakbookModel.cpp; nothing goes to the chat server while Valve reviews the chat (the build on
// the review branch must not meet a new kind of request). When Freakbook's live replies are
// deployed, Send's body changes and its callers do not.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FreakbookModel.h"
#include "FreakbookSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnFreakbookChanged);

UCLASS()
class SIBELIUSGAME_API UFreakbookSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UFreakbookSubsystem* Get(const UObject* WorldContextObject);

	const FFreakbookState& State() const { return Model; }

	/** Fired after anything changes: the items in the house listen, to hide once taken. */
	FOnFreakbookChanged OnChanged;

	/** notHUMAN has judged him: from now on the laptop lets him straight in. */
	void SetVerified(int32 MachinePercent);
	bool IsVerified() const { return Model.bVerified; }

	void Login();
	FFreakMessage Send(EFreakPerson Who, const FString& Message);
	bool Grant(EFreakPerson Who);
	void TakeItem(EFreakPerson Who);
	void ToggleLike(const FString& PostId);

	bool ItemStillThere(EFreakPerson Who) const { return FFreakbook::ItemStillThere(Model, Who); }

	/** The slot. Deliberately neither the Progression slot nor the agents' NpcChat slot. */
	static FString SlotName() { return TEXT("Freakbook"); }

	/** Forget Freakbook entirely: the slot is deleted, as UNpcChatSubsystem::ForgetAll does. */
	void ForgetAll();

	static FString ToFString(const std::string& Text) { return FString(UTF8_TO_TCHAR(Text.c_str())); }
	static std::string ToUtf8(const FString& Text) { return std::string(TCHAR_TO_UTF8(*Text)); }

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	void Changed();   // save, then tell whoever listens
	void LoadFromDisk();

	FFreakbookState Model;
};
