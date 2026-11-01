// GistSite.cpp — see the header.

#include "GistSite.h"
#include "GistPictureSubsystem.h"
#include "GistRules.h"
#include "TableSounds.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

const FName AGistSite::EntryId(TEXT("gist"));
const FName AGistSite::PaintingEntryId(TEXT("painting"));

namespace GistSiteDetail
{
	constexpr float PlaqueTextSize = 4.0f;
	constexpr float HoverHeight = 60.0f;
	constexpr float CanvasSize = 110.0f;      // cm, square: the server paints 1:1
	constexpr float CanvasCentreZ = 150.0f;   // about eye height for him
	constexpr float CanvasLean = 10.0f;       // degrees back, like a real easel
	/* The painting is unlit (M_GistPicture), so its brightness is set here, against the keep's
	   exposure: 1.0 and 0.5 washed it nearly white, 0.25 still pale; at 0.15 it matched the picture
	   the server sent (screenshots 00297-00304, 2026-11-01). */
	constexpr float CanvasBrightness = 0.15f;

	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString FromStd(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	// The word lists ship beside the catalog in Content/Data (staged as loose files).
	std::string ReadWords(const TCHAR* File)
	{
		FString Text;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Data") / File);
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Gist] could not read %s - the gist knows none of its words."), *Path);
		}
		return ToStd(Text);
	}
}

AGistSite::AGistSite()
{
	// ABuildSite starts with Tick off and turns it on for its own animations; a gist always has
	// one to run (the pop-in, then hover, spin...), so it ticks whenever it is shown.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Tags.Add(TEXT("NoWildRefactor"));   // R turns things into animals; a gist is silly enough

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Clay(TEXT("/Game/Gist/M_GistClay.M_GistClay"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/DragonsWager/M_WagerGold.M_WagerGold"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glass(TEXT("/Game/LanternBurst/M_LanternOrb.M_LanternOrb"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PictureMat(TEXT("/Game/Gist/M_GistPicture.M_GistPicture"));
	auto OrPlain = [](UMaterialInterface* M) { return M ? M : Plain.Object.Get(); };   // Plain is static: no capture

	Toy = CreateDefaultSubobject<USceneComponent>(TEXT("Toy"));
	Toy->SetupAttachment(RootComponent);
	UStaticMesh* Shapes[] = {Cube.Object, Sphere.Object, Cylinder.Object, Cone.Object};
	UMaterialInterface* Finishes[] = {OrPlain(Clay.Object), OrPlain(Gold.Object), OrPlain(Glass.Object)};
	for (int32 Shape = 0; Shape < 4; ++Shape)
	{
		for (int32 Finish = 0; Finish < 3; ++Finish)
		{
			auto* Piece = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Piece%d_%d"), Shape, Finish));
			Piece->SetupAttachment(Toy);
			Piece->SetStaticMesh(Shapes[Shape]);
			Piece->SetMaterial(0, Finishes[Finish]);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->NumCustomDataFloats = 4;   // colour, then glow (M_LanternOrb reads it)
			Pieces.Add(Piece);
		}
	}

	// One box for the whole toy: something to bump into, and to aim E at (the interactor
	// sweeps Visibility and stops at the first blocking hit).
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(RootComponent);
	Hitbox->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	PlaquePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaquePlate"));
	PlaquePlate->SetupAttachment(RootComponent);
	PlaquePlate->SetStaticMesh(Cube.Object);
	PlaquePlate->SetMaterial(0, Finishes[1]);
	PlaquePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Plaque = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Plaque"));
	Plaque->SetupAttachment(RootComponent);
	Plaque->SetHorizontalAlignment(EHTA_Center);
	Plaque->SetVerticalAlignment(EVRTA_TextCenter);
	Plaque->SetWorldSize(GistSiteDetail::PlaqueTextSize);
	Plaque->SetTextRenderColor(FColor(45, 28, 10));   // engraved brown on brass

	// THE GIST, illustrated: an easel of clay cubes and a canvas, hidden until a painting is asked for.
	Easel = CreateDefaultSubobject<USceneComponent>(TEXT("Easel"));
	Easel->SetupAttachment(RootComponent);
	EaselFrame = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EaselFrame"));
	EaselFrame->SetupAttachment(Easel);
	EaselFrame->SetStaticMesh(Cube.Object);
	EaselFrame->SetMaterial(0, Finishes[0]);
	EaselFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EaselFrame->NumCustomDataFloats = 4;
	Canvas = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Canvas"));
	Canvas->SetupAttachment(Easel);
	Canvas->SetStaticMesh(PlaneMesh.Object);
	Canvas->SetMaterial(0, OrPlain(PictureMat.Object));
	Canvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Easel->SetVisibility(false, /*bPropagateToChildren=*/true);
}

void AGistSite::PlaceEasel(const FVector& At, float Yaw)
{
	using namespace GistSiteDetail;
	Easel->SetRelativeLocationAndRotation(At, FRotator(0.0f, Yaw, 0.0f));

	if (EaselFrame->GetInstanceCount() == 0)
	{
		const float Ledge = CanvasCentreZ - CanvasSize * 0.5f - 4.0f;
		auto Bar = [this](const FVector& At, const FVector& SizeCm, const FRotator& Turn)
		{
			const int32 I = EaselFrame->AddInstance(FTransform(Turn, At, SizeCm / 100.0f), /*bWorldSpace=*/false);
			EaselFrame->SetCustomData(I, TArray<float>{0.22f, 0.10f, 0.035f, 0.0f}, true);   // walnut, linear
		};
		// Front legs outside the canvas's edges and behind it: at +-42 they crossed the painting.
		Bar(FVector(-16.0f, -(CanvasSize * 0.5f + 6.0f), 100.0f), FVector(5.0f, 5.0f, 205.0f), FRotator(-6.0f, 0.0f, 3.0f));
		Bar(FVector(-16.0f, CanvasSize * 0.5f + 6.0f, 100.0f), FVector(5.0f, 5.0f, 205.0f), FRotator(-6.0f, 0.0f, -3.0f));
		Bar(FVector(-55.0f, 0.0f, 92.0f), FVector(5.0f, 5.0f, 195.0f), FRotator(28.0f, 0.0f, 0.0f));    // back leg
		Bar(FVector(4.0f, 0.0f, Ledge), FVector(16.0f, CanvasSize + 14.0f, 4.0f), FRotator::ZeroRotator); // ledge
		Bar(FVector(-6.0f, 0.0f, CanvasCentreZ + CanvasSize * 0.5f + 4.0f), FVector(6.0f, 30.0f, 7.0f), FRotator::ZeroRotator); // clamp
	}
	/* The engine plane faces +Z with its texture's U along X and V along Y. Turned so the normal faces
	   +X (the front), U runs to the viewer's right and V runs down, then leaning back a little.
	   Pitched to face the front instead, the first painting hung on its side (2026-11-01). */
	Canvas->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, CanvasCentreZ), FRotator(0.0f, -90.0f, 90.0f - CanvasLean));
	Canvas->SetRelativeScale3D(FVector(CanvasSize / 100.0f, CanvasSize / 100.0f, 1.0f));
}

