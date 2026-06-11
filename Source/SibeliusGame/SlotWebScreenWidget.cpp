// SlotWebScreenWidget.cpp — SIB-34 Path A. See header.

#include "SlotWebScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "WebBrowser.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlotWeb, Log, All);

USlotWebScreenWidget::USlotWebScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USlotWebScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SlotWebRoot"));
		WidgetTree->RootWidget = Canvas;

		// Thin gold rim so the page sits in the cabinet's frame language.
		UBorder* Gold = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotWebRim"));
		Gold->SetBrushColor(FLinearColor(0.83f, 0.66f, 0.21f, 1.0f));
		Gold->SetPadding(FMargin(4.0f));

		Browser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("SlotWebBrowser"));
		Gold->SetContent(Browser);

		// SC3 layout rule: stretch anchors, never point-anchor + SetSize.
		// Tall centered band — the page is a portrait cabinet (~700x780 design).
		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Gold))
		{
			CSlot->SetAnchors(FAnchors(0.30f, 0.03f, 0.70f, 0.97f));
			CSlot->SetOffsets(FMargin(0.0f));
		}

		if (!PendingURL.IsEmpty())
		{
			Browser->LoadURL(PendingURL);
		}
	}

	return Super::RebuildWidget();
}

void USlotWebScreenWidget::LoadGame(const FString& URL)
{
	PendingURL = URL;
	if (Browser)
	{
		UE_LOG(LogSlotWeb, Display, TEXT("[SlotWeb] loading %s"), *URL);
		Browser->LoadURL(URL);
	}
}

TSharedPtr<SWidget> USlotWebScreenWidget::GetFocusTarget() const
{
	return Browser ? Browser->GetCachedWidget() : TSharedPtr<SWidget>();
}

FReply USlotWebScreenWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// SW3: catch Esc before Chromium does; everything else (Space, clicks)
	// flows through to the page — its own spin handler takes over.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnClosed.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
