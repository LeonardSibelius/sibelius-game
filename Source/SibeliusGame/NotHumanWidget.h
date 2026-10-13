// NotHumanWidget.h
//
// The laptop's screen for PROVE YOU'RE NOT HUMAN (NotHumanCaptcha.h has the idea and the
// questions). A native UUserWidget, like the Journal and the Generate panel: the layout is
// built in code - a browser window on a dark desktop, holding a CAPTCHA card - and it only
// presents FNotHumanRun. What the model cannot know it measures here: how long he took, whether
// he moved in the stillness test, when he clicked the reflex button, and where the checkbox
// that runs from the mouse has run to.
//
// CODE VISION works here as everywhere: hold [V]. Once Leonard has the power, each test shows
// the machine's view of itself; before, the strip says who gives it. In the stillness test V is
// a key like any other, and holding it counts as moving.
//
// The panel takes the keyboard while it is up (UIOnly); ANotHumanTerminal opens it and gives
// the game its input back when it closes. Esc closes it.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NotHumanCaptcha.h"
#include "NotHumanWidget.generated.h"

class SBorder;
class SBox;
class SButton;
class SEditableTextBox;
class STextBlock;
class SVerticalBox;
class UTexture2D;

DECLARE_DELEGATE(FOnNotHumanClosed);
DECLARE_DELEGATE(FOnNotHumanFreakbook);

UCLASS()
class SIBELIUSGAME_API UNotHumanWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNotHumanWidget(const FObjectInitializer& ObjectInitializer);

	FOnNotHumanClosed OnClosed;

	/**
	 * LOG IN TO FREAKBOOK (docs/FREAKBOOK.md): offered on the verdict, and on the checkbox page once
	 * he has been judged before - notHUMAN is the login. ANotHumanTerminal swaps the screens.
	 */
	FOnNotHumanFreakbook OnFreakbook;

	/** Back to the checkbox, with a fresh run. bHasCodeVision: whether [V] shows the machine's view. */
	void Open(bool bHasCodeVision);

	/** Development: straight to one test (sib.NotHuman.Show N), to see each without a whole run. */
	void DebugShowTest(int32 Kind);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EStage : uint8
	{
		Intro,       // the checkbox: "I'm not a human"
		Verifying,   // a moment of thinking about it
		Test,        // a test, waiting for his answer
		Judged,      // a test, answered: the verdict and NEXT
		Verdict,     // "You are 63% machine."
	};

	TSharedRef<SWidget> BuildLayout();
	TSharedRef<SWidget> BuildIntroBody();
	TSharedRef<SWidget> BuildTestBody();
	TSharedRef<SWidget> BuildVerdictBody();

	void ShowIntro();
	void ShowTest();
	void ShowVerdict();
	void Submit(const FNotHumanAnswer& Answer);
	void SubmitCurrent();      // VERIFY: the answer as it stands
	void OnAction();           // the footer button: VERIFY, I GIVE UP, NEXT or TRY AGAIN
	void Advance();
	void Close();
	void FocusForStage();
	void BuildInkTexture(const FNotHumanChallenge& Challenge);   // the inkblot, as a picture
	void RecordFlash();                                         // one association word answered, or out of time
	void RefreshChrome();      // header, progress, footer button, result line

	bool IsVisionOn() const { return bVHeld && bCanSee; }
	const FNotHumanChallenge& Test() const { return Run.Current(); }

	FNotHumanRun Run;
	EStage Stage = EStage::Intro;
	float StageTime = 0.0f;
	bool bCanSee = false;
	bool bVHeld = false;
	uint32 Seed = 1;

	// The answer being made.
	TArray<bool> TileSelected;
	FString Typed;
	float SliderValue = 50.0f;
	bool bMoved = false;
	FVector2D LastCursor = FVector2D::ZeroVector;
	FVector2D BoxPos = FVector2D(380.0, 150.0);

	// Word association: which word is up, when it went up, and what he typed for each so far.
	int32 FlashIndex = 0;
	float FlashStart = 0.0f;
	TArray<FString> FlashWords;
	TArray<float> FlashTimes;

	// The inkblot's picture, made from the model's pixels when the test comes up.
	UPROPERTY()
	TObjectPtr<UTexture2D> InkTexture;
	FSlateBrush InkBrush;

	// What has been judged, for the verdict's list.
	FNotHumanChallenge Judging;     // the test just answered (Run has moved on)
	FNotHumanVerdict LastVerdict;
	TArray<TPair<FString, bool>> History;

	// The parts of the layout that change.
	TSharedPtr<SBorder> BodyHolder;
	TSharedPtr<SBox> BodyBox;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> DetailText;
	TSharedPtr<STextBlock> ProgressText;
	TSharedPtr<STextBlock> ResultText;
	TSharedPtr<STextBlock> ActionLabel;
	TSharedPtr<SButton> ActionButton;
	TSharedPtr<SEditableTextBox> TextField;
};