UMaterialInstanceDynamic* AGistSite::CanvasMid()
{
	if (!CanvasMaterial)
	{
		CanvasMaterial = Canvas->CreateDynamicMaterialInstance(0);
		if (CanvasMaterial)
		{
			CanvasMaterial->SetScalarParameterValue(TEXT("Brightness"), GistSiteDetail::CanvasBrightness);
		}
	}
	return CanvasMaterial;
}

void AGistSite::ShowEasel()
{
	// A blank canvas: the artist is at work.
	if (UMaterialInstanceDynamic* Mid = CanvasMid())
	{
		Mid->SetScalarParameterValue(TEXT("Reveal"), 0.0f);
	}
	bEaselShown = true;
	Easel->SetVisibility(IsBuilt(), /*bPropagateToChildren=*/true);
}

void AGistSite::ShowPicture(UTexture2D* Picture, bool bFresh)
{
	if (!Picture)
	{
		return;
	}
	Painting = Picture;
	if (UMaterialInstanceDynamic* Mid = CanvasMid())
	{
		Mid->SetTextureParameterValue(TEXT("Picture"), Picture);
		Mid->SetScalarParameterValue(TEXT("Reveal"), bFresh ? 0.0f : 1.0f);
	}
	PaintReveal = bFresh ? 0.0f : -1.0f;
	bEaselShown = true;
	Easel->SetVisibility(IsBuilt(), /*bPropagateToChildren=*/true);
	if (bFresh)
	{
		TableSounds::Play(this, ETableSound::Chime1000, 0.45f, 1.4f);
	}
}

void AGistSite::HideEasel()
{
	bEaselShown = false;
	PaintReveal = -1.0f;
	Easel->SetVisibility(false, /*bPropagateToChildren=*/true);
}

