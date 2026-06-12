// AIClueTerminal.cpp — SIB-43. See header + docs/sib-43-clue-loop-notes.md.

#include "AIClueTerminal.h"

#include "AIApparition.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "SibeliusProgressSubsystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogClueTerminal, Log, All);

AAIClueTerminal::AAIClueTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	Shell = CreateDefaultSubobject<UBoxComponent>(TEXT("Shell"));
	SetRootComponent(Shell);
	Shell->SetBoxExtent(FVector(45.0f, 45.0f, 35.0f));

	// CL3: visible to the interaction trace, invisible to everything else.
	Shell->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shell->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shell->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Shell->SetGenerateOverlapEvents(false);
}

void AAIClueTerminal::Interact_Implementation(AActor* Interactor)
{
	// CL6: find the one placed god; never spawn a second.
	AAIApparition* Apparition = nullptr;
	for (TActorIterator<AAIApparition> It(GetWorld()); It; ++It)
	{
		Apparition = *It;
		break;
	}
	if (!Apparition)
	{
		UE_LOG(LogClueTerminal, Error, TEXT("[ClueTerminal] no AAIApparition in this level — run build_ai_apparition.py."));
		return;
	}

	bool bSlotPlayed = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
		{
			bSlotPlayed = Progress->bSlotPlayed;
		}
	}

	USoundBase* Voice = bSlotPlayed ? Clue2Voice.Get() : Clue1Voice.Get();
	UE_LOG(LogClueTerminal, Display, TEXT("[ClueTerminal] E — clue %d ceremony."), bSlotPlayed ? 2 : 1);
	Apparition->TriggerApparition(Voice);   // null voice = silent ceremony (CL7)
}

FText AAIClueTerminal::GetInteractionPrompt_Implementation() const
{
	return PromptText;
}
