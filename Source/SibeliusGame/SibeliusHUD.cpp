// SibeliusHUD.cpp — center reticle + developer overlay (SIB-39).

#include "SibeliusHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"                   // GetSmallFont
#include "EngineUtils.h"                      // TActorIterator
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"

#include "InventoryComponent.h"
#include "CompileTypes.h"                     // EResourceType
#include "BuildSite.h"
#include "HatchLock.h"
#include "RefactorableComponent.h"
#include "BranchSubsystem.h"
#include "BranchPIEComponent.h"

bool ASibeliusHUD::bOverlayVisible = true; // default ON

// Dev overlay text scale (2.0 = double size for Walt's 4K monitor). Single knob —
// scales both the glyph size and the line spacing. Bump to taste.
static constexpr float OverlayTextScale = 2.0f;

void ASibeliusHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawCrosshair();

	if (bOverlayVisible)
	{
		DrawDevOverlay();
	}
}

void ASibeliusHUD::DrawCrosshair()
{
	if (!Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float HalfThick = Thickness * 0.5f;

	// A "+" reticle with a small center gap, centered on the exact aim point.
	DrawRect(Color, CX - CenterGap - ArmLength, CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX + CenterGap,             CY - HalfThick, ArmLength, Thickness);
	DrawRect(Color, CX - HalfThick, CY - CenterGap - ArmLength, Thickness, ArmLength);
	DrawRect(Color, CX - HalfThick, CY + CenterGap,             Thickness, ArmLength);
}

void ASibeliusHUD::DrawDevOverlay()
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	float Y = 50.0f;
	auto Line = [this, &Y, Font](const FString& Text, const FLinearColor& Col)
	{
		DrawText(Text, Col, 16.0f, Y, Font, OverlayTextScale); // Scale param doubles the glyphs
		Y += 15.0f * OverlayTextScale;                         // keep line spacing in step
	};

	const FLinearColor Head(0.55f, 0.85f, 1.0f, 1.0f);
	const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor Dim(0.7f, 0.7f, 0.7f, 1.0f);
	const FLinearColor Warn(1.0f, 0.45f, 0.45f, 1.0f);

	Line(TEXT("== DEV OVERLAY ==   (H to hide)"), Head);

	APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	UWorld* W = GetWorld();

	// --- INVENTORY: every resource type + count ---
	Line(TEXT("INVENTORY"), Head);
	UInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (Inv)
	{
		if (const UEnum* ResEnum = StaticEnum<EResourceType>())
		{
			for (int32 i = 0; i < ResEnum->NumEnums(); ++i)
			{
				const FString Name = ResEnum->GetNameStringByIndex(i);
				if (Name.Contains(TEXT("_MAX")))
				{
					continue;
				}
				const int32 Count = Inv->GetCount(static_cast<EResourceType>(ResEnum->GetValueByIndex(i)));
				Line(FString::Printf(TEXT("  %s: %d"), *Name, Count), White);
			}
		}
	}
	else
	{
		Line(TEXT("  (no inventory)"), Dim);
	}

	// --- one world pass: nearest build site + progress counts ---
	ABuildSite* NearSite = nullptr;
	float NearDist = -1.0f;
	int32 RefacTotal = 0, RefacOn = 0, HatchTotal = 0, HatchLocked = 0, SiteTotal = 0, SiteBuilt = 0;
	const FVector PLoc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	if (W)
	{
		for (TActorIterator<ABuildSite> It(W); It; ++It)
		{
			++SiteTotal;
			if (It->IsBuilt()) { ++SiteBuilt; }
			if (Pawn)
			{
				const float D = FVector::Dist(PLoc, It->GetActorLocation());
				if (!NearSite || D < NearDist) { NearSite = *It; NearDist = D; }
			}
		}
		for (TActorIterator<AHatchLock> It(W); It; ++It)
		{
			++HatchTotal;
			if (It->IsLocked()) { ++HatchLocked; }
		}
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			TInlineComponentArray<URefactorableComponent*> Comps(*It);
			for (URefactorableComponent* C : Comps)
			{
				++RefacTotal;
				if (C->IsRefactored()) { ++RefacOn; }
			}
		}
	}

	// --- BUILD: nearest site state (incl. ghost diagnostics — regression still open) ---
	Line(TEXT("BUILD"), Head);
	if (NearSite)
	{
		const bool bGhostHidden = NearSite->GhostMesh ? NearSite->GhostMesh->bHiddenInGame : true;
		const bool bHasMesh = NearSite->GhostMesh && NearSite->GhostMesh->GetStaticMesh() != nullptr;
		Line(FString::Printf(TEXT("  near: %s  dist:%.0f/%.0f"), *NearSite->GetName(), NearDist, NearSite->InteractRadius), White);
		Line(FString::Printf(TEXT("  built:%d  canBuild:%d  cost:%d"),
			NearSite->IsBuilt() ? 1 : 0, NearSite->CanBuild(Inv) ? 1 : 0, NearSite->Cost), White);
		Line(FString::Printf(TEXT("  ghostHidden:%d  ghostMesh:%s"),
			bGhostHidden ? 1 : 0, bHasMesh ? TEXT("SET") : TEXT("NONE!")), bHasMesh ? Dim : Warn);
	}
	else
	{
		Line(TEXT("  (no build site)"), Dim);
	}

	// --- BRANCH state ---
	Line(TEXT("BRANCH"), Head);
	UBranchSubsystem* Branch = W ? W->GetSubsystem<UBranchSubsystem>() : nullptr;
	if (Branch)
	{
		Line(FString::Printf(TEXT("  depth:%d  branched:%s  deployAllowed:%s"),
			Branch->GetDepth(),
			Branch->IsBranched() ? TEXT("yes") : TEXT("no"),
			Branch->CanDeploy() ? TEXT("yes") : TEXT("no")), White);
	}
	else
	{
		Line(TEXT("  (no branch subsystem)"), Dim);
	}
	UBranchPIEComponent* PIE = Pawn ? Pawn->FindComponentByClass<UBranchPIEComponent>() : nullptr;
	Line(FString::Printf(TEXT("  lastDeploy: %s   inputGate: %s"),
		PIE ? *PIE->GetLastDeployStatus() : TEXT("-"),
		(PIE && PIE->IsLoadInputGated()) ? TEXT("ENGAGED") : TEXT("released")), Dim);

	// --- PROGRESS: chapter flags (no score system yet) ---
	Line(TEXT("PROGRESS"), Head);
	Line(FString::Printf(TEXT("  refactored: %d/%d"), RefacOn, RefacTotal), White);
	Line(FString::Printf(TEXT("  hatches locked: %d/%d"), HatchLocked, HatchTotal), White);
	Line(FString::Printf(TEXT("  built sites: %d/%d"), SiteBuilt, SiteTotal), White);
	Line(TEXT("  score: n/a"), Dim);

	// --- CONTROLS: every binding, for reference ---
	Line(TEXT("CONTROLS"), Head);
	Line(TEXT("  F slap    E interact    V vision"), White);
	Line(TEXT("  R refactor    B build"), White);
	Line(TEXT("  6 enter  7 merge  8 discard  9 clear-deploy(dev)  0 deploy"), White);
	Line(TEXT("  H hide/show overlay"), White);
}
