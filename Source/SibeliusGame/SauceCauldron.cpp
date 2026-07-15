// SauceCauldron.cpp — the Sauce shop (FUN-3, un-stubbed from the June 13 P0).

#include "SauceCauldron.h"
#include "SauceShopWidget.h"
#include "ProgressionSubsystem.h"   // the temple blend's one-time bounty
#include "Engine/Engine.h"          // GEngine screen messages (the blend ceremony)
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ASauceCauldron::ASauceCauldron()
{
	PrimaryActorTick.bCanEverTick = false;   // pure state holder; no tick needed

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// FUN-8.1: the E-target. Invisible; sized/scaled in-editor to wrap the props
	// that play the cauldron (the stove + pots). BookPickup's collision recipe.
	InteractZone = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractZone"));
	InteractZone->SetupAttachment(SceneRoot);
	InteractZone->SetBoxExtent(FVector(50.0f, 50.0f, 35.0f));
	InteractZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractZone->SetCollisionResponseToAllChannels(ECR_Block);
	InteractZone->SetGenerateOverlapEvents(false);
	InteractZone->SetCanEverAffectNavigation(false);

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

	// Walt closed the June TODO: a completed blend PAYS — the temple ceremony
	// grants a one-time sauce bounty (claimed through the progression save, so
	// re-cooking on a revisit gives spectacle, not double riches).
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		constexpr int32 BlendBounty = 100;
		if (Progression->ClaimOneTimeGrant(TEXT("Sauce.TempleBlend")))
		{
			Progression->GrantSauce(BlendBounty);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Emerald,
					FString::Printf(TEXT("THE SAUCE OF ALL KNOWLEDGE IS COMPLETE  +%d SAUCE  (total %d)"),
						BlendBounty, Progression->GetSauce()));
			}
		}
		else if (GEngine)
		{
			// Revisit: the ceremony replays but the bounty was already claimed.
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Silver,
				TEXT("The Sauce of All Knowledge is complete once more. Its riches were already given."));
		}
	}

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
