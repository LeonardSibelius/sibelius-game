// SauceCauldron.cpp — the Sauce shop (FUN-3, un-stubbed from the June 13 P0).

#include "SauceCauldron.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "SauceShopWidget.h"
#include "SauceFluidComponent.h"
#include "ProgressionSubsystem.h"   // the temple blend's one-time bounty
#include "Engine/Engine.h"          // GEngine screen messages (the blend ceremony)
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ASauceCauldron::ASauceCauldron()
{
	PrimaryActorTick.bCanEverTick = false;

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

	FocusCatcher = CreateDefaultSubobject<USphereComponent>(TEXT("FocusCatcher"));
	FocusCatcher->SetupAttachment(SceneRoot);
	FocusCatcher->InitSphereRadius(1.f);
	FocusCatcher->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FocusCatcher->SetGenerateOverlapEvents(false);
	FocusCatcher->SetCanEverAffectNavigation(false);

	CauldronMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CauldronMesh"));
	CauldronMesh->SetupAttachment(SceneRoot);

	ContentsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContentsMesh"));
	ContentsMesh->SetupAttachment(SceneRoot);
	ContentsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Fluid = CreateDefaultSubobject<USauceFluidComponent>(TEXT("SauceFluid"));
	Fluid->SetupAttachment(SceneRoot);
	Fluid->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Fluid->Role = ESauceFluidRole::Cauldron;
	Fluid->SimmerScale = 0.08f;
}

void ASauceCauldron::BeginPlay()
{
	Super::BeginPlay();

	if (FocusCatcher)
	{
		FocusCatcher->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FString Label = GetActorNameOrLabel();
	const bool bKitchen = Label.Contains(TEXT("Kitchen"));

	if (bKitchen)
	{
		// 0.9.7 kitchen: stove furniture is the shop. Do not leave a hero pot
		// in the aisle. Temple cauldrons must NOT take this path.
		if (CauldronMesh)
		{
			CauldronMesh->SetStaticMesh(nullptr);
			CauldronMesh->SetVisibility(false);
			CauldronMesh->SetHiddenInGame(true);
			CauldronMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (ContentsMesh)
		{
			ContentsMesh->SetStaticMesh(nullptr);
			ContentsMesh->SetVisibility(false);
			ContentsMesh->SetHiddenInGame(true);
		}
		return;
	}

	// Temple (and any other) cauldron: keep / restore the pot. An earlier
	// BeginPlay stripped EVERY cauldron and left fire-looking Niagara on the floor.
	if (CauldronMesh && !CauldronMesh->GetStaticMesh())
	{
		if (UStaticMesh* Pot = LoadObject<UStaticMesh>(nullptr,
			TEXT("/Game/MagicianLabatory/Source/Props/Pot/SM_Pot.SM_Pot")))
		{
			CauldronMesh->SetStaticMesh(Pot);
			CauldronMesh->SetVisibility(true);
			CauldronMesh->SetHiddenInGame(false);
			CauldronMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			CauldronMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}
	}
	if (ContentsMesh)
	{
		ContentsMesh->SetVisibility(false);
		ContentsMesh->SetHiddenInGame(true);
	}
}

void ASauceCauldron::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

bool ASauceCauldron::FeedSauce(float Delta)
{
	if (bComplete)
	{
		return false;   // idempotent once cooked (SS9)
	}

	BlendProgress = FMath::Clamp(BlendProgress + Delta, 0.f, 1.f);
	if (Fluid)
	{
		Fluid->SetBlendHeat(BlendProgress);
	}

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
			ASibeliusHUD::Toast(this,
				FString::Printf(TEXT("THE SAUCE OF ALL KNOWLEDGE IS COMPLETE  +%d SAUCE  (total %d)"),
					BlendBounty, Progression->GetSauce()),
				8.0f, SibeliusToast::Good);
		}
		else
		{
			// Revisit: the ceremony replays but the bounty was already claimed.
			ASibeliusHUD::Toast(this,
				TEXT("The Sauce of All Knowledge is complete once more. Its riches were already given."),
				6.0f, SibeliusToast::Info);
		}
	}

	OnSauceComplete.Broadcast();
	if (Fluid)
	{
		Fluid->NotifyBoilOver();
	}
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

bool ASauceCauldron::IsShopOpen() const
{
	return ShopWidget && ShopWidget->IsInViewport();
}

FText ASauceCauldron::GetInteractionPrompt_Implementation() const
{
	return PromptText;
}
