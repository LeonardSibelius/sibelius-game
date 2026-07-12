// SauceCauldron.cpp — the Sauce shop (FUN-3, un-stubbed from the June 13 P0).

#include "SauceCauldron.h"
#include "SauceShopWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ASauceCauldron::ASauceCauldron()
{
	PrimaryActorTick.bCanEverTick = false;   // pure state holder; no tick needed

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CauldronMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CauldronMesh"));
	CauldronMesh->SetupAttachment(SceneRoot);

	ContentsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContentsMesh"));
	ContentsMesh->SetupAttachment(SceneRoot);
	ContentsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ASauceCauldron::FeedSauce(float Delta)
{
	if (bComplete)
	{
		return false;   // idempotent once cooked (SS9)
	}

	BlendProgress = FMath::Clamp(BlendProgress + Delta, 0.f, 1.f);

	if (BlendProgress >= CompleteThreshold)
	{
		HandleComplete();
		return true;
	}
	return false;
}

void ASauceCauldron::HandleComplete()
{
	if (bComplete)
	{
		return;   // one-shot latch
	}
	bComplete = true;

	UE_LOG(LogTemp, Display, TEXT("[Sauce] The Sauce of All Knowledge is complete (BlendProgress=%.2f)."), BlendProgress);

	// P5 TODO: set USibeliusProgressSubsystem::bSauceComplete and TriggerApparition(clue voice) here.
	OnSauceComplete.Broadcast();
}

void ASauceCauldron::Interact_Implementation(AActor* Interactor)
{
	// FUN-3: E opens the shop. Deterministic spend point — earned Sauce buys
	// powers and upgrades at listed prices (the widget owns the catalog UI;
	// FSauceShop owns the logic).
	APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	if (!ShopWidget)
	{
		ShopWidget = CreateWidget<USauceShopWidget>(PC, USauceShopWidget::StaticClass());
	}
	if (ShopWidget && !ShopWidget->IsInViewport())
	{
		ShopWidget->AddToViewport(70);

		// UIOnly + focus on the widget so Esc/E land in its NativeOnKeyDown and
		// WASD stops moving the pawn (the Generate panel's pattern).
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(ShopWidget->TakeWidget());
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		ShopWidget->SetFocus();
	}
}

FText ASauceCauldron::GetInteractionPrompt_Implementation() const
{
	return PromptText;
}
