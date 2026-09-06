#include "ProteinMachine.h"
#include "ProgressionSubsystem.h"
#include "SibeliusHUD.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

const FName AProteinMachine::BurgerGrant(TEXT("City.Burger"));
const FName AProteinMachine::CoffeeGrant(TEXT("City.Coffee"));
const FName AProteinMachine::EnhancementGrant(TEXT("City.ProteinEnhanced"));

AProteinMachine::AProteinMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto Part = [&](const TCHAR* Name, UStaticMesh* Shape, const FVector& Position,
		const FVector& Scale, USceneComponent* Parent, bool Solid)
	{
		auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(Shape);
		Mesh->SetRelativeLocation(Position);
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetCollisionEnabled(Solid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		if (Parent == Root) OfficeParts.Add(Mesh);
		return Mesh;
	};
	// Local -X is the open entrance. Flush threshold; no door or travel transition.
	Part(TEXT("Floor"), Cube.Object, FVector(0,0,0), FVector(6.4,8,0.10), Root, true);
	Part(TEXT("BackWall"), Cube.Object, FVector(310,0,180), FVector(0.2,8,3.6), Root, true);
	Part(TEXT("LeftWall"), Cube.Object, FVector(0,-390,180), FVector(6.4,0.2,3.6), Root, true);
	Part(TEXT("RightWall"), Cube.Object, FVector(0,390,180), FVector(6.4,0.2,3.6), Root, true);
	Part(TEXT("Roof"), Cube.Object, FVector(0,0,370), FVector(6.4,8,0.2), Root, true);
	Part(TEXT("Fascia"), Cube.Object, FVector(-320,0,330), FVector(0.18,8,0.9), Root, true);
	Part(TEXT("AccentLeft"), Cube.Object, FVector(-323,-380,145), FVector(0.12,0.12,2.9), Root, false);
	Part(TEXT("AccentRight"), Cube.Object, FVector(-323,380,145), FVector(0.12,0.12,2.9), Root, false);
	Part(TEXT("Pedestal"), Cylinder.Object, FVector(100,0,45), FVector(1.4,1.4,0.8), Root, true);
	Part(TEXT("Projector"), Cylinder.Object, FVector(100,0,90), FVector(1.55,1.55,0.10), Root, false);
	auto Text = [&](const TCHAR* Name, const TCHAR* Words, FVector Position, float Size)
	{
		auto* Label = CreateDefaultSubobject<UTextRenderComponent>(Name);
		Label->SetupAttachment(Root);
		Label->SetRelativeLocation(Position);
		Label->SetRelativeRotation(FRotator(0,180,0));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(Size);
		Label->SetTextRenderColor(FColor(100,235,255));
		Label->SetText(FText::FromString(Words));
		return Label;
	};
	Text(TEXT("CompanySign"), TEXT("Protein Machines Inc."), FVector(-332,0,337), 36);
	Text(TEXT("Tagline"), TEXT("HUMAN ENHANCEMENT FOR SPACE TRAVEL"), FVector(-333,0,307), 19.5f);
	Text(TEXT("ExhibitLabel"), TEXT("MOLECULAR ADAPTATION"), FVector(294,0,280), 22);
	Status = Text(TEXT("Status"), TEXT("[E] Begin space-travel enhancement"), FVector(-12,0,100), 12);
	OfficeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OfficeLight"));
	OfficeLight->SetupAttachment(Root);
	OfficeLight->SetRelativeLocation(FVector(-100,0,270));
	OfficeLight->SetIntensity(1400);
	OfficeLight->SetAttenuationRadius(750);
	OfficeLight->SetLightColor(FLinearColor(0.65,0.85,1.0));
	OfficeLight->SetCastShadows(false);
	DisplayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DisplayPivot"));
	DisplayPivot->SetupAttachment(Root);
	DisplayPivot->SetRelativeLocation(FVector(100,0,184));
	ProteinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProteinMesh"));
	ProteinMesh->SetupAttachment(DisplayPivot);
	ProteinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// A deliberately stylized folded chain, not a scientific structure prediction.
	FVector Previous = FVector::ZeroVector;
	for (int32 Index=0; Index<48; ++Index)
	{
		const float T = Index * 0.38f;
		const FVector Point(48*FMath::Cos(T)+18*FMath::Sin(2.3f*T),
			48*FMath::Sin(T), 57*FMath::Sin(T*0.42f));
		const FString BeadName = FString::Printf(TEXT("ProteinBead%02d"), Index);
		ModelParts.Add(Part(*BeadName, Sphere.Object, Point, FVector(0.17), DisplayPivot, false));
		if (Index>0)
		{
			const FVector Delta = Point-Previous;
			const FString BondName = FString::Printf(TEXT("ProteinBond%02d"), Index);
			auto* Bond = Part(*BondName, Cylinder.Object, (Point+Previous)*0.5,
				FVector(0.045,0.045,Delta.Size()/100.0), DisplayPivot, false);
			Bond->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
			ModelParts.Add(Bond);
		}
		Previous = Point;
	}
	Reach = CreateDefaultSubobject<USphereComponent>(TEXT("Reach"));
	Reach->SetupAttachment(Root);
	Reach->SetRelativeLocation(FVector(20,0,160));
	Reach->InitSphereRadius(85);
	Reach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Reach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Reach->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}

