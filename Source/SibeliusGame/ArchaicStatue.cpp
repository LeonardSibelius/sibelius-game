// ArchaicStatue.cpp - the five old languages. See the header.

#include "ArchaicStatue.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "SibeliusHUD.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace ArchaicStatueDetail
{
	struct FLanguage
	{
		const TCHAR* Mesh;   // /Game/Statues/<this>
		const TCHAR* Name;
		int32 Year;
		/* DRAFTS FOR WALT TO REWRITE IN HIS OWN VOICE (docs/MEMOIR_VOICE.md). Only COBOL's and
		   Java's say anything about his life, and only what he has said himself: COBOL from 1977,
		   Java at IBM in 1995 and on the Army's iKrome until 2026. ASCII only - warnings-as-errors
		   and MSVC C4566. */
		const TCHAR* Line;
	};

	const FLanguage Languages[] =
	{
		{ TEXT("SM_Statue_FORTRAN_Discobolus"), TEXT("FORTRAN"), 1957,
			TEXT("FORTRAN, 1957.\nThe first time a scientist could write a formula and have the machine work it out.\nThe weather forecast still runs on it.") },
		{ TEXT("SM_Statue_COBOL_Doryphoros"), TEXT("COBOL"), 1959,
			TEXT("COBOL, 1959.\nMy first language, in 1977. It was written to read like English, so that managers could check the work.\nIt still moves the money in your bank.") },
		{ TEXT("SM_Statue_BASIC_Kore"), TEXT("BASIC"), 1964,
			TEXT("BASIC, 1964.\n10 PRINT \"HELLO\"   20 GOTO 10\nFor a whole generation, the first time a machine did what a kid told it to.") },
		{ TEXT("SM_Statue_Java_Zeus"), TEXT("JAVA"), 1995,
			TEXT("Java, 1995.\nI wrote it for IBM the year it came out, and for the Army until the day they let me go.\nEvery line typed by hand.") },
		{ TEXT("SM_Statue_Python_Laocoon"), TEXT("PYTHON"), 1991,
			TEXT("Python, 1991.\nThe language the AI writes for me now -\nand the last one anybody will remember typing by hand.") },
	};

	const FLanguage& For(EArchaicLanguage InLanguage)
	{
		const int32 Index = FMath::Clamp(static_cast<int32>(InLanguage), 0, static_cast<int32>(UE_ARRAY_COUNT(Languages)) - 1);
		return Languages[Index];
	}

	/** The tablet: 80 cm wide, 14 deep, 42 high, standing this far in front of the plinth. */
	const FVector TabletSize(80.0f, 14.0f, 42.0f);
	constexpr float TabletGap = 30.0f;
}

AArchaicStatue::AArchaicStatue()
{
	PrimaryActorTick.bCanEverTick = false;

	Statue = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Statue"));
	SetRootComponent(Statue);
	Statue->SetCollisionProfileName(TEXT("BlockAll"));

	Tablet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tablet"));
	Tablet->SetupAttachment(Statue);
	Tablet->SetCollisionProfileName(TEXT("BlockAll"));

	TabletWords = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TabletWords"));
	TabletWords->SetupAttachment(Tablet);
	TabletWords->SetUsingAbsoluteScale(true);   // the tablet is a scaled cube; the words are not
	TabletWords->SetHorizontalAlignment(EHTA_Center);
	TabletWords->SetVerticalAlignment(EVRTA_TextCenter);
	TabletWords->SetWorldSize(9.0f);
	TabletWords->SetTextRenderColor(FColor(236, 214, 160));   // pale gold, on the bronze
	TabletWords->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Tablet->SetStaticMesh(Cube.Object);
	}
	/* A BRONZE PLAQUE, NOT A MARBLE BLOCK. Walt, 2026-10-19, in the temple: "the white name sign in
	   front is still too bright" - a flat face toward Olympus's sun glared white even after the
	   marble was toned down, so the tablet is weathered bronze (M_StatueTablet, import_statues.py)
	   and its letters pale gold, like a museum's label. */
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Bronze(TEXT("/Game/Statues/M_StatueTablet.M_StatueTablet"));
	if (Bronze.Succeeded())
	{
		Tablet->SetMaterial(0, Bronze.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> First(TEXT("/Game/Statues/SM_Statue_FORTRAN_Discobolus.SM_Statue_FORTRAN_Discobolus"));
	if (First.Succeeded())
	{
		Statue->SetStaticMesh(First.Object);
	}
}

FString AArchaicStatue::MeshPathFor(EArchaicLanguage InLanguage)
{
	const TCHAR* Mesh = ArchaicStatueDetail::For(InLanguage).Mesh;
	return FString::Printf(TEXT("/Game/Statues/%s.%s"), Mesh, Mesh);
}

FString AArchaicStatue::Line() const
{
	return LineOverride.IsEmpty() ? FString(ArchaicStatueDetail::For(Language).Line) : LineOverride.ToString();
}

FString AArchaicStatue::TabletText() const
{
	const ArchaicStatueDetail::FLanguage& L = ArchaicStatueDetail::For(Language);
	return FString::Printf(TEXT("%s\n%d"), L.Name, L.Year);
}

void AArchaicStatue::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The language decides the statue. Its mesh is saved with the placed actor, so a cooked level
	// carries it and never runs this.
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPathFor(Language)))
	{
		Statue->SetStaticMesh(Mesh);
	}

	// The tablet stands just in front of the plinth, whatever the statue's footprint.
	const UStaticMesh* Mesh = Statue->GetStaticMesh();
	const FBox Box = Mesh ? Mesh->GetBoundingBox() : FBox(FVector(-50.0f), FVector(50.0f));
	const FVector& Size = ArchaicStatueDetail::TabletSize;
	const float FrontY = Box.Max.Y + ArchaicStatueDetail::TabletGap + Size.Y * 0.5f;
	Tablet->SetRelativeLocation(FVector(Box.GetCenter().X, FrontY, Size.Z * 0.5f));
	Tablet->SetRelativeScale3D(Size / 100.0f);   // the engine cube is a metre

	// On the tablet's front face. A TextRender reads from +X; turned 90 it reads from +Y. The
	// location is in the cube's own units (it spans -50..50 before the tablet's scale), so 51 is
	// just proud of the face.
	TabletWords->SetRelativeLocation(FVector(0.0f, 51.0f, 2.0f));
	TabletWords->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	TabletWords->SetText(FText::FromString(TabletText()));
}

FText AArchaicStatue::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("Sibelius", "ArchaicStatuePrompt", "Read the tablet  [E]");
}

void AArchaicStatue::Interact_Implementation(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->ShowMemoir(Line());
	}
}
