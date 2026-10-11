// NotHumanTerminal.h
//
// The living-room laptop, under the PROVE YOU'RE NOT HUMAN sign (Walt, 2026-10-11: "I want an
// 'E' trigger at the desk with the laptop to launch the idea"). Like AAIClueTerminal, an
// invisible shell placed over a computer mesh the level already has: it blocks Visibility only,
// so the focus trace lands on it and he walks straight through it, and the purchased laptop
// mesh is never touched.
//
// E opens UNotHumanWidget full screen and gives it the keyboard (UIOnly, cursor on); closing it
// (Esc) gives both back to the game - the input mode AND Slate's keyboard focus, the second half
// that SibeliusGameCharacter::CloseGenerate learned the hard way.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "NotHumanTerminal.generated.h"

class UBoxComponent;
class UNotHumanWidget;
class APlayerController;

UCLASS()
class SIBELIUSGAME_API ANotHumanTerminal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ANotHumanTerminal();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere, Category = "Not Human")
	TObjectPtr<UBoxComponent> Shell;

	UPROPERTY(EditAnywhere, Category = "Not Human")
	FText PromptText = FText::FromString(TEXT("Prove you're not human [E]"));

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Close();

	UPROPERTY()
	TObjectPtr<UNotHumanWidget> Screen;

	TWeakObjectPtr<APlayerController> UsingPC;
	bool bOpen = false;
};