TUniquePtr<Gist::FRecipe> AGistSite::Plan(const FString& InPrompt, const TArray<FString>& Blocklist)
{
	using namespace GistSiteDetail;
	// The word lists load once; the blocklist is the matcher's own, so the two never disagree.
	static const Gist::FVocab Words = Gist::MakeVocab(ReadWords(TEXT("GistNouns.csv")), ReadWords(TEXT("GistModifiers.csv")), std::string());
	Gist::FVocab Vocab = Words;
	for (const FString& Word : Blocklist)
	{
		Vocab.Blocklist.push_back(ToStd(Word.ToLower()));
	}
	TUniquePtr<Gist::FRecipe> Recipe = MakeUnique<Gist::FRecipe>(Gist::Build(Vocab, ToStd(InPrompt)));
	if (Recipe->bRefused || Recipe->bEmpty || Recipe->Parts.empty())
	{
		return nullptr;
	}
	return Recipe;
}

void AGistSite::AuthorPainting(const FString& InPrompt)
{
	using namespace GistSiteDetail;
	Prompt = InPrompt;
	bPaintingOnly = true;
	for (UInstancedStaticMeshComponent* Piece : Pieces)
	{
		Piece->ClearInstances();
	}
	Pupils.Reset();
	Behaviours = 0;
	// The easel stands where a toy would, facing him; the box is the easel, for bumping and for E.
	PlaceEasel(FVector::ZeroVector, 0.0f);
	Hitbox->SetBoxExtent(FVector(40.0f, CanvasSize * 0.5f + 12.0f, (CanvasCentreZ + CanvasSize * 0.5f) * 0.5f));
	Hitbox->SetRelativeLocation(FVector(-15.0f, 0.0f, (CanvasCentreZ + CanvasSize * 0.5f) * 0.5f));
	MarkGenerated(PaintingEntryId);
	RestoreBranchState(1);
}

AGistSite* AGistSite::Respawn(UWorld* World, const FString& InPrompt, const FTransform& Where, const FGuid& Id, FName Kind)
{
	if (Kind == PaintingEntryId)
	{
		// A painting comes back from the copy kept on disk when it arrived - never asked for twice.
		// Without it there is nothing to show, so nothing is restored.
		const UGistPictureSubsystem* Pictures = World ? UGistPictureSubsystem::Get(World) : nullptr;
		UTexture2D* Picture = Pictures ? Pictures->LoadCached(InPrompt) : nullptr;
		if (!Picture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Gist] the painting of \"%s\" (%s) is not on disk - not restored."), *InPrompt, *Id.ToString());
			return nullptr;
		}
		FActorSpawnParameters PaintingParams;
		PaintingParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGistSite* Painted = World->SpawnActor<AGistSite>(StaticClass(), Where, PaintingParams);
		if (!Painted)
		{
			return nullptr;
		}
		Painted->RestoreBranchId(Id);
		Painted->AuthorPainting(InPrompt);
		Painted->ShowPicture(Picture, /*bFresh=*/false);
		return Painted;
	}

	// The words were vetted when it was first made, so no blocklist here: a recreate.
	TUniquePtr<Gist::FRecipe> Recipe;
	if (World)
	{
		Recipe = Plan(InPrompt, TArray<FString>());
	}
	if (!Recipe)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gist] saved gist \"%s\" (%s) no longer builds anything - not restored."),
			*InPrompt, *Id.ToString());
		return nullptr;
	}
	// AlwaysSpawn, so the saved pose is honoured verbatim, as RespawnGeneratedSite does.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGistSite* Site = World->SpawnActor<AGistSite>(StaticClass(), Where, SpawnParams);
	if (!Site)
	{
		return nullptr;
	}
	Site->RestoreBranchId(Id);
	Site->Author(InPrompt, *Recipe, /*bFreshlyGenerated=*/false);
	return Site;
}

