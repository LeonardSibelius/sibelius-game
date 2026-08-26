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

	// ------------------------------------------------------------------
	// THE LEVEL OWNS THE DRESS MESH. BeginPlay must not add or strip one.
	//
	// This used to branch on GetActorNameOrLabel().Contains(TEXT("Kitchen")) —
	// the exact trap BookPickup.h documents. ActorLabel is WITH_EDITORONLY_DATA:
	// the cook strips it, so in a packaged build GetActorNameOrLabel() falls
	// back to the internal object name, which the placement script never set
	// (it only calls set_actor_label). The test was therefore ALWAYS FALSE in
	// the shipped game — so every cauldron took the temple branch below and
	// LOADED a pot. PIE hid the kitchen pot; the packaged build spawned one,
	// floating at the actor's counter height in the middle of the kitchen,
	// with nothing in any log. Map data survives cooking; editor labels do not.
	//
	// No test is needed, because the map data is already right and it cooks:
	//   L_Office_v02  CauldronMesh empty      (the stove furniture is the shop)
	//   L_AI_Temple   CauldronMesh = SM_Pot   (saved on the instance)
	// which is what the header already promises — "leave empty when real props
	// (the stove) play the part". Assign a mesh in the level or get nothing.
	//
	// The fire-looking Niagara that made the old strip necessary is gone too:
	// USauceFluidComponent gates the gas on Role, not on a name.
	// ------------------------------------------------------------------

	if (CauldronMesh && CauldronMesh->GetStaticMesh())
	{
		// A level-assigned pot has to block the camera trace or E never finds it
		// (the temple cauldron relied on the old branch for exactly this).
		CauldronMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CauldronMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	if (ContentsMesh)
	{
		// The sauce surface is revealed by the blend, never shown at rest.
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
