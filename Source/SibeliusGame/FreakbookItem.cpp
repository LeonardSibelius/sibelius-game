// FreakbookItem.cpp — see the header.

#include "FreakbookItem.h"

#include "FreakbookSubsystem.h"
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace FreakbookItemDetail
{
	bool Whose(const FName Person, EFreakPerson& Out)
	{
		return FFreakbook::FromId(TCHAR_TO_UTF8(*Person.ToString()), Out) && FFreakbook::Profile(Out).Item[0] != '\0';
	}
}

AFreakbookItem::AFreakbookItem()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // the focus trace finds it
	Body->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}
	if (Shape.Succeeded())
	{
		Body->SetMaterial(0, Shape.Object);
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Body);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFreakbookItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Dress();
}

void AFreakbookItem::Dress()
{
	using namespace FreakbookItemDetail;
	EFreakPerson Who = EFreakPerson::Dee;
	if (!Whose(Person, Who))
	{
		return;
	}
	// The cube is 100 cm; the body's scale makes each thing its real size. The label sits on top,
	// in the cube's own units, counter-scaled so it reads at the size written here.
	FVector Scale(0.28, 0.32, 0.06);                 // a binder lying flat: 28 x 32 x 6 cm
	FLinearColor Colour(0.02f, 0.08f, 0.45f);        // linear: a deep binder blue
	FString Words = TEXT("STMT-RUN 1983\nD. PICKERING");
	switch (Who)
	{
	case EFreakPerson::Gunther:
		Scale = FVector(0.16, 0.16, 0.15);           // a coconut, near enough
		Colour = FLinearColor(0.12f, 0.05f, 0.02f);
		Words = TEXT("REAL");
		break;
	case EFreakPerson::Brent:
		Scale = FVector(0.70, 0.10, 0.03);           // a championship belt, laid out
		Colour = FLinearColor(0.80f, 0.55f, 0.08f);
		Words = TEXT("CHAMP");
		break;
	default:
		break;
	}
	Body->SetRelativeScale3D(Scale);
	if (UMaterialInstanceDynamic* Paint = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Paint->SetVectorParameterValue(TEXT("Color"), Colour);
	}
	Label->SetText(FText::FromString(Words));
	Label->SetTextRenderColor(FColor(245, 240, 225));
	Label->SetWorldSize(3.2f);                                     // "STMT-RUN 1983" fits the binder's 32 cm
	Label->SetRelativeLocation(FVector(0.0, 0.0, 51.0));            // just above the top face
	Label->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));           // lying on it, face up
	Label->SetRelativeScale3D(FVector(1.0 / Scale.Z, 1.0 / Scale.Y, 1.0 / Scale.X));
}

void AFreakbookItem::BeginPlay()
{
	Super::BeginPlay();
	Dress();
	if (UFreakbookSubsystem* Book = UFreakbookSubsystem::Get(this))
	{
		ChangedHandle = Book->OnChanged.AddUObject(this, &AFreakbookItem::Refresh);
	}
	Refresh();
}

void AFreakbookItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFreakbookSubsystem* Book = UFreakbookSubsystem::Get(this))
	{
		Book->OnChanged.Remove(ChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AFreakbookItem::Refresh()
{
	using namespace FreakbookItemDetail;
	EFreakPerson Who = EFreakPerson::Dee;
	const UFreakbookSubsystem* Book = UFreakbookSubsystem::Get(this);
	const bool bThere = Book && Whose(Person, Who) && Book->ItemStillThere(Who);
	SetActorHiddenInGame(!bThere);
	SetActorEnableCollision(bThere);
}

FText AFreakbookItem::GetInteractionPrompt_Implementation() const
{
	using namespace FreakbookItemDetail;
	EFreakPerson Who = EFreakPerson::Dee;
	if (IsHidden() || !Whose(Person, Who))
	{
		return FText::GetEmpty();
	}
	switch (Who)
	{
	case EFreakPerson::Gunther: return FText::FromString(TEXT("A real coconut. It sloshes. Take it [E]"));
	case EFreakPerson::Brent:   return FText::FromString(TEXT("A championship belt. Gold-colored plastic. Take it [E]"));
	default:                    return FText::FromString(TEXT("A blue binder: STMT-RUN 1983, D. PICKERING. Take it [E]"));
	}
}

void AFreakbookItem::Interact_Implementation(AActor* Interactor)
{
	using namespace FreakbookItemDetail;
	EFreakPerson Who = EFreakPerson::Dee;
	UFreakbookSubsystem* Book = UFreakbookSubsystem::Get(this);
	if (!Book || IsHidden() || !Whose(Person, Who) || !Book->ItemStillThere(Who))
	{
		return;
	}
	Book->TakeItem(Who);   // Refresh hides it, through OnChanged
	const FFreakProfile& P = FFreakbook::Profile(Who);
	ASibeliusHUD::Toast(this, FString::Printf(TEXT("You took %s's %s. Send it from Freakbook, on the laptop downstairs."),
		*UFreakbookSubsystem::ToFString(P.Short), *UFreakbookSubsystem::ToFString(P.Item)), 6.0f);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Freakbook] Leonard took %s's %s"),
		*UFreakbookSubsystem::ToFString(P.Short), *UFreakbookSubsystem::ToFString(P.Item));
}
