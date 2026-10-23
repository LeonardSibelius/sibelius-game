// ArcadeMenuWidget.h
//
// E at a TV - the living room's or the bedroom's (InteractorComponent) - opens Leonard's
// Arcade. Two halves: NEW ASSIGNMENT on the left (four picks, the prompt they make, Copy, and
// where to save what the model gives back) and YOUR GAMES on the right (every game in the
// folder; click one to play it). ArcadeLibrary.h says why it works this way.
//
// PLAYING is this widget's other mode: the game fills the screen in Unreal's embedded Chromium,
// Esc comes back to the menu, Esc again to the house. The browser is OFFLINE - a request with a
// host is answered with nothing, the page may not navigate away or open a popup, and there is no
// context menu. Whatever a game asked the internet for is named afterwards, so the player can
// ask the model to fold it into the file.
//
// A GAME WITHOUT HIS CODE WORD ASKS FIRST (Walt, 2026-10-23: "yes add the gate"). Offline stops
// a page reaching the internet; it does not stop a page attacking the browser. Unreal 5.7's
// Chromium is 128, from 2024, and Epic runs it with Chromium's sandbox off (WebBrowserSingleton,
// no_sandbox), so a booby-trapped file from a stranger could use an old bug to run code on the
// PC. A game made from his own assignment carries his code word and plays at a click; anything
// else gets one screen saying so, and "Play anyway" - remembered for the session.
//
// IT DRAWS SMALL AND IS SCALED UP. Chromium renders off screen and every frame is copied into a
// texture, so a 2560x1440 page costs four times a 1280x720 one. The page is laid out
// sib.Arcade.RenderHeight pixels tall, in the screen's own shape, and a render transform - which
// changes how it is drawn, not the size Chromium is told - stretches it to fill the screen.
//
// Native (C++-built) like the dance menu. Close() is the one place the game's input comes back,
// as CloseScreen() is for the slot cabinet.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcadeLibrary.h"
#include "ArcadeMenuWidget.generated.h"

class SBox;
class SVerticalBox;
class SWebBrowserView;
class SWidget;
struct FWebNavigationRequest;

UCLASS()
class SIBELIUSGAME_API UArcadeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Show the arcade and take the keyboard and mouse. */
	void Open();

	/** Stop any game and hand input back to the game. Safe to call twice. */
	void Close();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:
	TSharedRef<SWidget> BuildMenu();
	TSharedRef<SWidget> BuildPlay(const FString& Url);
	TSharedRef<SWidget> BuildConfirm(int32 GameIndex);
	void ShowMenu();
	/** A game without his code word asks first (BuildConfirm) unless bConfirmed. */
	void Play(int32 GameIndex, bool bConfirmed = false);
	void StopPlaying();

	void Step(SibArcade::EPick Row, int32 Delta);
	void SurpriseMe();
	void CopyPrompt();
	void OpenFolder();
	void Rescan();
	void RebuildGameList();
	FString PlayerCode() const;

	// The browser's offline rule, applied three ways (ArcadeLibrary.h, IsOffline).
	bool HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	bool HandleLoadUrl(const FString& Method, const FString& Url, FString& Response);

	TSharedPtr<SBox> Root;
	TSharedPtr<SVerticalBox> GameList;
	TSharedPtr<SBox> PlayBox;
	TSharedPtr<SWebBrowserView> Browser;

	SibArcade::FAssignment Assignment;
	TArray<SibArcade::FGame> Games;
	FString Code;
	FString Folder;
	FString Notice;
	FLinearColor NoticeColor = FLinearColor::White;
	TSet<FString> BlockedHosts;
	FString PlayingTitle;
	bool bPlaying = false;
	bool bConfirming = false;     // the "before it plays" screen is up
	TSet<FString> TrustedPaths;   // games he said "Play anyway" to, this session
	int32 FocusTicks = 0;      // ticks left to keep putting the keyboard on the page
	float PlaySeconds = 0.0f;  // for the "[Esc]" hint, which fades
};
