// HangingBanner.cpp - see the header.

#include "HangingBanner.h"

#include "SibeliusGame.h"   // LogSibeliusGame

#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"   // FScreenshotRequest

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace HangingBannerDetail
{
	/* THE PLANE TURNED TO HANG. The engine's plane lies flat, its picture across X and down Y; a
	   viewer in front (+X) looking back has -Y on his right. So the front face sends the plane's X
	   to -Y and its Y to -Z, and the back face the mirror of that, so each side reads the right way
	   round. Rows are the axes the plane's own X, Y, Z become. */
	const FQuat FrontTurn = FMatrix(FVector(0.0, -1.0, 0.0), FVector(0.0, 0.0, -1.0), FVector(1.0, 0.0, 0.0), FVector::ZeroVector).ToQuat();
	const FQuat BackTurn = FMatrix(FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, -1.0), FVector(-1.0, 0.0, 0.0), FVector::ZeroVector).ToQuat();

	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		return Mesh;
	}
}

AHangingBanner::AHangingBanner()
{
	using namespace HangingBannerDetail;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // only -BannerShot wants it

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	Front = Part(this, SceneRoot, TEXT("Front"));
	Back = Part(this, SceneRoot, TEXT("Back"));
	Rod = Part(this, SceneRoot, TEXT("Rod"));
	CordLeft = Part(this, SceneRoot, TEXT("CordLeft"));
	CordRight = Part(this, SceneRoot, TEXT("CordRight"));
}

float AHangingBanner::BannerHeight() const
{
	if (Height > 0.0f)
	{
		return Height;
	}
	const float W = Picture ? Picture->GetSurfaceWidth() : 0.0f;
	const float H = Picture ? Picture->GetSurfaceHeight() : 0.0f;
	return W > 0.0f ? Width * H / W : Width * 0.5f;
}

void AHangingBanner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Hang();
}

void AHangingBanner::Hang()
{
	using namespace HangingBannerDetail;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// The hidden doors' sign material: a texture (Sprite) that glows a little (HiddenDoor.cpp).
	UMaterialInterface* Sign = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SlotFactory/Materials/M_fate_base.M_fate_base"));
	if (!Plane || !Cylinder)
	{
		return;
	}
	const float H = BannerHeight();
	const float Middle = -Drop - H * 0.5f;

	auto Face = [&](UStaticMeshComponent* Mesh, const FQuat& Turn, float Out)
	{
		Mesh->SetStaticMesh(Plane);
		Mesh->SetRelativeTransform(FTransform(Turn, FVector(Out, 0.0, Middle), FVector(Width / 100.0f, H / 100.0f, 1.0f)));
		if (Sign && Picture)
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, Sign))
			{
				MID->SetTextureParameterValue(TEXT("Sprite"), Picture);
				MID->SetScalarParameterValue(TEXT("Glow"), Glow);
			}
		}
	};
	Face(Front, FrontTurn, 0.4f);
	Face(Back, BackTurn, -0.4f);

	// A rod along the top edge, and two cords from it up to the ceiling.
	Rod->SetStaticMesh(Cylinder);
	Rod->SetRelativeTransform(FTransform(FRotator(0.0, 0.0, 90.0), FVector(0.0, 0.0, -Drop), FVector(0.025, 0.025, (Width + 8.0f) / 100.0f)));
	for (UStaticMeshComponent* Cord : { CordLeft.Get(), CordRight.Get() })
	{
		const double Side = Cord == CordLeft ? -1.0 : 1.0;
		Cord->SetStaticMesh(Cylinder);
		Cord->SetRelativeTransform(FTransform(FQuat::Identity, FVector(0.0, Side * Width * 0.4, -Drop * 0.5), FVector(0.006, 0.006, FMath::Max(Drop, 1.0f) / 100.0f)));
	}
}

void AHangingBanner::BeginPlay()
{
	Super::BeginPlay();
	Hang();
	if (FParse::Param(FCommandLine::Get(), TEXT("BannerShot")))
	{
		ShotAge = 0.0f;
		SetActorTickEnabled(true);
	}
}

void AHangingBanner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ShotAge < 0.0f)
	{
		return;
	}
	ShotAge += DeltaSeconds;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	// Three pictures a couple of seconds apart, once the room has streamed in: the front, the
	// back, and the banner in its room from further off.
	const FVector Centre = GetActorLocation() + GetActorUpVector() * (-Drop - BannerHeight() * 0.5f);
	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	struct FShot { float At; FVector Eye; const TCHAR* Name; };
	const FShot Shots[] = {
		{ 8.0f,  Centre + Fwd * 320.0 - FVector(0.0, 0.0, 60.0), TEXT("Banner_front.png") },
		{ 11.0f, Centre - Fwd * 320.0 - FVector(0.0, 0.0, 60.0), TEXT("Banner_back.png") },
		{ 14.0f, Centre + Fwd * 650.0 + Right * 250.0 - FVector(0.0, 0.0, 90.0), TEXT("Banner_room.png") },
	};
	if (ShotsTaken >= UE_ARRAY_COUNT(Shots))
	{
		return;
	}
	const FShot& Shot = Shots[ShotsTaken];
	if (ShotAge < Shot.At - 1.0f)
	{
		return;
	}
	if (!ShotCamera)
	{
		ShotCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Shot.Eye, FRotator::ZeroRotator);
	}
	if (ShotCamera)
	{
		ShotCamera->SetActorLocationAndRotation(Shot.Eye, (Centre - Shot.Eye).Rotation());
		PC->SetViewTargetWithBlend(ShotCamera, 0.0f);
	}
	if (ShotAge >= Shot.At)
	{
		FScreenshotRequest::RequestScreenshot(Shot.Name, /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
		UE_LOG(LogSibeliusGame, Display, TEXT("[Banner] %s: %s"), *GetActorNameOrLabel(), Shot.Name);
		++ShotsTaken;
	}
}