void AGistSite::Author(const FString& InPrompt, const Gist::FRecipe& Recipe, bool bFreshlyGenerated)
{
	using namespace GistSiteDetail;
	Prompt = InPrompt;
	Title = FromStd(Recipe.Title);
	Echo = FromStd(Recipe.Echo);
	Tagline = FromStd(Recipe.Tagline);
	Behaviours = Recipe.Behaviours;

	for (UInstancedStaticMeshComponent* Piece : Pieces)
	{
		Piece->ClearInstances();
	}
	Pupils.Reset();
	FVector LastEye = FVector::ZeroVector;
	float LastEyeSize = 0.0f;
	// The rules pick colours as a painter would (sRGB); materials read custom data as linear
	// light. Passed straight through, every toy came out pale - angry robots read pink.
	auto Linear = [](float C) { return FMath::Pow(FMath::Clamp(C, 0.0f, 1.0f), 2.2f); };
	for (const Gist::FPart& Part : Recipe.Parts)
	{
		const int32 PieceIndex = static_cast<int32>(Part.Shape) * 3 + static_cast<int32>(Part.Finish);
		UInstancedStaticMeshComponent* Piece = Pieces[PieceIndex];
		// The engine's basic shapes are 100 cm, centred, so a part's size is its scale x 100.
		const FVector At(Part.Location.X, Part.Location.Y, Part.Location.Z);
		const FVector Scale = FVector(Part.Size.X, Part.Size.Y, Part.Size.Z) / 100.0f;
		const FTransform Where(FRotator(Part.Rotation.X, Part.Rotation.Y, Part.Rotation.Z), At, Scale);
		const int32 Index = Piece->AddInstance(Where, /*bWorldSpace=*/false);
		const float Glow = Part.Finish == Gist::EFinish::Glass ? 1.0f : 0.0f;
		Piece->SetCustomData(Index, TArray<float>{Linear(Part.Colour.R), Linear(Part.Colour.G), Linear(Part.Colour.B), Glow}, true);

		// Each pupil follows the eye the rules put just before it; Tick turns it to the player.
		if (Part.Role == Gist::ERole::Eye)
		{
			LastEye = At;
			LastEyeSize = Part.Size.X;
		}
		else if (Part.Role == Gist::ERole::Pupil && LastEyeSize > 0.0f)
		{
			Pupils.Add({PieceIndex, Index, LastEye, LastEyeSize * 0.36f, At - LastEye, Scale});
		}
	}

	Gist::FVec Min, Max;
	Gist::Bounds(Recipe.Parts, Min, Max);
	ToyHeight = Max.Z - Min.Z;
	const float Lift = (Behaviours & Gist::Behaviour::Hover) ? HoverHeight : 0.0f;
	Hitbox->SetBoxExtent(FVector((Max.X - Min.X) * 0.5f, (Max.Y - Min.Y) * 0.5f, (Max.Z - Min.Z) * 0.5f).ComponentMax(FVector(10.0f)));
	Hitbox->SetRelativeLocation(FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f + Lift));
	// A brass label on the floor in front, leaning back like one in a museum.
	const FString Text = GetPlaqueText();
	Plaque->SetText(FText::FromString(Text));
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"));
	int32 Longest = 0;
	for (const FString& Line : Lines)
	{
		Longest = FMath::Max(Longest, Line.Len());
	}
	const float Width = FMath::Max(40.0f, Longest * PlaqueTextSize * 0.62f + 12.0f);
	const FRotator Lean(15.0f, 0.0f, 0.0f);
	const FVector PlateAt(Max.X + 45.0f, 0.0f, 22.0f);
	PlaquePlate->SetRelativeLocationAndRotation(PlateAt, Lean);
	PlaquePlate->SetRelativeScale3D(FVector(3.0f, Width, 26.0f) / 100.0f);
	Plaque->SetRelativeLocationAndRotation(PlateAt + Lean.RotateVector(FVector(1.8f, 0.0f, 0.0f)), Lean);

	MarkGenerated(EntryId);
	RestoreBranchState(1);
	if (bFreshlyGenerated)
	{
		OnGeneratedFresh();
	}
}

FString AGistSite::GetPlaqueText() const
{
	return FString::Printf(TEXT("%s\n\"%s\"\n%s"), *Title, *Echo, *Tagline);
}

void AGistSite::OnGeneratedFresh()
{
	// It pops out of nowhere with a little overshoot, and a chime.
	RevealAge = 0.0f;
	Toy->SetRelativeScale3D(FVector(0.01f));
	TableSounds::Play(this, ETableSound::Chime1000, 0.5f, 1.15f);
}

void AGistSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsBuilt())
	{
		return;
	}
	Clock += DeltaSeconds;

	float Pop = 1.0f;
	if (RevealAge >= 0.0f)
	{
		RevealAge += DeltaSeconds;
		const float T = FMath::Min(RevealAge / 0.6f, 1.0f);
		Pop = FMath::Max(0.01f, 1.0f - FMath::Cos(T * PI * 1.5f) * FMath::Exp(-4.0f * T));   // 0 -> ~1.07 -> 1
		if (T >= 1.0f)
		{
			RevealAge = -1.0f;
			Pop = 1.0f;
		}
	}

	using namespace Gist::Behaviour;
	FVector Offset(0.0f);
	FRotator Turn(0.0f);
	FVector Squash(1.0f);
	if (Behaviours & Hover)
	{
		Offset.Z += GistSiteDetail::HoverHeight + FMath::Sin(Clock * 2.0f) * 10.0f;
	}
	if (Behaviours & Spin)
	{
		Turn.Yaw += Clock * 90.0f;
	}
	if (Behaviours & Dance)
	{
		Offset.Z += FMath::Abs(FMath::Sin(Clock * 5.0f)) * 15.0f;
		Turn.Yaw += FMath::Sin(Clock * 5.0f) * 20.0f;
	}
	if (Behaviours & Bounce)
	{
		const float Up = FMath::Abs(FMath::Sin(Clock * 4.0f));
		Offset.Z += Up * 30.0f;
		Squash *= FVector(1.0f + (1.0f - Up) * 0.08f, 1.0f + (1.0f - Up) * 0.08f, 1.0f - (1.0f - Up) * 0.12f);
	}
	if (Behaviours & Wobble)
	{
		const float W = FMath::Sin(Clock * 9.0f) * 0.06f;
		Squash *= FVector(1.0f + W, 1.0f + W, 1.0f - W);
		Turn.Roll += FMath::Sin(Clock * 7.0f) * 4.0f;
	}
	if (Behaviours & Sway)
	{
		Turn.Roll += FMath::Sin(Clock * 1.2f) * 6.0f;
	}
	Toy->SetRelativeLocationAndRotation(Offset, Turn);
	Toy->SetRelativeScale3D(Squash * Pop);

	// A fresh painting fades in over the blank canvas, as if the paint were still going on.
	if (PaintReveal >= 0.0f && CanvasMaterial)
	{
		PaintReveal = FMath::Min(PaintReveal + DeltaSeconds / 1.2f, 1.0f);
		CanvasMaterial->SetScalarParameterValue(TEXT("Reveal"), FMath::SmoothStep(0.0f, 1.0f, PaintReveal));
		if (PaintReveal >= 1.0f)
		{
			PaintReveal = -1.0f;
		}
	}

	// Googly eyes watch him: each pupil eases round its eye toward the camera, lagging a
	// little so a spin or a hop sets them rolling. Not beyond 40 m - nobody can see them there.
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Pupils.Num() > 0 && PC && PC->PlayerCameraManager)
	{
		const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
		if (FVector::DistSquared(Camera, GetActorLocation()) < FMath::Square(4000.0f))
		{
			const FVector Watching = Toy->GetComponentTransform().InverseTransformPosition(Camera);
			for (FGistPupil& Pupil : Pupils)
			{
				const FVector Target = (Watching - Pupil.Eye).GetSafeNormal() * Pupil.Reach;
				Pupil.Offset = FMath::VInterpTo(Pupil.Offset, Target, DeltaSeconds, 6.0f);
				Pieces[Pupil.Piece]->UpdateInstanceTransform(Pupil.Instance,
					FTransform(FRotator::ZeroRotator, Pupil.Eye + Pupil.Offset, Pupil.Scale),
					/*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
			}
		}
	}
}

void AGistSite::RestoreBranchState(uint8 InState)
{
	Super::RestoreBranchState(InState);
	SetShown(InState != 0);
	SetActorTickEnabled(InState != 0);
}

void AGistSite::SetShown(bool bShown)
{
	const bool bToyShown = bShown && !bPaintingOnly;
	Toy->SetVisibility(bToyShown, /*bPropagateToChildren=*/true);
	PlaquePlate->SetVisibility(bToyShown);
	Plaque->SetVisibility(bToyShown);
	Easel->SetVisibility(bShown && bEaselShown, /*bPropagateToChildren=*/true);
	Hitbox->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AGistSite::Interact_Implementation(AActor* Interactor)
{
	// No refund and no Books: it cost sauce, and it goes back where it came from.
	if (IsBuilt())
	{
		RestoreBranchState(0);
		TableSounds::Play(this, ETableSound::Chime1000, 0.35f, 0.8f);
	}
}

FText AGistSite::GetInteractionPrompt_Implementation() const
{
	if (!IsBuilt())
	{
		return FText::GetEmpty();
	}
	return bPaintingOnly ? NSLOCTEXT("Sibelius", "GistTakeDown", "Take the painting down [E]")
		: NSLOCTEXT("Sibelius", "GistPutBack", "Send it back to the supply room [E]");
}
