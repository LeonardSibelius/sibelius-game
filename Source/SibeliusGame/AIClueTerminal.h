// AIClueTerminal.h
//
// SIB-43 — the oracle computers. An invisible interaction shell placed over
// an office computer mesh (SM_Monitor_A1 / SM_Laptop_A3 / SM_Keyboard_A1 —
// "because AI comes from computers, right?" — Walt). E at ANY time summons
// the full apparition ceremony with the CURRENT clue:
//   before the slot is played → Clue1Voice ("Find the Carousel of Fates")
//   after  (bSlotPlayed)      → Clue2Voice ("Find the Sauce of all Knowledge")
//
// The shell blocks ECC_Visibility ONLY (CL3) — the player walks through it,
// the focus trace lands on it. The licensed QuadArt actors are never touched
// (CL4). Terminals don't own a god: they find the placed AAIApparition and
// Trigger it (CL6).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "AIClueTerminal.generated.h"

class UBoxComponent;
class USoundBase;

UCLASS()
class SIBELIUSGAME_API AAIClueTerminal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AAIClueTerminal();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere, Category = "Clue Terminal")
	TObjectPtr<UBoxComponent> Shell;

	UPROPERTY(EditAnywhere, Category = "Clue Terminal")
	FText PromptText = FText::FromString(TEXT("Ask the AI [E]"));

	// Clue 1: the opening quest, replayed for anyone who asks (defaults to
	// the apparition's own S_ai_intro if left unset).
	UPROPERTY(EditAnywhere, Category = "Clue Terminal")
	TObjectPtr<USoundBase> Clue1Voice;

	// Clue 2: post-slot — the Sauce of all Knowledge (CL7: may not exist yet;
	// the ceremony runs silent until Walt records it).
	UPROPERTY(EditAnywhere, Category = "Clue Terminal")
	TObjectPtr<USoundBase> Clue2Voice;
};