bool AProteinMachine::HasMeal(const FProgressionState& State)
{
	return State.HasClaimed(BurgerGrant) && State.HasClaimed(CoffeeGrant);
}
bool AProteinMachine::HasMeal(const UObject* Context)
{
	const auto* Progress = UProgressionSubsystem::Get(Context);
	return Progress && HasMeal(Progress->GetStateForRead());
}
bool AProteinMachine::IsEnhanced(const UObject* Context)
{
	const auto* Progress = UProgressionSubsystem::Get(Context);
	return Progress && Progress->HasClaimedGrant(EnhancementGrant);
}
void AProteinMachine::BeginPlay()
{
	Super::BeginPlay();
	for (const auto& Part : ModelParts)
	{
		Part->SetHiddenInGame(ProteinMesh->GetStaticMesh()!=nullptr);
	}
	RefreshDisplay();
}
void AProteinMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DisplayPivot->AddLocalRotation(FRotator(0,RotationSpeed*DeltaSeconds,0));
	StatusElapsed += DeltaSeconds;
	if (StatusElapsed >= 0.5f)
	{
		StatusElapsed = 0;
		RefreshDisplay();
	}
}
FText AProteinMachine::GetInteractionPrompt_Implementation() const
{
	if (IsEnhanced(this)) return FText::FromString(TEXT("Space-travel enhancement complete [E]"));
	if (!HasMeal(this)) return FText::FromString(TEXT("Have your burger and coffee at the deli first [E]"));
	return FText::FromString(TEXT("Enhance your body for space travel [E]"));
}
void AProteinMachine::RefreshDisplay()
{
	Status->SetText(GetInteractionPrompt_Implementation());
	Status->SetTextRenderColor(IsEnhanced(this) ? FColor(100,255,160) : FColor(100,235,255));
}
void AProteinMachine::Interact_Implementation(AActor* Interactor)
{
	if (!IsValid(Interactor)) return;
	// The shell belongs to this actor too: touching an outside wall must not enhance him.
	const FVector Local = GetActorTransform().InverseTransformPosition(Interactor->GetActorLocation());
	if (Local.X < -300 || Local.X > 250 || FMath::Abs(Local.Y) > 300 || Local.Z < 0 || Local.Z > 280)
	{
		ASibeliusHUD::Toast(this,TEXT("Step inside Protein Machines Inc. and approach the rotating protein."),4,SibeliusToast::Info);
		return;
	}
	if (IsEnhanced(this))
	{
		ASibeliusHUD::Toast(this,TEXT("Enhancement complete. You are cleared for space travel."),4,SibeliusToast::Good);
		return;
	}
	if (!HasMeal(this))
	{
		ASibeliusHUD::Toast(this,TEXT("Have your burger AND coffee at Jacob's Downtown Deli, then return here."),5,SibeliusToast::Warn);
		return;
	}
	if (auto* Progress = UProgressionSubsystem::Get(this))
	{
		if (Progress->ClaimOneTimeGrant(EnhancementGrant))
		{
			ASibeliusHUD::Toast(this,TEXT("ENHANCEMENT COMPLETE. Your body is adapted for space travel. Proceed to the spaceport."),7,SibeliusToast::Good);
			RefreshDisplay();
		}
	}
}
