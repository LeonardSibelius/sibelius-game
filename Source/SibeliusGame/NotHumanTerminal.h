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
//
// FREAKBOOK lives on the same laptop (docs/FREAKBOOK.md; the sign says FREAKBOOK LOGIN): notHUMAN's
// LOG IN TO FREAKBOOK swaps its screen for UFreakbookWidget's, and logging out closes the laptop.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "NotHumanTerminal.generated.h"

class UBoxComponent;
class UFreakbookWidget;
class UNotHumanWidget;
class UUserWidget;
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

#if !UE_BUILD_SHIPPING
	/** Development (sib.Freakbook.Open): with the laptop open, Freakbook at one page. */
	void DebugFreakbook(int32 View, int32 Person);
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Close();
	void OpenFreakbook();
	void Focus(UUserWidget* Widget);

	UPROPERTY()
	TObjectPtr<UNotHumanWidget> Screen;

	UPROPERTY()
	TObjectPtr<UFreakbookWidget> Book;

	TWeakObjectPtr<APlayerController> UsingPC;
	bool bOpen = false;
};
