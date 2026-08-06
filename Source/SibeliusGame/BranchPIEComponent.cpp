// BranchPIEComponent.cpp — SIB-36 PIE consumers of UBranchSubsystem.

#include "BranchPIEComponent.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "BranchSubsystem.h"
#include "BookPickup.h"
#include "RefuserController.h"
#include "SibeliusGameCharacter.h"
#include "ElsewhereGameMode.h"      // SIB-47: skip deploy restore in the throwaway Elsewhere
#include "BuildSite.h"              // Test-Drive hint: what counts as branchable
#include "HatchLock.h"
#include "RefactorableComponent.h"
#include "UObject/UObjectIterator.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h" // input gate on load (SIB-37)
#include "BrainComponent.h"
#include "EngineUtils.h"            // TActorIterator
#include "Engine/Engine.h"          // GEngine
#include "Engine/World.h"
#include "TimerManager.h"           // SetTimerForNextTick (defer apply-on-load)

UBranchPIEComponent::UBranchPIEComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBranchSubsystem* UBranchPIEComponent::GetBranch() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBranchSubsystem>() : nullptr;
}

void UBranchPIEComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UBranchSubsystem* Branch = GetBranch())
	{
		DepthHandle = Branch->OnBranchDepthChanged.AddUObject(this, &UBranchPIEComponent::OnDepthChanged);
		OnDepthChanged(Branch->GetDepth()); // sync to current state (depth 0 at start)
	}

	// Walt (2026-07-19): the Test-Drive hint's proximity scan (timer, not tick).
	if (UWorld* ScanWorld = GetWorld())
	{
		ScanWorld->GetTimerManager().SetTimer(
			BranchableScanTimer, this, &UBranchPIEComponent::ScanForBranchable, 0.5f, true);
	}

	// Ch5 PIE hook (SIB-37): apply the deployed save on load. Gate input NOW so the
	// player can't act during the load->apply window (spike D2), then run the apply on
	// the next tick — after every branchable's BeginPlay has settled — and ungate.
	SetPlayerInputEnabled(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UBranchPIEComponent::ApplyDeployedOnLoad);
	}
	else
	{
		SetPlayerInputEnabled(true); // no world to defer through — don't leave input gated
	}
}

void UBranchPIEComponent::ApplyDeployedOnLoad()
{
	// SIB-47: the Elsewhere is a throwaway wonder room, NOT the deployed main world —
	// applying the deploy save here re-spawns the main game's generated build sites into
	// it (a stray cube on the curio). Skip the restore when we're in an Elsewhere.
	const UWorld* World = GetWorld();
	const bool bElsewhere = World && Cast<AElsewhereGameMode>(World->GetAuthGameMode()) != nullptr;

	if (!bElsewhere)
	{
		if (UBranchSubsystem* Branch = GetBranch())
		{
			// Load is always at Main; never overlay a deploy onto an open branch.
			if (Branch->GetDepth() == 0)
			{
				Branch->ApplyDeployedSave();
			}
		}
	}
	SetPlayerInputEnabled(true); // ungate — the world is now in its deployed state
}

void UBranchPIEComponent::SetPlayerInputEnabled(bool bEnabled)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		bLoadInputGated = false; // couldn't gate (not possessed yet) — treat as not gated
		return;
	}
	if (bEnabled)
	{
		Pawn->EnableInput(PC);
	}
	else
	{
		Pawn->DisableInput(PC);
	}
	bLoadInputGated = !bEnabled; // accurate gate state for the debug readout
}

void UBranchPIEComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UBranchSubsystem* Branch = GetBranch())
	{
		Branch->OnBranchDepthChanged.Remove(DepthHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BranchableScanTimer);
	}
	Super::EndPlay(Reason);
}

void UBranchPIEComponent::ScanForBranchable()
{
	bNearBranchable = false;
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) { return; }
	const FVector Loc = Owner->GetActorLocation();
	const float RadiusSq = FMath::Square(BranchableHintRadius);

	for (TActorIterator<ABuildSite> It(GetWorld()); It; ++It)
	{
		if (!It->IsConsumed() && FVector::DistSquared(Loc, It->GetActorLocation()) <= RadiusSq)
		{
			bNearBranchable = true;
			return;
		}
	}
	for (TActorIterator<AHatchLock> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(Loc, It->GetActorLocation()) <= RadiusSq)
		{
			bNearBranchable = true;
			return;
		}
	}
	for (TObjectIterator<URefactorableComponent> It; It; ++It)
	{
		const AActor* Refactorable = It->GetOwner();
		if (Refactorable && Refactorable->GetWorld() == GetWorld()
			&& FVector::DistSquared(Loc, Refactorable->GetActorLocation()) <= RadiusSq)
		{
			bNearBranchable = true;
			return;
		}
	}
}

void UBranchPIEComponent::OnDepthChanged(int32 Depth)
{
	ApplyDesaturation(Depth);
	// (The BRANCH marker moved to ASibeliusHUD::DrawBranchLayer — the old
	// AddOnScreenDebugMessage marker is compiled out of Shipping builds.)
	SetPickupsInert(Depth >= 1);
	FreezeRefusers(Depth >= 1);
}

