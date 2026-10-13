// FreakbookWidget.h
//
// Freakbook's screen on the laptop (FreakbookModel.h has the idea; docs/FREAKBOOK.md the design).
// A native UUserWidget like UNotHumanWidget, whose screen it follows from: the layout is built in
// code - a browser window on the dark desktop, a purple header, the people on the left, the feed
// in the middle, the people Leonard may know on the right - and it only presents the state that
// UFreakbookSubsystem keeps.
//
// The pages: the login, the feed, a profile, a conversation, Margo's dashboard and the bank's
// website with Jaxx's cookie banner. The last two are where two of the wants come true - opening
// the dashboard is its first view; clicking Accept is the banner's first Accept - and a profile's
// button sends Dee, Gunther or Brent the thing he found in the house.
//
// Replies "type" for a moment before they appear: the line is chosen and saved when he sends,
// and only shown a second or two later. Esc logs out and closes the laptop.
//
// Pictures are /Game/Freakbook/T_FB_<name> (make_freakbook_assets.py), loaded by name when first
// shown; the folder is cooked by name, since nothing in a level references them.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FreakbookModel.h"
#include "FreakbookWidget.generated.h"

class SBorder;
class SEditableTextBox;
class SScrollBox;
class UFreakbookSubsystem;
class UTexture2D;
struct FSlateBrush;

DECLARE_DELEGATE(FOnFreakbookClosed);

UCLASS()
class SIBELIUSGAME_API UFreakbookWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFreakbookWidget(const FObjectInitializer& ObjectInitializer);

	FOnFreakbookClosed OnClosed;

	/** The login page, fresh. */
	void Open();

#if !UE_BUILD_SHIPPING
	/** Development: straight to a page (sib.Freakbook.Open): 0 login, 1 feed, 2 profile, 3 thread, 4 dashboard, 5 bank. */
	void DebugShow(int32 InView, int32 InPerson);
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EView : uint8
	{
		Login,
		Feed,
		Profile,     // Person's
		Thread,      // the conversation with Person
		Dashboard,   // Margo's
		Bank,        // Jaxx's banner
	};

	TSharedRef<SWidget> BuildLayout();
	void Show(EView InView, EFreakPerson InPerson = EFreakPerson::Dee);
	void Refresh(bool bKeepScroll = false);

	TSharedRef<SWidget> BuildLogin();
	TSharedRef<SWidget> BuildLeft();
	TSharedRef<SWidget> BuildRight();
	TSharedRef<SWidget> BuildFeed();
	TSharedRef<SWidget> BuildProfile();
	TSharedRef<SWidget> BuildThread();
	TSharedRef<SWidget> BuildDashboard();
	TSharedRef<SWidget> BuildBank();
	TSharedRef<SWidget> BuildPost(const FFreakPost& Post);
	TSharedRef<SWidget> BuildNotice();

	TSharedRef<SWidget> Avatar(const FString& Picture, float Size);
	TSharedRef<SWidget> Photo(const FString& Picture, float Width);
	const FSlateBrush* PictureBrush(const FString& Picture);

	void SendDraft();
	void Close();
	UFreakbookSubsystem* Book() const;

	EView View = EView::Login;
	EFreakPerson Person = EFreakPerson::Dee;
	float Clock = 0.0f;
	float RevealAt = -1.0f;   // the last reply is "typing" until the clock passes this
	FString Notice;           // one line over the page, after he did something
	FString Draft;            // what he is typing, kept across rebuilds
	int32 BankBanner = 0;     // 0 the banner; 1 its settings (a second banner); 2 the X; 3 accepted

	TMap<FString, TSharedPtr<FSlateBrush>> Brushes;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> Textures;

	TSharedPtr<SBorder> Body;
	TSharedPtr<SScrollBox> Scroll;
	TSharedPtr<SEditableTextBox> Field;
};
