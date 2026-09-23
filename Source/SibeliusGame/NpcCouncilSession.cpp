// NpcCouncilSession.cpp — the council meeting panel. See header.

#include "NpcCouncilSession.h"

#include "NpcChatWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "SibeliusGame.h"

namespace
{
	const TCHAR* Unavailable = TEXT("The council cannot meet right now. You can still continue the story.");
}

bool UNpcCouncilSession::IsAvailable()
{
	// Looked up by name: sib.Chat.Enable is defined in InteractorComponent.cpp, and one
	// variable deciding both doors is the point.
	const IConsoleVariable* Enable = IConsoleManager::Get().FindConsoleVariable(TEXT("sib.Chat.Enable"));
	if (!Enable || Enable->GetInt() == 0)
	{
		return false;
	}

	const ENpcChatMode Mode = UNpcChatSubsystem::CurrentMode();
	return Mode == ENpcChatMode::Mock
		|| (Mode == ENpcChatMode::Proxy && !UNpcChatSubsystem::CouncilUrl().IsEmpty());
}

UNpcChatSubsystem* UNpcCouncilSession::GetChat() const
{
	const APlayerController* PC = OwningPC.Get();
	UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNpcChatSubsystem>() : nullptr;
}

void UNpcCouncilSession::Open(APlayerController* PC)
{
	if (!PC || bOpen)
	{
		return;
	}
	OwningPC = PC;

	if (!Widget)
	{
		Widget = CreateWidget<UNpcChatWidget>(PC, UNpcChatWidget::StaticClass());
		if (!Widget)
		{
			UE_LOG(LogSibeliusGame, Warning, TEXT("[Council] panel failed to create"));
			return;
		}
		Widget->OnSend.BindUObject(this, &UNpcCouncilSession::HandleSend);
		Widget->OnClose.BindUObject(this, &UNpcCouncilSession::HandleClose);
	}

	bOpen = true;

	if (!Widget->IsInViewport())
	{
		Widget->AddToViewport(70);   // the dancer chat's layer: above the menu and every panel
	}
	Widget->SetVisibility(ESlateVisibility::Visible);
	// Tall and centred: no face to keep clear, and five speakers a turn.
	Widget->SetCouncilPanel();
	Widget->SetCouncilButtonVisible(false);   // already at the table
	Widget->ResetTranscript();
	Widget->SetAgentName(TEXT("The Council"));
	Widget->SetStatus(FText::GetEmpty());
	Widget->SetInputEnabled(true);

	if (UNpcChatSubsystem::IsMock())
	{
		Widget->AppendTurn(FString(), TEXT("Placeholder replies. Nothing leaves this machine."));
	}

	/* THE TABLE PICKS UP WHERE IT LEFT OFF, for this play session. Shown for the same
	   reason the dancer panel shows what she remembers: this is exactly what the agents
	   are about to be given, made legible instead of implied. */
	if (const UNpcChatSubsystem* Chat = GetChat())
	{
		const TArray<FNpcCouncilLine>& Table = Chat->GetCouncilHistory();
		if (Table.Num() > 0)
		{
			Widget->AppendTurn(FString(), TEXT("--- earlier at the table ---"));
			for (const FNpcCouncilLine& Line : Table)
			{
				Widget->AppendTurn(Line.DisplayName, Line.Text);
			}
			Widget->AppendTurn(FString(), TEXT("--- now ---"));
		}
		else
		{
			Widget->AppendTurn(FString(),
				TEXT("Kaia, Isla, Aisling, Nyra and Elise are at the table. Say something to all of them."));
		}
	}

	// UIOnly on the text box, as the dancer panel does: game keys stay out while typing.
	FInputModeUIOnly Mode;
	if (TSharedPtr<SWidget> InputSlate = Widget->GetInputSlate())
	{
		Mode.SetWidgetToFocus(InputSlate);
	}
	else
	{
		Mode.SetWidgetToFocus(Widget->TakeWidget());
	}
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	Widget->FocusInput();

	UE_LOG(LogSibeliusGame, Display, TEXT("[Council] meeting opened"));
}

