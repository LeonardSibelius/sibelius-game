// CoffeeCup.cpp — see the header for why this is not a shop offer.

#include "CoffeeCup.h"

#include "ProgressionSubsystem.h"
#include "SibeliusGame.h"        // LogSibeliusGame
#include "SibeliusHUD.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ACoffeeCup::ACoffeeCup()
{
	PrimaryActorTick.bCanEverTick = false;

	// An empty root that nothing is measured against but everything hangs off. See the
	// header: making the sphere the root is what sent three cups to world origin.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// The food sits AT the actor's location, so placing one puts the burger where you
	// dropped it and the hitbox is somebody else's problem.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// And the target volume floats above it, in the clear air Walt was already aiming at.
	// Query-only: felt by the interact trace, walked through by the player.
	Reach = CreateDefaultSubobject<USphereComponent>(TEXT("Reach"));
	Reach->SetupAttachment(Root);
	Reach->InitSphereRadius(ReachRadius);
	Reach->SetRelativeLocation(FVector(0.0f, 0.0f, ReachHeight));
	Reach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Reach->SetCollisionResponseToAllChannels(ECR_Block);
	Reach->SetHiddenInGame(true);
}

void ACoffeeCup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Live in the editor: drag ReachRadius or ReachHeight and the wireframe moves. The
	// alternative is a rebuild per guess, which is how an afternoon disappears.
	// Reach is NOT the root now, so relative really is relative and this is safe.
	if (Reach)
	{
		Reach->SetSphereRadius(ReachRadius);
		Reach->SetRelativeLocation(FVector(0.0f, 0.0f, ReachHeight));
	}
}

void ACoffeeCup::BeginPlay()
{
	Super::BeginPlay();

	/* SAY SO WHEN THERE IS NO MESH. ACathedralDoor shipped for weeks as a working,
	   invisible, non-collidable nothing that logged "the way is open" while Walt stood
	   in front of it seeing bare wall. The failure is silent by nature: the actor is
	   fine, the prompt is fine, there is simply nothing to look at. One warning at
	   BeginPlay costs nothing and turns an evening into a glance at the log. */
	if (Mesh && !Mesh->GetStaticMesh())
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Coffee] '%s' has no mesh assigned - it is invisible. Set Mesh to a cup "
			     "from RestaurantScene in the Details panel."), *GetName());
	}
}

FText ACoffeeCup::GetInteractionPrompt_Implementation() const
{
	if (bTakenForThisVisit)
	{
		return FText::GetEmpty();   // no prompt on an empty saucer
	}
	FString Line = PromptText;
	Line.ReplaceInline(TEXT("{0}"), *FString::FromInt(Price));
	return FText::FromString(Line);
}

void ACoffeeCup::Interact_Implementation(AActor* Interactor)
{
	if (bTakenForThisVisit)
	{
		return;
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;   // headless / no save: quietly do nothing rather than give it away
	}

	/* TrySpendSauce IS THE CHECK. It refuses and leaves the wallet untouched when the
	   balance is short, so asking "can he afford it" separately would be a second copy
	   of the same rule that could disagree with the first. One call, one answer. */
	if (!Progression->TrySpendSauce(Price))
	{
		FString Line = TooPoorLine;
		Line.ReplaceInline(TEXT("{0}"), *FString::FromInt(Price));
		ASibeliusHUD::Toast(this, Line, 3.0f, SibeliusToast::Warn);
		return;
	}

	if (!MealGrant.IsNone())
	{
		Progression->ClaimOneTimeGrant(MealGrant);
	}

	bTakenForThisVisit = true;

	// Visual AND collision together - the CV4/CV8 lesson the cathedral door records:
	// a hidden thing that still answers the focus trace is a prompt attached to a ghost.
	if (Mesh)
	{
		Mesh->SetHiddenInGame(true);
	}
	// The sphere is what the trace finds, so the sphere is what has to stop answering -
	// hiding only the mesh would leave a prompt attached to an invisible ball.
	if (Reach)
	{
		Reach->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ASibeliusHUD::Toast(this, BoughtLine, 4.0f, SibeliusToast::Good);

	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Coffee] bought for %d Sauce. Balance now %d."),
		Price, Progression->GetStateForRead().Sauce);
}
