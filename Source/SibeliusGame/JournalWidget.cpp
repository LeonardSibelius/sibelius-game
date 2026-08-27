// JournalWidget.cpp — SIB-41 Journal panel (native UMG, reads docs/NARRATIVE.md).

#include "JournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProgressionSubsystem.h"   // the player's earned memoir record

UJournalWidget::UJournalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UJournalWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// Aged-paper / parchment reading panel (Leonard's notebook).
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JournalBg"));
		Bg->SetBrushColor(FLinearColor(0.92f, 0.88f, 0.78f, 0.97f)); // cream/tan
		Bg->SetPadding(FMargin(28.0f));

		ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("JournalScroll"));

		BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalBody"));
		BodyText->SetAutoWrapText(true);
		BodyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));               // readable on 4K
		BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.10f, 0.05f, 1.0f))); // dark-brown ink

		ScrollBox->AddChild(BodyText);
		Bg->SetContent(ScrollBox);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Bg))
		{
			// Centered reading panel covering most of the screen.
			CSlot->SetAnchors(FAnchors(0.12f, 0.10f, 0.88f, 0.90f));
			CSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}

		ApplyText(); // show whatever's cached (or a placeholder) the instant BodyText exists
	}

	return Super::RebuildWidget();
}

void UJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The tree exists here (AddToViewport just built it) — load the narrative now.
	RefreshFromNarrative();
}

void UJournalWidget::ApplyText()
{
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(JournalText.IsEmpty() ? TEXT("(Journal — no text loaded)") : JournalText));
	}
}

void UJournalWidget::RefreshFromNarrative()
{
	// PK17: staged-first, dev-fallback (the ResolveWebGameURL pattern).
	// Content/Journal/HOW_TO_PLAY.md ships as a NonUFS loose file; docs/ is the
	// editor-time source of truth. (Walt: J is the player's how-to-play guide
	// now — the making-of narrative lives on in docs/NARRATIVE.md for readers.)
	FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Journal/HOW_TO_PLAY.md"));
	if (!FPaths::FileExists(FullPath))
	{
		FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("docs/HOW_TO_PLAY.md"));
	}

	FString Raw;
	if (FFileHelper::LoadFileToString(Raw, *FullPath))
	{
		JournalText = CleanMarkdown(Raw);
		UE_LOG(LogTemp, Display, TEXT("[Journal] loaded %d chars from %s"), JournalText.Len(), *FullPath);
	}
	else
	{
		// Print the attempted path + reason right in the panel so failures are visible.
		const bool bExists = FPaths::FileExists(FullPath);
		JournalText = FString::Printf(
			TEXT("Journal unavailable — could not load the narrative.\n\nPath attempted:\n%s\n\nReason: %s\n\n(docs/ is editor-only; it is not staged into a packaged build — for shipping, bake the text into Content.)"),
			*FullPath, bExists ? TEXT("file exists but the read failed") : TEXT("file not found at that path"));
		UE_LOG(LogTemp, Warning, TEXT("[Journal] FAILED to load '%s' (exists=%d)"), *FullPath, bExists ? 1 : 0);
	}

	JournalText += ComposeMemoirRecord();
	JournalText += ComposeArchitects();
	ApplyText();
}

