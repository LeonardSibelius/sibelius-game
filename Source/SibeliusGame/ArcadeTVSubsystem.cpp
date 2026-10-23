// ArcadeTVSubsystem.cpp — the TVs switched on to Leonard's Arcade. See the header.

#include "ArcadeTVSubsystem.h"
#include "ArcadeLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeTV, Log, All);

namespace ArcadeTVNS
{
	/** Where a TV's glass is, in its mesh's own space. The glass faces the mesh's +Y. */
	struct FGlass
	{
		FVector Center;
		FVector2D Size;   // width along X, height along Z
	};

	/* MEASURED, 2026-10-23, from each mesh's screen section (material slot 1, its vertices):
	     SM_TV_A1 (the living room's, on its stand): x -58.62..58.01, z 6.66..72.89, y 5.72
	     SM_TV_A2 (the bedroom's, on the wall):      x -69.16..69.20, z -36.76..37.83, y 4.25
	   A cooked mesh keeps no vertices the CPU can read, so they are numbers here, not a lookup.
	   Any other TV someone tags gets the front of its bounds - close, if not exact. */
	FGlass GlassOf(const UStaticMesh* Mesh)
	{
		const FString Name = Mesh->GetName();
		if (Name == TEXT("SM_TV_A1"))
		{
			return { FVector(-0.305, 5.72, 39.775), FVector2D(116.63, 66.23) };
		}
		if (Name == TEXT("SM_TV_A2"))
		{
			return { FVector(0.02, 4.25, 0.535), FVector2D(138.36, 74.59) };
		}
		const FBox Box = Mesh->GetBoundingBox();
		return { FVector(Box.GetCenter().X, Box.Max.Y, Box.GetCenter().Z),
			FVector2D(Box.GetSize().X * 0.92, Box.GetSize().Z * 0.85) };
	}

	// As bright as an unlit screen can go (its colours top out at 1): the living room is lit, and
	// the first look, at softer colours, read as a TV in daylight rather than one calling you over.
	/* THE GLOW (Walt, 2026-10-23: "yes do the glow"). Unreal's widget screen material tops out at
	   1, which in the lit living room read as steel, not neon. M_ArcadeTVScreen is that material
	   (Widget3DPassThrough, copied, opaque and one-sided) with a Glow multiplier before its
	   emissive, so the screen can shine past 1 and bloom. Cooked by a DirectoriesToAlwaysCook
	   line: only this path names it. */
	const TCHAR* GlowMaterialPath = TEXT("/Game/ArcadeTV/M_ArcadeTVScreen.M_ArcadeTVScreen");
	TAutoConsoleVariable<float> CVarTVGlow(
		TEXT("sib.Arcade.TVGlow"),
		4.0f,
		TEXT("How brightly the arcade TVs shine (the Glow on M_ArcadeTVScreen). 0 = Unreal's plain widget screen. ")
		TEXT("Read when a level starts."),
		ECVF_Default);

	const FLinearColor Neon(0.12f, 0.7f, 1.0f);   // saturated: at Glow 4 a paler cyan washes out to white
	const FLinearColor Warm(1.0f, 0.92f, 0.72f);
	const FLinearColor Amber(1.0f, 0.8f, 0.3f);

	TSharedRef<SWidget> MakeScreen()
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		auto Rule = [White]()
		{
			return SNew(SBox).HeightOverride(5.0f).WidthOverride(900.0f)
				[
					SNew(SBorder).BorderImage(White).BorderBackgroundColor(Neon * 0.8f)
				];
		};

		// ScaleToFit, so the same screen fits a TV of any shape. Deep navy, not black - a
		// switched-on screen glows even where nothing is written on it - and dark, because the
		// Glow multiplies it too: neon wants the light in the letters, not in the field.
		return SNew(SBorder)
			.BorderImage(White)
			.BorderBackgroundColor(FLinearColor(0.01f, 0.018f, 0.055f, 1.0f))
			.Padding(FMargin(70.0f, 50.0f))
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 26)[ Rule() ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "ArcadeTVTitle", "LEONARD'S ARCADE"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 96))
						.ColorAndOpacity(Neon)
						.ShadowOffset(FVector2D(0.0f, 5.0f))
						.ShadowColorAndOpacity(FLinearColor(0.05f, 0.35f, 1.0f, 0.55f))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "ArcadeTVSub", "games made with one prompt"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 42))
						.ColorAndOpacity(Warm)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 26, 0, 0)[ Rule() ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 34, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "ArcadeTVPress", "PRESS  E"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 40))
						.ColorAndOpacity_Lambda([]()
						{
							// A slow pulse, the way an attract screen asks for a coin.
							const float A = 0.55f + 0.45f * FMath::Sin((float)FPlatformTime::Seconds() * 2.6f);
							return FSlateColor(FLinearColor(Amber.R, Amber.G, Amber.B, A));
						})
					]
				]
			];
	}
}

bool UArcadeTVSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UArcadeTVSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	using namespace ArcadeTVNS;

	int32 Lit = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* TV = *It;
		if (!TV->ActorHasTag(SibArcade::TVTag()))
		{
			continue;
		}
		UStaticMeshComponent* Body = TV->FindComponentByClass<UStaticMeshComponent>();
		if (!Body || !Body->GetStaticMesh())
		{
			continue;
		}
		const FGlass Glass = GlassOf(Body->GetStaticMesh());

		/* The widget's plane faces its +X, so a quarter turn faces it out of the glass (+Y).
		   1280 widget pixels across, scaled to the glass's width: the TV's own (uneven) scale
		   then stretches it exactly as it stretches the glass. 0.35 cm proud of the glass. */
		const float Across = 1280.0f;
		UWidgetComponent* Screen = NewObject<UWidgetComponent>(TV, TEXT("ArcadeScreen"));
		Screen->SetupAttachment(Body);
		Screen->SetRelativeLocation(Glass.Center + FVector(0.0, 0.35, 0.0));
		Screen->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
		Screen->SetRelativeScale3D(FVector(Glass.Size.X / Across));
		Screen->SetWidgetSpace(EWidgetSpace::World);
		Screen->SetDrawSize(FVector2D(Across, Across * Glass.Size.Y / Glass.Size.X));
		Screen->SetPivot(FVector2D(0.5f, 0.5f));
		Screen->SetBlendMode(EWidgetBlendMode::Opaque);
		Screen->SetTwoSided(false);
		Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // E's trace still finds the TV
		Screen->SetGenerateOverlapEvents(false);
		Screen->SetCastShadow(false);
		Screen->SetWindowFocusable(false);
		Screen->SetRedrawTime(1.0f / 30.0f);   // the pulse needs no more
		Screen->RegisterComponent();
		const float Glow = CVarTVGlow.GetValueOnGameThread();
		if (Glow > 0.0f)
		{
			if (UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, GlowMaterialPath))
			{
				Screen->SetMaterial(0, GlowMaterial);
				if (UMaterialInstanceDynamic* MID = Screen->GetMaterialInstance())
				{
					MID->SetScalarParameterValue(TEXT("Glow"), Glow);
				}
			}
			else
			{
				UE_LOG(LogArcadeTV, Warning, TEXT("[ArcadeTV] %s is missing - the TVs fall back to the plain screen"), GlowMaterialPath);
			}
		}
		Screen->SetSlateWidget(MakeScreen());
		++Lit;
	}
	if (Lit > 0)
	{
		UE_LOG(LogArcadeTV, Display, TEXT("[ArcadeTV] %d TV(s) switched on to Leonard's Arcade"), Lit);
	}
}