void UNpcCouncilSession::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;

	// Cancel first: a late answer must never land in the next meeting, or be replayed.
	if (RequestId != 0)
	{
		if (UNpcChatSubsystem* Chat = GetChat())
		{
			Chat->Cancel(RequestId);
		}
		RequestId = 0;
	}
	PendingLine.Reset();

	if (Widget)
	{
		// Collapsed, not removed: a callback must never destroy the widget calling it.
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		Widget->SetInputEnabled(true);
	}

	if (APlayerController* PC = OwningPC.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}

	// The keyboard too, not just the input mode — the Generate-twice lesson.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[Council] meeting closed"));
}

void UNpcCouncilSession::HandleClose()
{
	Close();
}

void UNpcCouncilSession::HandleSend(const FString& PlayerLine)
{
	if (!bOpen || !Widget || RequestId != 0)
	{
		return;
	}

	UNpcChatSubsystem* Chat = GetChat();
	if (!Chat)
	{
		Widget->SetStatus(FText::FromString(Unavailable));
		return;
	}

	FOnNpcCouncilReply Reply;
	Reply.BindUObject(this, &UNpcCouncilSession::HandleReply);
	RequestId = Chat->SendCouncil(PlayerLine, Reply);

	if (RequestId == 0)
	{
		Widget->SetStatus(FText::FromString(Chat->IsBusy()
			? TEXT("One at a time - someone is still answering.")
			: Unavailable));
		Widget->FocusInput();
		return;
	}

	PendingLine = PlayerLine;
	Widget->AppendTurn(TEXT("You"), PlayerLine);
	Widget->SetInputEnabled(false);
	// Five agents, one after another: a few seconds, and the status says why.
	Widget->SetStatus(FText::FromString(TEXT("The council is talking it over...")));
}

void UNpcCouncilSession::HandleReply(const TArray<FNpcCouncilLine>& Lines, const FString& CareReply, ENpcChatFailure Failure)
{
	if (!bOpen || !Widget)
	{
		return;
	}
	RequestId = 0;

	if (Failure == ENpcChatFailure::None && !CareReply.IsEmpty())
	{
		/* THE CARE REPLY IS NOT RECORDED. The table stopped for a player who may be in
		   trouble; the next turn should not open with five agents riffing on that line. */
		Widget->AppendTurn(TEXT("The council"), CareReply);
		Widget->SetStatus(FText::GetEmpty());
	}
	else if (Failure == ENpcChatFailure::None)
	{
		for (const FNpcCouncilLine& Line : Lines)
		{
			Widget->AppendTurn(Line.DisplayName, Line.Text);
		}
		Widget->SetStatus(FText::GetEmpty());

		// Recorded together, only now: a turn nobody answered is not a turn the table had.
		if (UNpcChatSubsystem* Chat = GetChat())
		{
			Chat->RecordCouncilTurn(PendingLine, Lines);
		}
	}
	else
	{
		const TCHAR* Line = Unavailable;
		switch (Failure)
		{
		case ENpcChatFailure::Quota:
			Line = TEXT("The council has nothing left to say today. The story continues without them.");
			break;
		case ENpcChatFailure::Network:
			Line = TEXT("No connection. The council needs one; the story does not.");
			break;
		case ENpcChatFailure::Timeout:
			Line = TEXT("The council took too long to answer. Try once more, or leave the table.");
			break;
		case ENpcChatFailure::Blocked:
			Line = TEXT("The council will not answer that one.");
			break;
		default:
			break;
		}
		Widget->SetStatus(FText::FromString(Line));
	}

	PendingLine.Reset();
	Widget->SetInputEnabled(true);
	Widget->FocusInput();
}