void UBranchPIEComponent::ApplyDesaturation(int32 Depth)
{
	// Target the camera the player view actually renders THROUGH. FindComponentByClass
	// returns the first UCameraComponent on the owner, which isn't guaranteed to be the
	// FP camera — write the wrong one and nothing changes on screen. Pin it to the
	// character's real FP camera; fall back to a generic find only if the owner isn't
	// our character type.
	AActor* Owner = GetOwner();
	UCameraComponent* Cam = nullptr;
	if (ASibeliusGameCharacter* Char = Cast<ASibeliusGameCharacter>(Owner))
	{
		Cam = Char->GetFirstPersonCameraComponent();
	}
	if (!Cam && Owner)
	{
		Cam = Owner->FindComponentByClass<UCameraComponent>();
	}
	if (!Cam)
	{
		return;
	}

	// The camera's own post process blends in LAST (on top of any PostProcessVolume),
	// so this wins as long as the weight is up and each channel is flagged for override.
	Cam->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& PP = Cam->PostProcessSettings;

	if (Depth <= 0)
	{
		// Depth 0 must be EXACTLY normal — release every channel we drive so the
		// office reads as authored (warm), not a faint leftover of the branch grade.
		PP.bOverride_ColorSaturation = false;
		PP.bOverride_ColorContrast = false;
		PP.bOverride_ColorGain = false;
		return;
	}

	// Branched: unmistakably "a different reality".
	//  - saturation collapses hard with depth (depth 1 already strongly drained);
	//  - a cold blue cast fights the warm office lighting;
	//  - a slight contrast lift makes it read crisp/clinical rather than just dull.
	const float Sat = FMath::Clamp(1.0f - Depth * SaturationPerDepth, 0.0f, 1.0f);
	PP.bOverride_ColorSaturation = true;          // flag MUST be set or the value is ignored
	PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0f); // (1,1,1,1) = unchanged, 0 = greyscale

	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.0f); // >1 = crisper midtone separation

	PP.bOverride_ColorGain = true;                // per-channel multiplier: pull warmth, push cold
	PP.ColorGain = FVector4(0.82f, 0.92f, 1.20f, 1.0f); // R down, B up = cold blue cast
}

void UBranchPIEComponent::SetPickupsInert(bool bInert)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Pickups are suspended while branched (locked product decision), so the set
	// is stable for the branch's duration; iterate and flip each.
	for (TActorIterator<ABookPickup> It(World); It; ++It)
	{
		It->SetInert(bInert);
	}
}

void UBranchPIEComponent::FreezeRefusers(bool bFreeze)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Spike T2: FREEZE dynamic actors, never snapshot them. Halting the pawn's
	// time dilation stops movement/anim; stopping the AI brain stops the chase.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		ARefuserController* RC = Pawn ? Cast<ARefuserController>(Pawn->GetController()) : nullptr;
		if (!RC)
		{
			continue;
		}
		Pawn->CustomTimeDilation = bFreeze ? 0.0f : 1.0f;
		if (UBrainComponent* Brain = RC->GetBrainComponent())
		{
			if (bFreeze)
			{
				Brain->StopLogic(TEXT("BranchReality"));
			}
			else
			{
				Brain->RestartLogic();
			}
		}
	}
}

void UBranchPIEComponent::Toast(const FString& Msg, const FColor& Color) const
{
	// Was AddOnScreenDebugMessage — compiled out of Shipping, so Test-Drive's branch and
	// merge feedback was invisible to players. The BRANCH x N marker already moved to the
	// HUD for this reason; this is the rest of the same fix.
	ASibeliusHUD::Toast(this, Msg, 2.5f, FLinearColor::FromSRGBColor(Color));
}

void UBranchPIEComponent::Debug_Enter()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->EnterBranch()
		? FString::Printf(TEXT("EnterBranch -> depth %d"), Branch->GetDepth())
		: TEXT("EnterBranch refused"), FColor::Cyan);
}

void UBranchPIEComponent::Debug_Merge()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->MergeBranch()
		? FString::Printf(TEXT("Merge: kept live -> depth %d"), Branch->GetDepth())
		: TEXT("Merge: nothing to resolve"), FColor::Green);
}

void UBranchPIEComponent::Debug_Discard()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->DiscardBranch()
		? FString::Printf(TEXT("Discard: restored -> depth %d"), Branch->GetDepth())
		: TEXT("Discard: nothing to resolve"), FColor::Orange);
}

void UBranchPIEComponent::Debug_Deploy()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	if (Branch->RequestDeploy())
	{
		// SIB-37: RequestDeploy now persists at Main (Ch5 Phase 1+), so say so.
		Toast(TEXT("DEPLOYED — changes saved"), FColor::Green);
		LastDeployStatus = TEXT("DEPLOYED");
	}
	else
	{
		// Deploy refusal must be legible to the player, not just the log.
		Toast(TEXT("Deploy REFUSED — merge or discard the branch first"), FColor::Red);
		LastDeployStatus = TEXT("refused (branched)");
	}
}

void UBranchPIEComponent::Debug_ClearDeploy()
{
	if (UBranchSubsystem* Branch = GetBranch())
	{
		Branch->ClearDeployedSave();
		Toast(TEXT("Deploy save cleared — fresh authored world on next load"), FColor::Yellow);
		LastDeployStatus = TEXT("cleared");
	}
}