/* THE ARCHITECTS - the standing invitation, and the toll.

   Walt, 2026-08-27: "I always resented the Arrogant Architects at all of my many coding
   jobs." That is the whole reason this enemy has a name instead of a health bar. The
   Refusers were already refusing; calling them Architects is what tells the player WHY -
   the men who drew the system on a whiteboard and were never once there at 2 a.m. when
   it threw.

   NO NEW SAVE FIELD, for the same reason ComposeMemoirRecord adds none: the number is
   already in FProgressionState::SlotLifetimeMeters, which locked decision 2 says can
   never be cleared. A second copy could only ever disagree with the first.
*/
FString UJournalWidget::ComposeArchitects() const
{
	const UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this);
	if (!Prog)
	{
		return FString();
	}
	const FProgressionState& S = Prog->GetStateForRead();

	FString Out = TEXT("\n\n\nTHE ARCHITECTS\n\n")
		TEXT("    Mrs. Hall keeps an army. She calls them Refusers. They were\n")
		TEXT("    Architects first - the ones who drew the system on a whiteboard\n")
		TEXT("    and went to lunch, and were never once there at two in the morning\n")
		TEXT("    when it threw. Forty years of them. Every job. Every one of them\n")
		TEXT("    certain, and none of them on call.\n\n")
		TEXT("    The cathedral machine is the toll.\n\n");

	if (S.IsBattleQualified())
	{
		Out += FString::Printf(
			TEXT("    It has paid out %lld credits. The toll was %lld. It is paid, and\n")
			TEXT("    the field is waiting.\n"),
			S.BattleCreditsPaid(), FProgressionState::BattleQualifyingCoinOut);
	}
	else
	{
		// The remainder, not just the total. "You have 14,850" is a fact; "you are
		// 14,150 short" is a reason to sit back down.
		const int64 Short = FProgressionState::BattleQualifyingCoinOut - S.BattleCreditsPaid();
		Out += FString::Printf(
			TEXT("    It has paid out %lld credits of the %lld that buys a way in.\n")
			TEXT("    %lld to go. Keep pulling.\n"),
			S.BattleCreditsPaid(), FProgressionState::BattleQualifyingCoinOut, Short);
	}
	return Out;
}

FString UJournalWidget::ComposeMemoirRecord() const
{
	/* THE PLAYER'S OWN RECORD (docs/SPINE.md Move 3).

	   Walt's eight messages to former employers are the strongest writing in this project,
	   and until now each one appeared for twelve seconds at a power unlock and was gone
	   forever — no record, no way to read it again. Forty years spent as loose change.

	   NO NEW SAVE FIELD. "Which memoirs has the player earned" is already answered by
	   state the save holds: the five earned powers are in UnlockedMask, and Code Vision —
	   which a fresh save already owns, so it is never "unlocked" — is marked by the
	   Hall.FirstUse.CodeVision grant claimed the first time V is actually used. Storing a
	   second list would be duplicate state that can disagree with the first. The two
	   PLACARD messages (Bally, San Diego County) are not powers and WILL need storage when
	   they land; that is the point to add a field, not before. */
	const UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this);
	if (!Prog)
	{
		return FString();
	}
	const FProgressionState& S = Prog->GetStateForRead();

	FString Out;
	int32 Count = 0;
	for (int32 i = 0; i < static_cast<int32>(EPowerVerb::Count); ++i)
	{
		const EPowerVerb Verb = static_cast<EPowerVerb>(i);
		if (!S.IsUnlocked(Verb))
		{
			continue;
		}
		// Code Vision is owned from the first frame; it counts only once actually used.
		if (Verb == EPowerVerb::CodeVision && !S.HasClaimed(TEXT("Hall.FirstUse.CodeVision")))
		{
			continue;
		}
		const FString Line = PowerVerbMemoir(Verb);
		if (Line.IsEmpty())
		{
			continue;
		}
		Out += FString::Printf(TEXT("\n%s\n    %s\n"), *PowerVerbDisplayName(Verb), *Line);
		++Count;
	}

	if (Count == 0)
	{
		return TEXT("\n\n\nWHAT I WOULD TELL THEM\n\n")
			TEXT("    Nothing yet. Earn a power and you will have something to say.\n");
	}

	return FString::Printf(
		TEXT("\n\n\nWHAT I WOULD TELL THEM\n\n")
		TEXT("    %d of 6 so far. Forty years of hand-coded systems, and every one of\n")
		TEXT("    them either failed or is being retired.\n%s"),
		Count, *Out);
}

FString UJournalWidget::CleanMarkdown(const FString& Raw)
{
	TArray<FString> Lines;
	Raw.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

	for (FString& Line : Lines)
	{
		// Strip leading ATX header (#) / blockquote (>) markers.
		Line.TrimStartInline();
		while (Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(">")))
		{
			Line.RemoveAt(0);
			Line.TrimStartInline();
		}
		// Drop emphasis asterisks (light cleanup).
		Line.ReplaceInline(TEXT("**"), TEXT(""), ESearchCase::CaseSensitive);
		Line.ReplaceInline(TEXT("*"), TEXT(""), ESearchCase::CaseSensitive);
	}

	return FString::Join(Lines, TEXT("\n"));
}
